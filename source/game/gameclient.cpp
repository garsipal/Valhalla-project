#include "game.h"

namespace game
{
    #include "ctf.h"
    #include "elimination.h"

    clientmode *cmode = NULL;
    ctfclientmode ctfmode;
    eliminationclientmode eliminationmode;

    void setclientmode()
    {
        if(m_ctf) cmode = &ctfmode;
        else if(m_elimination) cmode = &eliminationmode;
        else cmode = NULL;
    }

    bool senditemstoserver = false, sendcrc = false; // after a map change, since server doesn't have map data
    int lastping = 0;

    bool connected = false, remote = false, demoplayback = false, gamepaused = false;
    int sessionid = 0, mastermode = MM_OPEN, gamespeed = 100;
    string servdesc = "", servauth = "", connectpass = "";

    VARP(deadpush, 1, 5, 20);

    void switchname(const char* name)
    {
        filtertext(self->name, name, T_WHITESPACE | T_NAME, MAXNAMELEN, MAXNAMEUNILEN);
        if (!self->name[0]) copystring(self->name, "player");
        if (!name[0])
        {
            copystring(self->name, "player");
        }
        addmsg(N_SWITCHNAME, "rs", self->name);
    }
    void printname()
    {
        conoutf(CON_ECHO, "\fs\f1Your name is:\fr %s", colorname(self));
    }
    ICOMMAND(name, "sN", (char *s, int *numargs),
    {
        if(*numargs > 0) switchname(s);
        else if(!*numargs) printname();
        else result(colorname(self));
    });
    ICOMMAND(getname, "", (), result(self->name));

    void switchteam(const char *team)
    {
        int num = isdigit(team[0]) ? parseint(team) : teamnumber(team);
        if(!validteam(num)) return;
        if(self->clientnum < 0) self->team = num;
        else addmsg(N_SWITCHTEAM, "ri", num);
    }
    void printteam()
    {
        if((self->clientnum >= 0 && !m_teammode) || !validteam(self->team)) conoutf(CON_ECHO, "\fs\f1You are not in a team\fr");
        else conoutf("\fs\f1Your team is:\fr \fs%s%s\fr", teamtextcode[self->team], teamnames[self->team]);
    }
    ICOMMAND(team, "sN", (char *s, int *numargs),
    {
        if(*numargs > 0) switchteam(s);
        else if(!*numargs) printteam();
        else if((self->clientnum < 0 || m_teammode) && validteam(self->team)) result(tempformatstring("\fs%s%s\fr", teamtextcode[self->team], teamnames[self->team]));
    });
    ICOMMAND(getteam, "", (), intret((self->clientnum < 0 || m_teammode) && validteam(self->team) ? self->team : 0));
    ICOMMAND(getteamname, "i", (int *num), result(teamname(*num)));
    ICOMMAND(getteamcolor, "", (), intret(teamtextcolor[m_teammode ? self->team : 0]));
    ICOMMAND(getteamtextcode, "", (), result(teamtextcode[m_teammode ? self->team : 0]));

    struct authkey
    {
        char *name, *key, *desc;
        int lastauth;

        authkey(const char *name, const char *key, const char *desc)
            : name(newstring(name)), key(newstring(key)), desc(newstring(desc)),
              lastauth(0)
        {
        }

        ~authkey()
        {
            DELETEA(name);
            DELETEA(key);
            DELETEA(desc);
        }
    };
    vector<authkey *> authkeys;

    authkey *findauthkey(const char *desc = "")
    {
        loopv(authkeys) if(!strcmp(authkeys[i]->desc, desc) && !strcasecmp(authkeys[i]->name, self->name)) return authkeys[i];
        loopv(authkeys) if(!strcmp(authkeys[i]->desc, desc)) return authkeys[i];
        return NULL;
    }

    VARP(autoauth, 0, 1, 1);

    void addauthkey(const char *name, const char *key, const char *desc)
    {
        loopvrev(authkeys) if(!strcmp(authkeys[i]->desc, desc) && !strcmp(authkeys[i]->name, name)) delete authkeys.remove(i);
        if(name[0] && key[0]) authkeys.add(new authkey(name, key, desc));
    }
    ICOMMAND(authkey, "sss", (char *name, char *key, char *desc), addauthkey(name, key, desc));

    bool hasauthkey(const char *name, const char *desc)
    {
        if(!name[0] && !desc[0]) return authkeys.length() > 0;
        loopvrev(authkeys) if(!strcmp(authkeys[i]->desc, desc) && !strcmp(authkeys[i]->name, name)) return true;
        return false;
    }

    ICOMMAND(hasauthkey, "ss", (char *name, char *desc), intret(hasauthkey(name, desc) ? 1 : 0));

    void genauthkey(const char *secret)
    {
        if(!secret[0]) { conoutf(CON_ERROR, "you must specify a secret password"); return; }
        vector<char> privkey, pubkey;
        genprivkey(secret, privkey, pubkey);
        conoutf("private key: %s", privkey.getbuf());
        conoutf("public key: %s", pubkey.getbuf());
    }
    COMMAND(genauthkey, "s");

    void saveauthkeys()
    {
        string fname = "config/server/auth.cfg";
        stream *f = openfile(path(fname), "w");
        if(!f) { conoutf(CON_ERROR, "failed to open %s for writing", fname); return; }
        loopv(authkeys)
        {
            authkey *a = authkeys[i];
            f->printf("authkey %s %s %s\n", escapestring(a->name), escapestring(a->key), escapestring(a->desc));
        }
        conoutf("saved authkeys to %s", fname);
        delete f;
    }
    COMMAND(saveauthkeys, "");

    void sendmapinfo()
    {
        if(!connected) return;
        sendcrc = true;
        if(self->state!=CS_SPECTATOR || self->privilege || !remote) senditemstoserver = true;
    }

    void writeclientinfo(stream *f)
    {
        f->printf("name %s\n", escapestring(self->name));
    }

    bool allowedittoggle()
    {
        if(editmode) return true;
        if(!m_edit || (isconnected() && multiplayer(false) && !m_edit)) return false;
        return execidentbool("allowedittoggle", true);
    }

    void edittoggled(bool on)
    {
        addmsg(N_EDITMODE, "ri", on ? 1 : 0);
        if (self->state == CS_DEAD)
        {
            setdeathstate(self, true);
        }
        camera::camera.zoomstate.disable();
        self->suicided = self->respawned = -2;
        checkfollow();
    }

    const char *getclientname(int cn)
    {
        gameent *d = getclient(cn);
        return d ? d->name : "";
    }
    ICOMMAND(getclientname, "i", (int *cn), result(getclientname(*cn)));

    ICOMMAND(getclientcolorname, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) result(colorname(d));
    });

    int getclientteam(int cn)
    {
        gameent *d = getclient(cn);
        return m_teammode && d && validteam(d->team) ? d->team : 0;
    }
    ICOMMAND(getclientteam, "i", (int *cn), intret(getclientteam(*cn)));

    int getclientmodel(int cn)
    {
        gameent *d = cn < 0 ? self : getclient(cn);
        return d ? d->playermodel : -1;
    }
    ICOMMAND(getclientmodel, "b", (int *cn), intret(getclientmodel(*cn)));

    int getclientcolor(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->state!=CS_SPECTATOR ? getplayercolor(d, m_teammode && validteam(d->team) ? d->team : 0) : 0xFFFFFF;
    }
    ICOMMAND(getclientcolor, "i", (int *cn), intret(getclientcolor(*cn)));

    const char *getclientcountrycode(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->aitype == AI_NONE ? d->country_code : "";
    }
    ICOMMAND(getclientcountrycode, "i", (int *cn), result(getclientcountrycode(*cn)));

    const char *getclientcountryname(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->aitype == AI_NONE ? d->country_name : "";
    }
    ICOMMAND(getclientcountryname, "i", (int *cn), result(getclientcountryname(*cn)));

    const char *getclientpos(int cn)
    {
        gameent *d = getclient(cn);
        gameent *s = followingplayer(self);
        if(!d || d->state == CS_SPECTATOR || (d != s && !d->holdingflag && !isally(d, s))) return "0 0 0";
        return tempformatstring("%f %f %f", d->o.x, d->o.y, d->o.z);
    }
    ICOMMAND(getclientpos, "i", (int *cn), result(getclientpos(*cn)));

    int getclientyaw(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->state != CS_SPECTATOR ? d->yaw : 0;
    }
    ICOMMAND(getclientyaw, "i", (int *cn), intret(getclientyaw(*cn)));

    ICOMMAND(getclientfrags, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->frags);
    });

    ICOMMAND(getclientscore, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(!d)
        {
            intret(0);
            return;
        }
        if(m_ctf) intret(d->flags);
        else intret(d->points);
    });

    ICOMMAND(getclientdeaths, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->deaths);
    });

    ICOMMAND(getclienthealth, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->health);
    });

    ICOMMAND(getclientmaxhealth, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->maxhealth);
    });

    ICOMMAND(getclientshield, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->shield);
    });

    ICOMMAND(getclientweapon, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->gunselect);
    });

    ICOMMAND(isgunselect, "s", (char* gun),
    {
        gameent* d = followingplayer(self);
        int weapon = getweapon(gun);
        intret(validgun(weapon) && d->gunselect == weapon ? 1 : 0);
    });

    ICOMMAND(getlastswitchattempt, "", (),
    {
        gameent* d = followingplayer(self);
        intret(d->lastswitchattempt);
    });

    ICOMMAND(getclientammo, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->ammo[d->gunselect]);
    });

    ICOMMAND(hasammo, "s", (char* gun),
    {
        gameent *d = followingplayer(self);
        int weapon = getweapon(gun);
        intret(validgun(weapon) && d->ammo[weapon] ? 1 : 0);
    });

    ICOMMAND(getclientpowerup, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->poweruptype);
    });

    ICOMMAND(getclientpowerupmillis, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->powerupmillis);
    });

    ICOMMAND(getlastpickupmillis, "", (),
    {
        gameent *d = followingplayer(self);
        intret(d->lastpickupmillis);
    });

    ICOMMAND(getclientflag, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->holdingflag);
    });

    ICOMMAND(getclientlives, "", (),
    {
        intret(self->lives);
    });

    ICOMMAND(getclientprivilege, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->privilege);
    });

    bool isprivileged(int cn)
    {
        gameent* d = cn < 0 ? self : getclient(cn);
        return d && d->privilege >= PRIV_MASTER;
    }
    ICOMMAND(isprivileged, "b", (int *cn), intret(isprivileged(*cn) ? 1 : 0));

    bool isauth(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->privilege >= PRIV_AUTH;
    }
    ICOMMAND(isauth, "i", (int *cn), intret(isauth(*cn) ? 1 : 0));

    bool isadmin(int cn)
    {
        gameent *d = getclient(cn);
        return d && d->privilege >= PRIV_ADMIN;
    }
    ICOMMAND(isadmin, "i", (int *cn), intret(isadmin(*cn) ? 1 : 0));

    ICOMMAND(getmastermode, "", (), intret(mastermode));
    ICOMMAND(getmastermodename, "i", (int *mm), result(server::mastermodename(*mm, "")));

    bool isspectator(int cn)
    {
        gameent *d = cn < 0 ? self : getclient(cn);
        return d && d->state==CS_SPECTATOR;
    }
    ICOMMAND(isspectator, "b", (int *cn), intret(isspectator(*cn) ? 1 : 0));

    bool isghost(int cn)
    {
        gameent *d = cn < 0 ? self : getclient(cn);
        return d && d->state==CS_SPECTATOR && d->ghost;
    }
    ICOMMAND(isghost, "b", (int *cn), intret(isghost(*cn) ? 1 : 0));

    ICOMMAND(islagged, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d) intret(d->state==CS_LAGGED ? 1 : 0);
    });

    bool isdead(int cn)
    {
        gameent *d = cn < 0 ? self : getclient(cn);
        return d && d->state==CS_DEAD;
    }
    ICOMMAND(isdead, "b", (int *cn), intret(isdead(*cn) ? 1 : 0));

    bool isai(int cn, int type)
    {
        gameent *d = getclient(cn);
        int aitype = type > 0 && type < AI_MAX ? type : AI_BOT;
        return d && d->aitype==aitype;
    }
    ICOMMAND(isai, "ii", (int *cn, int *type), intret(isai(*cn, *type) ? 1 : 0));

    VARP(playersearch, 0, 3, 10);

    int parseplayer(const char *arg)
    {
        char *end;
        int n = strtol(arg, &end, 10);
        if(*arg && !*end)
        {
            if(n!=self->clientnum && !clients.inrange(n)) return -1;
            return n;
        }
        // try case sensitive first
        loopv(players)
        {
            gameent *o = players[i];
            if(!strcmp(arg, o->name)) return o->clientnum;
        }
        // nothing found, try case insensitive
        loopv(players)
        {
            gameent *o = players[i];
            if(cubecaseequal(o->name, arg)) return o->clientnum;
        }
        int len = strlen(arg);
        if(playersearch && len >= playersearch)
        {
            // try case insensitive prefix
            loopv(players)
            {
                gameent *o = players[i];
                if(cubecaseequal(o->name, arg, len)) return o->clientnum;
            }
            // try case insensitive substring
            loopv(players)
            {
                gameent *o = players[i];
                if(cubecasefind(o->name, arg)) return o->clientnum;
            }
        }
        return -1;
    }
    ICOMMAND(getclientnum, "s", (char *name), intret(name[0] ? parseplayer(name) : self->clientnum));

    void listclients(bool local, bool bots)
    {
        vector<char> buf;
        string cn;
        int numclients = 0;
        if(local && connected)
        {
            formatstring(cn, "%d", self->clientnum);
            buf.put(cn, strlen(cn));
            numclients++;
        }
        loopv(clients) if(clients[i] && (bots || clients[i]->aitype == AI_NONE))
        {
            formatstring(cn, "%d", clients[i]->clientnum);
            if(numclients++) buf.add(' ');
            buf.put(cn, strlen(cn));
        }
        buf.add('\0');
        result(buf.getbuf());
    }
    ICOMMAND(listclients, "bb", (int *local, int *bots), listclients(*local>0, *bots!=0));

    void clearbans()
    {
        addmsg(N_CLEARBANS, "r");
    }
    COMMAND(clearbans, "");

    void kick(const char *victim, const char *reason)
    {
        int vn = parseplayer(victim);
        if(vn>=0 && vn!=self->clientnum) addmsg(N_KICK, "ris", vn, reason);
    }
    COMMAND(kick, "ss");

    void mute(const char *victim, const char *reason)
    {
        int vn = parseplayer(victim);
        if(vn>=0 && vn!=self->clientnum) addmsg(N_MUTE, "riis", vn, 1, reason);
    }
    COMMAND(mute, "ss");

    void unmute(const char *victim)
    {
        int vn = parseplayer(victim);
        if(vn>=0 && vn!=self->clientnum) addmsg(N_MUTE, "riis", vn, 0, "");
    }
    COMMAND(unmute, "ss");

    void authkick(const char *desc, const char *victim, const char *reason)
    {
        authkey *a = findauthkey(desc);
        int vn = parseplayer(victim);
        if(a && vn>=0 && vn!=self->clientnum)
        {
            a->lastauth = lastmillis;
            addmsg(N_AUTHKICK, "rssis", a->desc, a->name, vn, reason);
        }
    }
    ICOMMAND(authkick, "ss", (const char *victim, const char *reason), authkick("", victim, reason));
    ICOMMAND(sauthkick, "ss", (const char *victim, const char *reason), if(servauth[0]) authkick(servauth, victim, reason));
    ICOMMAND(dauthkick, "sss", (const char *desc, const char *victim, const char *reason), if(desc[0]) authkick(desc, victim, reason));

    vector<int> ignores;

    void ignore(int cn)
    {
        gameent *d = getclient(cn);
        if(!d || d == self) return;
        conoutf(CON_ECHO, "\fs\f1Ignoring:\fr %s", d->name);
        if(ignores.find(cn) < 0) ignores.add(cn);
    }

    void unignore(int cn)
    {
        if(ignores.find(cn) < 0) return;
        gameent *d = getclient(cn);
        if(d) conoutf(CON_ECHO, "\fs\f1Stopped ignoring:\fr %s", d->name);
        ignores.removeobj(cn);
    }

    bool isignored(int cn) { return ignores.find(cn) >= 0; }

    ICOMMAND(ignore, "s", (char *arg), ignore(parseplayer(arg)));
    ICOMMAND(unignore, "s", (char *arg), unignore(parseplayer(arg)));
    ICOMMAND(isignored, "s", (char *arg), intret(isignored(parseplayer(arg)) ? 1 : 0));

    void setteam(const char *who, const char *team)
    {
        int i = parseplayer(who);
        if(i < 0) return;
        int num = isdigit(team[0]) ? parseint(team) : teamnumber(team);
        if(!validteam(num)) return;
        addmsg(N_SETTEAM, "rii", i, num);
    }
    COMMAND(setteam, "ss");

    void hashpwd(const char *pwd)
    {
        if(self->clientnum<0) return;
        string hash;
        server::hashpassword(self->clientnum, sessionid, pwd, hash);
        result(hash);
    }
    COMMAND(hashpwd, "s");

    void setmaster(const char *arg, const char *who)
    {
        if(!arg[0]) return;
        int val = 1, cn = self->clientnum;
        if(who[0])
        {
            cn = parseplayer(who);
            if(cn < 0) return;
        }
        string hash = "";
        if(!arg[1] && isdigit(arg[0])) val = parseint(arg);
        else
        {
            if(cn != self->clientnum) return;
            server::hashpassword(self->clientnum, sessionid, arg, hash);
        }
        addmsg(N_SETMASTER, "riis", cn, val, hash);
    }
    COMMAND(setmaster, "ss");
    ICOMMAND(mastermode, "i", (int *val), addmsg(N_MASTERMODE, "ri", *val));

    bool tryauth(const char *desc)
    {
        authkey *a = findauthkey(desc);
        if(!a) return false;
        a->lastauth = lastmillis;
        addmsg(N_AUTHTRY, "rss", a->desc, a->name);
        return true;
    }
    ICOMMAND(auth, "s", (char *desc), tryauth(desc));
    ICOMMAND(sauth, "", (), if(servauth[0]) tryauth(servauth));
    ICOMMAND(dauth, "s", (char *desc), if(desc[0]) tryauth(desc));

    ICOMMAND(getservdesc, "", (), result(servdesc));
    ICOMMAND(getservauth, "", (), result(servauth));

    void togglespectator(int val, const char *who)
    {
        int i = who[0] ? parseplayer(who) : self->clientnum;
        if(i>=0) addmsg(N_SPECTATOR, "riii", i, val, 0);
    }
    ICOMMAND(spectator, "is", (int *val, char *who), togglespectator(*val, who));

    ICOMMAND(checkmaps, "", (), addmsg(N_CHECKMAPS, "r"));

    int gamemode = INT_MAX, nextmode = INT_MAX;
    int mutators = 0, nextmutators = 0;
    int scorelimit = 0;
    string clientmap = "";

    void changemapserv(const char *name, int mode, int mut, int _scorelimit) // forced map change from the server
    {
        if(multiplayer(false) && !m_mp(mode))
        {
            conoutf(CON_ERROR, "mode %s (%d) not supported in multiplayer", server::modeprettyname(gamemode), gamemode);
            loopi(NUMGAMEMODES) if(m_mp(STARTGAMEMODE + i)) { mode = STARTGAMEMODE + i; break; }
        }

        gamemode = mode;
        nextmode = mode;
        mutators = nextmutators = mut;
        scorelimit = _scorelimit;
        if(editmode) toggleedit();
        if(m_demo) { entities::resetspawns(); return; }
        if((m_edit && !name[0]) || !load_world(name))
        {
            emptymap(0, true, name);
            senditemstoserver = false;
        }
        startgame();
    }

    void setmode(int mode)
    {
        if(multiplayer(false) && !m_mp(mode))
        {
            conoutf(CON_ERROR, "mode %s (%d) not supported in multiplayer",  server::modeprettyname(mode), mode);
            intret(0);
            return;
        }
        nextmode = mode;
        intret(1);
    }
    ICOMMAND(mode, "i", (int *val), setmode(*val));

    ICOMMAND(getmode, "", (), intret(gamemode));
    ICOMMAND(getnextmode, "", (), intret(m_valid(nextmode) ? nextmode : (remote ? 1 : 0)));
    ICOMMAND(getmodename, "i", (int *mode), result(server::modename(*mode, "")));
    ICOMMAND(getmodedesc, "i", (int *mode), result(server::modedesc(*mode, "")));
    ICOMMAND(getmodeprettyname, "i", (int *mode), result(server::modeprettyname(*mode, "")));

    void setmutator(int mut)
    {
        if(!(nextmutators & mutator[mut].flags))
        {
            nextmutators &= ~(mutator[mut].exclude);
            nextmutators |= mutator[mut].flags;
        }
        else nextmutators &= ~(mutator[mut].flags);
    }
    ICOMMAND(mutator, "i", (int *val), setmutator(*val));

    ICOMMAND(getnextmutators, "", (), intret(nextmutators));
    ICOMMAND(getmutatordesc, "i", (int *mut), result(m_validmutator(*mut) ? mutator[*mut].info : ""));
    ICOMMAND(getmutatorprettyname, "i", (int *mut), result(m_validmutator(*mut) ? mutator[*mut].prettyname : ""));

    ICOMMAND(timeremaining, "i", (int *formatted),
    {
        int val = max(maplimit - lastmillis, 0)/1000;
        if(*formatted) result(tempformatstring("%d:%02d", val/60, val%60));
        else intret(val);
    });
    ICOMMAND(isgamewaiting, "", (), intret(gamewaiting ? 1 : 0));
    ICOMMAND(intermission, "", (), intret(intermission ? 1 : 0));
    ICOMMAND(getscorelimit, "", (), intret(scorelimit));

    ICOMMANDS("m_ctf", "i", (int *mode), { int gamemode = *mode; intret(m_ctf); });
    ICOMMANDS("m_teammode", "i", (int *mode), { int gamemode = *mode; intret(m_teammode); });
    ICOMMANDS("m_elim", "i", (int *mode), { int gamemode = *mode; intret(m_elimination); });
    ICOMMANDS("m_lms", "i", (int *mode), { int gamemode = *mode; intret(m_lms); });
    ICOMMANDS("m_berserk", "i", (int *mode), { int gamemode = *mode; intret(m_berserker); });
    ICOMMANDS("m_infect", "i", (int *mode), { int gamemode = *mode; intret(m_infection); });
    ICOMMANDS("m_invasion", "i", (int *mode), { int gamemode = *mode; intret(m_invasion); });
    ICOMMANDS("m_betrayal", "i", (int *mode), { int gamemode = *mode; intret(m_betrayal); });
    ICOMMANDS("m_dm", "i", (int *mode), { int gamemode = *mode; intret(m_dm); });
    ICOMMANDS("m_demo", "i", (int *mode), { int gamemode = *mode; intret(m_demo); });
    ICOMMANDS("m_edit", "i", (int *mode), { int gamemode = *mode; intret(m_edit); });
    ICOMMANDS("m_lobby", "i", (int *mode), { int gamemode = *mode; intret(m_lobby); });
    ICOMMANDS("m_timed", "i", (int *mode), { int gamemode = *mode; intret(m_timed); });
    ICOMMANDS("m_story", "i", (int *mode), { int gamemode = *mode; intret(m_story); });
    ICOMMANDS("m_insta", "", (), { intret(m_insta(mutators)); });

    void changemap(const char *name, int mode, int muts) // request map change, server may ignore
    {
        if(!remote)
        {
            server::forcemap(name, mode, muts);
            if(!isconnected()) localconnect();
        }
        else if(self->state!=CS_SPECTATOR || self->privilege)
        {
            addmsg(N_MAPVOTE, "rsii", name, mode, muts);
        }
    }
    void changemap(const char *name)
    {
        changemap(name, m_valid(nextmode) ? nextmode : (remote ? 1 : 0), nextmutators);
    }
    ICOMMAND(map, "s", (char *name), changemap(name));

    void forceedit(const char *name)
    {
        changemap(name, 0, 0);
    }

    void forceintermission()
    {
        if(!remote && !hasnonlocalclients()) server::startintermission();
        else addmsg(N_FORCEINTERMISSION, "r");
    }

    void newmap(int size)
    {
        addmsg(N_NEWMAP, "ri", size);
    }

    void updateroundstate(int state)
    {
        if(state & ROUND_END)
        {
            betweenrounds = true;
        }
        else if(state & ROUND_START)
        {
            if(m_hunt && !hunterchosen) hunterchosen = true;
            betweenrounds = false;
        }
        if(state & ROUND_RESET)
        {
            if(m_hunt && hunterchosen) hunterchosen = false;
            removeprojectiles();
        }
        if(state & ROUND_WAIT)
        {
            gamewaiting = true;
        }
        else if(state & ROUND_UNWAIT)
        {
            gamewaiting = false;
        }
    }

    int needclipboard = -1;

    void sendclipboard()
    {
        uchar *outbuf = NULL;
        int inlen = 0, outlen = 0;
        if(!packeditinfo(localedit, inlen, outbuf, outlen))
        {
            outbuf = NULL;
            inlen = outlen = 0;
        }
        packetbuf p(16 + outlen, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_CLIPBOARD);
        putint(p, inlen);
        putint(p, outlen);
        if(outlen > 0) p.put(outbuf, outlen);
        sendclientpacket(p.finalize(), 1);
        needclipboard = -1;
    }

    void edittrigger(const selinfo &sel, int op, int arg1, int arg2, int arg3, const VSlot *vs)
    {
        if(m_edit) switch(op)
        {
            case EDIT_FLIP:
            case EDIT_COPY:
            case EDIT_PASTE:
            case EDIT_DELCUBE:
            {
                switch(op)
                {
                    case EDIT_COPY: needclipboard = 0; break;
                    case EDIT_PASTE:
                        if(needclipboard > 0)
                        {
                            c2sinfo(true);
                            sendclipboard();
                        }
                        break;
                }
                addmsg(N_EDITF + op, "ri9i4",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner);
                break;
            }
            case EDIT_ROTATE:
            {
                addmsg(N_EDITF + op, "ri9i5",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1);
                break;
            }
            case EDIT_MAT:
            case EDIT_FACE:
            {
                addmsg(N_EDITF + op, "ri9i6",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1, arg2);
                break;
            }
            case EDIT_TEX:
            {
                int tex1 = shouldpacktex(arg1);
                if(addmsg(N_EDITF + op, "ri9i6",
                    sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                    sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                    tex1 ? tex1 : arg1, arg2))
                {
                    messages.pad(2);
                    int offset = messages.length();
                    if(tex1) packvslot(messages, arg1);
                    *(ushort *)&messages[offset-2] = lilswap(ushort(messages.length() - offset));
                }
                break;
            }
            case EDIT_REPLACE:
            {
                int tex1 = shouldpacktex(arg1), tex2 = shouldpacktex(arg2);
                if(addmsg(N_EDITF + op, "ri9i7",
                    sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                    sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                    tex1 ? tex1 : arg1, tex2 ? tex2 : arg2, arg3))
                {
                    messages.pad(2);
                    int offset = messages.length();
                    if(tex1) packvslot(messages, arg1);
                    if(tex2) packvslot(messages, arg2);
                    *(ushort *)&messages[offset-2] = lilswap(ushort(messages.length() - offset));
                }
                break;
            }
            case EDIT_CALCLIGHT:
            case EDIT_REMIP:
            {
                addmsg(N_EDITF + op, "r");
                break;
            }
            case EDIT_VSLOT:
            {
                if(addmsg(N_EDITF + op, "ri9i6",
                    sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                    sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                    arg1, arg2))
                {
                    messages.pad(2);
                    int offset = messages.length();
                    packvslot(messages, vs);
                    *(ushort *)&messages[offset-2] = lilswap(ushort(messages.length() - offset));
                }
                break;
            }
            case EDIT_UNDO:
            case EDIT_REDO:
            {
                uchar *outbuf = NULL;
                int inlen = 0, outlen = 0;
                if(packundo(op, inlen, outbuf, outlen))
                {
                    if(addmsg(N_EDITF + op, "ri2", inlen, outlen)) messages.put(outbuf, outlen);
                    delete[] outbuf;
                }
                break;
            }
        }
    }

    void printvar(gameent *d, ident *id)
    {
        if(id) switch(id->type)
        {
            case ID_VAR:
            {
                int val = *id->storage.i;
                string str;
                if(val < 0)
                    formatstring(str, "%d", val);
                else if(id->flags&IDF_HEX && id->maxval==0xFFFFFF)
                    formatstring(str, "0x%.6X (\fs\f3R\fr: %d, \fs\f0G\fr: %d, \fs\f1B\fr: %d)", val, (val>>16)&0xFF, (val>>8)&0xFF, val&0xFF);
                else
                    formatstring(str, id->flags&IDF_HEX ? "0x%X" : "%d", val);
                conoutf(CON_GAMEINFO, "%s \fs\f2set map variable \fr%s\fs\f2 to\fr %s", colorname(d), id->name, str);
                break;
            }
            case ID_FVAR:
                conoutf(CON_GAMEINFO, "%s \fs\f2set map variable \fr%s\fs\f2 to\fr %s", colorname(d), id->name, floatstr(*id->storage.f));
                break;
            case ID_SVAR:
                conoutf(CON_GAMEINFO, "%s \fs\f2set map variable \fr%s\fs\f2 to \fr%s", colorname(d), id->name, *id->storage.s);
                break;
        }
    }

    void vartrigger(ident *id)
    {
        if(!m_edit) return;
        switch(id->type)
        {
            case ID_VAR:
                addmsg(N_EDITVAR, "risi", ID_VAR, id->name, *id->storage.i);
                break;

            case ID_FVAR:
                addmsg(N_EDITVAR, "risf", ID_FVAR, id->name, *id->storage.f);
                break;

            case ID_SVAR:
                addmsg(N_EDITVAR, "riss", ID_SVAR, id->name, *id->storage.s);
                break;
            default: return;
        }
        printvar(self, id);
    }

    void pausegame(bool val)
    {
        if(!connected) return;
        if(!remote) server::forcepaused(val);
        else addmsg(N_PAUSEGAME, "ri", val ? 1 : 0);
    }
    ICOMMAND(pausegame, "i", (int *val), pausegame(*val > 0));
    ICOMMAND(paused, "iN$", (int *val, int *numargs, ident *id),
    {
        if(*numargs > 0) pausegame(clampvar(id, *val, 0, 1) > 0);
        else if(*numargs < 0) intret(gamepaused ? 1 : 0);
        else printvar(id, gamepaused ? 1 : 0);
    });

    bool ispaused()
    { 
        return gamepaused;
    }

    void changegamespeed(int val)
    {
        if(!connected) return;
        if(!remote) server::forcegamespeed(val);
        else addmsg(N_GAMESPEED, "ri", val);
    }
    ICOMMAND(gamespeed, "iN$", (int *val, int *numargs, ident *id),
    {
        if(*numargs > 0) changegamespeed(clampvar(id, *val, 10, 1000));
        else if(*numargs < 0) intret(gamespeed);
        else printvar(id, gamespeed);
    });
    ICOMMAND(prettygamespeed, "", (), result(tempformatstring("%d.%02dx", gamespeed/100, gamespeed%100)));

    int scaletime(int t) { return t*gamespeed; }

    // collect c2s messages conveniently
    vector<uchar> messages;
    int messagecn = -1, messagereliable = false;

    bool addmsg(int type, const char *fmt, ...)
    {
        if(!connected) return false;
        static uchar buf[MAXTRANS];
        ucharbuf p(buf, sizeof(buf));
        putint(p, type);
        int numi = 1, numf = 0, nums = 0, mcn = -1;
        bool reliable = false;
        if(fmt)
        {
            va_list args;
            va_start(args, fmt);
            while(*fmt) switch(*fmt++)
            {
                case 'r': reliable = true; break;
                case 'c':
                {
                    gameent *d = va_arg(args, gameent *);
                    mcn = !d || d == self ? -1 : d->clientnum;
                    break;
                }
                case 'v':
                {
                    int n = va_arg(args, int);
                    int *v = va_arg(args, int *);
                    loopi(n) putint(p, v[i]);
                    numi += n;
                    break;
                }

                case 'i':
                {
                    int n = isdigit(*fmt) ? *fmt++-'0' : 1;
                    loopi(n) putint(p, va_arg(args, int));
                    numi += n;
                    break;
                }
                case 'f':
                {
                    int n = isdigit(*fmt) ? *fmt++-'0' : 1;
                    loopi(n) putfloat(p, (float)va_arg(args, double));
                    numf += n;
                    break;
                }
                case 's': sendstring(va_arg(args, const char *), p); nums++; break;
            }
            va_end(args);
        }
        int num = nums || numf ? 0 : numi, msgsize = server::msgsizelookup(type);
        if(msgsize && num!=msgsize) { fatal("Inconsistent msg size for %d (%d != %d)", type, num, msgsize); }
        if(reliable) messagereliable = true;
        if(mcn != messagecn)
        {
            static uchar mbuf[16];
            ucharbuf m(mbuf, sizeof(mbuf));
            putint(m, N_FROMAI);
            putint(m, mcn);
            messages.put(mbuf, m.length());
            messagecn = mcn;
        }
        messages.put(buf, p.length());
        return true;
    }

    void connectattempt(const char *name, const char *password, const ENetAddress &address)
    {
        copystring(connectpass, password);
    }

    void connectfail()
    {
        memset(connectpass, 0, sizeof(connectpass));
    }

    void gameconnect(bool _remote)
    {
        remote = _remote;
        execident("on_connect");
    }

    void gamedisconnect(bool cleanup)
    {
        if(remote) stopfollowing();
        ignores.setsize(0);
        connected = remote = false;
        self->clientnum = -1;
        servdesc[0] = '\0';
        servauth[0] = '\0';
        if(editmode) toggleedit();
        sessionid = 0;
        mastermode = MM_OPEN;
        messages.setsize(0);
        messagereliable = false;
        messagecn = -1;
        self->respawn();
        self->lifesequence = 0;
        self->state = CS_ALIVE;
        self->privilege = PRIV_NONE;
        sendcrc = senditemstoserver = false;
        demoplayback = false;
        gamepaused = false;
        gamespeed = 100;
        clearclients(false);
        if(cleanup)
        {
            nextmode = gamemode = INT_MAX;
            nextmutators = mutators = 0;
            clientmap[0] = '\0';
        }
        cleargame();
        execident("on_disconnect");
    }

    VARP(chatsound, 0, 1, 2); // 0 = no chat sound, 1 = always plays, 2 = only for whispers

    void toserver(char *text)
    {
        conoutf(CON_CHAT, "%s: \fs%s%s\fr", teamcolorname(self), chatcolor(self), text);
        addmsg(N_TEXT, "rcs", self, text);
    }
    COMMANDN(say, toserver, "C");

    void sayteam(char *text, int sound = -1)
    {
        if(!m_teammode || !validteam(self->team) || (m_round && self->state == CS_DEAD) || self->state == CS_SPECTATOR) return;
        conoutf(CON_TEAMCHAT, "%s \fs%s(team)\fr: \fs%s%s\fr", teamcolorname(self), teamtextcode[self->team], teamtextcode[self->team], text);
        if(sound >= 0 && self->state != CS_DEAD && self->state != CS_SPECTATOR) playsound(sound);
        addmsg(N_SAYTEAM, "rcsi", self, text, sound);
    }
    COMMAND(sayteam, "C");

    void whisper(const char *recipient, const char *text)
    {
        int rcn = parseplayer(recipient);
        gameent *rec = getclient(rcn);
        if(!rec || rec->clientnum < 0 || rec == self || rec->aitype == AI_BOT)
        {
            conoutf(CON_CHAT, "\f5Invalid recipient");
            return;
        }
        addmsg(N_WHISPER, "rcis", self, rec->clientnum, text);
        conoutf(CON_CHAT, "%s \fs\f5(whisper to \fr%s\fs\f5)\fr: \fs\f5%s\fr", teamcolorname(self), colorname(rec), text);
    }
    COMMAND(whisper, "ss");

    ICOMMAND(servcmd, "C", (char *cmd), addmsg(N_SERVCMD, "rs", cmd));

    int lastvoicecom = 0;

    void voicecom(int sound, char *text, bool isteam)
    {
        if(!text || !text[0] || (self->role >= ROLE_BERSERKER && self->role <= ROLE_ZOMBIE)) return;
        if(!lastvoicecom || lastmillis - lastvoicecom > VOICECOM_DELAY)
        {
            if(!isteam || (isteam && !m_teammode))
            {
                if(sound >= 0) msgsound(sound, self);
                toserver(text);
            }
            else
            {
                sayteam(text, sound >= 0 ? sound : -1);
            }
            lastvoicecom = lastmillis;
        }
    }

    static void sendposition(gameent *d, packetbuf &q)
    {
        putint(q, N_POS);
        putuint(q, d->clientnum);
        // 3 bits phys state, 1 bit life sequence, 2 bits move, 2 bits strafe
        uchar physstate = d->physstate | ((d->lifesequence&1)<<3) | ((d->move&3)<<4) | ((d->strafe&3)<<6);
        q.put(physstate);
        ivec o = ivec(vec(d->o.x, d->o.y, d->o.z-d->eyeheight).mul(DMF));
        uint vel = min(int(d->vel.magnitude()*DVELF), 0xFFFF), fall = min(int(d->falling.magnitude()*DVELF), 0xFFFF);
        // 3 bits position, 1 bit velocity, 3 bits falling, 1 bit material, 1 bit crouching
        uint flags = 0;
        if(o.x < 0 || o.x > 0xFFFF) flags |= 1<<0;
        if(o.y < 0 || o.y > 0xFFFF) flags |= 1<<1;
        if(o.z < 0 || o.z > 0xFFFF) flags |= 1<<2;
        if(vel > 0xFF) flags |= 1<<3;
        if(fall > 0)
        {
            flags |= 1<<4;
            if(fall > 0xFF) flags |= 1<<5;
            if(d->falling.x || d->falling.y || d->falling.z > 0) flags |= 1<<6;
        }
        if (lookupmaterial(d->o) & MAT_DAMAGE || lookupmaterial(d->feetpos()) & MAT_DAMAGE || lookupmaterial(d->feetpos()) & MAT_LAVA)
        {
            flags |= 1 << 7;
        }
        if(d->crouching < 0) flags |= 1<<8;
        putuint(q, flags);
        loopk(3)
        {
            q.put(o[k]&0xFF);
            q.put((o[k]>>8)&0xFF);
            if(o[k] < 0 || o[k] > 0xFFFF) q.put((o[k]>>16)&0xFF);
        }
        uint dir = (d->yaw < 0 ? 360 + int(d->yaw)%360 : int(d->yaw)%360) + clamp(int(d->pitch+90), 0, 180)*360;
        q.put(dir&0xFF);
        q.put((dir>>8)&0xFF);
        q.put(clamp(int(d->roll+90), 0, 180));
        q.put(vel&0xFF);
        if(vel > 0xFF) q.put((vel>>8)&0xFF);
        float velyaw, velpitch;
        vectoyawpitch(d->vel, velyaw, velpitch);
        uint veldir = (velyaw < 0 ? 360 + int(velyaw)%360 : int(velyaw)%360) + clamp(int(velpitch+90), 0, 180)*360;
        q.put(veldir&0xFF);
        q.put((veldir>>8)&0xFF);
        if(fall > 0)
        {
            q.put(fall&0xFF);
            if(fall > 0xFF) q.put((fall>>8)&0xFF);
            if(d->falling.x || d->falling.y || d->falling.z > 0)
            {
                float fallyaw, fallpitch;
                vectoyawpitch(d->falling, fallyaw, fallpitch);
                uint falldir = (fallyaw < 0 ? 360 + int(fallyaw)%360 : int(fallyaw)%360) + clamp(int(fallpitch+90), 0, 180)*360;
                q.put(falldir&0xFF);
                q.put((falldir>>8)&0xFF);
            }
        }
    }

    void sendposition(gameent *d, bool reliable)
    {
        if(d->state != CS_ALIVE && d->state != CS_EDITING) return;
        packetbuf q(100, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
        sendposition(d, q);
        sendclientpacket(q.finalize(), 0);
    }

    void sendpositions()
    {
        loopv(players)
        {
            gameent *d = players[i];
            if((d == self || d->ai) && (d->state == CS_ALIVE || d->state == CS_EDITING))
            {
                packetbuf q(100);
                sendposition(d, q);
                for(int j = i+1; j < players.length(); j++)
                {
                    gameent *d = players[j];
                    if((d == self || d->ai) && (d->state == CS_ALIVE || d->state == CS_EDITING))
                        sendposition(d, q);
                }
                sendclientpacket(q.finalize(), 0);
                break;
            }
        }
    }

    void sendmessages()
    {
        packetbuf p(MAXTRANS);
        if(sendcrc)
        {
            p.reliable();
            sendcrc = false;
            const char *mname = getclientmap();
            putint(p, N_MAPCRC);
            sendstring(mname, p);
            putint(p, mname[0] ? getmapcrc() : 0);
        }
        if(senditemstoserver)
        {
            p.reliable();
            entities::putitems(p);
            if(cmode) cmode->senditems(p);
            senditemstoserver = false;
        }
        if(messages.length())
        {
            p.put(messages.getbuf(), messages.length());
            messages.setsize(0);
            if(messagereliable) p.reliable();
            messagereliable = false;
            messagecn = -1;
        }
        if(totalmillis-lastping>250)
        {
            putint(p, N_PING);
            putint(p, totalmillis);
            lastping = totalmillis;
        }
        sendclientpacket(p.finalize(), 1);
    }

    void c2sinfo(bool force) // send update to the server
    {
        static int lastupdate = -1000;
        if(totalmillis - lastupdate < 40 && !force) return; // don't update faster than 30fps
        lastupdate = totalmillis;
        sendpositions();
        sendmessages();
        flushclient();
    }

    void sendintro()
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_CONNECT);
        sendstring(self->name, p);
        putint(p, self->playermodel);
        putint(p, self->playercolor);
        sendstring(self->preferred_flag, p);
        string hash = "";
        if(connectpass[0])
        {
            server::hashpassword(self->clientnum, sessionid, connectpass, hash);
            memset(connectpass, 0, sizeof(connectpass));
        }
        sendstring(hash, p);
        authkey *a = servauth[0] && autoauth ? findauthkey(servauth) : NULL;
        if(a)
        {
            a->lastauth = lastmillis;
            sendstring(a->desc, p);
            sendstring(a->name, p);
        }
        else
        {
            sendstring("", p);
            sendstring("", p);
        }
        sendclientpacket(p.finalize(), 1);
    }

    void updatepos(gameent *d)
    {
        // update the position of other clients in the game in our world
        // don't care if he's in the scenery or other players,
        // just don't overlap with our client

        const float r = self->radius+d->radius;
        const float dx = self->o.x-d->o.x;
        const float dy = self->o.y-d->o.y;
        const float dz = self->o.z-d->o.z;
        const float rz = self->aboveeye+d->eyeheight;
        const float fx = (float)fabs(dx), fy = (float)fabs(dy), fz = (float)fabs(dz);
        if(fx<r && fy<r && fz<rz && self->state!=CS_SPECTATOR && d->state!=CS_DEAD)
        {
            if(fx<fy) d->o.y += dy<0 ? r-fy : -(r-fy);  // push aside
            else      d->o.x += dx<0 ? r-fx : -(r-fx);
        }
        int lagtime = totalmillis-d->lastupdate;
        if(lagtime)
        {
            if(d->state!=CS_SPAWNING && d->lastupdate) d->plag = (d->plag*5+lagtime)/6;
            d->lastupdate = totalmillis;
        }
    }

    void parsepositions(ucharbuf &p)
    {
        int type;
        while(p.remaining()) switch(type = getint(p))
        {
            case N_DEMOPACKET: break;
            case N_POS:                        // position of another client
            {
                int cn = getuint(p), physstate = p.get(), flags = getuint(p);
                vec o, vel, falling;
                float yaw, pitch, roll;
                loopk(3)
                {
                    int n = p.get(); n |= p.get()<<8; if(flags&(1<<k)) { n |= p.get()<<16; if(n&0x800000) n |= ~0U<<24; }
                    o[k] = n/DMF;
                }
                int dir = p.get(); dir |= p.get()<<8;
                yaw = dir%360;
                pitch = clamp(dir/360, 0, 180)-90;
                roll = clamp(int(p.get()), 0, 180)-90;
                int mag = p.get(); if(flags&(1<<3)) mag |= p.get()<<8;
                dir = p.get(); dir |= p.get()<<8;
                vecfromyawpitch(dir%360, clamp(dir/360, 0, 180)-90, 1, 0, vel);
                vel.mul(mag/DVELF);
                if(flags&(1<<4))
                {
                    mag = p.get(); if(flags&(1<<5)) mag |= p.get()<<8;
                    if(flags&(1<<6))
                    {
                        dir = p.get(); dir |= p.get()<<8;
                        vecfromyawpitch(dir%360, clamp(dir/360, 0, 180)-90, 1, 0, falling);
                    }
                    else falling = vec(0, 0, -1);
                    falling.mul(mag/DVELF);
                }
                else falling = vec(0, 0, 0);
                int seqcolor = (physstate>>3)&1;
                gameent *d = getclient(cn);
                if(!d || d->lifesequence < 0 || seqcolor!=(d->lifesequence&1) || d->state==CS_DEAD) continue;
                float oldyaw = d->yaw, oldpitch = d->pitch, oldroll = d->roll;
                d->yaw = yaw;
                d->pitch = pitch;
                d->roll = roll;
                d->move = (physstate>>4)&2 ? -1 : (physstate>>4)&1;
                d->strafe = (physstate>>6)&2 ? -1 : (physstate>>6)&1;
                d->crouching = (flags&(1<<8))!=0 ? -1 : abs(d->crouching);
                vec oldpos(d->o);
                d->o = o;
                d->o.z += d->eyeheight;
                d->vel = vel;
                d->falling = falling;
                d->physstate = physstate&7;
                physics::updatephysstate(d);
                updatepos(d);
                if(smoothmove && d->smoothmillis>=0 && oldpos.dist(d->o) < smoothdist)
                {
                    d->newpos = d->o;
                    d->newyaw = d->yaw;
                    d->newpitch = d->pitch;
                    d->newroll = d->roll;
                    d->o = oldpos;
                    d->yaw = oldyaw;
                    d->pitch = oldpitch;
                    d->roll = oldroll;
                    (d->deltapos = oldpos).sub(d->newpos);
                    d->deltayaw = oldyaw - d->newyaw;
                    if(d->deltayaw > 180) d->deltayaw -= 360;
                    else if(d->deltayaw < -180) d->deltayaw += 360;
                    d->deltapitch = oldpitch - d->newpitch;
                    d->deltaroll = oldroll - d->newroll;
                    d->smoothmillis = lastmillis;
                }
                else d->smoothmillis = 0;
                if(d->state==CS_LAGGED || d->state==CS_SPAWNING)
                {
                    if(d->state == CS_SPAWNING)
                    {
                        spawneffect(d);
                    }
                    d->state = CS_ALIVE;
                }
                break;
            }

            case N_TELEPORT:
            {
                int cn = getint(p), tp = getint(p), td = getint(p);
                gameent *d = getclient(cn);
                if(!d || d->lifesequence < 0 || d->state==CS_DEAD) continue;
                entities::teleporteffects(d, tp, td, false);
                break;
            }

            case N_JUMPPAD:
            {
                int cn = getint(p), jp = getint(p);
                gameent *d = getclient(cn);
                if(!d || d->lifesequence < 0 || d->state==CS_DEAD) continue;
                entities::jumppadeffects(d, jp, false);
                break;
            }

            default:
                neterr("type");
                return;
        }
    }

    void parsestate(gameent *d, ucharbuf &p, bool resume = false)
    {
        if(!d) { static gameent dummy; d = &dummy; }
        if(resume)
        {
            if(d==self) getint(p);
            else d->state = getint(p);
            d->frags = getint(p);
            d->flags = getint(p);
            d->deaths = getint(p);
            d->points = getint(p);
            if(d==self)
            {
                getint(p); // poweruptype
                getint(p); // powerupmillis
                getint(p); // role
            }
            else
            {
                d->poweruptype = getint(p);
                d->powerupmillis = getint(p);
                d->role = getint(p);
            }
        }
        d->lifesequence = getint(p);
        d->health = getint(p);
        d->maxhealth = getint(p);
        d->shield = getint(p);
        if(resume && d==self)
        {
            getint(p); // gunselect
            loopi(NUMGUNS) getint(p);
        }
        else
        {
            int gun = getint(p);
            d->gunselect = clamp(gun, 0, NUMGUNS-1);
            loopi(NUMGUNS) d->ammo[i] = getint(p);
        }
    }

    void validcountrycode(char *dst, const char *src)
    {
        if(src) loopi(MAXSTRLEN)
        {
            char c = *src++;
            if(iscubealnum(c) || c == '_' || c == '-') *dst++ = c;
            else break;
        }
        *dst = 0;
    }

    void parsemessages(int cn, gameent *d, ucharbuf &p)
    {
        static char text[MAXTRANS];
        int type;
        bool mapchanged = false, demopacket = false;

        while(p.remaining()) switch(type = getint(p))
        {
            case N_DEMOPACKET: demopacket = true; break;

            case N_SERVINFO:                   // welcome messsage from the server
            {
                int mycn = getint(p), prot = getint(p);
                if(prot!=PROTOCOL_VERSION)
                {
                    conoutf(CON_ERROR, "you are using a different game protocol (you: %d, server: %d)", PROTOCOL_VERSION, prot);
                    setsvar("lastdisconnectreason", "protocol mismatch");
                    disconnect();
                    return;
                }
                sessionid = getint(p);
                self->clientnum = mycn;      // we are now connected
                if(getint(p) > 0)
                {
                    //conoutf(CON_ERROR, "this server is password protected");
                }
                getstring(servdesc, p, sizeof(servdesc));
                getstring(servauth, p, sizeof(servauth));

                sendintro();
                break;
            }

            case N_WELCOME:
            {
                connected = true;
                notifywelcome();
                break;
            }

            case N_COUNTRY:
            {
                int cn = getint(p);
                gameent *d = getclient(cn);
                if(!d)
                {
                    getstring(text, p);
                    getstring(text, p);
                    break;
                }
                getstring(text, p);
                filtertext(text, text, T_NONE, MAXCOUNTRYCODELEN);
                validcountrycode(d->country_code, text);

                getstring(text, p);
                filtertext(d->country_name, text, T_WHITESPACE, MAXSTRLEN);
                break;
            }

            case N_PAUSEGAME:
            {
                bool val = getint(p) > 0;
                int cn = getint(p);
                gameent *a = cn >= 0 ? getclient(cn) : NULL;
                if(!demopacket)
                {
                    gamepaused = val;
                    self->attacking = ACT_IDLE;
                }
                if(a) conoutf(CON_GAMEINFO, "%s \fs\f2%s the game\fr", colorname(a), val ? "paused" : "resumed");
                else conoutf(CON_GAMEINFO, "\f2Game is %s", val ? "paused" : "resumed");
                pauseaudio(val);
                break;
            }

            case N_GAMESPEED:
            {
                int val = clamp(getint(p), 10, 1000), cn = getint(p);
                gameent *a = cn >= 0 ? getclient(cn) : NULL;
                if(!demopacket) gamespeed = val;
                if(a) conoutf(CON_GAMEINFO, "%s \fs\f2set game speed to\fr %d", colorname(a), val);
                else conoutf(CON_GAMEINFO, "\fs\f2Game speed is\fr %d", val);
                break;
            }

            case N_CLIENT:
            {
                int cn = getint(p), len = getuint(p);
                ucharbuf q = p.subbuf(len);
                parsemessages(cn, getclient(cn), q);
                break;
            }

            case N_SOUND:
                if(!d || d->state == CS_DEAD || d->state == CS_SPECTATOR) return;
                playsound(getint(p), d);
                break;

            case N_SCORE:
            {
                int cn = getint(p), points = getint(p);
                gameent *d = getclient(cn);
                if(d) d->points = points;
                break;
            }

            case N_TEXT:
            {
                int cn = getint(p);
                gameent *d = getclient(cn);
                getstring(text, p);
                filtertext(text, text, T_WHITESPACE | T_FORCESPACE);
                if(!d || isignored(d->clientnum)) break;
                if(d->state!=CS_DEAD && d->state!=CS_SPECTATOR)
                    particle_textcopy(d->abovehead(), text, PART_TEXT, 2000, 0x32FF64, 4.0f, -8, "default");
                conoutf(CON_CHAT, "%s: \fs%s%s\fr", teamcolorname(d), chatcolor(d), text);
                if(chatsound == 1) playsound(S_CHAT);
                break;
            }

            case N_SAYTEAM:
            {
                int tcn = getint(p);
                gameent *t = getclient(tcn);
                getstring(text, p);
                filtertext(text, text, T_WHITESPACE | T_FORCESPACE);
                int sound = getint(p);
                if(!t || isignored(t->clientnum)) break;
                if(sound >= 0 && (t->state != CS_DEAD || t->state != CS_SPECTATOR)) playsound(sound, t);
                int team = validteam(t->team) ? t->team : 0;
                if(t->state!=CS_DEAD)
                    particle_textcopy(t->abovehead(), text, PART_TEXT, 2000, teamtextcolor[team], 4.0f, -8, "default");
                conoutf(CON_TEAMCHAT, "%s \fs%s(team)\fr: \fs%s%s\fr", teamcolorname(t), teamtextcode[t->team], teamtextcode[t->team], text);
                if(chatsound == 1) playsound(S_CHAT);
                break;
            }

            case N_WHISPER:
            {
                int scn = getint(p);
                gameent *s = getclient(scn);
                getstring(text, p);
                filtertext(text, text, T_WHITESPACE | T_FORCESPACE);
                if(!s || isignored(s->clientnum)) break;
                conoutf(CON_CHAT, "%s \fs\f5(whisper)\fr: \fs\f5%s\fr", teamcolorname(s), text);
                if(chatsound) playsound(S_CHAT);
                break;
            }

            case N_MAPCHANGE:
            {
                getstring(text, p);
                int mode = getint(p), muts = getint(p), scorelimit = getint(p), items = getint(p);
                changemapserv(text, mode, muts, scorelimit);
                mapchanged = true;
                if(items) entities::spawnitems();
                else senditemstoserver = false;
                break;
            }

            case N_FORCEDEATH:
            {
                int cn = getint(p);
                gameent *d = cn==self->clientnum ? self : newclient(cn);
                if(!d) break;
                if(d==self)
                {
                    if(editmode) toggleedit();
                }
                else d->resetinterp();
                d->state = CS_DEAD;
                checkfollow();
                break;
            }

            case N_ITEMLIST:
            {
                int n;
                while((n = getint(p))>=0 && !p.overread())
                {
                    if(mapchanged) entities::setspawn(n, true, true);
                    getint(p); // type
                }
                break;
            }

            case N_INITCLIENT: // another client either connected or changed name/team
            {
                int cn = getint(p), notify = getint(p);
                gameent *d = newclient(cn);
                if(!d)
                {
                    getstring(text, p);
                    getint(p);
                    getint(p);
                    getint(p);
                    getstring(text, p);
                    getstring(text, p);
                    break;
                }
                getstring(text, p);
                filtertext(text, text, T_WHITESPACE | T_NAME, MAXNAMELEN, MAXNAMEUNILEN);
                if(!text[0]) copystring(text, "player"); // if no text is specified for the name change, change to default name
                if(d->name[0]) // already connected but the client changed their name
                {
                    if(notify && strcmp(d->name, text) && !isignored(d->clientnum))
                    {
                        conoutf(CON_CHAT, "%s \fs\f0is now known as\fr %s", colorname(d), colorname(d, text));
                    }
                }
                else // new client joined
                {
                    if(d!=self && notify)
                    {
                        conoutf(CON_CHAT, "%s \fs\f0joined the game\fr", colorname(d, text));
                        if(chatsound == 1) playsound(S_CHAT);
                    }
                    if(needclipboard >= 0) needclipboard++;
                }
                copystring(d->name, text, MAXNAMELEN+1);
                d->team = getint(p);
                if(!validteam(d->team)) d->team = 0;
                d->playermodel = getint(p);
                d->playercolor = getint(p);

                getstring(text, p);
                filtertext(text, text, T_NONE, MAXCOUNTRYCODELEN);
                validcountrycode(d->country_code, text);

                getstring(text, p);
                filtertext(text, text, T_WHITESPACE, MAXSTRLEN);
                copystring(d->country_name, text);

                break;
            }

            case N_SWITCHNAME:
                getstring(text, p);
                if(d)
                {
                    filtertext(text, text, T_WHITESPACE | T_NAME, MAXNAMELEN, MAXNAMEUNILEN);
                    if(!text[0]) copystring(text, "player");
                    if(strcmp(text, d->name))
                    {
                        if(!isignored(d->clientnum)) conoutf(CON_CHAT, "%s \fs\f0is now known as\fr %s", colorname(d), colorname(d, text));
                        copystring(d->name, text, MAXNAMELEN+1);
                    }
                }
                break;

            case N_SWITCHMODEL:
            {
                int model = getint(p);
                if(d)
                {
                    d->playermodel = model;
                    if(d->ragdoll) cleanragdoll(d);
                }
                break;
            }

            case N_SWITCHCOLOR:
            {
                int color = getint(p);
                if(d) d->playercolor = color;
                break;
            }

            case N_CDIS:
                clientdisconnected(getint(p));
                break;

            case N_SPAWN:
            {
                if(d)
                {
                    if(d!=hudplayer() && d->state==CS_DEAD && d->lastpain) saveragdoll(d);
                    else cleanragdoll(d);
                    d->respawn();
                }
                parsestate(d, p);
                if(!d) break;
                d->state = CS_SPAWNING;
                if(d == followingplayer()) d->lasthit = 0;
                checkfollow();
                break;
            }

            case N_SPAWNSTATE:
            {
                int scn = getint(p);
                gameent *s = getclient(scn);
                if(!s)
                {
                    parsestate(NULL, p);
                    break;
                }
                if(s!=hudplayer() && s->state==CS_DEAD && s->lastpain) saveragdoll(s);
                else cleanragdoll(s);
                if(s == self)
                {
                    if(editmode) toggleedit();
                }
                s->respawn();
                parsestate(s, p);
                s->state = CS_ALIVE;
                pickgamespawn(s);
                if(cmode) cmode->respawned(s);
                ai::spawned(s);
                if(s == self) spawneffect(s);
                checkfollow();
                addmsg(N_SPAWN, "rcii", s, s->lifesequence, s->gunselect);
                break;
            }

            case N_SHOTEVENT:
            {
                int scn = getint(p), atk = getint(p);
                gameent *s = getclient(scn);
                if(!s || !validatk(atk)) break;
                int gun = attacks[atk].gun;
                if (validgun(gun))
                {
                    s->gunselect = gun;
                }
                if(!s->haspowerup(PU_AMMO) && s->role != ROLE_BERSERKER)
                {
                    s->ammo[gun] -= attacks[atk].use;
                }
                s->gunwait = attacks[atk].attackdelay;
                s->lastattack = atk;
                break;
            }

            case N_SHOTFX:
            {
                int acn = getint(p), atk = getint(p), id = getint(p), hit = getint(p);
                vec from, to;
                loopk(3) from[k] = getint(p)/DMF;
                loopk(3) to[k] = getint(p)/DMF;
                gameent *actor = getclient(acn);
                if(!actor || !validatk(atk)) break;
                actor->lastaction = lastmillis;
                shoteffects(atk, from, to, actor, false, id, actor->lastaction, hit ? 1 : 0);
                break;
            }

            case N_EXPLODEFX:
            {
                int ecn = getint(p), atk = getint(p), id = getint(p);
                gameent *e = getclient(ecn);
                if(!e || !validatk(atk)) break;
                explodeeffects(atk, e, false, id);
                break;
            }

            case N_REGENERATE:
            {
                int cn = getint(p), health = getint(p);
                gameent *d = cn==self->clientnum ? self : getclient(cn);
                if(d) d->health = health;
                break;
            }

            case N_REPAMMO:
            {
                int cn = getint(p), gun = getint(p), ammo = getint(p);
                gameent *d = cn==self->clientnum ? self : getclient(cn);
                if(d) d->ammo[gun] = ammo;
                break;
            }

            case N_DAMAGE:
            {
                int tcn = getint(p), acn = getint(p),
                    atk = getint(p), damage = getint(p),
                    flags = getint(p), health = getint(p), shield = getint(p);
                vec to;
                loopk(3) to[k] = getint(p)/DMF;
                gameent *target = getclient(tcn),
                       *actor = getclient(acn);
                if(!target || !actor) break;
                target->health = health;
                target->shield = shield;
                damageentity(damage, target, actor, atk, flags, false);
                applyhiteffects(damage, target, actor, to.iszero() ? target->o : to, atk, flags, false);
                break;
            }

            case N_HITPUSH:
            {
                int tcn = getint(p), atk = getint(p), damage = getint(p);
                gameent *target = getclient(tcn);
                vec dir;
                loopk(3) dir[k] = getint(p)/DNF;
                if(!target || !validatk(atk)) break;
                target->hitpush(damage * (target->health <= 0 ? deadpush : 1), dir, NULL, atk);
                break;
            }

            case N_DIED:
            {
                int vcn = getint(p), acn = getint(p), frags = getint(p), tfrags = getint(p),
                    atk = getint(p), flags = getint(p);
                gameent *victim = getclient(vcn),
                        *actor = getclient(acn);
                if(!actor) break;
                actor->frags = frags;
                if(m_teammode) setteaminfo(actor->team, tfrags);
                if(!victim) break;
                kill(victim, actor, atk, flags);
                break;
            }

            case N_TEAMINFO:
                loopi(MAXTEAMS)
                {
                    int frags = getint(p);
                    if(m_teammode) setteaminfo(1+i, frags);
                }
                break;

            case N_GUNSELECT:
            {
                if(!d) return;
                int gun = getint(p);
                if(!validgun(gun)) return;
                d->gunselect = gun;
                d->lastswitchattempt = lastmillis;
                doweaponchangeffects(d, gun);
                break;
            }

            case N_TAUNT:
            {
                if(!d || lastmillis-d->lasttaunt < TAUNT_DELAY) return;
                d->lasttaunt = lastmillis;
                playsound(getplayermodelinfo(d).tauntsound, d);
                break;
            }

            case N_ANNOUNCE:
            {
                int sound = getint(p);
                getstring(text, p);
                if(sound >= 0) playsound(sound, NULL, NULL, NULL, SND_ANNOUNCER);
                if(text[0]) conoutf(CON_GAMEINFO, "%s", text);
                break;
            }

            case N_RESUME:
            {
                for(;;)
                {
                    int cn = getint(p);
                    if(p.overread() || cn<0) break;
                    gameent *d = (cn == self->clientnum ? self : newclient(cn));
                    parsestate(d, p, true);
                }
                break;
            }

            case N_ITEMSPAWN:
            {
                int i = getint(p);
                if(!entities::ents.inrange(i)) break;
                entities::setspawn(i, true);
                ai::itemspawned(i);
                break;
            }

            case N_ITEMACC:            // server acknowledges that I picked up this item
            {
                int i = getint(p), cn = getint(p);
                gameent *d = getclient(cn);
                entities::pickupeffects(i, d);
                d->lastpickupmillis = lastmillis;
                break;
            }

            case N_CLIPBOARD:
            {
                int cn = getint(p), unpacklen = getint(p), packlen = getint(p);
                gameent *d = getclient(cn);
                ucharbuf q = p.subbuf(max(packlen, 0));
                if(d) unpackeditinfo(d->edit, q.buf, q.maxlen, unpacklen);
                break;
            }
            case N_UNDO:
            case N_REDO:
            {
                int cn = getint(p), unpacklen = getint(p), packlen = getint(p);
                gameent *d = getclient(cn);
                ucharbuf q = p.subbuf(max(packlen, 0));
                if(d) unpackundo(q.buf, q.maxlen, unpacklen);
                break;
            }

            case N_EDITF:              // coop editing messages
            case N_EDITT:
            case N_EDITM:
            case N_FLIP:
            case N_COPY:
            case N_PASTE:
            case N_ROTATE:
            case N_REPLACE:
            case N_DELCUBE:
            case N_EDITVSLOT:
            {
                if(!d) return;
                selinfo sel;
                sel.o.x = getint(p); sel.o.y = getint(p); sel.o.z = getint(p);
                sel.s.x = getint(p); sel.s.y = getint(p); sel.s.z = getint(p);
                sel.grid = getint(p); sel.orient = getint(p);
                sel.cx = getint(p); sel.cxs = getint(p); sel.cy = getint(p), sel.cys = getint(p);
                sel.corner = getint(p);
                switch(type)
                {
                    case N_EDITF: { int dir = getint(p), mode = getint(p); if(sel.validate()) mpeditface(dir, mode, sel, false); break; }
                    case N_EDITT:
                    {
                        int tex = getint(p),
                            allfaces = getint(p);
                        if(p.remaining() < 2) return;
                        int extra = lilswap(*(const ushort *)p.pad(2));
                        if(p.remaining() < extra) return;
                        ucharbuf ebuf = p.subbuf(extra);
                        if(sel.validate()) mpedittex(tex, allfaces, sel, ebuf);
                        break;
                    }
                    case N_EDITM: { int mat = getint(p), filter = getint(p); if(sel.validate()) mpeditmat(mat, filter, sel, false); break; }
                    case N_FLIP: if(sel.validate()) mpflip(sel, false); break;
                    case N_COPY: if(d && sel.validate()) mpcopy(d->edit, sel, false); break;
                    case N_PASTE: if(d && sel.validate()) mppaste(d->edit, sel, false); break;
                    case N_ROTATE: { int dir = getint(p); if(sel.validate()) mprotate(dir, sel, false); break; }
                    case N_REPLACE:
                    {
                        int oldtex = getint(p),
                            newtex = getint(p),
                            insel = getint(p);
                        if(p.remaining() < 2) return;
                        int extra = lilswap(*(const ushort *)p.pad(2));
                        if(p.remaining() < extra) return;
                        ucharbuf ebuf = p.subbuf(extra);
                        if(sel.validate()) mpreplacetex(oldtex, newtex, insel>0, sel, ebuf);
                        break;
                    }
                    case N_DELCUBE: if(sel.validate()) mpdelcube(sel, false); break;
                    case N_EDITVSLOT:
                    {
                        int delta = getint(p),
                            allfaces = getint(p);
                        if(p.remaining() < 2) return;
                        int extra = lilswap(*(const ushort *)p.pad(2));
                        if(p.remaining() < extra) return;
                        ucharbuf ebuf = p.subbuf(extra);
                        if(sel.validate()) mpeditvslot(delta, allfaces, sel, ebuf);
                        break;
                    }
                }
                break;
            }
            case N_REMIP:
                if(!d) return;
                conoutf(CON_GAMEINFO, "%s \fs\f2remipped\fr", colorname(d));
                mpremip(false);
                break;
            case N_CALCLIGHT:
                if(!d) return;
                conoutf(CON_GAMEINFO, "%s \fs\f2computed lights\fr", colorname(d));
                mpcalclight(false);
                break;

            case N_EDITENT: // Edit an entity in (local) multiplayer.
            {
                if(!d) return;
                int i = getint(p);
                float x = getint(p)/DMF, y = getint(p)/DMF, z = getint(p)/DMF;
                int type = getint(p);
                int attr1 = getint(p), attr2 = getint(p), attr3 = getint(p), attr4 = getint(p), attr5 = getint(p);

                mpeditent(i, vec(x, y, z), type, attr1, attr2, attr3, attr4, attr5, false);
                break;
            }
            case N_EDITVAR:
            {
                if(!d) return;
                int type = getint(p);
                getstring(text, p);
                string name;
                filtertext(name, text, T_NONE);
                ident *id = getident(name);
                switch(type)
                {
                    case ID_VAR:
                    {
                        int val = getint(p);
                        if(id && id->flags&IDF_OVERRIDE && !(id->flags&IDF_READONLY)) setvar(name, val);
                        break;
                    }
                    case ID_FVAR:
                    {
                        float val = getfloat(p);
                        if(id && id->flags&IDF_OVERRIDE && !(id->flags&IDF_READONLY)) setfvar(name, val);
                        break;
                    }
                    case ID_SVAR:
                    {
                        getstring(text, p);
                        if(id && id->flags&IDF_OVERRIDE && !(id->flags&IDF_READONLY)) setsvar(name, text);
                        break;
                    }
                }
                printvar(d, id);
                break;
            }

            case N_PONG:
                addmsg(N_CLIENTPING, "i", self->ping = (self->ping*5+totalmillis-getint(p))/6);
                break;

            case N_CLIENTPING:
                if(!d) return;
                d->ping = getint(p);
                break;

            case N_TIMEUP:
            {
                int time = getint(p);
                int update = getint(p);
                updatetimer(time, update);
                break;
            }

            case N_SERVMSG:
                getstring(text, p);
                conoutf("%s", text);
                break;

            case N_SENDDEMOLIST:
            {
                int demos = getint(p);
                if(demos <= 0) conoutf("no demos available");
                else loopi(demos)
                {
                    getstring(text, p);
                    if(p.overread()) break;
                    conoutf("%d. %s", i+1, text);
                }
                break;
            }

            case N_DEMOPLAYBACK:
            {
                int on = getint(p);
                if(on) self->state = CS_SPECTATOR;
                else clearclients();
                demoplayback = on!=0;
                self->clientnum = getint(p);
                gamepaused = false;
                checkfollow();
                execident(on ? "demostart" : "demoend");
                break;
            }

            case N_ROUND:
            {
                int state = getint(p);
                updateroundstate(state);
                break;
            }

            case N_CURRENTMASTER:
            {
                int mm = getint(p), mn;
                loopv(players) players[i]->privilege = PRIV_NONE;
                while((mn = getint(p))>=0 && !p.overread())
                {
                    gameent *m = mn==self->clientnum ? self : newclient(mn);
                    int priv = getint(p);
                    if(m) m->privilege = priv;
                }
                if(mm != mastermode)
                {
                    mastermode = mm;
                    conoutf("\fs\f0master mode is\fr %s%s", mastermodecolors[mastermode+1], server::mastermodename(mastermode));
                }
                break;
            }

            case N_MASTERMODE:
            {
                mastermode = getint(p);
                conoutf("\fs\f0master mode is\fr %s%s", mastermodecolors[mastermode+1], server::mastermodename(mastermode));
                break;
            }

            case N_EDITMODE:
            {
                int val = getint(p);
                if(!d) break;
                if(val)
                {
                    d->editstate = d->state;
                    d->state = CS_EDITING;
                }
                else
                {
                    d->state = d->editstate;
                    if (d->state == CS_DEAD)
                    {
                        setdeathstate(d, true);
                    }
                }
                checkfollow();
                break;
            }

            case N_SPECTATOR:
            {
                int sn = getint(p), val = getint(p), waiting = getint(p);
                gameent *s;
                if(sn==self->clientnum)
                {
                    s = self;
                    if(val && remote && !self->privilege) senditemstoserver = false;
                }
                else s = newclient(sn);
                if(!s) return;
                if(val)
                {
                    if(s == self && editmode)
                    {
                        toggleedit();
                    }
                    else if(!waiting) conoutf("%s \fs\f0has entered spectator mode\fr", colorname(s));
                    saveragdoll(s);
                    s->state = CS_SPECTATOR;
                    execident("on_spectate");
                    if (s == self)
                    {
                        camera::restore();
                    }
                }
                else if(s->state == CS_SPECTATOR)
                {
                    setdeathstate(s, true);
                    conoutf("%s \fs\f0has left spectator mode\fr", colorname(s));
                    execident("on_unspectate");
                }
                s->ghost = waiting;
                checkfollow();
                break;
            }

            case N_SETTEAM:
            {
                int wn = getint(p), team = getint(p), reason = getint(p);
                gameent *w = getclient(wn);
                if(!w) return;
                w->team = validteam(team) ? team : 0;
                static const char * const fmt[2] = { "%s \fs\f0switched to team\fr %s%s", "%s \fs\f0forced to team\fr %s%s"};
                if(reason >= 0 && size_t(reason) < sizeof(fmt)/sizeof(fmt[0]))
                    conoutf(fmt[reason], colorname(w), teamtextcode[w->team], teamnames[w->team]);
                break;
            }

            case N_ASSIGNROLE:
            {
                int tcn = getint(p), acn = getint(p),
                    role = getint(p);
                gameent *d = getclient(tcn),
                        *actor = getclient(acn);
                if(!d || !actor) break;
                if(role == ROLE_BERSERKER)
                {
                    if (!m_berserker)
                    {
                        break;
                    }
                    d->stopchannelsound(Chan_PowerUp);
                    conoutf(CON_GAMEINFO, "%s \f2is the berserker!", colorname(d));
                    playsound(S_BERSERKER, d);
                    particle_flare(d->o, d->o, 500, PART_COMICS, 0xFFFFFF, 0.1f, NULL, 30.0f);
                    // Temporary:
                    camera::thirdperson = 1;
                }
                else if(role == ROLE_ZOMBIE)
                {
                    if (!m_infection)
                    {
                        break;
                    }
                    if (!hunterchosen && d == actor)
                    {
                        hunterchosen = true;
                    }
                    writeobituary(d, actor, ATK_ZOMBIE);
                    d->stopchannelsound(Chan_PowerUp);
                    stopownersounds(d);
                    playsound(S_INFECTED, d);
                    particle_splash(PART_SPARK, 20, 200, d->o, 0x9BCF0F, 2.0f + rndscale(5.0f), 180, 50);
                    doweaponchangeffects(d, GUN_ZOMBIE);
                    d->lastswitchattempt = lastmillis;
                }
                if (d == followingplayer(self))
                {
                    addscreenflash(150);
                }
                d->assignrole(role);
                break;
            }

            case N_VOOSH:
            {
                int cn = getint(p), gun = getint(p);
                vooshgun = gun;
                gameent *d = getclient(cn);
                if(!d) return;
                d->voosh(gun);
                doweaponchangeffects(d, gun);
                d->lastswitchattempt = lastmillis;
                if(d == self) playsound(S_VOOSH);
                break;
            }

            #define PARSEMESSAGES 1
            #include "ctf.h"
            #include "elimination.h"
            #undef PARSEMESSAGES

            case N_NEWMAP:
            {
                int size = getint(p);
                if(size>=0) emptymap(size, true, NULL);
                else enlargemap(true);
                if(d && d!=self)
                {
                    int newsize = 0;
                    while(1<<newsize < getworldsize()) newsize++;
                    conoutf(CON_GAMEINFO, size>=0 ? "%s \fs\f2started a new map of size\fr %d" : "%s \fs\f2enlarged the map to size\fr %d", colorname(d), newsize);
                }
                break;
            }

            case N_REQAUTH:
            {
                getstring(text, p);
                if(autoauth && text[0] && tryauth(text)) conoutf("server requested authkey \"%s\"", text);
                break;
            }

            case N_AUTHCHAL:
            {
                getstring(text, p);
                authkey *a = findauthkey(text);
                uint id = (uint)getint(p);
                getstring(text, p);
                vector<char> buf;
                if(a && a->lastauth && lastmillis - a->lastauth < 60*1000 && answerchallenge(a->key, text, buf))
                {
                    vector<char> buf;
                    answerchallenge(a->key, text, buf);
                    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
                    putint(p, N_AUTHANS);
                    sendstring(a->desc, p);
                    putint(p, id);
                    sendstring(buf.getbuf(), p);
                    sendclientpacket(p.finalize(), 1);
                }
                break;
            }

            case N_INITAI:
            {
                int bn = getint(p), on = getint(p), at = getint(p), sk = clamp(getint(p), 1, 101), pm = getint(p), col = getint(p), team = getint(p);
                string name;
                getstring(text, p);
                filtertext(name, text, T_NAME, MAXNAMELEN, MAXNAMEUNILEN);
                gameent *b = newclient(bn);
                if(!b) break;
                ai::init(b, at, on, sk, bn, pm, col, name, team);
                break;
            }

            case N_SERVCMD:
                getstring(text, p);
                break;

            default:
                neterr("type", cn < 0);
                return;
        }
    }

    struct demoreq
    {
        int tag;
        string name;
    };
    vector<demoreq> demoreqs;
    enum { MAXDEMOREQS = 7 };
    static int lastdemoreq = 0;

    void receivefile(packetbuf &p)
    {
        int type;
        while(p.remaining()) switch(type = getint(p))
        {
            case N_DEMOPACKET: return;
            case N_SENDDEMO:
            {
                string fname;
                fname[0] = '\0';
                int tag = getint(p);
                loopv(demoreqs) if(demoreqs[i].tag == tag)
            {
                    copystring(fname, demoreqs[i].name);
                    demoreqs.remove(i);
                    break;
                }
                if(!fname[0])
                {
                    time_t t = time(NULL);
                    size_t len = strftime(fname, sizeof(fname), "%Y-%m-%d_%H.%M.%S", localtime(&t));
                    fname[min(len, sizeof(fname)-1)] = '\0';
                }
                int len = strlen(fname);
                if(len < 4 || strcasecmp(&fname[len-4], ".dmo")) concatstring(fname, ".dmo");
                stream *demo = NULL;
                if(const char *buf = server::getdemofile(fname, true)) demo = openrawfile(buf, "wb");
                if(!demo) demo = openrawfile(fname, "wb");
                if(!demo) return;
                conoutf("received demo \"%s\"", fname);
                ucharbuf b = p.subbuf(p.remaining());
                demo->write(b.buf, b.maxlen);
                delete demo;
                break;
            }

            case N_SENDMAP:
            {
                if(!m_edit) return;
                string oldname;
                copystring(oldname, getclientmap());
                defformatstring(mname, "getmap_%d", lastmillis);
                defformatstring(fname, "data/map/%s.ogz", mname);
                stream *map = openrawfile(path(fname), "wb");
                if(!map) return;
                conoutf("received map");
                ucharbuf b = p.subbuf(p.remaining());
                map->write(b.buf, b.maxlen);
                delete map;
                if(load_world(mname, oldname[0] ? oldname : NULL))
                    entities::spawnitems(true);
                remove(findfile(fname, "rb"));
                break;
            }
        }
    }

    void parsepacketclient(int chan, packetbuf &p)   // processes any updates from the server
    {
        if(p.packet->flags&ENET_PACKET_FLAG_UNSEQUENCED) return;
        switch(chan)
        {
            case 0:
                parsepositions(p);
                break;

            case 1:
                parsemessages(-1, NULL, p);
                break;

            case 2:
                receivefile(p);
                break;
        }
    }

    void getmap()
    {
        if(!m_edit) { conoutf(CON_ERROR, "\"getmap\" only works in edit mode"); return; }
        conoutf("getting map...");
        addmsg(N_GETMAP, "r");
    }
    COMMAND(getmap, "");

    void stopdemo()
    {
        if(remote)
        {
            if(self->privilege<PRIV_MASTER) return;
            addmsg(N_STOPDEMO, "r");
        }
        else server::stopdemo();
    }
    COMMAND(stopdemo, "");

    void recorddemo(int val)
    {
        if(remote && self->privilege<PRIV_MASTER) return;
        addmsg(N_RECORDDEMO, "ri", val);
    }
    ICOMMAND(recorddemo, "i", (int *val), recorddemo(*val));

    void cleardemos(int val)
    {
        if(remote && self->privilege<PRIV_MASTER) return;
        addmsg(N_CLEARDEMOS, "ri", val);
    }
    ICOMMAND(cleardemos, "i", (int *val), cleardemos(*val));

    void getdemo(char *val, char *name)
    {
        int i = 0;
        if(isdigit(val[0]) || name[0]) i = parseint(val);
        else name = val;
        if(i<=0) conoutf("getting demo...");
        else conoutf("getting demo %d...", i);
        ++lastdemoreq;
        if(name[0])
        {
            if(demoreqs.length() >= MAXDEMOREQS) demoreqs.remove(0);
            demoreq &r = demoreqs.add();
            r.tag = lastdemoreq;
            copystring(r.name, name);
        }
        addmsg(N_GETDEMO, "rii", i, lastdemoreq);
    }
    ICOMMAND(getdemo, "ss", (char *val, char *name), getdemo(val, name));

    void listdemos()
    {
        conoutf("listing demos...");
        addmsg(N_LISTDEMOS, "r");
    }
    COMMAND(listdemos, "");

    void sendmap()
    {
        if(!m_edit || (self->state==CS_SPECTATOR && remote && !self->privilege)) { conoutf(CON_ERROR, "\"sendmap\" only works in coop edit mode"); return; }
        conoutf("sending map...");
        defformatstring(mname, "sendmap_%d", lastmillis);
        save_world(mname, true);
        defformatstring(fname, "data/map/%s.ogz", mname);
        stream *map = openrawfile(path(fname), "rb");
        if(map)
        {
            stream::offset len = map->size();
            if(len > 4*1024*1024) conoutf(CON_ERROR, "map is too large");
            else if(len <= 0) conoutf(CON_ERROR, "could not read map");
            else
            {
                sendfile(-1, 2, map);
                if(needclipboard >= 0) needclipboard++;
            }
            delete map;
        }
        else conoutf(CON_ERROR, "could not read map");
        remove(findfile(fname, "rb"));
    }
    COMMAND(sendmap, "");

    void gotoplayer(const char *arg)
    {
        if(self->state!=CS_SPECTATOR && self->state!=CS_EDITING) return;
        int i = parseplayer(arg);
        if(i>=0)
        {
            gameent *d = getclient(i);
            if(!d || d==self) return;
            self->o = d->o;
            vec dir;
            vecfromyawpitch(self->yaw, self->pitch, 1, 0, dir);
            self->o.add(dir.mul(-32));
            self->resetinterp();
        }
    }
    COMMANDN(goto, gotoplayer, "s");

    void gotosel()
    {
        if(self->state!=CS_EDITING) return;
        self->o = getselpos();
        vec dir;
        vecfromyawpitch(self->yaw, self->pitch, 1, 0, dir);
        self->o.add(dir.mul(-32));
        self->resetinterp();
    }
    COMMAND(gotosel, "");
}

