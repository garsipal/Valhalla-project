#include "game.h"
#include "geoip.h"

namespace game
{
    void parseoptions(vector<const char *> &args)
    {
        loopv(args)
#ifndef STANDALONE
            if(!game::clientoption(args[i]))
#endif
            if(!server::serveroption(args[i]))
                conoutf(CON_ERROR, "unknown command-line option: %s", args[i]);
    }

    const char *gameident() { return "Valhalla Project"; }
}

extern ENetAddress masteraddress;

namespace server
{
    struct server_entity            // server side version of "entity" type
    {
        int type;
        int spawntime;
        bool spawned;
    };

    struct serverevent // server-wide event scheduled for a specific time
    {
        int millis;
        void (*action)();
        void flush() { (action)(); }
    };
    namespace serverevents
    {
        void add(void (*action)(), int future = 0);
    }

    static const int DEATHMILLIS = 300;

    struct clientinfo;

    struct gameevent
    {
        virtual ~gameevent() {}

        virtual bool flush(clientinfo *ci, int fmillis);
        virtual void process(clientinfo *ci) {}

        virtual bool keepable() const { return false; }
    };

    struct timedevent : gameevent
    {
        int millis;

        bool flush(clientinfo *ci, int fmillis);
    };

    struct hitinfo
    {
        int target, lifesequence, rays, flags;
        float dist;
        vec dir;
    };

    struct shotevent : timedevent
    {
        int id, atk;
        vec from, to;
        vector<hitinfo> hits;

        void process(clientinfo *ci);
    };

    struct explodeevent : timedevent
    {
        int id, atk;
        vector<hitinfo> hits;

        bool keepable() const { return true; }

        void process(clientinfo *ci);
    };

    struct suicideevent : gameevent
    {
        void process(clientinfo *ci);
    };

    struct pickupevent : gameevent
    {
        int ent;

        void process(clientinfo *ci);
    };

    template <int N>
    struct projectilestate
    {
        int projs[N];
        int numprojs;

        projectilestate() : numprojs(0) {}

        void reset()
        { 
            numprojs = 0;
        }

        void add(int val)
        {
            if (numprojs >= N)
            {
                numprojs = 0;
            }
            projs[numprojs++] = val;
        }

        bool remove(int val)
        {
            loopi(numprojs) if (projs[i] == val)
            {
                projs[i] = projs[--numprojs];
                return true;
            }
            return false;
        }
    };

    struct servstate : gamestate
    {
        vec o, oldpos;
        int state, editstate;
        int lastdeath, deadflush, lastspawn, lifesequence;
        int lastpain, lastdamage, lastregeneration;
        int lastmove, lastshot, lastatk;
        projectilestate<8> projectiles;
        int frags, flags, deaths, points, teamkills, shotdamage, damage, spree;
        int lasttimeplayed, timeplayed;
        float effectiveness;

        servstate() : state(CS_DEAD), editstate(CS_DEAD), lifesequence(0), lastpain(0), lastdamage(0), lastregeneration(0), lastmove(0) {}

        bool isalive(int gamemillis)
        {
            return state==CS_ALIVE || (state==CS_DEAD && gamemillis - lastdeath <= DEATHMILLIS);
        }

        bool waitexpired(int gamemillis)
        {
            return gamemillis - lastshot >= gunwait;
        }

        void reset()
        {
            if(state!=CS_SPECTATOR) state = editstate = CS_DEAD;
            maxhealth = 100;
            projectiles.reset();

            timeplayed = 0;
            effectiveness = 0;
            frags = flags = deaths = points = teamkills = shotdamage = damage = 0;

            lastdeath = 0;

            respawn();
        }

        void respawn()
        {
            gamestate::respawn();
            o = oldpos = vec(-1e10f, -1e10f, -1e10f);
            deadflush = 0;
            lastspawn = -1;
            lastshot = 0;
            role = ROLE_NONE;
            spree = 0;
        }

        void reassign()
        {
            respawn();
            projectiles.reset();
        }
    };

    struct savedscore
    {
        uint ip;
        string name;
        int frags, flags, deaths, points, teamkills, shotdamage, damage;
        int timeplayed;
        float effectiveness;

        void save(servstate &gs)
        {
            frags = gs.frags;
            flags = gs.flags;
            deaths = gs.deaths;
            points = gs.points;
            teamkills = gs.teamkills;
            shotdamage = gs.shotdamage;
            damage = gs.damage;
            timeplayed = gs.timeplayed;
            effectiveness = gs.effectiveness;
        }

        void restore(servstate &gs)
        {
            gs.frags = frags;
            gs.flags = flags;
            gs.deaths = deaths;
            gs.points = points;
            gs.teamkills = teamkills;
            gs.shotdamage = shotdamage;
            gs.damage = damage;
            gs.timeplayed = timeplayed;
            gs.effectiveness = effectiveness;
        }
    };

    extern int gamemillis, nextexceeded;

    struct clientinfo
    {
        int clientnum, ownernum, connectmillis, sessionid, overflow;
        string name, mapvote;
        int team, playermodel, playercolor;
        int modevote, mutsvote;
        int privilege;
        bool connected, local, timesync, ghost, mute;
        int gameoffset, lastevent, pushed, exceeded;
        servstate state;
        vector<gameevent *> events;
        vector<uchar> position, messages;
        uchar *wsdata;
        int wslen;
        vector<clientinfo *> bots;
        int ping, aireinit;
        string clientmap;
        int mapcrc;
        bool warned, damagemat;
        ENetPacket *getdemo, *getmap, *clipboard;
        int lastclipboard, needclipboard;
        int connectauth;
        uint authreq;
        string authname, authdesc;
        void *authchallenge;
        int authkickvictim;
        char *authkickreason;
        char preferred_flag[MAXCOUNTRYCODELEN+1];
        char country_code[MAXCOUNTRYCODELEN+1];
        string country_name;
        char customflag_code[MAXCOUNTRYCODELEN+1];
        string customflag_name;

        clientinfo() : getdemo(NULL), getmap(NULL), clipboard(NULL), authchallenge(NULL), authkickreason(NULL) { reset(); mute = false; }
        ~clientinfo() { events.deletecontents(); cleanclipboard(); cleanauth(); }

        void addevent(gameevent *e)
        {
            if(state.state==CS_SPECTATOR || events.length()>100) delete e;
            else events.add(e);
        }

        enum
        {
            PUSHMILLIS = 3000
        };

        int calcpushrange()
        {
            ENetPeer *peer = getclientpeer(ownernum);
            return PUSHMILLIS + (peer ? peer->roundTripTime + peer->roundTripTimeVariance : ENET_PEER_DEFAULT_ROUND_TRIP_TIME);
        }

        bool checkpushed(int millis, int range)
        {
            return millis >= pushed - range && millis <= pushed + range;
        }

        void scheduleexceeded()
        {
            if(state.state!=CS_ALIVE || !exceeded) return;
            int range = calcpushrange();
            if(!nextexceeded || exceeded + range < nextexceeded) nextexceeded = exceeded + range;
        }

        void setexceeded()
        {
            if(state.state==CS_ALIVE && !exceeded && !checkpushed(gamemillis, calcpushrange())) exceeded = gamemillis;
            scheduleexceeded();
        }

        void setpushed()
        {
            pushed = max(pushed, gamemillis);
            if(exceeded && checkpushed(exceeded, calcpushrange())) exceeded = 0;
        }

        bool checkexceeded()
        {
            return state.state==CS_ALIVE && exceeded && gamemillis > exceeded + calcpushrange();
        }

        void mapchange()
        {
            mapvote[0] = 0;
            modevote = INT_MAX;
            mutsvote = 0;
            state.reset();
            events.deletecontents();
            overflow = 0;
            timesync = false;
            lastevent = 0;
            exceeded = 0;
            pushed = 0;
            clientmap[0] = '\0';
            mapcrc = 0;
            warned = false;
            damagemat = false;
        }

        void reassign()
        {
            state.reassign();
            events.deletecontents();
            timesync = false;
            lastevent = 0;
        }

        void cleanclipboard(bool fullclean = true)
        {
            if(clipboard) { if(--clipboard->referenceCount <= 0) enet_packet_destroy(clipboard); clipboard = NULL; }
            if(fullclean) lastclipboard = 0;
        }

        void cleanauthkick()
        {
            authkickvictim = -1;
            DELETEA(authkickreason);
        }

        void cleanauth(bool full = true)
        {
            authreq = 0;
            if(authchallenge) { freechallenge(authchallenge); authchallenge = NULL; }
            if(full) cleanauthkick();
        }

        void reset()
        {
            name[0] = 0;
            team = 0;
            playermodel = -1;
            playercolor = 0;
            privilege = PRIV_NONE;
            connected = local = ghost = false;
            connectauth = 0;
            position.setsize(0);
            messages.setsize(0);
            ping = 0;
            aireinit = 0;
            needclipboard = 0;
            cleanclipboard();
            cleanauth();
            mapchange();
            preferred_flag[0] = country_code[0] = country_name[0] = customflag_code[0] = customflag_name[0] = 0;
        }

        int geteventmillis(int servmillis, int clientmillis)
        {
            if(!timesync || (events.empty() && state.waitexpired(servmillis)))
            {
                timesync = true;
                gameoffset = servmillis - clientmillis;
                return servmillis;
            }
            else return gameoffset + clientmillis;
        }
    };

    struct ban
    {
        int time, expire;
        uint ip;
    };

    namespace aimanager
    {
        extern void removeai(clientinfo *ci);
        extern void clearai();
        extern void checkai();
        extern void reqadd(clientinfo *ci, int skill);
        extern void reqdel(clientinfo *ci);
        extern void setbotlimit(clientinfo *ci, int limit);
        extern void setbotbalance(clientinfo *ci, bool balance);
        extern void changemap();
        extern void addclient(clientinfo *ci);
        extern void changeteam(clientinfo *ci);
    }

    #define MM_MODE 0xF
    #define MM_AUTOAPPROVE 0x1000
    #define MM_PRIVSERV (MM_MODE | MM_AUTOAPPROVE)
    #define MM_PUBSERV ((1<<MM_OPEN) | (1<<MM_VETO))
    #define MM_COOPSERV (MM_AUTOAPPROVE | MM_PUBSERV | (1<<MM_LOCKED))

    bool notgotitems = true;        // true when map has changed and waiting for clients to send item
    int gamemode = 0, mutators = 0;
    int gamemillis = 0, gamelimit = 0, gamescorelimit = 0, roundgamelimit = 0, gameroundlimit = 0;
    int nextexceeded = 0, gamespeed = 100;
    bool gamepaused = false, shouldstep = true;
    string smapname = "";
    int interm = 0;
    enet_uint32 lastsend = 0;
    int mastermode = MM_OPEN, mastermask = MM_PRIVSERV;
    stream *mapdata = NULL;

    VAR(timelimit, 0, 10, 60);
    VAR(scorelimit, -1, -1, 1000);
    VAR(roundtimelimit, 1, 3, 10);
    VAR(roundlimit, 0, 10, 30);

    VAR(selfdamage, 0, 1, 1);
    VAR(teamdamage, 0, 1, 1);

    namespace serverevents
    {
        vector<serverevent> events;
        void add(void (*action)(), int future)
        {
            serverevent &ev = events.add();
            ev.millis = gamemillis + future;
            ev.action = action;
        }
        void del(void (*action)())
        {
            loopv(events)
            {
                serverevent &ev = events[i];
                if(ev.action == action) events.remove(i--);
            }
        }
        void process()
        {
            loopv(events)
            {
                serverevent &ev = events[i];
                if(gamemillis > ev.millis)
                {
                    if(ev.millis > 0) ev.flush();
                    events.remove(i--);
                }
            }
        }
        void invalidate()
        {
            loopv(events) events[i].millis = -1;
        }
    }

    vector<uint> allowedips;
    vector<ban> bannedips;

    void addban(uint ip, int expire)
    {
        allowedips.removeobj(ip);
        ban b;
        b.time = totalmillis;
        b.expire = totalmillis + expire;
        b.ip = ip;
        loopv(bannedips) if(bannedips[i].expire - b.expire > 0) { bannedips.insert(i, b); return; }
        bannedips.add(b);
    }

    vector<clientinfo *> connects, clients, bots;

    void kickclients(uint ip, clientinfo *actor = NULL, int priv = PRIV_NONE)
    {
        loopvrev(clients)
        {
            clientinfo &c = *clients[i];
            if(c.state.aitype != AI_NONE || c.privilege >= PRIV_ADMIN || c.local) continue;
            if(actor && ((c.privilege > priv && !actor->local) || c.clientnum == actor->clientnum)) continue;
            if(getclientip(c.clientnum) == ip) disconnect_client(c.clientnum, DISC_KICK);
        }
    }

    struct maprotation
    {
        static int exclude;
        int modes;
        string map;

        int calcmodemask() const { return modes&(1<<NUMGAMEMODES) ? modes & ~exclude : modes; }
        bool hasmode(int mode, int offset = STARTGAMEMODE) const { return (calcmodemask() & (1 << (mode-offset))) != 0; }

        int findmode(int mode) const
        {
            if(!hasmode(mode)) loopi(NUMGAMEMODES) if(hasmode(i, 0)) return i+STARTGAMEMODE;
            return mode;
        }

        bool match(int reqmode, const char *reqmap) const
        {
            return hasmode(reqmode) && (!map[0] || !reqmap[0] || !strcmp(map, reqmap));
        }

        bool includes(const maprotation &rot) const
        {
            return rot.modes == modes ? rot.map[0] && !map[0] : (rot.modes & modes) == rot.modes;
        }
    };
    int maprotation::exclude = 0;
    vector<maprotation> maprotations;
    int curmaprotation = 0;

    VAR(lockmaprotation, 0, 0, 2);

    void maprotationreset()
    {
        maprotations.setsize(0);
        curmaprotation = 0;
        maprotation::exclude = 0;
    }

    void nextmaprotation()
    {
        curmaprotation++;
        if(maprotations.inrange(curmaprotation) && maprotations[curmaprotation].modes) return;
        do curmaprotation--;
        while(maprotations.inrange(curmaprotation) && maprotations[curmaprotation].modes);
        curmaprotation++;
    }

    int findmaprotation(int mode, const char *map)
    {
        for(int i = max(curmaprotation, 0); i < maprotations.length(); i++)
        {
            maprotation &rot = maprotations[i];
            if(!rot.modes) break;
            if(rot.match(mode, map)) return i;
        }
        int start;
        for(start = max(curmaprotation, 0) - 1; start >= 0; start--) if(!maprotations[start].modes) break;
        start++;
        for(int i = start; i < curmaprotation; i++)
        {
            maprotation &rot = maprotations[i];
            if(!rot.modes) break;
            if(rot.match(mode, map)) return i;
        }
        int best = -1;
        loopv(maprotations)
        {
            maprotation &rot = maprotations[i];
            if(rot.match(mode, map) && (best < 0 || maprotations[best].includes(rot))) best = i;
        }
        return best;
    }

    bool searchmodename(const char *haystack, const char *needle)
    {
        if(!needle[0]) return true;
        do
        {
            if(needle[0] != '.')
            {
                haystack = strchr(haystack, needle[0]);
                if(!haystack) break;
                haystack++;
            }
            const char *h = haystack, *n = needle+1;
            for(; *h && *n; h++)
            {
                if(*h == *n) n++;
                else if(*h != ' ') break;
            }
            if(!*n) return true;
            if(*n == '.') return !*h;
        } while(needle[0] != '.');
        return false;
    }

    int genmodemask(vector<char *> &modes)
    {
        int modemask = 0;
        loopv(modes)
        {
            const char *mode = modes[i];
            int op = mode[0];
            switch(mode[0])
            {
                case '*':
                    modemask |= 1<<NUMGAMEMODES;
                    loopk(NUMGAMEMODES) if(m_checknot(k+STARTGAMEMODE, M_DEMO|M_EDIT|M_LOCAL)) modemask |= 1<<k;
                    continue;
                case '!':
                    mode++;
                    if(mode[0] != '?') break;
                    // fall through
                case '?':
                    mode++;
                    loopk(NUMGAMEMODES) if(searchmodename(gamemodes[k].name, mode))
                    {
                        if(op == '!') modemask &= ~(1<<k);
                        else modemask |= 1<<k;
                    }
                    continue;
            }
            int modenum = INT_MAX;
            if(isdigit(mode[0])) modenum = atoi(mode);
            else loopk(NUMGAMEMODES) if(searchmodename(gamemodes[k].name, mode)) { modenum = k+STARTGAMEMODE; break; }
            if(!m_valid(modenum)) continue;
            switch(op)
            {
                case '!': modemask &= ~(1 << (modenum - STARTGAMEMODE)); break;
                default: modemask |= 1 << (modenum - STARTGAMEMODE); break;
            }
        }
        return modemask;
    }

    bool addmaprotation(int modemask, const char *map)
    {
        if(!map[0]) loopk(NUMGAMEMODES) if(modemask&(1<<k) && !m_check(k+STARTGAMEMODE, M_EDIT)) modemask &= ~(1<<k);
        if(!modemask) return false;
        if(!(modemask&(1<<NUMGAMEMODES))) maprotation::exclude |= modemask;
        maprotation &rot = maprotations.add();
        rot.modes = modemask;
        copystring(rot.map, map);
        return true;
    }

    void addmaprotations(tagval *args, int numargs)
    {
        vector<char *> modes, maps;
        for(int i = 0; i + 1 < numargs; i += 2)
        {
            explodelist(args[i].getstr(), modes);
            explodelist(args[i+1].getstr(), maps);
            int modemask = genmodemask(modes);
            if(maps.length()) loopvj(maps) addmaprotation(modemask, maps[j]);
            else addmaprotation(modemask, "");
            modes.deletearrays();
            maps.deletearrays();
        }
        if(maprotations.length() && maprotations.last().modes)
        {
            maprotation &rot = maprotations.add();
            rot.modes = 0;
            rot.map[0] = '\0';
        }
    }

    COMMAND(maprotationreset, "");
    COMMANDN(maprotation, addmaprotations, "ss2V");

    struct demofile
    {
        string info;
        uchar *data;
        int len;
    };

    vector<demofile> demos;

    bool demonextmatch = false;
    stream *demotmp = NULL, *demorecord = NULL, *demoplayback = NULL;
    int nextplayback = 0, demomillis = 0;

    VAR(maxdemos, 0, 5, 25);
    VAR(maxdemosize, 0, 16, 31);
    VAR(restrictdemos, 0, 1, 1);
    VARF(autorecorddemo, 0, 0, 1, demonextmatch = autorecorddemo!=0);

    VAR(restrictpausegame, 0, 1, 1);
    VAR(restrictgamespeed, 0, 1, 1);

    SVAR(servername, "");
    SVAR(serverpass, "");
    SVAR(adminpass, "");
    VARF(publicserver, 0, 0, 2, {
        switch(publicserver)
        {
            case 0: default: mastermask = MM_PRIVSERV; break;
            case 1: mastermask = MM_PUBSERV; break;
            case 2: mastermask = MM_COOPSERV; break;
        }
    });
    SVAR(servermessage, "");

    bool firstblood = false;

    bool betweenrounds = false, checkround = false;

    bool isberserkerdead = true, hunterchosen = false;

    struct teamkillkick
    {
        int modes, limit, ban;

        bool match(int mode) const
        {
            return (modes&(1<<(mode-STARTGAMEMODE)))!=0;
        }

        bool includes(const teamkillkick &tk) const
        {
            return tk.modes != modes && (tk.modes & modes) == tk.modes;
        }
    };
    vector<teamkillkick> teamkillkicks;

    void teamkillkickreset()
    {
        teamkillkicks.setsize(0);
    }

    void addteamkillkick(char *modestr, int *limit, int *ban)
    {
        vector<char *> modes;
        explodelist(modestr, modes);
        teamkillkick &kick = teamkillkicks.add();
        kick.modes = genmodemask(modes);
        kick.limit = *limit;
        kick.ban = *ban > 0 ? *ban*60000 : (*ban < 0 ? 0 : 30*60000);
        modes.deletearrays();
    }

    COMMAND(teamkillkickreset, "");
    COMMANDN(teamkillkick, addteamkillkick, "sii");

    struct teamkillinfo
    {
        uint ip;
        int teamkills;
    };
    vector<teamkillinfo> teamkills;
    bool shouldcheckteamkills = false;

    void addteamkill(clientinfo *actor, clientinfo *victim, int n)
    {
        if(!m_timed || actor->state.aitype != AI_NONE || actor->local || actor->privilege || (victim && victim->state.aitype != AI_NONE)) return;
        shouldcheckteamkills = true;
        uint ip = getclientip(actor->clientnum);
        loopv(teamkills) if(teamkills[i].ip == ip)
        {
            teamkills[i].teamkills += n;
            return;
        }
        teamkillinfo &tk = teamkills.add();
        tk.ip = ip;
        tk.teamkills = n;
    }

    void checkteamkills()
    {
        teamkillkick *kick = NULL;
        if(m_timed) loopv(teamkillkicks) if(teamkillkicks[i].match(gamemode) && (!kick || kick->includes(teamkillkicks[i])))
            kick = &teamkillkicks[i];
        if(kick) loopvrev(teamkills)
        {
            teamkillinfo &tk = teamkills[i];
            if(tk.teamkills >= kick->limit)
            {
                if(kick->ban > 0) addban(tk.ip, kick->ban);
                kickclients(tk.ip);
                teamkills.removeunordered(i);
            }
        }
        shouldcheckteamkills = false;
    }

    void *newclientinfo() { return new clientinfo; }
    void deleteclientinfo(void *ci) { delete (clientinfo *)ci; }

    clientinfo *getinfo(int n)
    {
        if(n < MAXCLIENTS) return (clientinfo *)getclientinfo(n);
        n -= MAXCLIENTS;
        return bots.inrange(n) ? bots[n] : NULL;
    }

    uint mcrc = 0;
    vector<entity> ments;
    vector<server_entity> sents;
    vector<savedscore> scores;

    int msgsizelookup(int msg)
    {
        static int sizetable[NUMMSG] = { -1 };
        if(sizetable[0] < 0)
        {
            memset(sizetable, -1, sizeof(sizetable));
            for(const int *p = msgsizes; *p >= 0; p += 2) sizetable[p[0]] = p[1];
        }
        return msg >= 0 && msg < NUMMSG ? sizetable[msg] : -1;
    }

    const char *modename(int n, const char *unknown)
    {
        if(m_valid(n)) return gamemodes[n - STARTGAMEMODE].name;
        return unknown;
    }

    const char *modedesc(int n, const char *unknown)
    {
        if(m_valid(n) && gamemodes[n - STARTGAMEMODE].info) return gamemodes[n - STARTGAMEMODE].info;
        return unknown;
    }

    const char *modeprettyname(int n, const char *unknown)
    {
        if(m_valid(n)) return gamemodes[n - STARTGAMEMODE].prettyname;
        return unknown;
    }

    const char *mastermodename(int n, const char *unknown)
    {
        return (n>=MM_START && size_t(n-MM_START)<sizeof(mastermodenames)/sizeof(mastermodenames[0])) ? mastermodenames[n-MM_START] : unknown;
    }

    void sendservmsg(const char *s) { sendf(-1, 1, "ris", N_SERVMSG, s); }

    void sendservmsgf(const char *fmt, ...) PRINTFARGS(1, 2);
    void sendservmsgf(const char *fmt, ...)
    {
         defvformatstring(s, fmt, fmt);
         sendf(-1, 1, "ris", N_SERVMSG, s);
    }

    void resetitems()
    {
        mcrc = 0;
        ments.setsize(0);
        sents.setsize(0);
    }

    bool serveroption(const char *arg)
    {
        return false;
    }

    void serverinit()
    {
        smapname[0] = '\0';
        resetitems();
    }

    int numclients(int exclude = -1, bool excludespec = true, bool excludeai = true, bool priv = false)
    {
        int n = 0;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->clientnum != exclude
               && (!excludespec || (ci->ghost || ci->state.state != CS_SPECTATOR) || (priv && (ci->privilege || ci->local))) && (!excludeai || ci->state.aitype == AI_NONE)
            ) n++;
        }
        return n;
    }

    bool duplicatename(clientinfo *ci, const char *name)
    {
        if(!name) name = ci->name;
        loopv(clients) if(clients[i]!=ci && duplicatestring(name, clients[i]->name)) return true;
        return false;
    }

    const char *colorname(clientinfo *ci, const char *name = NULL)
    {
        if(!name) name = ci->name;
        if(name[0] && !duplicatename(ci, name) && ci->state.aitype == AI_NONE) return name;
        static string cname[3];
        static int cidx = 0;
        cidx = (cidx+1)%3;
        formatstring(cname[cidx], ci->state.aitype == AI_NONE ? "%s \fs\f5(%d)\fr" : "%s \fs\f5[%d]\fr", name, ci->clientnum);
        return cname[cidx];
    }

    void sendspawn(clientinfo *ci);

    struct servmode
    {
        virtual ~servmode() {}

        virtual void entergame(clientinfo *ci) {}
        virtual void leavegame(clientinfo *ci, bool disconnecting = false) {}

        virtual bool canspawn(clientinfo *ci, bool connecting = false) { return true; }
        virtual void spawned(clientinfo *ci) {}
        virtual int fragvalue(clientinfo *victim, clientinfo *actor)
        {
            if(victim==actor || sameteam(victim->team, actor->team)) return -1;
            return 1;
        }
        virtual void died(clientinfo *victim, clientinfo *actor) {}
        virtual bool canchangeteam(clientinfo *ci, int oldteam, int newteam) { return true; }
        virtual void changeteam(clientinfo *ci, int oldteam, int newteam) {}
        virtual void initclient(clientinfo *ci, packetbuf &p, bool connecting) {}
        virtual void update() {}
        virtual void cleanup() {}
        virtual void setup() {}
        virtual void newmap() {}
        virtual void intermission() {}
        virtual bool hidefrags() { return false; }
        virtual int getteamscore(int team) { return 0; }
        virtual void getteamscores(vector<teamscore> &scores) {}
        virtual bool extinfoteam(int team, ucharbuf &p) { return false; }
    };

    #define SERVMODE 1
    #include "ctf.h"
    #include "elimination.h"

    ctfservmode ctfmode;
    eliminationservmode eliminationmode;
    servmode *smode = NULL;

    bool canspawnitem(int type)
    {
        if (!validitem(type) || m_noitems(mutators))
        {
            return false;
        }

        switch (type)
        {
            case I_AMMO_SG: case I_AMMO_SMG: case I_AMMO_PULSE: case I_AMMO_RL: case I_AMMO_RAIL: case I_AMMO_GRENADE:
            {
                if (m_insta(mutators) || m_tactics(mutators) || m_voosh(mutators))
                {
                    return false;
                }
                break;
            }

            case I_YELLOWSHIELD: case I_REDSHIELD:
            {
                if (m_insta(mutators) || m_effic(mutators))
                {
                    return false;
                }
                break;
            }

            case I_HEALTH:
            {
                if (m_insta(mutators) || m_effic(mutators) || m_vampire(mutators))
                {
                    return false;
                }
                break;
            }

            case I_MEGAHEALTH: case I_ULTRAHEALTH:
            {
                if (m_insta(mutators) || m_vampire(mutators))
                {
                    return false;
                }
                break;
            }

            case I_DDAMAGE: case I_ARMOR: case I_INFINITEAMMO:
            {
                if (m_insta(mutators) || m_nopowerups(mutators))
                {
                    return false;
                }
                break;
            }

            case I_HASTE: case I_AGILITY: case I_INVULNERABILITY:
            {
                if (m_nopowerups(mutators))
                {
                    return false;
                }
                break;
            }

            default: break;
        }
        return true;
    }

    int spawntime(int type)
    {
        return itemstats[type-I_AMMO_SG].respawntime * 1000;
    }

    bool delayspawn(int type)
    {
        return !m_story && itemstats[type - I_AMMO_SG].isspawndelayed;
    }

    bool allowpickup()
    {
        return !((!m_infection && !m_betrayal && betweenrounds) || (m_hunt && hunterchosen && betweenrounds));
    }

    bool pickup(int i, int sender)         // server side item pickup, acknowledge first client that gets it
    {
        if(!allowpickup()) return false;
        if((gamelimit && m_timed && gamemillis>=gamelimit) || !sents.inrange(i) || !sents[i].spawned) return false;
        clientinfo *ci = getinfo(sender);
        if(!ci) return false;
        if(!ci->local && !ci->state.canpickup(sents[i].type))
        {
            sendf(ci->ownernum, 1, "ri3", N_ITEMACC, i, -1);
            return false;
        }
        sents[i].spawned = false;
        sents[i].spawntime = spawntime(sents[i].type);
        sendf(-1, 1, "ri3", N_ITEMACC, i, sender);
        ci->state.pickup(sents[i].type);
        return true;
    }

    static teaminfo teaminfos[MAXTEAMS];

    void clearteaminfo()
    {
        loopi(MAXTEAMS) teaminfos[i].reset();
    }

    clientinfo *choosebestclient(float &bestrank)
    {
        clientinfo *best = NULL;
        bestrank = -1;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.timeplayed<0) continue;
            float rank = ci->state.state!=CS_SPECTATOR ? ci->state.effectiveness/max(ci->state.timeplayed, 1) : -1;
            if(!best || rank > bestrank) { best = ci; bestrank = rank; }
        }
        return best;
    }

    VAR(persistteams, 0, 0, 1);

    void autoteam()
    {
        vector<clientinfo *> team[MAXTEAMS];
        float teamrank[MAXTEAMS] = {0};
        for(int round = 0, remaining = clients.length(); remaining>=0; round++)
        {
            int first = round&1, second = (round+1)&1, selected = 0;
            while(teamrank[first] <= teamrank[second])
            {
                float rank;
                clientinfo *ci = choosebestclient(rank);
                if(!ci) break;
                if(smode && smode->hidefrags()) rank = 1;
                else if(selected && rank<=0) break;
                ci->state.timeplayed = -1;
                team[first].add(ci);
                if(rank>0) teamrank[first] += rank;
                selected++;
                if(rank<=0) break;
            }
            if(!selected) break;
            remaining -= selected;
        }
        loopi(MAXTEAMS)
        {
            loopvj(team[i])
            {
                clientinfo *ci = team[i][j];
                if(ci->team == 1+i) continue;
                if(persistteams && validteam(ci->team) && (!smode || smode->canchangeteam(ci, 1+i, ci->team))) continue;
                ci->team = 1+i;
                sendf(-1, 1, "riiii", N_SETTEAM, ci->clientnum, ci->team, -1);
            }
        }
    }

    struct teamrank
    {
        float rank;
        int clients;

        teamrank() : rank(0), clients(0) {}
    };

    int chooseworstteam(clientinfo *exclude = NULL)
    {
        teamrank teamranks[MAXTEAMS];
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci==exclude || ci->state.aitype!=AI_NONE || ci->state.state==CS_SPECTATOR || !validteam(ci->team)) continue;

            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
            ci->state.lasttimeplayed = lastmillis;

            teamrank &ts = teamranks[ci->team-1];
            ts.rank += ci->state.effectiveness/max(ci->state.timeplayed, 1);
            ts.clients++;
        }
        teamrank *worst = &teamranks[0];
        for(int i = 1; i < MAXTEAMS; i++)
        {
            teamrank &ts = teamranks[i];
            if(smode && smode->hidefrags())
            {
                if(ts.clients < worst->clients || (ts.clients == worst->clients && ts.rank < worst->rank)) worst = &ts;
            }
            else if(ts.rank < worst->rank || (ts.rank == worst->rank && ts.clients < worst->clients)) worst = &ts;
        }
        return 1+int(worst-teamranks);
    }

    void prunedemos(int extra = 0)
    {
        int n = clamp(demos.length() + extra - maxdemos, 0, demos.length());
        if(n <= 0) return;
        loopi(n) delete[] demos[i].data;
        demos.remove(0, n);
    }

    void adddemo()
    {
        if(!demotmp) return;
        int len = (int)min(demotmp->size(), stream::offset((maxdemosize<<20) + 0x10000));
        demofile &d = demos.add();
        time_t t = time(NULL);
        char *timestr = ctime(&t), *trim = timestr + strlen(timestr);
        while(trim>timestr && iscubespace(*--trim)) *trim = '\0';
        formatstring(d.info, "%s: %s, %s, %.2f%s", timestr, modeprettyname(gamemode), smapname, len > 1024*1024 ? len/(1024*1024.f) : len/1024.0f, len > 1024*1024 ? "MB" : "kB");
        sendservmsgf("demo \"%s\" recorded", d.info);
        d.data = new uchar[len];
        d.len = len;
        demotmp->seek(0, SEEK_SET);
        demotmp->read(d.data, len);
        DELETEP(demotmp);
    }

    void enddemorecord()
    {
        if(!demorecord) return;

        DELETEP(demorecord);

        if(!demotmp) return;
        if(!maxdemos || !maxdemosize) { DELETEP(demotmp); return; }

        prunedemos(1);
        adddemo();
    }

    void writedemo(int chan, void *data, int len)
    {
        if(!demorecord) return;
        int stamp[3] = { gamemillis, chan, len };
        lilswap(stamp, 3);
        demorecord->write(stamp, sizeof(stamp));
        demorecord->write(data, len);
        if(demorecord->rawtell() >= (maxdemosize<<20)) enddemorecord();
    }

    void recordpacket(int chan, void *data, int len)
    {
        writedemo(chan, data, len);
    }

    int welcomedemopacket(packetbuf& p);
    void sendwelcome(clientinfo *ci);

    void setupdemorecord()
    {
        if(!m_mp(gamemode) || m_edit) return;

        demotmp = opentempfile("demorecord", "w+b");
        if(!demotmp) return;

        stream *f = opengzfile(NULL, "wb", demotmp);
        if (!f)
        { 
            DELETEP(demotmp);
            return;
        }

        sendservmsg("recording demo");

        demorecord = f;

        demoheader hdr;
        memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
        hdr.version = DEMO_VERSION;
        hdr.protocol = PROTOCOL_VERSION;
        lilswap(&hdr.version, 2);
        demorecord->write(&hdr, sizeof(demoheader));

        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        welcomedemopacket(p);
        writedemo(1, p.buf, p.len);
    }

    void listdemos(int cn)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_SENDDEMOLIST);
        putint(p, demos.length());
        loopv(demos) sendstring(demos[i].info, p);
        sendpacket(cn, 1, p.finalize());
    }

    void cleardemos(int n)
    {
        if(!n)
        {
            loopv(demos) delete[] demos[i].data;
            demos.shrink(0);
            sendservmsg("cleared all demos");
        }
        else if(demos.inrange(n-1))
        {
            delete[] demos[n-1].data;
            demos.remove(n-1);
            sendservmsgf("cleared demo %d", n);
        }
    }

    static void freegetmap(ENetPacket *packet)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->getmap == packet) ci->getmap = NULL;
        }
    }

    static void freegetdemo(ENetPacket *packet)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->getdemo == packet) ci->getdemo = NULL;
        }
    }

    void senddemo(clientinfo *ci, int num, int tag)
    {
        if(ci->getdemo) return;
        if(!num) num = demos.length();
        if(!demos.inrange(num-1)) return;
        demofile &d = demos[num-1];
        if((ci->getdemo = sendf(ci->clientnum, 2, "riim", N_SENDDEMO, tag, d.len, d.data)))
            ci->getdemo->freeCallback = freegetdemo;
    }

    void enddemoplayback()
    {
        if(!demoplayback) return;
        DELETEP(demoplayback);

        loopv(clients) sendf(clients[i]->clientnum, 1, "ri3", N_DEMOPLAYBACK, 0, clients[i]->clientnum);

        sendservmsg("demo playback finished");

        loopv(clients) sendwelcome(clients[i]);
    }

    SVARP(demodir, "demo");

    const char *getdemofile(const char *file, bool init)
    {
        if(!demodir[0]) return NULL;
        static string buf;
        copystring(buf, demodir);
        int dirlen = strlen(buf);
        if(buf[dirlen] != '/' && buf[dirlen] != '\\' && dirlen+1 < (int)sizeof(buf)) { buf[dirlen++] = '/'; buf[dirlen] = '\0'; }
        if(init)
        {
            const char *dir = findfile(buf, "w");
            if(!fileexists(dir, "w")) createdir(dir);
        }
        concatstring(buf, file);
        return buf;
    }

    void setupdemoplayback()
    {
        if(demoplayback) return;
        demoheader hdr;
        string msg;
        msg[0] = '\0';
        string file;
        copystring(file, smapname);
        int len = strlen(file);
        if(len < 4 || strcasecmp(&file[len-4], ".dmo")) concatstring(file, ".dmo");
        if(const char *buf = getdemofile(file, false)) demoplayback = opengzfile(buf, "rb");
        if(!demoplayback) demoplayback = opengzfile(file, "rb");
        if(!demoplayback) formatstring(msg, "could not read demo \"%s\"", file);
        else if(demoplayback->read(&hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic)))
            formatstring(msg, "\"%s\" is not a demo file", file);
        else
        {
            lilswap(&hdr.version, 2);
            if(hdr.version!=DEMO_VERSION) formatstring(msg, "demo \"%s\" requires an %s version of Tesseract", file, hdr.version<DEMO_VERSION ? "older" : "newer");
            else if(hdr.protocol!=PROTOCOL_VERSION) formatstring(msg, "demo \"%s\" requires an %s version of Tesseract", file, hdr.protocol<PROTOCOL_VERSION ? "older" : "newer");
        }
        if(msg[0])
        {
            DELETEP(demoplayback);
            sendservmsg(msg);
            return;
        }

        sendservmsgf("playing demo \"%s\"", file);

        sendf(-1, 1, "ri3", N_DEMOPLAYBACK, 1, -1);

        if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
        {
            enddemoplayback();
            return;
        }
        lilswap(&nextplayback, 1);
    }

    void readdemo()
    {
        if(!demoplayback) return;
        while(gamemillis>=nextplayback)
        {
            int chan, len;
            if(demoplayback->read(&chan, sizeof(chan))!=sizeof(chan) ||
               demoplayback->read(&len, sizeof(len))!=sizeof(len))
            {
                enddemoplayback();
                return;
            }
            lilswap(&chan, 1);
            lilswap(&len, 1);
            ENetPacket *packet = enet_packet_create(NULL, len+1, 0);
            if(!packet || demoplayback->read(packet->data+1, len)!=size_t(len))
            {
                if(packet) enet_packet_destroy(packet);
                enddemoplayback();
                return;
            }
            packet->data[0] = N_DEMOPACKET;
            sendpacket(-1, chan, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            if(!demoplayback) break;
            if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
            {
                enddemoplayback();
                return;
            }
            lilswap(&nextplayback, 1);
        }
    }

    void timeupdate(int secs)
    {
        if(!demoplayback) return;
        if(secs <= 0) interm = -1;
        else gamelimit = max(gamelimit, nextplayback + secs*1000);
    }

    void seekdemo(char *t)
    {
        if(!demoplayback) return;
        bool rev = *t == '-';
        if(rev) t++;
        int mins = strtoul(t, &t, 10), secs = 0, millis = 0;
        if(*t == ':') secs = strtoul(t+1, &t, 10);
        else { secs = mins; mins = 0; }
        if(*t == '.') millis = strtoul(t+1, &t, 10);
        int offset = max(millis + (mins*60 + secs)*1000, 0), prevmillis = gamemillis;
        if(rev) while(gamelimit - offset > gamemillis)
        {
            gamemillis = gamelimit - offset;
            readdemo();
        }
        else if(offset > gamemillis)
        {
            gamemillis = offset;
            readdemo();
        }
        if(gamemillis > prevmillis)
        {
            if(!interm) sendf(-1, 1, "ri3", N_TIMEUP, max((gamelimit - gamemillis)/1000, 1), TimeUpdate_Match);
#ifndef STANDALONE
            game::clearscreeneffects();
#endif
        }
    }

    ICOMMAND(demotime, "sN$", (char *t, int *numargs, ident *id),
    {
        if(*numargs > 0) seekdemo(t);
        else
        {
            int secs = gamemillis/1000;
            defformatstring(str, "%d:%02d.%03d", secs/60, secs%60, gamemillis%1000);
            if(*numargs < 0) result(str);
            else printsvar(id, str);
        }
    });

    void stopdemo()
    {
        if(m_demo) enddemoplayback();
        else enddemorecord();
    }

    void pausegame(bool val, clientinfo *ci = NULL)
    {
        if(gamepaused==val) return;
        gamepaused = val;
        sendf(-1, 1, "riii", N_PAUSEGAME, gamepaused ? 1 : 0, ci ? ci->clientnum : -1);
    }

    void checkpausegame()
    {
        if(!gamepaused) return;
        int admins = 0;
        loopv(clients) if(clients[i]->privilege >= (restrictpausegame ? PRIV_ADMIN : PRIV_MASTER) || clients[i]->local) admins++;
        if(!admins) pausegame(false);
    }

    void forcepaused(bool paused)
    {
        pausegame(paused);
    }

    bool ispaused() { return gamepaused; }

    void changegamespeed(int val, clientinfo *ci = NULL)
    {
        val = clamp(val, 10, 1000);
        if(gamespeed==val) return;
        gamespeed = val;
        sendf(-1, 1, "riii", N_GAMESPEED, gamespeed, ci ? ci->clientnum : -1);
    }

    void forcegamespeed(int speed)
    {
        changegamespeed(speed);
    }

    int scaletime(int t) { return t*gamespeed; }

    SVAR(serverauth, "");

    struct userkey
    {
        char *name;
        char *desc;

        userkey() : name(NULL), desc(NULL) {}
        userkey(char *name, char *desc) : name(name), desc(desc) {}
    };

    static inline uint hthash(const userkey &k) { return ::hthash(k.name); }
    static inline bool htcmp(const userkey &x, const userkey &y) { return !strcmp(x.name, y.name) && !strcmp(x.desc, y.desc); }

    struct userinfo : userkey
    {
        void *pubkey;
        int privilege;

        userinfo() : pubkey(NULL), privilege(PRIV_NONE) {}
        ~userinfo() { delete[] name; delete[] desc; if(pubkey) freepubkey(pubkey); }
    };
    hashset<userinfo> users;

    void adduser(char *name, char *desc, char *pubkey, char *priv)
    {
        userkey key(name, desc);
        userinfo &u = users[key];
        if(u.pubkey) { freepubkey(u.pubkey); u.pubkey = NULL; }
        if(!u.name) u.name = newstring(name);
        if(!u.desc) u.desc = newstring(desc);
        u.pubkey = parsepubkey(pubkey);
        switch(priv[0])
        {
            case 'a': case 'A': u.privilege = PRIV_ADMIN; break;
            case 'm': case 'M': default: u.privilege = PRIV_AUTH; break;
            case 'n': case 'N': u.privilege = PRIV_NONE; break;
        }
    }
    COMMAND(adduser, "ssss");

    void clearusers()
    {
        users.clear();
    }
    COMMAND(clearusers, "");

    void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen)
    {
        char buf[2*sizeof(string)];
        formatstring(buf, "%d %d %s", cn, sessionid, pwd);
        if(!hashstring(buf, result, maxlen)) *result = '\0';
    }

    bool checkpassword(clientinfo *ci, const char *wanted, const char *given)
    {
        string hash;
        hashpassword(ci->clientnum, ci->sessionid, wanted, hash, sizeof(hash));
        return !strcmp(hash, given);
    }

    void revokemaster(clientinfo *ci)
    {
        ci->privilege = PRIV_NONE;
        if(ci->state.state==CS_SPECTATOR && !ci->local)
        {
            aimanager::removeai(ci);
        }
    }

    extern void connected(clientinfo *ci);

    bool setmaster(clientinfo *ci, bool val, const char *pass = "", const char *authname = NULL, const char *authdesc = NULL, int authpriv = PRIV_MASTER, bool force = false, bool trial = false)
    {
        if(authname && !val) return false;
        const char *name = "[unknown]";
        if(val)
        {
            bool haspass = adminpass[0] && checkpassword(ci, adminpass, pass);
            int wantpriv = ci->local || haspass ? PRIV_ADMIN : authpriv;
            if(wantpriv <= ci->privilege) return true;
            else if(wantpriv <= PRIV_MASTER && !force)
            {
                if(ci->state.state==CS_SPECTATOR)
                {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "Spectators may not claim master.");
                    return false;
                }
                loopv(clients) if(ci!=clients[i] && clients[i]->privilege)
                {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "Master is already claimed.");
                    return false;
                }
                if(!authname && !(mastermask&MM_AUTOAPPROVE) && !ci->privilege && !ci->local)
                {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "This server requires you to use the \"/auth\" command to claim master.");
                    return false;
                }
            }
            if(trial) return true;
            ci->privilege = wantpriv;
            if(validprivilege(ci->privilege)) name = privilegenames[ci->privilege];
        }
        else
        {
            if(!ci->privilege) return false;
            if(trial) return true;
            if(validprivilege(ci->privilege)) name = privilegenames[ci->privilege];
            revokemaster(ci);
        }
        bool hasmaster = false;
        loopv(clients) if(clients[i]->local || clients[i]->privilege >= PRIV_MASTER) hasmaster = true;
        if(!hasmaster)
        {
            mastermode = MM_OPEN;
            allowedips.shrink(0);
        }
        string msg;
        if(val && authname)
        {
            if(authdesc && authdesc[0]) formatstring(msg, "%s claimed %s privileges as '\fs\f5%s\fr' [\fs\f0%s\fr]", colorname(ci), name, authname, authdesc);
            else formatstring(msg, "%s claimed %s privileges as '\fs\f5%s\fr'", colorname(ci), name, authname);
        }
        else formatstring(msg, "%s %s %s privileges", colorname(ci), val ? "claimed" : "relinquished", name);
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_SERVMSG);
        sendstring(msg, p);
        putint(p, N_CURRENTMASTER);
        putint(p, mastermode);
        loopv(clients) if(clients[i]->privilege >= PRIV_MASTER)
        {
            putint(p, clients[i]->clientnum);
            putint(p, clients[i]->privilege);
        }
        putint(p, -1);
        sendpacket(-1, 1, p.finalize());
        checkpausegame();
        return true;
    }

    bool trykick(clientinfo *ci, int victim, const char *reason = NULL, const char *authname = NULL, const char *authdesc = NULL, int authpriv = PRIV_NONE, bool trial = false)
    {
        int priv = ci->privilege;
        if(authname)
        {
            if(priv >= authpriv || ci->local) authname = authdesc = NULL;
            else priv = authpriv;
        }
        if((priv || ci->local) && ci->clientnum!=victim)
        {
            clientinfo *vinfo = (clientinfo *)getclientinfo(victim);
            if(vinfo && vinfo->connected && (priv >= vinfo->privilege || ci->local) && vinfo->privilege < PRIV_ADMIN && !vinfo->local)
            {
                if(trial) return true;
                string kicker;
                if(authname)
                {
                    if(authdesc && authdesc[0]) formatstring(kicker, "%s as '\fs\f5%s\fr' [\fs\f0%s\fr]", colorname(ci), authname, authdesc);
                    else formatstring(kicker, "%s as '\fs\f5%s\fr'", colorname(ci), authname);
                }
                else copystring(kicker, colorname(ci));
                if(reason && reason[0])
                {
                    sendservmsgf("%s kicked %s because: %s", kicker, colorname(vinfo), reason);
                    if(isdedicatedserver()) logoutf("%s kicked %s because: %s", kicker, colorname(vinfo), reason);
                }
                else
                {
                    sendservmsgf("%s kicked %s", kicker, colorname(vinfo));
                    if(isdedicatedserver()) logoutf("%s kicked %s", kicker, colorname(vinfo));
                }
                uint ip = getclientip(victim);
                addban(ip, 4*60*60000);
                kickclients(ip, ci, priv);
            }
        }
        return false;
    }

    void trymute(clientinfo *ci, int victim, int val, const char *reason = NULL)
    {
        if((ci->privilege || ci->local) && ci->clientnum!=victim)
        {
            clientinfo *vinfo = (clientinfo *)getclientinfo(victim);
            if(vinfo && vinfo->connected && (ci->privilege >= vinfo->privilege || ci->local) && vinfo->privilege < PRIV_AUTH && !vinfo->local)
            {
                const char *action = val <= 0 ? "unmuted" : "muted";
                if(reason && reason[0])
                {
                    sendservmsgf("%s %s %s because: %s", colorname(ci), action, colorname(vinfo), reason);
                    if(isdedicatedserver()) logoutf("%s %s %s because: %s", colorname(ci), action, colorname(vinfo), reason);
                }
                else
                {
                    sendservmsgf("%s %s %s", colorname(ci), action, colorname(vinfo));
                    if(isdedicatedserver()) logoutf("%s %s %s", colorname(ci), action, colorname(vinfo));
                }
                vinfo->mute = val;
            }
        }
    }

    savedscore *findscore(clientinfo *ci, bool insert)
    {
        uint ip = getclientip(ci->clientnum);
        if(!ip && !ci->local) return 0;
        if(!insert)
        {
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(oi->clientnum != ci->clientnum && getclientip(oi->clientnum) == ip && !strcmp(oi->name, ci->name))
                {
                    oi->state.timeplayed += lastmillis - oi->state.lasttimeplayed;
                    oi->state.lasttimeplayed = lastmillis;
                    static savedscore curscore;
                    curscore.save(oi->state);
                    return &curscore;
                }
            }
        }
        loopv(scores)
        {
            savedscore &sc = scores[i];
            if(sc.ip == ip && !strcmp(sc.name, ci->name)) return &sc;
        }
        if(!insert) return 0;
        savedscore &sc = scores.add();
        sc.ip = ip;
        copystring(sc.name, ci->name);
        return &sc;
    }

    void savescore(clientinfo *ci)
    {
        savedscore *sc = findscore(ci, true);
        if(sc) sc->save(ci->state);
    }

    static struct msgfilter
    {
        uchar msgmask[NUMMSG];

        msgfilter(int msg, ...)
        {
            memset(msgmask, 0, sizeof(msgmask));
            va_list msgs;
            va_start(msgs, msg);
            for(uchar val = 1; msg < NUMMSG; msg = va_arg(msgs, int))
            {
                if(msg < 0) val = uchar(-msg);
                else msgmask[msg] = val;
            }
            va_end(msgs);
        }

        uchar operator[](int msg) const { return msg >= 0 && msg < NUMMSG ? msgmask[msg] : 0; }
    } msgfilter(-1, N_CONNECT, N_SERVINFO, N_INITCLIENT, N_WELCOME, N_MAPCHANGE, N_SERVMSG,
                N_DAMAGE, N_HITPUSH, N_SHOTEVENT, N_SHOTFX, N_EXPLODEFX, N_REGENERATE, N_REPAMMO, N_DIED, 4, N_FORCEDEATH,
                N_TEAMINFO, N_ITEMACC, N_ITEMSPAWN, N_TIMEUP,
                N_CDIS, N_CURRENTMASTER, N_PONG, N_RESUME,
                N_ANNOUNCE, N_SENDDEMOLIST, N_SENDDEMO, N_DEMOPLAYBACK, N_SENDMAP,
                N_DROPFLAG, N_SCOREFLAG, N_RETURNFLAG, N_RESETFLAG, N_ROUND, N_ROUNDSCORE, N_ASSIGNROLE, N_SCORE, N_VOOSH,
                N_CLIENT, N_AUTHCHAL, N_INITAI, N_DEMOPACKET, -2, N_CALCLIGHT, N_REMIP, N_NEWMAP, N_GETMAP, N_SENDMAP,
                N_CLIPBOARD, -3, N_EDITENT, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE, N_EDITVAR, N_EDITVSLOT,
                N_UNDO, N_REDO, -4, N_POS, NUMMSG),
      connectfilter(-1, N_CONNECT, -2, N_AUTHANS, -3, N_PING, NUMMSG);

    int checktype(int type, clientinfo *ci)
    {
        if(ci)
        {
            if(!ci->connected) switch(connectfilter[type])
            {
                // allow only before authconnect
                case 1: return !ci->connectauth ? type : -1;
                // allow only during authconnect
                case 2: return ci->connectauth ? type : -1;
                // always allow
                case 3: return type;
                // never allow
                default: return -1;
            }
            if(ci->local) return type;
        }
        switch(msgfilter[type])
        {
            // server-only messages
            case 1: return ci ? -1 : type;
            // only allowed in coop-edit
            case 2: if(m_edit) break; return -1;
            // only allowed in coop-edit, no overflow check
            case 3: return m_edit ? type : -1;
            // no overflow check
            case 4: return type;
        }
        if(ci && ++ci->overflow >= 200) return -2;
        return type;
    }

    struct worldstate
    {
        int uses, len;
        uchar *data;

        worldstate() : uses(0), len(0), data(NULL) {}

        void setup(int n) { len = n; data = new uchar[n]; }
        void cleanup() { DELETEA(data); len = 0; }
        bool contains(const uchar *p) const { return p >= data && p < &data[len]; }
    };
    vector<worldstate> worldstates;
    bool reliablemessages = false;

    void cleanworldstate(ENetPacket *packet)
    {
        loopv(worldstates)
        {
            worldstate &ws = worldstates[i];
            if(!ws.contains(packet->data)) continue;
            ws.uses--;
            if(ws.uses <= 0)
            {
                ws.cleanup();
                worldstates.removeunordered(i);
            }
            break;
        }
    }

    void flushclientposition(clientinfo &ci)
    {
        if(ci.position.empty() || (!hasnonlocalclients() && !demorecord)) return;
        packetbuf p(ci.position.length(), 0);
        p.put(ci.position.getbuf(), ci.position.length());
        ci.position.setsize(0);
        sendpacket(-1, 0, p.finalize(), ci.ownernum);
    }

    static void sendpositions(worldstate &ws, ucharbuf &wsbuf)
    {
        if(wsbuf.empty()) return;
        int wslen = wsbuf.length();
        recordpacket(0, wsbuf.buf, wslen);
        wsbuf.put(wsbuf.buf, wslen);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            uchar *data = wsbuf.buf;
            int size = wslen;
            if(ci.wsdata >= wsbuf.buf) { data = ci.wsdata + ci.wslen; size -= ci.wslen; }
            if(size <= 0) continue;
            ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(ci.clientnum, 0, packet);
            if(packet->referenceCount) { ws.uses++; packet->freeCallback = cleanworldstate; }
            else enet_packet_destroy(packet);
        }
        wsbuf.offset(wsbuf.length());
    }

    static inline void addposition(worldstate &ws, ucharbuf &wsbuf, int mtu, clientinfo &bi, clientinfo &ci)
    {
        if(bi.position.empty()) return;
        if(wsbuf.length() + bi.position.length() > mtu) sendpositions(ws, wsbuf);
        int offset = wsbuf.length();
        wsbuf.put(bi.position.getbuf(), bi.position.length());
        bi.position.setsize(0);
        int len = wsbuf.length() - offset;
        if(ci.wsdata < wsbuf.buf) { ci.wsdata = &wsbuf.buf[offset]; ci.wslen = len; }
        else ci.wslen += len;
    }

    static void sendmessages(worldstate &ws, ucharbuf &wsbuf)
    {
        if(wsbuf.empty()) return;
        int wslen = wsbuf.length();
        recordpacket(1, wsbuf.buf, wslen);
        wsbuf.put(wsbuf.buf, wslen);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            uchar *data = wsbuf.buf;
            int size = wslen;
            if(ci.wsdata >= wsbuf.buf) { data = ci.wsdata + ci.wslen; size -= ci.wslen; }
            if(size <= 0) continue;
            ENetPacket *packet = enet_packet_create(data, size, (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(ci.clientnum, 1, packet);
            if(packet->referenceCount) { ws.uses++; packet->freeCallback = cleanworldstate; }
            else enet_packet_destroy(packet);
        }
        wsbuf.offset(wsbuf.length());
    }

    static inline void addmessages(worldstate &ws, ucharbuf &wsbuf, int mtu, clientinfo &bi, clientinfo &ci)
    {
        if(bi.messages.empty()) return;
        if(wsbuf.length() + 10 + bi.messages.length() > mtu) sendmessages(ws, wsbuf);
        int offset = wsbuf.length();
        putint(wsbuf, N_CLIENT);
        putint(wsbuf, bi.clientnum);
        putuint(wsbuf, bi.messages.length());
        wsbuf.put(bi.messages.getbuf(), bi.messages.length());
        bi.messages.setsize(0);
        int len = wsbuf.length() - offset;
        if(ci.wsdata < wsbuf.buf) { ci.wsdata = &wsbuf.buf[offset]; ci.wslen = len; }
        else ci.wslen += len;
    }

    bool buildworldstate()
    {
        int wsmax = 0;
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            ci.overflow = 0;
            ci.wsdata = NULL;
            wsmax += ci.position.length();
            if(ci.messages.length()) wsmax += 10 + ci.messages.length();
        }
        if(wsmax <= 0)
        {
            reliablemessages = false;
            return false;
        }
        worldstate &ws = worldstates.add();
        ws.setup(2*wsmax);
        int mtu = getservermtu() - 100;
        if(mtu <= 0) mtu = ws.len;
        ucharbuf wsbuf(ws.data, ws.len);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            addposition(ws, wsbuf, mtu, ci, ci);
            loopvj(ci.bots) addposition(ws, wsbuf, mtu, *ci.bots[j], ci);
        }
        sendpositions(ws, wsbuf);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            addmessages(ws, wsbuf, mtu, ci, ci);
            loopvj(ci.bots) addmessages(ws, wsbuf, mtu, *ci.bots[j], ci);
        }
        sendmessages(ws, wsbuf);
        reliablemessages = false;
        if(ws.uses) return true;
        ws.cleanup();
        worldstates.drop();
        return false;
    }

    bool sendpackets(bool force)
    {
        if(clients.empty() || (!hasnonlocalclients() && !demorecord)) return false;
        enet_uint32 curtime = enet_time_get()-lastsend;
        if(curtime<40 && !force) return false;
        bool flush = buildworldstate();
        lastsend += curtime - (curtime%40);
        return flush;
    }

    template<class T>
    void sendstate(servstate &gs, T &p)
    {
        putint(p, gs.lifesequence);
        putint(p, gs.health);
        putint(p, gs.maxhealth);
        putint(p, gs.shield);
        putint(p, gs.gunselect);
        loopi(NUMGUNS) putint(p, gs.ammo[i]);
    }

    int vooshgun = -1;

    void spawnstate(clientinfo *ci)
    {
        servstate &gs = ci->state;
        gs.spawnstate(gamemode, mutators, vooshgun);
        gs.lifesequence = (gs.lifesequence + 1)&0x7F;
    }

    void sendspawn(clientinfo *ci)
    {
        servstate &gs = ci->state;
        spawnstate(ci);
        sendf(ci->ownernum, 1, "rii6v",
              N_SPAWNSTATE, ci->clientnum, gs.lifesequence,
              gs.health, gs.maxhealth, gs.shield, gs.gunselect,
              NUMGUNS, gs.ammo);
        gs.lastspawn = gamemillis;
        gs.lastmove = lastmillis;
    }

    int welcomepacket(packetbuf& p, clientinfo* ci);

    void sendwelcome(clientinfo *ci)
    {
        if (ci)
        {
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            int chan = welcomepacket(p, ci);
            sendpacket(ci->clientnum, chan, p.finalize());
        }
    }

    void putinitclient(clientinfo *ci, packetbuf &p, bool notify = false)
    {
        if(ci->state.aitype != AI_NONE)
        {
            putint(p, N_INITAI);
            putint(p, ci->clientnum);
            putint(p, ci->ownernum);
            putint(p, ci->state.aitype);
            putint(p, ci->state.skill);
            putint(p, ci->playermodel);
            putint(p, ci->playercolor);
            putint(p, ci->team);
            sendstring(ci->name, p);
        }
        else
        {
            putint(p, N_INITCLIENT);
            putint(p, ci->clientnum);
            putint(p, notify);
            sendstring(ci->name, p);
            putint(p, ci->team);
            putint(p, ci->playermodel);
            putint(p, ci->playercolor);
            sendstring(ci->customflag_code, p);
            sendstring(ci->customflag_name, p);
        }
    }

    void welcomeinitclient(packetbuf &p, int exclude = -1)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(!ci->connected || ci->clientnum == exclude) continue;

            putinitclient(ci, p);
        }
    }

    bool hasmap(clientinfo *ci)
    {
        return (m_edit && (clients.length() > 0 || ci->local)) ||
               (smapname[0] && (!gamelimit || !m_timed || (m_round && !interm) || (gamemillis < gamelimit) || (ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) || numclients(ci->clientnum, true, true, true)));
    }

    int welcomedemopacket(packetbuf& p)
    {
        putint(p, N_WELCOME);

        putint(p, N_MAPCHANGE);
        sendstring(smapname, p);
        putint(p, gamemode);
        putint(p, mutators);
        putint(p, gamescorelimit);
        putint(p, notgotitems ? 1 : 0);
        putint(p, N_TIMEUP);
        putint(p, gamewaiting || (gamemillis < gamelimit && !interm) ? max((gamelimit - gamemillis) / 1000, 1) : 0);
        putint(p, TimeUpdate_Match);
        if (!notgotitems)
        {
            putint(p, N_ITEMLIST);
            loopv(sents) if (sents[i].spawned)
            {
                putint(p, i);
                putint(p, sents[i].type);
            }
            putint(p, -1);
        }
        bool hasmaster = false;
        if (mastermode != MM_OPEN)
        {
            putint(p, N_CURRENTMASTER);
            putint(p, mastermode);
            hasmaster = true;
        }
        loopv(clients) if (clients[i]->privilege >= PRIV_MASTER)
        {
            if (!hasmaster)
            {
                putint(p, N_CURRENTMASTER);
                putint(p, mastermode);
                hasmaster = true;
            }
            putint(p, clients[i]->clientnum);
            putint(p, clients[i]->privilege);
        }
        if (hasmaster) putint(p, -1);
        if (gamepaused)
        {
            putint(p, N_PAUSEGAME);
            putint(p, 1);
            putint(p, -1);
        }
        if (gamespeed != 100)
        {
            putint(p, N_GAMESPEED);
            putint(p, gamespeed);
            putint(p, -1);
        }
        if (m_teammode)
        {
            putint(p, N_TEAMINFO);
            loopi(MAXTEAMS)
            {
                teaminfo& t = teaminfos[i];
                putint(p, t.frags);
            }
        }
        putint(p, N_RESUME);
        loopv(clients)
        {
            clientinfo* oi = clients[i];
            putint(p, oi->clientnum);
            putint(p, oi->state.state);
            putint(p, oi->state.frags);
            putint(p, oi->state.flags);
            putint(p, oi->state.deaths);
            putint(p, oi->state.points);
            putint(p, oi->state.poweruptype);
            putint(p, oi->state.powerupmillis);
            putint(p, oi->state.role);
            sendstate(oi->state, p);
        }
        putint(p, -1);
        welcomeinitclient(p, -1);
        if (smode) smode->initclient(NULL, p, true);
        return 1;
    }

    int welcomepacket(packetbuf &p, clientinfo *ci)
    {
        putint(p, N_WELCOME);

        putint(p, N_COUNTRY);
        putint(p, ci->clientnum);
        sendstring(ci->customflag_code, p);
        sendstring(ci->customflag_name, p);

        putint(p, N_MAPCHANGE);
        sendstring(smapname, p);
        putint(p, gamemode);
        putint(p, mutators);
        putint(p, gamescorelimit);
        putint(p, notgotitems ? 1 : 0);
        if (gamelimit && m_timed && smapname[0])
        {
            putint(p, N_TIMEUP);
            putint(p, gamewaiting || (gamemillis < gamelimit && !interm) ? max((gamelimit - gamemillis)/1000, 1) : 0);
            putint(p, TimeUpdate_Match);
        }
        if (!notgotitems)
        {
            putint(p, N_ITEMLIST);
            loopv(sents) if(sents[i].spawned)
            {
                putint(p, i);
                putint(p, sents[i].type);
            }
            putint(p, -1);
        }
        bool hasmaster = false;
        if (mastermode != MM_OPEN)
        {
            putint(p, N_CURRENTMASTER);
            putint(p, mastermode);
            hasmaster = true;
        }
        loopv(clients) if (clients[i]->privilege >= PRIV_MASTER)
        {
            if (!hasmaster)
            {
                putint(p, N_CURRENTMASTER);
                putint(p, mastermode);
                hasmaster = true;
            }
            putint(p, clients[i]->clientnum);
            putint(p, clients[i]->privilege);
        }
        if (hasmaster) putint(p, -1);
        if (gamepaused)
        {
            putint(p, N_PAUSEGAME);
            putint(p, 1);
            putint(p, -1);
        }
        if (gamespeed != 100)
        {
            putint(p, N_GAMESPEED);
            putint(p, gamespeed);
            putint(p, -1);
        }
        if (m_teammode)
        {
            putint(p, N_TEAMINFO);
            loopi(MAXTEAMS)
            {
                teaminfo &t = teaminfos[i];
                putint(p, t.frags);
            }
        }
        putint(p, N_SETTEAM);
        putint(p, ci->clientnum);
        putint(p, ci->team);
        putint(p, -1);
        if ((m_demo || m_mp(gamemode)) && ci->state.state!=CS_SPECTATOR)
        {
            if ((smode && !smode->canspawn(ci, true)) || (((m_hunt && hunterchosen) || (m_round && !m_infection && !m_betrayal)) && numclients(-1, true, false) > 1))
            {
                ci->state.state = CS_DEAD;
                putint(p, N_FORCEDEATH);
                putint(p, ci->clientnum);
                sendf(-1, 1, "ri2x", N_FORCEDEATH, ci->clientnum, ci->clientnum);
            }
            else
            {
                servstate &gs = ci->state;
                spawnstate(ci);
                putint(p, N_SPAWNSTATE);
                putint(p, ci->clientnum);
                sendstate(gs, p);
                gs.lastspawn = gamemillis;
            }
        }
        if (ci && ci->state.state==CS_SPECTATOR)
        {
            putint(p, N_SPECTATOR);
            putint(p, ci->clientnum);
            putint(p, 1);
            putint(p, ci->ghost);
            sendf(-1, 1, "ri4x", N_SPECTATOR, ci->clientnum, 1, ci->ghost, ci->clientnum);
        }
        if (clients.length() > 1)
        {
            putint(p, N_RESUME);
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(oi->clientnum == ci->clientnum) continue;
                putint(p, oi->clientnum);
                putint(p, oi->state.state);
                putint(p, oi->state.frags);
                putint(p, oi->state.flags);
                putint(p, oi->state.deaths);
                putint(p, oi->state.points);
                putint(p, oi->state.poweruptype);
                putint(p, oi->state.powerupmillis);
                putint(p, oi->state.role);
                sendstate(oi->state, p);
            }
            putint(p, -1);
            if (ci)
            {
                welcomeinitclient(p, ci->clientnum);
            }
        }
        if (smode)
        {
            smode->initclient(ci, p, true);
        }
        return 1;
    }

    bool restorescore(clientinfo *ci)
    {
        //if(ci->local) return false;
        savedscore *sc = findscore(ci, false);
        if(sc)
        {
            sc->restore(ci->state);
            return true;
        }
        return false;
    }

    void sendresume(clientinfo *ci)
    {
        servstate &gs = ci->state;
        sendf(-1, 1, "ri3i9i3vi", N_RESUME, ci->clientnum, gs.state,
            gs.frags, gs.flags, gs.deaths, gs.points,
            gs.poweruptype, gs.powerupmillis,
            gs.role, gs.lifesequence,
            gs.health, gs.maxhealth, gs.shield, gs.gunselect,
            NUMGUNS, gs.ammo, -1);
    }

    void sendinitclient(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putinitclient(ci, p, true);
        sendpacket(-1, 1, p.finalize(), ci->clientnum);
    }

    void loaditems()
    {
        resetitems();
        notgotitems = true;
        if(m_edit || !loadents(smapname, ments, &mcrc))
            return;
        loopv(ments) if(canspawnitem(ments[i].type))
        {
            server_entity se = { NOTUSED, 0, false };
            while(sents.length()<=i) sents.add(se);
            sents[i].type = ments[i].type;
            if(m_mp(gamemode) && delayspawn(sents[i].type)) sents[i].spawntime = spawntime(sents[i].type);
            else sents[i].spawned = true;
        }
        notgotitems = false;
    }

    /* intermission logic:
     * useful to decide whether to end a game or a round
     * when to start overtime or when to reset a timer
     */
    VAR(overtime, 0, 2, 10);

    void updatetimelimit(int add, bool reset = false, int type = TimeUpdate_Match)
    {
        if(reset) gamelimit = min(gamelimit, gamemillis);
        gamelimit = max(gamemillis, gamelimit) + add * 60000;
        sendf(-1, 1, "ri3", N_TIMEUP, max((gamelimit - gamemillis)/1000, 1), type);
    }

    bool checkovertime(bool timeisup)
    {
        if(!gamelimit || !m_timed || !overtime) return false;
        int topteam = 0;
        int topscore = INT_MIN;
        bool tied = false;
        if(m_teammode)
        {
            vector<teamscore> scores;
            if(smode && smode->hidefrags()) smode->getteamscores(scores);
            loopv(clients)
            {
                clientinfo *ci = clients[i];
                if(ci->state.state==CS_SPECTATOR || !validteam(ci->team)) continue;
                int score = 0;
                if(smode && smode->hidefrags())
                {
                    int idx = scores.htfind(ci->team);
                    if(idx >= 0) score = scores[idx].score;
                }
                else score = teaminfos[ci->team-1].frags;
                if(!topteam || score > topscore) { topteam = ci->team; topscore = score; tied = false; }
                else if(score == topscore && ci->team != topteam) tied = true;
            }
        }
        else
        {
            loopv(clients)
            {
                clientinfo *ci = clients[i];
                if(ci->state.state==CS_SPECTATOR) continue;
                int score = ci->state.frags;
                if(score > topscore) { topscore = score; tied = false; }
                else if(score == topscore) tied = true;
            }
        }
        if(!tied) return false;
        sendf(-1, 1, "ri2s", N_ANNOUNCE, S_ANNOUNCER_OVERTIME, m_teammode ? "\f2Overtime: teams are tied" : "\f2Overtime: scores are tied");
        if (!m_round && timeisup)
        {
            updatetimelimit(overtime, false, TimeUpdate_Overtime);
        }
        return true;
    }

    VAR(intermissionlimit, 10, 30, 60);

    void gameover()
    {
        sendf(-1, 1, "ri3", N_TIMEUP, intermissionlimit, TimeUpdate_Intermission);
        if(smode) smode->intermission();
        serverevents::invalidate();
        changegamespeed(100);
        interm = gamemillis + intermissionlimit * 1000;
    }

    void checkintermission(bool force = false)
    {
        if(gamemillis < gamelimit || interm) return;
        if(force)
        {
            gameover();
            return;
        }
        if(m_round)
        {
            checkplayers(true);
            return;
        }
        if(!checkovertime(true)) gameover();
    }

    void startintermission()
    {
        gamelimit = min(gamelimit, gamemillis);
        checkintermission(true);
    }

    /* score system:
     * here we do things like checking for the best (winning) player,
     * adding score to individual players or teams,
     * checking the frag limit, etc.
     */
    clientinfo *winningclient()
    {
        clientinfo *best = NULL;
        int highscore = -1;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state == CS_SPECTATOR) continue;
            int score = m_dm ? ci->state.frags : ci->state.points;
            if(score > highscore)
            {
                best = ci;
                highscore = score;
            }
        }
        return best;
    }

    void checkscorelimit(clientinfo *ci)
    {
        if(gamescorelimit == 0 || m_ctf || m_elimination) return;
        teaminfo *team = m_teammode && validteam(ci->team) ? &teaminfos[ci->team-1] : NULL;
        int highscore = team && m_teammode ? team->frags : ci->state.frags;
        if(!m_dm) highscore = ci->state.points;
        else
        {
            int remain = gamescorelimit - highscore;
            if(remain == 10 || remain == 5 || remain == 1)
            {
                defformatstring(announcement, "\f2%d kill%s remain%s", remain, remain == 1 ? "" : "s", remain == 1 ? "s" : "");
                int announcementsound = remain == 10 ? S_ANNOUNCER_10_KILLS : (remain == 5 ? S_ANNOUNCER_5_KILLS : S_ANNOUNCER_1_KILL);
                sendf(-1, 1, "ri2s", N_ANNOUNCE, announcementsound, announcement);
            }
        }
        if(highscore >= gamescorelimit)
        {
            if(checkovertime()) return;
            startintermission();
            defformatstring(winner, "%s%s \fs\f2reached the score limit\fr", team ? teamtextcode[ci->team] : "", team ? teamnames[ci->team] : colorname(ci));
            sendf(-1, 1, "ri2s", N_ANNOUNCE, NULL, winner);
        }
    }

    void addscore(clientinfo *ci, int score = 1)
    {
        ci->state.points += score;
        sendf(-1, 1, "ri3", N_SCORE, ci->clientnum, ci->state.points);
        if(m_edit || gamescorelimit == 0) return;
        clientinfo *best = winningclient();
        if(ci == best) checkscorelimit(best);
    }

    // function to choose a random client used in certain game modes
    clientinfo *random(clientinfo *exclude = NULL)
    {
        if(clients.length() <= (exclude ? 1 : 0)) return NULL;
        clientinfo *client = NULL;
        int attempts = 0;
        while(attempts < clients.length())
        {
            attempts++;
            client = clients[rnd(clients.length())];
            if(client == exclude || client->state.state != CS_ALIVE) continue;
            return client;
        }
        return client;
    }

    // function to convert a player to a berserker in the same mode
    void makeberserker(clientinfo *ci)
    {
        if (!m_berserker || !ci || ci->state.state != CS_ALIVE || !isberserkerdead)
        {
            return;
        }
        ci->state.assignrole(ROLE_BERSERKER);
        sendf(-1, 1, "ri4", N_ASSIGNROLE, ci->clientnum, ci->clientnum, ROLE_BERSERKER);
        isberserkerdead = false;
    }

    void checkberserker(clientinfo *ci)
    {
        if(m_berserker && ci->state.role == ROLE_BERSERKER)
        {
            isberserkerdead = true;
        }
    }

    /* functions used to manage "infection" mode specifically:
     * things like choosing random player(s) to infect at the beginning
     * of each round, infect message and effects, etc.
     */
    void infect(clientinfo *ci, clientinfo *actor)
    {
        if (!m_infection || !ci || ci->state.state != CS_ALIVE)
        {
            return;
        }
        ci->state.assignrole(ROLE_ZOMBIE);
        sendf(-1, 1, "ri4", N_ASSIGNROLE, ci->clientnum, actor->clientnum, ROLE_ZOMBIE);
        if(!hunterchosen) sendf(-1, 1, "ri2s", N_ANNOUNCE, S_INFECTION, "\fs\f2Infection has begun\fr");
    }

    void chooserandomclient()
    {
        clientinfo *hunter = random();
        if(hunter != NULL)
        {
            if(m_infection) infect(hunter, hunter);
            else if(m_betrayal) hunter->state.role = ROLE_TRAITOR;
        }
        checkroundwait();
    }

    static void startzombieround()
    {
        if(hunterchosen || interm) return;
        const int numplayers = numclients(-1, true, false);
        if(numplayers > 0)
        {
            int np = max(numplayers/4, 1);
            loopi(np) chooserandomclient();
            updateroundstate(ROUND_START);
            if(!gamewaiting) checkplayers();
        }
    }

    static void startbetrayalround()
    {
        if(hunterchosen || interm) return;
        chooserandomclient();
        updateroundstate(ROUND_START);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state != CS_ALIVE && ci->state.state != CS_LAGGED) continue;
            if(ci->state.role == ROLE_TRAITOR)
            {
                sendf(ci->clientnum, 1, "ri2s", N_ANNOUNCE, S_TRAITOR, "\f2You are the traitor");
                continue;
            }
            sendf(ci->clientnum, 1, "ri2s", N_ANNOUNCE, S_VICTIM, "\f2You are a victim");
        }
    }

    /* functions used to manage rounds and check players
     * in round-based modes only
     */
    void updateroundstate(int state, bool send)
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
        }
        if(state & ROUND_WAIT)
        {
            gamewaiting = true;
        }
        else if(state & ROUND_UNWAIT)
        {
            gamewaiting = false;
        }

        if(send) sendf(-1, 1, "rii", N_ROUND, state);
    }

    void checkroundwait()
    {
        bool notenoughplayers = numclients(-1, true, false) <= 1;
        if(gamewaiting != notenoughplayers) // don't do anything if waiting state is on point
        {
            if(notenoughplayers) updateroundstate(ROUND_WAIT);
            else updateroundstate(ROUND_UNWAIT);
        }
    }

    void roundrespawn()
    {
        loopv(clients)
        {
            if(clients[i]->state.state != CS_EDITING && (clients[i]->state.state != CS_SPECTATOR || clients[i]->ghost))
            {
                clientinfo *ci = clients[i];
                if(ci->ghost)
                {
                    ci->ghost = false;
                    extern void unspectate(clientinfo *ci);
                    unspectate(ci);
                }
                ci->state.respawn();
                sendspawn(ci);
                ci->state.projectiles.reset();
            }
        }
    }

    int rounds = 0;

    void newround()
    {
        if(interm) return;
        updatetimelimit(roundtimelimit, true);
        roundrespawn();
        if(m_hunt)
        {
            updateroundstate(ROUND_RESET);
            serverevents::add(m_infection ? &startzombieround : &startbetrayalround, 10000);
        }
        else updateroundstate(ROUND_RESET|ROUND_START);
        if(!m_infection && !m_betrayal) checkroundwait();
    }

    bool countround()
    {
        if(!gameroundlimit || gamewaiting) return false;
        rounds++;
        if(rounds >= gameroundlimit)
        {
            if(checkovertime()) return false;
            startintermission();
            if(rounds == gameroundlimit) sendf(-1, 1, "ri2s", N_ANNOUNCE, NULL, "\f2Maximum number of rounds has been reached");
            return true;
        }
        return false;
    }

    void endround()
    {
        if(betweenrounds || interm) return;
        updateroundstate(ROUND_END);
        if(!countround()) serverevents::add(&newround, 5000);
    }

    void shouldcheckround()
    {
        if(smode || !m_round) return; // team modes like elimination have their own check rules
        checkround = true;
    }

    bool gamewaiting = false;

    void checkplayers(bool timeisup)
    {
        if(!m_round || betweenrounds) return;
        if(numclients(-1, true, false) <= 1 && !gamewaiting) updateroundstate(ROUND_WAIT);
        int survivors = 0, hunters = 0;
        if(!m_elimination) loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state != CS_ALIVE && ci->state.state != CS_LAGGED) continue;
            if(ci->state.role == ROLE_ZOMBIE || ci->state.role == ROLE_TRAITOR) hunters++;
            else survivors++;
        }
        bool rewardhunters = false, rewardsurvivors = false;
        int score = 0;
        if(m_hunt && hunterchosen)
        {
            if((survivors <= 0 && hunters <= 0) || (timeisup && survivors <= 0 && hunters > 0))
            {
                sendf(-1, 1, "ri2s", N_ANNOUNCE, timeisup ? S_WIN_SURVIVORS : S_WIN_ZOMBIES, timeisup? "\f2Time is up": "\f2Nobody survived");
                endround();
            }
            else if(hunters <= 0 || (timeisup && survivors > 0))
            {
                sendf(-1, 1, "ri2s", N_ANNOUNCE, S_WIN_SURVIVORS, "\f2Survivors win the round");
                rewardsurvivors = true;
                score = 5;
                endround();
            }
            else if(survivors <= 0)
            {
                sendf(-1, 1, "ri2s", N_ANNOUNCE, S_WIN_ZOMBIES, m_infection ? "\f2Zombies win the round" : "\f2Traitor wins the round");
                rewardhunters = true;
                score = m_infection ? 1 : 5;
                endround();
            }
        }
        else if(m_lms)
        {
            if(survivors <= 0)
            {
                sendf(-1, 1, "ri2s", N_ANNOUNCE, S_ROUND, "\f2Nobody survived");
                endround();
            }
            else if(survivors < 2)
            {
                if(numclients(-1, true, false) < 2) return;
                rewardsurvivors = true;
                score = 1;
                endround();
            }
        }
        if(timeisup && !betweenrounds)
        {
            sendf(-1, 1, "ri2s", N_ANNOUNCE, S_ROUND, "\f2Time is up");
            endround(); // make sure to start a new round if we ran out of time in any round-based mode
        }
        if(gamewaiting || m_elimination || (!rewardsurvivors && !rewardhunters)) return;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state != CS_ALIVE && ci->state.state != CS_LAGGED) continue;
            if((rewardsurvivors && ci->state.role >= ROLE_ZOMBIE) || (rewardhunters && ci->state.role < ROLE_ZOMBIE)) continue;
            addscore(ci, score);
            if(!interm && m_lms && ci->state.aitype == AI_NONE)
            {
                sendf(ci->clientnum, 1, "ri2s", N_ANNOUNCE, S_ROUND_WIN, "\f2You win the round");
            }
        }
    }

    /* here we manage stuff like that freaking voosh mutator
     * crazy stuff
     */
    int vooshtime = 0, previousvooshgun = -1;

    void sendvoosh(int gun)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            sendf(-1, 1, "ri3", N_VOOSH, ci->clientnum, gun);
            if(ci->state.state == CS_DEAD || ci->state.state == CS_SPECTATOR || ci->state.role == ROLE_ZOMBIE) continue;
            ci->state.voosh(gun);
        }
    }

    void voosh()
    {
        previousvooshgun = vooshgun;
        do vooshgun = rnd(NUMGUNS - 2); while(vooshgun == previousvooshgun);
        sendvoosh(vooshgun);
    }

    void changemap(const char *s, int mode, int muts)
    {
        stopdemo();
        pausegame(false);
        changegamespeed(100);
        if(smode) smode->cleanup();
        aimanager::clearai();
        serverevents::invalidate();

        gamemode = mode;
        mutators = muts;
        gamemillis = 0;
        if(m_round)
        {
            rounds = 0;
            gamelimit = roundtimelimit*60000;
            gameroundlimit = roundlimit;
        }
        else gamelimit = timelimit*60000;
        if(scorelimit < 0) // automatically determine a suitable score limit for each mode
        {
            if(m_ctf || m_elimination) gamescorelimit = 10;
            else if(m_lms) gamescorelimit = 8;
            else if(m_dm && m_teammode) gamescorelimit = 60; // TDM
            else gamescorelimit = 30;
        }
        else gamescorelimit = scorelimit;
        interm = nextexceeded = 0;
        copystring(smapname, s);
        loaditems();
        scores.shrink(0);
        shouldcheckteamkills = false;
        teamkills.shrink(0);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
        }

        firstblood = false;
        if(m_round)
        {
            if(m_hunt)
            {
                gamewaiting = false;
                hunterchosen = false;
                updateroundstate(ROUND_END);
                serverevents::add(m_infection ? &startzombieround : &startbetrayalround, 10000);
            }
            else
            {
                updateroundstate(ROUND_START, false);
                checkroundwait();
            }
        }
        else
        {
            gamewaiting = false;
            if(betweenrounds) betweenrounds = false;
            if(m_berserker) isberserkerdead = true;
        }

        if(m_voosh(mutators))
        {
            vooshtime = gamemillis + 20000;
            vooshgun = rnd(NUMGUNS - 2);
        }

        if(!m_mp(gamemode)) kicknonlocalclients(DISC_LOCAL);

        sendf(-1, 1, "risi4", N_MAPCHANGE, smapname, gamemode, mutators, gamescorelimit, 1);

        clearteaminfo();
        if(m_teammode) autoteam();

        if(m_ctf) smode = &ctfmode;
        else if(m_elimination) smode = &eliminationmode;
        else smode = NULL;

        if(gamelimit && m_timed && smapname[0])
        {
            sendf(-1, 1, "ri3", N_TIMEUP, gamewaiting || (gamemillis < gamelimit && !interm) ? max((gamelimit - gamemillis)/1000, 1) : 0, TimeUpdate_Match);
        }

        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->mapchange();
            ci->state.lasttimeplayed = lastmillis;
            if(m_mp(gamemode) && ci->state.state!=CS_SPECTATOR) sendspawn(ci);
        }

        aimanager::changemap();

        if(m_demo)
        {
            if(clients.length()) setupdemoplayback();
        }
        else
        {
            if(demonextmatch) setupdemorecord();
            demonextmatch = autorecorddemo!=0;
        }

        if(smode) smode->setup();
    }

    void rotatemap(bool next)
    {
        if(!maprotations.inrange(curmaprotation))
        {
            changemap("", 0, 0);
            return;
        }
        if(next)
        {
            curmaprotation = findmaprotation(gamemode, smapname);
            if(curmaprotation >= 0) nextmaprotation();
            else curmaprotation = smapname[0] ? max(findmaprotation(gamemode, ""), 0) : 0;
        }
        maprotation &rot = maprotations[curmaprotation];
        changemap(rot.map, rot.findmode(gamemode), mutators);
    }

    struct votecount
    {
        char *map;
        int mode, muts, count;
        votecount() {}
        votecount(char *s, int n, int m) : map(s), mode(n), muts(m), count(0) {}
    };

    void checkvotes(bool force = false)
    {
        vector<votecount> votes;
        int maxvotes = 0;
        loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->state.state==CS_SPECTATOR && !oi->privilege && !oi->local) continue;
            if(oi->state.aitype!=AI_NONE) continue;
            maxvotes++;
            if(!m_valid(oi->modevote)) continue;
            votecount *vc = NULL;
            loopvj(votes) if(!strcmp(oi->mapvote, votes[j].map) && oi->modevote==votes[j].mode && oi->mutsvote==votes[j].muts)
            {
                vc = &votes[j];
                break;
            }
            if(!vc) vc = &votes.add(votecount(oi->mapvote, oi->modevote, oi->mutsvote));
            vc->count++;
        }
        votecount *best = NULL;
        loopv(votes) if(!best || votes[i].count > best->count || (votes[i].count == best->count && rnd(2))) best = &votes[i];
        if(force || (best && best->count > maxvotes/2))
        {
            if(demorecord) enddemorecord();
            if(best && (best->count > (force ? 1 : maxvotes/2)))
            {
                sendservmsg(force ? "vote passed by default" : "vote passed by majority");
                changemap(best->map, best->mode, best->muts);
            }
            else rotatemap(true);
        }
    }

    void forcemap(const char *map, int mode, int muts)
    {
        stopdemo();
        if(!map[0] && !m_check(mode, M_EDIT))
        {
            int idx = findmaprotation(mode, smapname);
            if(idx < 0 && smapname[0]) idx = findmaprotation(mode, "");
            if(idx < 0) return;
            map = maprotations[idx].map;
        }
        if(hasnonlocalclients()) sendservmsgf("local player forced %s on map %s", modeprettyname(mode), map[0] ? map : "[new map]");
        changemap(map, mode, muts);
    }

    void vote(const char *map, int reqmode, int reqmuts, int sender)
    {
        clientinfo *ci = getinfo(sender);
        if(!ci || (ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) || (!ci->local && !m_mp(reqmode))) return;
        if(!m_valid(reqmode)) return;
        if(!map[0] && !m_check(reqmode, M_EDIT))
        {
            int idx = findmaprotation(reqmode, smapname);
            if(idx < 0 && smapname[0]) idx = findmaprotation(reqmode, "");
            if(idx < 0) return;
            map = maprotations[idx].map;
        }
        if(lockmaprotation && !ci->local && ci->privilege < (lockmaprotation > 1 ? PRIV_ADMIN : PRIV_MASTER) && findmaprotation(reqmode, map) < 0)
        {
            sendf(sender, 1, "ris", N_SERVMSG, "This server has locked the map rotation.");
            return;
        }
        copystring(ci->mapvote, map);
        ci->modevote = reqmode;
        ci->mutsvote = reqmuts;
        if(ci->local || (ci->privilege && mastermode>=MM_VETO))
        {
            if(demorecord) enddemorecord();
            if(!ci->local || hasnonlocalclients())
                sendservmsgf("%s forced %s on map %s", colorname(ci), modeprettyname(ci->modevote), ci->mapvote[0] ? ci->mapvote : "[new map]");
            changemap(ci->mapvote, ci->modevote, ci->mutsvote);
        }
        else
        {
            sendservmsgf("%s proposes %s on map %s (select map to vote)", colorname(ci), modeprettyname(reqmode), map[0] ? map : "[new map]");
            checkvotes();
        }
    }

    bool isally(const clientinfo *a, const clientinfo *b)
    {
        return (m_teammode && sameteam(a->team, b->team)) || (m_role && a->state.role == b->state.role);
    }

    void died(clientinfo *target, clientinfo *actor, int atk, int damage, int flags = 0)
    {
        servstate &ts = target->state;
        ts.deaths++;
        ts.spree = 0;
        int value = (m_berserker && target->state.role == ROLE_BERSERKER) ? 5 : 1,
            fragvalue = smode ? smode->fragvalue(target, actor) : (target==actor || isally(target, actor) ? -1 : value);
        actor->state.frags += fragvalue;
        if(!isally(target, actor))
        {
            actor->state.spree++;
        }
        else actor->state.spree = 0;
        if(fragvalue>0)
        {
            int friends = 0, enemies = 0; // note: friends also includes the fragger
            if(m_teammode) loopv(clients) if(clients[i]->team != actor->team) enemies++; else friends++;
            else { friends = 1; enemies = clients.length()-1; }
            actor->state.effectiveness += fragvalue*friends/float(max(enemies, 1));
        }
        teaminfo *t = m_teammode && validteam(actor->team) ? &teaminfos[actor->team-1] : NULL;
        if(t) t->frags += fragvalue;
        int kflags = 0; // flags = hit flags, kflags = kill flags
        if(!firstblood && target != actor && !isally(target, actor))
        {
            firstblood = true;
            kflags |= KILL_FIRST;
        }
        if (actor->state.spree > 0) switch (actor->state.spree)
        {
            case 5: kflags |= KILL_SPREE; break;
            case 10: kflags |= KILL_SAVAGE; break;
            case 15: kflags |= KILL_UNSTOPPABLE; break;
            case 20:
            case 25:
            case 30:
            case 35:
            case 40:
            case 45:
            case 50:
            {
                kflags |= KILL_LEGENDARY;
                if (actor->state.spree >= 50) actor->state.spree = 0; // Reset the spree.
                break;
            }
        }
        if(flags & Hit_Head) kflags |= KILL_HEADSHOT;
        if(m_berserker)
        {
            checkberserker(target);
            if(target!=actor && (isberserkerdead || target->state.role == ROLE_BERSERKER))
            {
                makeberserker(actor);
                kflags |= KILL_BERSERKER;
            }
            if(m_berserker && !m_vampire(mutators) && actor->state.role == ROLE_BERSERKER)
            {
                actor->state.health = min(actor->state.health + 50, actor->state.maxhealth);
                sendf(-1, 1, "ri3", N_REGENERATE, actor->clientnum, actor->state.health);
            }
        }
        bool hidekillinfo = m_betrayal && actor->state.role == ROLE_TRAITOR; // cover up traitor's kills and display them as suicides in the obituary
        if(hidekillinfo)
        {
            kflags |= KILL_TRAITOR;
            sendf(actor->clientnum, 1, "ri7", N_DIED, target->clientnum, actor->clientnum, actor->state.frags, 0, atk, kflags); // send only to actor
            sendf(-1, 1, "ri7x", N_DIED, target->clientnum, target->clientnum, 0, 0, atk, kflags, actor->clientnum); // send to other players excluding actor
        }
        else sendf(-1, 1, "ri7", N_DIED, target->clientnum, actor->clientnum, actor->state.frags, t ? t->frags : 0, atk, kflags);
        target->position.setsize(0);
        if(smode) smode->died(target, actor);
        ts.state = CS_DEAD;
        ts.lastdeath = gamemillis;
        if(actor!=target && m_teammode && actor->team == target->team)
        {
            actor->state.teamkills++;
            addteamkill(actor, target, 1);
        }
        if(m_dm) addscore(actor, fragvalue);
        ts.deadflush = ts.lastdeath + DEATHMILLIS;
        // don't issue respawn yet until DEATHMILLIS has elapsed
        // ts.respawn();
    }

    void suicide(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(gs.state!=CS_ALIVE) return;
        checkberserker(ci);
        teaminfo *t = NULL;
        if(!gamewaiting && !betweenrounds && !hunterchosen && !interm)
        {
            int fragvalue = smode ? smode->fragvalue(ci, ci) : -1;
            ci->state.frags += fragvalue;
            ci->state.points--;
            sendf(-1, 1, "ri3", N_SCORE, ci->clientnum, ci->state.points);
            ci->state.deaths++;
            if(m_teammode && validteam(ci->team)) t = &teaminfos[ci->team-1];
            if(t) t->frags += fragvalue;
        }
        sendf(-1, 1, "ri7", N_DIED, ci->clientnum, ci->clientnum, gs.frags, t ? t->frags : 0, -1, 0);
        ci->position.setsize(0);
        if(smode) smode->died(ci, NULL);
        gs.state = CS_DEAD;
        gs.lastdeath = gamemillis;
        gs.respawn();
        shouldcheckround();
    }

    void dodamage(clientinfo *target, clientinfo *actor, int damage, int atk, int flags = 0, const vec &hitpush = vec(0, 0, 0), const vec to = vec(0, 0, 0))
    {
        if((target == actor && !selfdamage) || (isally(target, actor) && !teamdamage) || (m_round && betweenrounds)) return;
        servstate &ts = target->state;
        ts.dodamage(damage, flags & Hit_Environment? true : false);
        target->state.lastpain = lastmillis;
        sendf(-1, 1, "rii9i", N_DAMAGE, target->clientnum, actor->clientnum, atk, damage, flags, ts.health, ts.shield, int(to.x*DMF), int(to.y*DMF), int(to.z*DMF));
        if(target!=actor && damage > 0)
        {
            if(!isally(target, actor))
            {
                actor->state.damage += damage;
                if(m_vampire(mutators))
                {
                    actor->state.health = min(actor->state.health + damage / (actor->state.role == ROLE_BERSERKER ? 2 : 1), actor->state.maxhealth);
                    sendf(-1, 1, "ri3", N_REGENERATE, actor->clientnum, actor->state.health);
                }
            }
            else if(!m_teammode && !m_betrayal) dodamage(actor, actor, damage, atk, flags);
        }
        if(target==actor) target->setpushed();
        else if(!hitpush.iszero())
        {
            ivec v(vec(hitpush).rescale(DNF));
            sendf(ts.health<=0 ? -1 : target->ownernum, 1, "ri7", N_HITPUSH, target->clientnum, atk, damage, v.x, v.y, v.z);
            target->setpushed();
        }
        if(ts.health<=0 || atk == ATK_INSTA)
        {
            if(!m_teammode && isally(target, actor)) suicide(actor);
            if(m_infection)
            {
                if(target == actor || target->state.role == ROLE_ZOMBIE) died(target, actor, atk, damage, flags);
                else
                {
                    infect(target, actor);
                    addscore(actor);
                }
            }
            else died(target, actor, atk, damage, flags);
            shouldcheckround();
        }
    }

    void suicideevent::process(clientinfo *ci)
    {
        suicide(ci);
    }

    void instantkill(int& damage, int extradamage, clientinfo* target, clientinfo* actor, int atk, int flags)
    {
        target->state.instantkill(extradamage);
        damage = target->state.health;
        died(target, actor, atk, damage, flags);
    }

    int calculatedamage(int damage, clientinfo *target, clientinfo *actor, int atk, int flags)
    {
        if(target != actor)
        {
            if(target->state.haspowerup(PU_INVULNERABILITY) && !actor->state.haspowerup(PU_INVULNERABILITY))
            {
                return 0;
            }
        }
        if(!(flags & Hit_Environment))
        {
            if (attacks[atk].damage < 0)
            {
                int extradamage = flags & Hit_Head ? attacks[atk].headshotdamage : 0;
                instantkill(damage, extradamage, target, actor, atk, flags);
                return target->state.health;
            }
            if(attacks[atk].headshotdamage)
            {
				// Weapons deal locational damage only if headshot damage is specified.
                if(flags & Hit_Head)
                {
                    if(m_mayhem(mutators)) // force death if it's a blow to the head when the Mayhem mutator is enabled
                    {
                        instantkill(damage, attacks[atk].headshotdamage, target, actor, atk, flags);
                        return damage;
                    }
                    else damage += attacks[atk].headshotdamage;
                }
                if(flags & Hit_Legs) damage /= 2;
            }
            if(actor->state.haspowerup(PU_DAMAGE) || actor->state.role == ROLE_BERSERKER) damage *= 2;
            if((isally(target, actor) || target == actor) && !m_betrayal) damage /= DAMAGE_ALLYDIV;
        }
        if (target->state.haspowerup(PU_ARMOR) || target->state.role == ROLE_BERSERKER) damage /= 2;
        if(!damage) damage = 1;
        return damage;
    }

    void explodeevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if (!gs.projectiles.remove(id))
        {
            return;
        }
        sendf(-1, 1, "ri4x", N_EXPLODEFX, ci->clientnum, atk, id, ci->ownernum);
        loopv(hits)
        {
            hitinfo &h = hits[i];
            clientinfo *target = getinfo(h.target);
            if(!target || target->state.state!=CS_ALIVE || h.lifesequence!=target->state.lifesequence || h.dist<0 || h.dist>attacks[atk].exprad) continue;

            bool dup = false;
            loopj(i) if(hits[j].target==h.target) { dup = true; break; }
            if(dup) continue;
            float radiusdamage = attacks[atk].damage*(1-h.dist/EXP_DISTSCALE/attacks[atk].exprad);
            dodamage(target, ci, calculatedamage(radiusdamage, target, ci, atk, h.flags), atk, 0, h.dir);
        }
    }

    void shotevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        int wait = millis - gs.lastshot;
        if(!gs.isalive(gamemillis) || wait<gs.gunwait || !validatk(atk))
        {
            return;
        }
        int gun = attacks[atk].gun;
        if(attacks[atk].action != ACT_MELEE && !gs.ammo[gun]) return;
        if(attacks[atk].range && from.dist(to) > attacks[atk].range + 1) return;
        if(!gs.haspowerup(PU_AMMO))
        {
            gs.ammo[gun] -= attacks[atk].use;
        }
        gs.lastshot = millis;
        gs.lastmove = lastmillis;
        gs.lastatk = atk;
        int gunwait = attacks[atk].attackdelay;
        if(gs.haspowerup(PU_HASTE) || gs.role == ROLE_BERSERKER)
        {
            gunwait /= 2;
        }
        gs.gunwait = gunwait;
        sendf(-1, 1, "ri3x", N_SHOTEVENT, ci->clientnum, atk, ci->ownernum);
        gs.shotdamage += attacks[atk].damage*attacks[atk].rays;
        bool hit = false;
        if (isweaponprojectile(attacks[atk].projectile))
        {
            gs.projectiles.add(id);
        }
        else
        {
            int totalrays = 0, maxrays = attacks[atk].rays;
            loopv(hits)
            {
                hitinfo &h = hits[i];
                clientinfo *target = getinfo(h.target);
                if(!target || target->state.state!=CS_ALIVE || h.lifesequence!=target->state.lifesequence || h.rays<1 || h.dist > attacks[atk].range + 1) continue;

                totalrays += h.rays;
                if(totalrays>maxrays) continue;
                int raydamage = h.rays*attacks[atk].damage, damage = calculatedamage(raydamage, target, ci, atk, h.flags);
                dodamage(target, ci, damage, atk, h.flags, h.dir, to);
                hit = true;
                sendf(-1, 1, "rii9ix", N_SHOTFX, ci->clientnum, atk, id, hit, int(from.x*DMF), int(from.y*DMF), int(from.z*DMF), int(to.x*DMF), int(to.y*DMF), int(to.z*DMF), ci->ownernum);

            }
        }
        if(hit) return;
        sendf(-1, 1, "rii9ix", N_SHOTFX, ci->clientnum, atk, id, hit, int(from.x*DMF), int(from.y*DMF), int(from.z*DMF), int(to.x*DMF), int(to.y*DMF), int(to.z*DMF), ci->ownernum);
    }

    void pickupevent::process(clientinfo *ci)
    {
        servstate &gs = ci->state;
        if(m_mp(gamemode) && !gs.isalive(gamemillis)) return;
        pickup(ent, ci->clientnum);
    }

    bool gameevent::flush(clientinfo *ci, int fmillis)
    {
        process(ci);
        return true;
    }

    bool timedevent::flush(clientinfo *ci, int fmillis)
    {
        if(millis > fmillis) return false;
        else if(millis >= ci->lastevent)
        {
            ci->lastevent = millis;
            process(ci);
        }
        return true;
    }

    void clearevent(clientinfo *ci)
    {
        delete ci->events.remove(0);
    }

    void flushevents(clientinfo *ci, int millis)
    {
        while(ci->events.length())
        {
            gameevent *ev = ci->events[0];
            if(ev->flush(ci, millis)) clearevent(ci);
            else break;
        }
    }

    VAR(inactivitytime, 60000, 60000, 300000);

    void processevents()
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            flushevents(ci, gamemillis);
            if(ci->state.poweruptype && ci->state.powerupmillis)
            {
                ci->state.powerupmillis = max(ci->state.powerupmillis - curtime, 0);
                if(!ci->state.powerupmillis)
                {
                    ci->state.powerupmillis = 0;
                    ci->state.poweruptype = PU_NONE;
                }
            }
            if(ci->state.state == CS_ALIVE)
            {
                if(!ci->local && !m_edit && lastmillis - ci->state.lastmove >= inactivitytime)
                { // basic inactivity check
                    if(ci->state.aitype == AI_NONE)
                    {
                        extern void forcespectator(clientinfo *ci);
                        forcespectator(ci);
                        sendf(ci->clientnum, 1, "ri2s", N_ANNOUNCE, NULL, "\f0You entered spectator mode due to inactivity");
                    }
                    else if(m_round) suicide(ci);
                }
                if(!(ci->state.role == ROLE_BERSERKER || ci->state.role == ROLE_ZOMBIE) // zombies and berserker are unaffected by this
                   && ci->state.health > ci->state.maxhealth && lastmillis - ci->state.lastregeneration > 1000)
                {
                    ci->state.health = max(ci->state.health - 1, ci->state.maxhealth);
                    sendf(-1, 1, "ri3", N_REGENERATE, ci->clientnum, ci->state.health);
                    ci->state.lastregeneration = lastmillis;
                }
                if((m_berserker && ci->state.role == ROLE_BERSERKER) || m_vampire(mutators))
                {
                    if(lastmillis-ci->state.lastpain > 2800 && lastmillis-ci->state.lastregeneration > 1000)
                    {
                        int subtract = ci->state.role == ROLE_BERSERKER ? 5 : 1;
                        ci->state.health = max(ci->state.health-subtract, 0);
                        sendf(-1, 1, "ri3", N_REGENERATE, ci->clientnum, ci->state.health);
                        if(ci->state.health<=0) suicide(ci);
                        ci->state.lastregeneration = lastmillis;
                    }
                }
                if(ci->damagemat)
                {
                    if(lastmillis-ci->state.lastdamage >= DELAY_ENVIRONMENT_DAMAGE && !ci->state.haspowerup(PU_INVULNERABILITY))
                    {
                        dodamage(ci, ci, calculatedamage(DAMAGE_ENVIRONMENT, ci, ci, -1, Hit_Environment), -1, Hit_Environment);
                        ci->state.lastdamage = lastmillis;
                    }
                }
            }

        }
        serverevents::process();
    }

    void cleartimedevents(clientinfo *ci)
    {
        int keep = 0;
        loopv(ci->events)
        {
            if(ci->events[i]->keepable())
            {
                if(keep < i)
                {
                    for(int j = keep; j < i; j++) delete ci->events[j];
                    ci->events.remove(keep, i - keep);
                    i = keep;
                }
                keep = i+1;
                continue;
            }
        }
        while(ci->events.length() > keep) delete ci->events.pop();
        ci->timesync = false;
    }

    bool remainingminutes(int remaining, int oldgamemillis)
    {
        int milliseconds = remaining * 60000;
        if (milliseconds == gamelimit) return false;
        return (gamemillis >= (gamelimit - milliseconds) && oldgamemillis < (gamelimit - milliseconds));
    }

    void serverupdate()
    {
        if(shouldstep && !gamepaused)
        {
            int oldgamemillis = gamemillis;
            gamemillis += curtime;

            if(gamelimit && m_timed)
            {
                if(remainingminutes(1, oldgamemillis)) // one minute
                {
                    sendf(-1, 1, "ri2s", N_ANNOUNCE, S_ANNOUNCER_1_MINUTE, "\f2One minute remains");
                }
                else if(remainingminutes(5, oldgamemillis)) // five minutes
                {
                    sendf(-1, 1, "ri2s", N_ANNOUNCE, S_ANNOUNCER_5_MINUTES, "\f2Five minutes remain");
                }
            }

            if(m_demo) readdemo();
            else if(!gamelimit || !m_timed || (m_round && !interm) || gamemillis < gamelimit)
            {
                processevents();
                if(curtime)
                {
                    loopv(sents) if(sents[i].spawntime) // spawn entities when timer reached
                    {
                        sents[i].spawntime -= curtime;
                        if(sents[i].spawntime<=0)
                        {
                            sents[i].spawntime = 0;
                            sents[i].spawned = true;
                            sendf(-1, 1, "ri2", N_ITEMSPAWN, i);
                        }
                    }
                }
                aimanager::checkai();
                if(smode) smode->update();
            }

            if(!interm)
            {
                if(m_voosh(mutators))
                {
                    if(gamemillis > vooshtime)
                    {
                        voosh();
                        vooshtime = gamemillis + 20000;
                    }
                }
                if(checkround)
                {
                    checkround = false;
                    checkplayers();
                }
            }
        }

        while(bannedips.length() && bannedips[0].expire-totalmillis <= 0) bannedips.remove(0);
        loopv(connects) if(totalmillis-connects[i]->connectmillis>15000) disconnect_client(connects[i]->clientnum, DISC_TIMEOUT);

        if(nextexceeded && gamemillis > nextexceeded && (!gamelimit || !m_timed || (m_round && !interm) || gamemillis < gamelimit))
        {
            nextexceeded = 0;
            loopvrev(clients)
            {
                clientinfo &c = *clients[i];
                if(c.state.aitype != AI_NONE) continue;
                if(c.checkexceeded()) disconnect_client(c.clientnum, DISC_MSGERR);
                else c.scheduleexceeded();
            }
        }

        if(shouldcheckteamkills) checkteamkills();

        if(shouldstep && !gamepaused)
        {
            if(gamelimit && m_timed && smapname[0] && gamemillis-curtime>0) checkintermission();
            if(interm > 0 && gamemillis>interm)
            {
                if(demorecord) enddemorecord();
                interm = -1;
                checkvotes(true);
            }
        }

        shouldstep = clients.length() > 0;
    }

    void forcespectator(clientinfo *ci)
    {
        if(ci->state.state==CS_ALIVE) suicide(ci);
        if(smode) smode->leavegame(ci);
        ci->state.state = CS_SPECTATOR;
        ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
        if(!ci->local && (!ci->privilege || ci->warned))
        {
            aimanager::removeai(ci);
        }
        sendf(-1, 1, "ri4", N_SPECTATOR, ci->clientnum, 1, ci->ghost);
    }

    struct crcinfo
    {
        int crc, matches;

        crcinfo() {}
        crcinfo(int crc, int matches) : crc(crc), matches(matches) {}

        static bool compare(const crcinfo &x, const crcinfo &y) { return x.matches > y.matches; }
    };

    VAR(modifiedmapspectator, 0, 1, 2);

    void checkmaps(int req = -1)
    {
        if(m_edit || !smapname[0]) return;
        vector<crcinfo> crcs;
        int total = 0, unsent = 0, invalid = 0;
        if(mcrc) crcs.add(crcinfo(mcrc, clients.length() + 1));
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==CS_SPECTATOR || ci->state.aitype != AI_NONE) continue;
            total++;
            if(!ci->clientmap[0])
            {
                if(ci->mapcrc < 0) invalid++;
                else if(!ci->mapcrc) unsent++;
            }
            else
            {
                crcinfo *match = NULL;
                loopvj(crcs) if(crcs[j].crc == ci->mapcrc) { match = &crcs[j]; break; }
                if(!match) crcs.add(crcinfo(ci->mapcrc, 1));
                else match->matches++;
            }
        }
        if(!mcrc && total - unsent < min(total, 4)) return;
        crcs.sort(crcinfo::compare);
        string msg;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==CS_SPECTATOR || ci->state.aitype != AI_NONE || ci->clientmap[0] || ci->mapcrc >= 0 || (req < 0 && ci->warned)) continue;
            formatstring(msg, "%s has modified map \"%s\"", colorname(ci), smapname);
            sendf(req, 1, "ris", N_SERVMSG, msg);
            if(req < 0) ci->warned = true;
        }
        if(crcs.length() >= 2) loopv(crcs)
        {
            crcinfo &info = crcs[i];
            if(i || info.matches <= crcs[i+1].matches) loopvj(clients)
            {
                clientinfo *ci = clients[j];
                if(ci->state.state==CS_SPECTATOR || ci->state.aitype != AI_NONE || !ci->clientmap[0] || ci->mapcrc != info.crc || (req < 0 && ci->warned)) continue;
                formatstring(msg, "%s has modified map \"%s\"", colorname(ci), smapname);
                sendf(req, 1, "ris", N_SERVMSG, msg);
                if(req < 0) ci->warned = true;
            }
        }
        if(req < 0 && modifiedmapspectator && (mcrc || modifiedmapspectator > 1)) loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(!ci->local && ci->warned && ci->state.state != CS_SPECTATOR) forcespectator(ci);
        }
    }

    bool shouldspectate(clientinfo *ci)
    {
        return !ci->local && ci->warned && modifiedmapspectator && (mcrc || modifiedmapspectator > 1);
    }

    void unspectate(clientinfo *ci)
    {
        if(shouldspectate(ci)) return;
        ci->state.state = CS_DEAD;
        ci->state.respawn();
        ci->state.lasttimeplayed = lastmillis;
        aimanager::addclient(ci);
        sendf(-1, 1, "ri4", N_SPECTATOR, ci->clientnum, 0, ci->ghost);
        if(ci->clientmap[0] || ci->mapcrc) checkmaps();
        if(!hasmap(ci)) rotatemap(true);
        checkplayers();
    }

    void sendservinfo(clientinfo *ci)
    {
        sendf(ci->clientnum, 1, "ri5ss", N_SERVINFO, ci->clientnum, PROTOCOL_VERSION, ci->sessionid, serverpass[0] ? 1 : 0, servername, serverauth);
    }

    void noclients()
    {
        bannedips.shrink(0);
        aimanager::clearai();
    }

    void localconnect(int n)
    {
        clientinfo *ci = getinfo(n);
        ci->clientnum = ci->ownernum = n;
        ci->connectmillis = totalmillis;
        ci->sessionid = (rnd(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;
        ci->local = true;

        connects.add(ci);
        sendservinfo(ci);
    }

    void localdisconnect(int n)
    {
        if(m_demo) enddemoplayback();
        clientdisconnect(n);
    }

    int clientconnect(int n, uint ip)
    {
        clientinfo *ci = getinfo(n);
        ci->clientnum = ci->ownernum = n;
        ci->connectmillis = totalmillis;
        ci->sessionid = (rnd(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;

        connects.add(ci);
        if(!m_mp(gamemode)) return DISC_LOCAL;
        sendservinfo(ci);
        return DISC_NONE;
    }

    void clientdisconnect(int n)
    {
        clientinfo *ci = getinfo(n);
        loopv(clients) if(clients[i]->authkickvictim == ci->clientnum) clients[i]->cleanauth();
        if(ci->connected)
        {
            if(ci->privilege) setmaster(ci, false);
            if(smode) smode->leavegame(ci, true);
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
            savescore(ci);
            checkberserker(ci);
            sendf(-1, 1, "ri2", N_CDIS, n);
            clients.removeobj(ci);
            aimanager::removeai(ci);
            if(!numclients(-1, false, true)) noclients(); // bans clear when server empties
            if(ci->local) checkpausegame();
            shouldcheckround();
        }
        else connects.removeobj(ci);
    }

    int reserveclients() { return 3; }

    extern void verifybans();

    struct banlist
    {
        vector<ipmask> bans;

        void clear() { bans.shrink(0); }

        bool check(uint ip)
        {
            loopv(bans) if(bans[i].check(ip)) return true;
            return false;
        }

        void add(const char *ipname)
        {
            ipmask ban;
            ban.parse(ipname);
            bans.add(ban);

            verifybans();
        }
    } ipbans, gbans;

    bool checkbans(uint ip)
    {
        loopv(bannedips) if(bannedips[i].ip==ip) return true;
        return ipbans.check(ip) || gbans.check(ip);
    }

    void verifybans()
    {
        loopvrev(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.aitype != AI_NONE || ci->local || ci->privilege >= PRIV_ADMIN) continue;
            if(checkbans(getclientip(ci->clientnum))) disconnect_client(ci->clientnum, DISC_IPBAN);
        }
    }

    ICOMMAND(clearipbans, "", (), ipbans.clear());
    ICOMMAND(ipban, "s", (const char *ipname), ipbans.add(ipname));

    int allowconnect(clientinfo *ci, const char *pwd = "")
    {
        if(ci->local) return DISC_NONE;
        if(!m_mp(gamemode)) return DISC_LOCAL;
        if(serverpass[0])
        {
            if(!checkpassword(ci, serverpass, pwd)) return DISC_PASSWORD;
            return DISC_NONE;
        }
        if(adminpass[0] && checkpassword(ci, adminpass, pwd)) return DISC_NONE;
        if(numclients(-1, false, true)>=maxclients) return DISC_MAXCLIENTS;
        uint ip = getclientip(ci->clientnum);
        if(checkbans(ip)) return DISC_IPBAN;
        if(mastermode>=MM_PRIVATE && allowedips.find(ip)<0) return DISC_PRIVATE;
        return DISC_NONE;
    }

    bool allowbroadcast(int n)
    {
        clientinfo *ci = getinfo(n);
        return ci && ci->connected;
    }

    clientinfo *findauth(uint id)
    {
        loopv(clients) if(clients[i]->authreq == id) return clients[i];
        return NULL;
    }

    void authfailed(clientinfo *ci)
    {
        if(!ci) return;
        ci->cleanauth();
        if(ci->connectauth) disconnect_client(ci->clientnum, ci->connectauth);
    }

    void authfailed(uint id)
    {
        authfailed(findauth(id));
    }

    void authsucceeded(uint id)
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        ci->cleanauth(ci->connectauth!=0);
        if(ci->connectauth) connected(ci);
        if(ci->authkickvictim >= 0)
        {
            if(setmaster(ci, true, "", ci->authname, NULL, PRIV_AUTH, false, true))
                trykick(ci, ci->authkickvictim, ci->authkickreason, ci->authname, NULL, PRIV_AUTH);
            ci->cleanauthkick();
        }
        else setmaster(ci, true, "", ci->authname, NULL, PRIV_AUTH);
    }

    void authchallenged(uint id, const char *val, const char *desc = "")
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        sendf(ci->clientnum, 1, "risis", N_AUTHCHAL, desc, id, val);
    }

    uint nextauthreq = 0;

    bool tryauth(clientinfo *ci, const char *user, const char *desc)
    {
        ci->cleanauth();
        if(!nextauthreq) nextauthreq = 1;
        ci->authreq = nextauthreq++;
        filtertext(ci->authname, user, T_NONE, 100);
        copystring(ci->authdesc, desc);
        if(ci->authdesc[0])
        {
            userinfo *u = users.access(userkey(ci->authname, ci->authdesc));
            if(u)
            {
                uint seed[3] = { ::hthash(serverauth) + detrnd(size_t(ci) + size_t(user) + size_t(desc), 0x10000), uint(totalmillis), randomMT() };
                vector<char> buf;
                ci->authchallenge = genchallenge(u->pubkey, seed, sizeof(seed), buf);
                sendf(ci->clientnum, 1, "risis", N_AUTHCHAL, desc, ci->authreq, buf.getbuf());
            }
            else ci->cleanauth();
        }
        else if(!requestmasterf("reqauth %u %s\n", ci->authreq, ci->authname))
        {
            ci->cleanauth();
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "not connected to authentication server");
        }
        if(ci->authreq) return true;
        if(ci->connectauth) disconnect_client(ci->clientnum, ci->connectauth);
        return false;
    }

    bool answerchallenge(clientinfo *ci, uint id, char *val, const char *desc)
    {
        if(ci->authreq != id || strcmp(ci->authdesc, desc))
        {
            ci->cleanauth();
            return !ci->connectauth;
        }
        for(char *s = val; *s; s++)
        {
            if(!isxdigit(*s)) { *s = '\0'; break; }
        }
        if(desc[0])
        {
            if(ci->authchallenge && checkchallenge(val, ci->authchallenge))
            {
                userinfo *u = users.access(userkey(ci->authname, ci->authdesc));
                if(u)
                {
                    if(ci->connectauth) connected(ci);
                    if(ci->authkickvictim >= 0)
                    {
                        if(setmaster(ci, true, "", ci->authname, ci->authdesc, u->privilege, false, true))
                            trykick(ci, ci->authkickvictim, ci->authkickreason, ci->authname, ci->authdesc, u->privilege);
                    }
                    else setmaster(ci, true, "", ci->authname, ci->authdesc, u->privilege);
                }
            }
            ci->cleanauth();
        }
        else if(!requestmasterf("confauth %u %s\n", id, val))
        {
            ci->cleanauth();
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "not connected to authentication server");
        }
        return ci->authreq || !ci->connectauth;
    }

    void masterconnected()
    {
    }

    void masterdisconnected()
    {
        loopvrev(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->authreq) authfailed(ci);
        }
    }

    void processmasterinput(const char *cmd, int cmdlen, const char *args)
    {
        uint id;
        string val;
        if(sscanf(cmd, "failauth %u", &id) == 1)
            authfailed(id);
        else if(sscanf(cmd, "succauth %u", &id) == 1)
            authsucceeded(id);
        else if(sscanf(cmd, "chalauth %u %255s", &id, val) == 2)
            authchallenged(id, val);
        else if(matchstring(cmd, cmdlen, "cleargbans"))
            gbans.clear();
        else if(sscanf(cmd, "addgban %100s", val) == 1)
            gbans.add(val);
    }

    void receivefile(int sender, uchar *data, int len)
    {
        if(!m_edit || len <= 0 || len > 4*1024*1024) return;
        clientinfo *ci = getinfo(sender);
        if(ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) return;
        if(mapdata) DELETEP(mapdata);
        mapdata = opentempfile("mapdata", "w+b");
        if(!mapdata) { sendf(sender, 1, "ris", N_SERVMSG, "failed to open temporary file for map"); return; }
        mapdata->write(data, len);
        sendservmsgf("[%s sent a map to server, \"/getmap\" to receive it]", colorname(ci));
    }

    void sendclipboard(clientinfo *ci)
    {
        if(!ci->lastclipboard || !ci->clipboard) return;
        bool flushed = false;
        loopv(clients)
        {
            clientinfo &e = *clients[i];
            if(e.clientnum != ci->clientnum && e.needclipboard - ci->lastclipboard >= 0)
            {
                if (!flushed)
                { 
                    flushserver(true);
                    flushed = true;
                }
                sendpacket(e.clientnum, 1, ci->clipboard);
            }
        }
    }

    void connected(clientinfo *ci)
    {
        if(m_demo) enddemoplayback();

        if(!hasmap(ci)) rotatemap(false);

        shouldstep = true;

        connects.removeobj(ci);
        clients.add(ci);

        ci->connectauth = 0;
        ci->connected = true;
        ci->needclipboard = totalmillis ? totalmillis : 1;
        if(mastermode>=MM_LOCKED) ci->state.state = CS_SPECTATOR;
        ci->state.lasttimeplayed = lastmillis;

        ci->team = m_teammode ? chooseworstteam(ci) : 0;

        geoip_lookup_ip(getclientip(ci->clientnum), ci->country_code, ci->country_name);
        geoip_set_custom_flag(ci->preferred_flag, ci->country_code, ci->country_name, ci->customflag_code, ci->customflag_name);

        sendwelcome(ci);
        if(restorescore(ci)) sendresume(ci);
        sendinitclient(ci);

        aimanager::addclient(ci);

        if(m_demo) setupdemoplayback();

        if(servermessage[0]) sendf(ci->clientnum, 1, "ris", N_SERVMSG, servermessage);

        shouldcheckround();
    }

    VAR(spectatorchat, 0, 0, 1);

    bool canseemessage(clientinfo *a, clientinfo *b)
    {
        if(spectatorchat && a->state.state == CS_SPECTATOR && b->state.state != CS_SPECTATOR) return false;
        if(m_round && a->state.state == CS_DEAD && b->state.state != CS_DEAD) return false;
        return true;
    }

    void parsepacket(int sender, int chan, packetbuf &p)     // has to parse exactly each byte of the packet
    {
        if(sender<0 || p.packet->flags&ENET_PACKET_FLAG_UNSEQUENCED || chan > 2) return;
        char text[MAXTRANS];
        int type;
        clientinfo *ci = sender>=0 ? getinfo(sender) : NULL, *cq = ci, *cm = ci;
        if(ci && !ci->connected)
        {
            if(chan==0) return;
            else if(chan!=1) { disconnect_client(sender, DISC_MSGERR); return; }
            else while(p.length() < p.maxlen) switch(checktype(getint(p), ci))
            {
                case N_CONNECT:
                {
                    getstring(text, p);
                    filtertext(text, text, T_WHITESPACE | T_NAME, MAXNAMELEN, MAXNAMEUNILEN);
                    if(!text[0]) copystring(text, "player");
                    copystring(ci->name, text, MAXNAMELEN+1);
                    ci->playermodel = getint(p);
                    ci->playercolor = getint(p);
                    getstring(text, p);
                    filtertext(ci->preferred_flag, text, T_NONE, MAXCOUNTRYCODELEN);

                    string password, authdesc, authname;
                    getstring(password, p, sizeof(password));
                    getstring(authdesc, p, sizeof(authdesc));
                    getstring(authname, p, sizeof(authname));
                    int disc = allowconnect(ci, password);
                    if(disc)
                    {
                        if(disc == DISC_LOCAL || !serverauth[0] || strcmp(serverauth, authdesc) || !tryauth(ci, authname, authdesc))
                        {
                            disconnect_client(sender, disc);
                            return;
                        }
                        ci->connectauth = disc;
                    }
                    else connected(ci);
                    break;
                }

                case N_AUTHANS:
                {
                    string desc, ans;
                    getstring(desc, p, sizeof(desc));
                    uint id = (uint)getint(p);
                    getstring(ans, p, sizeof(ans));
                    if(!answerchallenge(ci, id, ans, desc))
                    {
                        disconnect_client(sender, ci->connectauth);
                        return;
                    }
                    break;
                }

                case N_PING:
                    getint(p);
                    break;

                default:
                    disconnect_client(sender, DISC_MSGERR);
                    return;
            }
            return;
        }
        else if(chan==2)
        {
            receivefile(sender, p.buf, p.maxlen);
            return;
        }

        if(p.packet->flags&ENET_PACKET_FLAG_RELIABLE) reliablemessages = true;
        #define QUEUE_AI clientinfo *cm = cq;
        #define QUEUE_MSG { if(cm && (!cm->local || demorecord || hasnonlocalclients())) while(curmsg<p.length()) cm->messages.add(p.buf[curmsg++]); }
        #define QUEUE_BUF(body) { \
            if(cm && (!cm->local || demorecord || hasnonlocalclients())) \
            { \
                curmsg = p.length(); \
                { body; } \
            } \
        }
        #define QUEUE_INT(n) QUEUE_BUF(putint(cm->messages, n))
        #define QUEUE_UINT(n) QUEUE_BUF(putuint(cm->messages, n))
        #define QUEUE_STR(text) QUEUE_BUF(sendstring(text, cm->messages))
        int curmsg;
        while((curmsg = p.length()) < p.maxlen) switch(type = checktype(getint(p), ci))
        {
            case N_POS:
            {
                int pcn = getuint(p);
                p.get();
                uint flags = getuint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                vec pos;
                loopk(3)
                {
                    int n = p.get(); n |= p.get()<<8; if(flags&(1<<k)) { n |= p.get()<<16; if(n&0x800000) n |= ~0U<<24; }
                    pos[k] = n/DMF;
                }
                loopk(3) p.get();
                int mag = p.get(); if(flags&(1<<3)) mag |= p.get()<<8;
                int dir = p.get(); dir |= p.get()<<8;
                vec vel = vec((dir%360)*RAD, (clamp(dir/360, 0, 180)-90)*RAD).mul(mag/DVELF);
                if(flags&(1<<4))
                {
                    p.get(); if(flags&(1<<5)) p.get();
                    if(flags&(1<<6)) loopk(2) p.get();
                }
                if(cp)
                {
                    if((!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                    {
                        if(!ci->local && !m_edit && max(vel.magnitude2(), (float)fabs(vel.z)) >= 210)
                            cp->setexceeded();
                        cp->position.setsize(0);
                        while(curmsg<p.length()) cp->position.add(p.buf[curmsg++]);
                    }
                    cp->state.oldpos = cp->state.o;
                    cp->state.o = pos;
                    if(cp->state.oldpos != cp->state.o) cp->state.lastmove = lastmillis;
                    cp->damagemat = (flags&0x80)!=0;
                }
                break;
            }

            case N_TELEPORT:
            {
                int pcn = getint(p), teleport = getint(p), teledest = getint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                if(cp && (!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                {
                    flushclientposition(*cp);
                    sendf(-1, 0, "ri4x", N_TELEPORT, pcn, teleport, teledest, cp->ownernum);
                }
                break;
            }

            case N_JUMPPAD:
            {
                int pcn = getint(p), jumppad = getint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                if(cp && (!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                {
                    cp->setpushed();
                    flushclientposition(*cp);
                    sendf(-1, 0, "ri3x", N_JUMPPAD, pcn, jumppad, cp->ownernum);
                }
                break;
            }

            case N_FROMAI:
            {
                int qcn = getint(p);
                if(qcn < 0) cq = ci;
                else
                {
                    cq = getinfo(qcn);
                    if(cq && qcn != sender && cq->ownernum != sender) cq = NULL;
                }
                break;
            }

            case N_EDITMODE:
            {
                int val = getint(p);
                if(!ci->local && !m_edit) break;
                if(val ? ci->state.state!=CS_ALIVE && ci->state.state!=CS_DEAD : ci->state.state!=CS_EDITING) break;
                if(smode)
                {
                    if(val)
                    {
                        smode->leavegame(ci);
                    }
                    else
                    {
                        smode->entergame(ci);
                    }

                }
                if(val)
                {
                    ci->state.editstate = ci->state.state;
                    ci->state.state = CS_EDITING;
                    ci->events.deletecontents();
                    ci->state.projectiles.reset();
                }
                else ci->state.state = ci->state.editstate;
                QUEUE_MSG;
                break;
            }

            case N_MAPCRC:
            {
                getstring(text, p);
                int crc = getint(p);
                if(!ci) break;
                if(strcmp(text, smapname))
                {
                    if(ci->clientmap[0])
                    {
                        ci->clientmap[0] = '\0';
                        ci->mapcrc = 0;
                    }
                    else if(ci->mapcrc > 0) ci->mapcrc = 0;
                    break;
                }
                copystring(ci->clientmap, text);
                ci->mapcrc = text[0] ? crc : 1;
                checkmaps();
                if(cq && cq != ci && cq->ownernum != ci->clientnum) cq = NULL;
                break;
            }

            case N_CHECKMAPS:
                checkmaps(sender);
                break;

            case N_TRYSPAWN:
                if(!ci || !cq || cq->state.state!=CS_DEAD || cq->state.lastspawn>=0 || (!m_edit && cq->state.lastdeath && gamemillis+curtime-cq->state.lastdeath <= DELAY_RESPAWN)) break;
                if((smode && !smode->canspawn(cq)) || (((m_hunt && hunterchosen) || (m_round && !m_infection && !m_betrayal)) && numclients(-1, true, false) > 1))
                {
                    if(cq->state.aitype==AI_NONE)
                    {
                        cq->ghost = true;
                        forcespectator(cq);
                    }
                    break;
                }
                else if(cq->state.aitype==AI_NONE)
                {
                    cq->ghost = false;
                    unspectate(cq);
                }
                if(!ci->clientmap[0] && !ci->mapcrc)
                {
                    ci->mapcrc = -1;
                    checkmaps();
                    if(ci == cq) { if(ci->state.state != CS_DEAD) break; }
                    else if(cq->ownernum != ci->clientnum) { cq = NULL; break; }
                }
                if(cq->state.deadflush)
                {
                    flushevents(cq, cq->state.deadflush);
                    cq->state.respawn();
                }
                cleartimedevents(cq);
                sendspawn(cq);
                break;

            case N_GUNSELECT:
            {
                int gunselect = getint(p);
                if(!cq || cq->state.state!=CS_ALIVE || !validgun(gunselect)) break;
                cq->state.gunselect = gunselect;
                cq->state.lastmove = lastmillis;
                QUEUE_AI;
                QUEUE_MSG;
                break;
            }

            case N_SPAWN:
            {
                int ls = getint(p), gunselect = getint(p);
                if(!cq || (cq->state.state!=CS_ALIVE && cq->state.state!=CS_DEAD && cq->state.state!=CS_EDITING) ||
                   ls!=cq->state.lifesequence || cq->state.lastspawn<0 || !validgun(gunselect)) break;
                cq->state.lastspawn = -1;
                cq->state.state = CS_ALIVE;
                cq->state.gunselect = gunselect;
                cq->exceeded = 0;
                if(smode) smode->spawned(cq);
                QUEUE_AI;
                QUEUE_BUF({
                    putint(cm->messages, N_SPAWN);
                    sendstate(cq->state, cm->messages);
                });
                break;
            }

            case N_SUICIDE:
            {
                if(cq) cq->addevent(new suicideevent);
                break;
            }

            case N_SHOOT:
            {
                shotevent *shot = new shotevent;
                shot->id = getint(p);
                shot->millis = cq ? cq->geteventmillis(gamemillis, shot->id) : 0;
                shot->atk = getint(p);
                loopk(3) shot->from[k] = getint(p)/DMF;
                loopk(3) shot->to[k] = getint(p)/DMF;
                int hits = getint(p);
                loopk(hits)
                {
                    if(p.overread()) break;
                    hitinfo &hit = shot->hits.add();
                    hit.target = getint(p);
                    hit.lifesequence = getint(p);
                    hit.dist = getint(p)/DMF;
                    hit.rays = getint(p);
                    hit.flags = getint(p);
                    loopk(3) hit.dir[k] = getint(p)/DNF;
                }
                if(cq)
                {
                    cq->addevent(shot);
                    cq->setpushed();
                }
                else delete shot;
                break;
            }

            case N_EXPLODE:
            {
                explodeevent *exp = new explodeevent;
                int cmillis = getint(p);
                exp->millis = cq ? cq->geteventmillis(gamemillis, cmillis) : 0;
                exp->atk = getint(p);
                exp->id = getint(p);
                int hits = getint(p);
                loopk(hits)
                {
                    if(p.overread()) break;
                    hitinfo &hit = exp->hits.add();
                    hit.target = getint(p);
                    hit.lifesequence = getint(p);
                    hit.dist = getint(p)/DMF;
                    hit.rays = getint(p);
                    hit.flags = getint(p);
                    loopk(3) hit.dir[k] = getint(p)/DNF;
                }
                if(cq) cq->addevent(exp);
                else delete exp;
                break;
            }

            case N_ITEMPICKUP:
            {
                int n = getint(p);
                if(!cq) break;
                pickupevent *pickup = new pickupevent;
                pickup->ent = n;
                cq->addevent(pickup);
                break;
            }

            case N_TEXT:
            {
                getstring(text, p);
                if(cq->mute) break;
                filtertext(text, text, T_WHITESPACE | T_FORCESPACE);
                loopv(clients)
                {
                    clientinfo *c = clients[i];
                    if(c == cq || c->state.aitype != AI_NONE || !canseemessage(cq, c)) continue;
                    sendf(c->clientnum, 1, "riis", N_TEXT, cq->clientnum, text);
                }
                bool ghost = cq->state.state==CS_SPECTATOR || (m_round && (cq->ghost || cq->state.state==CS_DEAD));
                if(isdedicatedserver() && cq) logoutf("%s%s %s", colorname(cq), ghost ? " <spectator>:" : ":", text);
                break;
            }

            case N_SAYTEAM:
            {
                getstring(text, p);
                int sound = getint(p);
                if(!ci || !cq || cq->mute || !m_teammode || !validteam(cq->team) || cq->state.state==CS_SPECTATOR || (m_round && cq->state.state==CS_DEAD)) break;
                filtertext(text, text, T_WHITESPACE | T_FORCESPACE);
                loopv(clients)
                {
                    clientinfo *t = clients[i];
                    if(t == cq || t->state.aitype != AI_NONE || cq->team != t->team) continue;
                    sendf(t->clientnum, 1, "riisi", N_SAYTEAM, cq->clientnum, text, sound);
                }
                if(isdedicatedserver() && cq) logoutf("%s <%s>: %s", colorname(cq), teamnames[cq->team], text);
                break;
            }

            case N_WHISPER:
            {
                int rcn = getint(p);
                getstring(text, p);
                if(!cq || cq->mute) break;
                filtertext(text, text, T_WHITESPACE);
                clientinfo *recipient = NULL;
                loopv(clients)
                {
                    clientinfo *r = clients[i];
                    if(r == cq || r->state.aitype != AI_NONE || r->clientnum != rcn) continue;
                    recipient = r;
                    break;
                }
                if(!recipient) break;
                sendf(recipient->clientnum, 1, "riis", N_WHISPER, cq->clientnum, text);
                if(isdedicatedserver() && cq) logoutf("%s <whisper to %s>: %s", colorname(cq), colorname(recipient), text);
                break;
            }

            case N_SWITCHNAME:
            {
                QUEUE_MSG;
                getstring(text, p);
                filtertext(ci->name, text, T_WHITESPACE | T_NAME, MAXNAMELEN, MAXNAMEUNILEN);
                if (!ci->name[0])
                {
                    copystring(ci->name, "player");
                }
                QUEUE_STR(ci->name);
                break;
            }

            case N_SWITCHMODEL:
            {
                ci->playermodel = getint(p);
                QUEUE_MSG;
                break;
            }

            case N_SWITCHCOLOR:
            {
                ci->playercolor = getint(p);
                QUEUE_MSG;
                break;
            }

            case N_SWITCHTEAM:
            {
                int team = getint(p);
                if(m_teammode && validteam(team) && ci->team != team && (!smode || smode->canchangeteam(ci, ci->team, team)))
                {
                    if(ci->state.state==CS_ALIVE) suicide(ci);
                    ci->team = team;
                    aimanager::changeteam(ci);
                    sendf(-1, 1, "riiii", N_SETTEAM, sender, ci->team, ci->state.state==CS_SPECTATOR ? -1 : 0);
                }
                break;
            }

            case N_MAPVOTE:
            {
                getstring(text, p);
                filtertext(text, text, T_WHITESPACE);
                fixmapname(text);
                int reqmode = getint(p), reqmuts = getint(p);
                vote(text, reqmode, reqmuts, sender);
                break;
            }

            case N_ITEMLIST:
            {
                if((ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) || !notgotitems || strcmp(ci->clientmap, smapname)) { while(getint(p)>=0 && !p.overread()) getint(p); break; }
                int n;
                while((n = getint(p))>=0 && n<MAXENTS && !p.overread())
                {
                    server_entity se = { NOTUSED, 0, false };
                    while(sents.length()<=n) sents.add(se);
                    sents[n].type = getint(p);
                    if(canspawnitem(sents[n].type))
                    {
                        if(m_mp(gamemode) && delayspawn(sents[n].type)) sents[n].spawntime = spawntime(sents[n].type);
                        else sents[n].spawned = true;
                    }
                }
                notgotitems = false;
                break;
            }

            case N_EDITENT:
            {
                int i = getint(p);
                loopk(3) getint(p);
                int type = getint(p);
                loopk(5) getint(p);
                if(!ci || ci->state.state==CS_SPECTATOR) break;
                QUEUE_MSG;
                bool canspawn = canspawnitem(type);
                if(i<MAXENTS && (sents.inrange(i) || canspawnitem(type)))
                {
                    server_entity se = { NOTUSED, 0, false };
                    while(sents.length()<=i) sents.add(se);
                    sents[i].type = type;
                    if(canspawn ? !sents[i].spawned : (sents[i].spawned || sents[i].spawntime))
                    {
                        sents[i].spawntime = canspawn ? 1 : 0;
                        sents[i].spawned = false;
                    }
                }
                break;
            }

            case N_EDITVAR:
            {
                int type = getint(p);
                getstring(text, p);
                switch(type)
                {
                    case ID_VAR: getint(p); break;
                    case ID_FVAR: getfloat(p); break;
                    case ID_SVAR: getstring(text, p);
                }
                if(ci && ci->state.state!=CS_SPECTATOR) QUEUE_MSG;
                break;
            }

            case N_PING:
                sendf(sender, 1, "i2", N_PONG, getint(p));
                break;

            case N_CLIENTPING:
            {
                int ping = getint(p);
                if(ci)
                {
                    ci->ping = ping;
                    loopv(ci->bots) ci->bots[i]->ping = ping;
                }
                QUEUE_MSG;
                break;
            }

            case N_MASTERMODE:
            {
                int mm = getint(p);
                if((ci->privilege || ci->local) && mm>=MM_OPEN && mm<=MM_PRIVATE)
                {
                    if((ci->privilege>=PRIV_ADMIN || ci->local) || (mastermask&(1<<mm)))
                    {
                        mastermode = mm;
                        allowedips.shrink(0);
                        if(mm>=MM_PRIVATE)
                        {
                            loopv(clients) allowedips.add(getclientip(clients[i]->clientnum));
                        }
                        sendf(-1, 1, "rii", N_MASTERMODE, mastermode);
                        //sendservmsgf("mastermode is now %s (%d)", mastermodename(mastermode), mastermode);
                    }
                    else
                    {
                        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("mastermode %d is disabled on this server", mm));
                    }
                }
                break;
            }

            case N_CLEARBANS:
            {
                if(ci->privilege || ci->local)
                {
                    bannedips.shrink(0);
                    sendservmsg("cleared all bans");
                }
                break;
            }

            case N_KICK:
            {
                int victim = getint(p);
                getstring(text, p);
                filtertext(text, text, T_NEWLINES | T_WHITESPACE);
                trykick(ci, victim, text);
                break;
            }

            case N_MUTE:
            {
                int victim = getint(p), val = getint(p);
                getstring(text, p);
                filtertext(text, text, T_NEWLINES | T_WHITESPACE);
                trymute(ci, victim, val, text);
                break;
            }

            case N_SPECTATOR:
            {
                int spectator = getint(p), val = getint(p), waiting = getint(p);
                if(!ci->privilege && !ci->local && (spectator!=sender || (ci->state.state==CS_SPECTATOR && mastermode>=MM_LOCKED))) break;
                clientinfo *spinfo = (clientinfo *)getclientinfo(spectator); // no bots
                if(!spinfo || !spinfo->connected || (spinfo->state.state==CS_SPECTATOR ? val : !val)) break;
                spinfo->ghost = waiting;
                if(spinfo->state.state!=CS_SPECTATOR && val)
                    forcespectator(spinfo);
                else if(spinfo->state.state==CS_SPECTATOR && !val)
                {
                    unspectate(spinfo);
                }
                if(cq && cq != ci && cq->ownernum != ci->clientnum) cq = NULL;
                break;
            }

            case N_SETTEAM:
            {
                int who = getint(p), team = getint(p);
                if(!ci->privilege && !ci->local) break;
                clientinfo *wi = getinfo(who);
                if(!m_teammode || !validteam(team) || !wi || !wi->connected || wi->team == team) break;
                if(!smode || smode->canchangeteam(wi, wi->team, team))
                {
                    if(wi->state.state==CS_ALIVE) suicide(wi);
                    wi->team = team;
                }
                aimanager::changeteam(wi);
                sendf(-1, 1, "riiii", N_SETTEAM, who, wi->team, 1);
                break;
            }

            case N_FORCEINTERMISSION:
                if(ci->local && !hasnonlocalclients()) startintermission();
                break;

            case N_RECORDDEMO:
            {
                int val = getint(p);
                if(ci->privilege < (restrictdemos ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                if(!maxdemos || !maxdemosize)
                {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "the server has disabled demo recording");
                    break;
                }
                demonextmatch = val!=0;
                sendservmsgf("demo recording is %s for next match", demonextmatch ? "enabled" : "disabled");
                break;
            }

            case N_STOPDEMO:
            {
                if(ci->privilege < (restrictdemos ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                stopdemo();
                break;
            }

            case N_CLEARDEMOS:
            {
                int demo = getint(p);
                if(ci->privilege < (restrictdemos ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                cleardemos(demo);
                break;
            }

            case N_LISTDEMOS:
                if(!ci->privilege && !ci->local && ci->state.state==CS_SPECTATOR) break;
                listdemos(sender);
                break;

            case N_GETDEMO:
            {
                int n = getint(p), tag = getint(p);
                if(!ci->privilege && !ci->local && ci->state.state==CS_SPECTATOR) break;
                senddemo(ci, n, tag);
                break;
            }

            case N_GETMAP:
                if(!mapdata) sendf(sender, 1, "ris", N_SERVMSG, "no map to send");
                else if(ci->getmap) sendf(sender, 1, "ris", N_SERVMSG, "already sending map");
                else
                {
                    sendservmsgf("[%s is getting the map]", colorname(ci));
                    if((ci->getmap = sendfile(sender, 2, mapdata, "ri", N_SENDMAP)))
                        ci->getmap->freeCallback = freegetmap;
                    ci->needclipboard = totalmillis ? totalmillis : 1;
                }
                break;

            case N_NEWMAP:
            {
                int size = getint(p);
                if(!ci->privilege && !ci->local && ci->state.state==CS_SPECTATOR) break;
                if(size>=0)
                {
                    smapname[0] = '\0';
                    resetitems();
                    notgotitems = false;
                    if(smode) smode->newmap();
                }
                QUEUE_MSG;
                break;
            }

            case N_SETMASTER:
            {
                int mn = getint(p), val = getint(p);
                getstring(text, p);
                if(mn != ci->clientnum)
                {
                    if(!ci->privilege && !ci->local) break;
                    clientinfo *minfo = (clientinfo *)getclientinfo(mn);
                    if(!minfo || !minfo->connected || (!ci->local && minfo->privilege >= ci->privilege) || (val && minfo->privilege)) break;
                    setmaster(minfo, val!=0, "", NULL, NULL, PRIV_MASTER, true);
                }
                else setmaster(ci, val!=0, text);
                // don't broadcast the master password
                break;
            }

            case N_ADDBOT:
            {
                aimanager::reqadd(ci, getint(p));
                break;
            }

            case N_DELBOT:
            {
                aimanager::reqdel(ci);
                break;
            }

            case N_BOTLIMIT:
            {
                int limit = getint(p);
                if(ci) aimanager::setbotlimit(ci, limit);
                break;
            }

            case N_BOTBALANCE:
            {
                int balance = getint(p);
                if(ci) aimanager::setbotbalance(ci, balance!=0);
                break;
            }

            case N_AUTHTRY:
            {
                string desc, name;
                getstring(desc, p, sizeof(desc));
                getstring(name, p, sizeof(name));
                tryauth(ci, name, desc);
                break;
            }

            case N_AUTHKICK:
            {
                string desc, name;
                getstring(desc, p, sizeof(desc));
                getstring(name, p, sizeof(name));
                int victim = getint(p);
                getstring(text, p);
                filtertext(text, text, T_NEWLINES | T_WHITESPACE);
                int authpriv = PRIV_AUTH;
                if(desc[0])
                {
                    userinfo *u = users.access(userkey(name, desc));
                    if(u) authpriv = u->privilege; else break;
                }
                if(ci->local || ci->privilege >= authpriv) trykick(ci, victim, text);
                else if(trykick(ci, victim, text, name, desc, authpriv, true) && tryauth(ci, name, desc))
                {
                    ci->authkickvictim = victim;
                    ci->authkickreason = newstring(text);
                }
                break;
            }

            case N_AUTHANS:
            {
                string desc, ans;
                getstring(desc, p, sizeof(desc));
                uint id = (uint)getint(p);
                getstring(ans, p, sizeof(ans));
                answerchallenge(ci, id, ans, desc);
                break;
            }

            case N_PAUSEGAME:
            {
                int val = getint(p);
                if(ci->privilege < (restrictpausegame ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                pausegame(val > 0, ci);
                break;
            }

            case N_GAMESPEED:
            {
                int val = getint(p);
                if(ci->privilege < (restrictgamespeed ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                changegamespeed(val, ci);
                break;
            }

            case N_COPY:
                ci->cleanclipboard();
                ci->lastclipboard = totalmillis ? totalmillis : 1;
                goto genericmsg;

            case N_PASTE:
                if(ci->state.state!=CS_SPECTATOR) sendclipboard(ci);
                goto genericmsg;

            case N_CLIPBOARD:
            {
                int unpacklen = getint(p), packlen = getint(p);
                ci->cleanclipboard(false);
                if(ci->state.state==CS_SPECTATOR)
                {
                    if(packlen > 0) p.subbuf(packlen);
                    break;
                }
                if(packlen <= 0 || packlen > (1<<16) || unpacklen <= 0)
                {
                    if(packlen > 0) p.subbuf(packlen);
                    packlen = unpacklen = 0;
                }
                packetbuf q(32 + packlen, ENET_PACKET_FLAG_RELIABLE);
                putint(q, N_CLIPBOARD);
                putint(q, ci->clientnum);
                putint(q, unpacklen);
                putint(q, packlen);
                if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                ci->clipboard = q.finalize();
                ci->clipboard->referenceCount++;
                break;
            }

            case N_EDITT:
            case N_REPLACE:
            case N_EDITVSLOT:
            {
                int size = server::msgsizelookup(type);
                if(size<=0) { disconnect_client(sender, DISC_MSGERR); return; }
                loopi(size-1) getint(p);
                if(p.remaining() < 2) { disconnect_client(sender, DISC_MSGERR); return; }
                int extra = lilswap(*(const ushort *)p.pad(2));
                if(p.remaining() < extra) { disconnect_client(sender, DISC_MSGERR); return; }
                p.pad(extra);
                if(ci && ci->state.state!=CS_SPECTATOR) QUEUE_MSG;
                break;
            }

            case N_UNDO:
            case N_REDO:
            {
                int unpacklen = getint(p), packlen = getint(p);
                if(!ci || ci->state.state==CS_SPECTATOR || packlen <= 0 || packlen > (1<<16) || unpacklen <= 0)
                {
                    if(packlen > 0) p.subbuf(packlen);
                    break;
                }
                if(p.remaining() < packlen) { disconnect_client(sender, DISC_MSGERR); return; }
                packetbuf q(32 + packlen, ENET_PACKET_FLAG_RELIABLE);
                putint(q, type);
                putint(q, ci->clientnum);
                putint(q, unpacklen);
                putint(q, packlen);
                if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                sendpacket(-1, 1, q.finalize(), ci->clientnum);
                break;
            }

            case N_SERVCMD:
                getstring(text, p);
                break;

            case N_COUNTRY:
            {
                getstring(text, p);
                filtertext(ci->preferred_flag, text, T_NONE, MAXCOUNTRYCODELEN);
                geoip_set_custom_flag(ci->preferred_flag, ci->country_code, ci->country_name, ci->customflag_code, ci->customflag_name);
                packetbuf q(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
                putint(q, N_COUNTRY);
                putint(q, ci->clientnum);
                sendstring(ci->customflag_code, q);
                sendstring(ci->customflag_name, q);
                sendpacket(-1, 1, q.finalize());
                break;
            }

            #define PARSEMESSAGES 1
            #include "ctf.h"
            #undef PARSEMESSAGES

            case -1:
                disconnect_client(sender, DISC_MSGERR);
                return;

            case -2:
                disconnect_client(sender, DISC_OVERFLOW);
                return;

            default: genericmsg:
            {
                int size = server::msgsizelookup(type);
                if(size<=0) { disconnect_client(sender, DISC_MSGERR); return; }
                loopi(size-1) getint(p);
                if(ci) switch(msgfilter[type])
                {
                    case 2: case 3: if(ci->state.state != CS_SPECTATOR) QUEUE_MSG; break;
                    default: if(cq && (ci != cq || ci->state.state!=CS_SPECTATOR)) { QUEUE_AI; QUEUE_MSG; } break;
                }
                break;
            }
        }
    }

    int laninfoport() { return VALHALLA_LANINFO_PORT; }
    int serverport() { return VALHALLA_SERVER_PORT; }
    const char *defaultmaster() { return "valhalla-fps.net"; }
    int masterport() { return VALHALLA_MASTER_PORT; }
    int numchannels() { return 3; }

    #include "extinfo.h"

    void serverinforeply(ucharbuf &req, ucharbuf &p)
    {
        if(req.remaining() && !getint(req))
        {
            extserverinforeply(req, p);
            return;
        }

        putint(p, PROTOCOL_VERSION);
        putint(p, numclients(-1, false, false));
        putint(p, maxclients);
        putint(p, gamepaused || gamespeed != 100 ? 5 : 3); // number of attrs following
        putint(p, gamemode);
        putint(p, gamelimit && m_timed ? max((gamelimit - gamemillis)/1000, 0) : 0);
        putint(p, serverpass[0] ? MM_PASSWORD : (!m_mp(gamemode) ? MM_PRIVATE : (mastermode || mastermask&MM_AUTOAPPROVE ? mastermode : MM_AUTH)));
        if(gamepaused || gamespeed != 100)
        {
            putint(p, gamepaused ? 1 : 0);
            putint(p, gamespeed);
        }
        sendstring(smapname, p);
        sendstring(servername, p);
        putint(p, currentversion);
        sendserverinforeply(p);
    }

    int protocolversion() { return PROTOCOL_VERSION; }

    #include "aimanager.h"
}
