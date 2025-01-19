// creation of scoreboard
#include "game.h"

namespace game
{
    VARP(showservinfo, 0, 1, 1);
    VARP(showclientnum, 0, 0, 1);
    VARP(showpj, 0, 0, 1);
    VARP(showping, 0, 1, 1);
    VARP(showspectators, 0, 1, 1);
    VARP(highlightscore, 0, 1, 1);
    VARP(showconnecting, 0, 0, 1);

    static teaminfo teaminfos[MAXTEAMS];

    void clearteaminfo()
    {
        loopi(MAXTEAMS) teaminfos[i].reset();
    }

    void setteaminfo(int team, int frags)
    {
        if(!validteam(team)) return;
        teaminfo &t = teaminfos[team-1];
        t.frags = frags;
    }

    static inline bool playersort(const gameent *a, const gameent *b)
    {
        if(a->state==CS_SPECTATOR)
        {
            if(b->state==CS_SPECTATOR) return strcmp(a->name, b->name) < 0;
            else return false;
        }
        else if(b->state==CS_SPECTATOR) return true;
        if(m_ctf)
        {
            if(a->flags > b->flags) return true;
            if(a->flags < b->flags) return false;
        }
        else if(m_lms || m_infection)
        {
            if(a->points > b->points) return true;
            if(a->points < b->points) return false;
        }
        if(a->frags > b->frags) return true;
        if(a->frags < b->frags) return false;
        return strcmp(a->name, b->name) < 0;
    }

    void getbestplayers(vector<gameent *> &best)
    {
        loopv(players)
        {
            gameent *o = players[i];
            if((o->state!=CS_SPECTATOR || (o->state==CS_SPECTATOR && o->ghost))) best.add(o);
        }
        best.sort(playersort);
        while(best.length() > 1 && best.last()->frags < best[0]->frags) best.drop();
    }

    void getbestteams(vector<int> &best)
    {
        if(cmode && cmode->hidefrags())
        {
            vector<teamscore> teamscores;
            cmode->getteamscores(teamscores);
            teamscores.sort(teamscore::compare);
            while(teamscores.length() > 1 && teamscores.last().score < teamscores[0].score) teamscores.drop();
            loopv(teamscores) best.add(teamscores[i].team);
        }
        else
        {
            int bestfrags = INT_MIN;
            loopi(MAXTEAMS)
            {
                teaminfo &t = teaminfos[i];
                bestfrags = max(bestfrags, t.frags);
            }
            loopi(MAXTEAMS)
            {
                teaminfo &t = teaminfos[i];
                if(t.frags >= bestfrags) best.add(1+i);
            }
        }
    }

    static vector<gameent *> teamplayers[1+MAXTEAMS], spectators;

    static void groupplayers()
    {
        loopi(1+MAXTEAMS) teamplayers[i].setsize(0);
        spectators.setsize(0);
        loopv(players)
        {
            gameent *o = players[i];
            if(!showconnecting && !o->name[0]) continue;
            if(!o->ghost && o->state==CS_SPECTATOR) { spectators.add(o); continue; }
            int team = m_teammode && validteam(o->team) ? o->team : 0;
            teamplayers[team].add(o);
        }
        loopi(1+MAXTEAMS) teamplayers[i].sort(playersort);
        spectators.sort(playersort);
    }

    void removegroupedplayer(gameent *d)
    {
        loopi(1+MAXTEAMS) teamplayers[i].removeobj(d);
        spectators.removeobj(d);
    }

    void refreshscoreboard()
    {
        groupplayers();
    }

    COMMAND(refreshscoreboard, "");
    ICOMMAND(numscoreboard, "i", (int *team), intret(*team < 0 ? spectators.length() : (*team <= MAXTEAMS ? teamplayers[*team].length() : 0)));
    ICOMMAND(loopscoreboard, "rie", (ident *id, int *team, uint *body),
    {
        if(*team > MAXTEAMS) return;
        loopstart(id, stack);
        vector<gameent *> &p = *team < 0 ? spectators : teamplayers[*team];
        loopv(p)
        {
            loopiter(id, stack, p[i]->clientnum);
            execute(body);
        }
        loopend(id, stack);
    });

    ICOMMAND(scoreboardstatus, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            bool isalive = d->state != CS_DEAD && !d->ghost;
            int status = !isalive ? 0x606060 : 0xFFFFFF;
            if(validprivilege(d->privilege))
            {
                status = privilegecolors[d->privilege];
                if(!isalive) status = (status>>1)&0x7F7F7F;
            }
            intret(status);
        }
    });

    ICOMMAND(scoreboardpj, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d && d != self)
        {
            if(d->state==CS_LAGGED) result("LAG");
            else intret(d->plag);
        }
    });

    ICOMMAND(scoreboardping, "i", (int *cn),
    {
        gameent *d = getclient(*cn);
        if(d)
        {
            if(!showpj && d->state==CS_LAGGED) result("LAG");
            else intret(d->ping);
        }
    });

    ICOMMAND(scoreboardshowfrags, "", (), intret(cmode && cmode->hidefrags() ? 0 : 1));
    ICOMMAND(scoreboardshowclientnum, "", (), intret(showclientnum || self->privilege>=PRIV_MASTER ? 1 : 0));
    ICOMMAND(scoreboardmultiplayer, "", (), intret(multiplayer(false) || demoplayback ? 1 : 0));

    ICOMMAND(scoreboardhighlight, "i", (int *cn),
        intret(*cn == self->clientnum && highlightscore && (multiplayer(false) || demoplayback || players.length() > 1) ? 0x808080 : 0));

    ICOMMAND(scoreboardservinfo, "", (),
    {
        if(!showservinfo) return;
        const ENetAddress *address = connectedpeer();
        if(address && self->clientnum >= 0)
        {
            if(servdesc[0])
            {
                filtertext(servdesc, servdesc, T_COLORS | T_WHITESPACE);
                result(servdesc);
            }
            else
            {
                string hostname;
                if(enet_address_get_host_ip(address, hostname, sizeof(hostname)) >= 0)
                    result(tempformatstring("%s:%d", hostname, address->port));
            }
        }
    });

    ICOMMAND(scoreboardmode, "", (),
    {
        result(server::modeprettyname(gamemode));
    });

    ICOMMAND(scoreboardmap, "", (),
    {
        const char *mname = getclientmap();
        result(mname[0] ? mname : "[new map]");
    });

    ICOMMAND(scoreboardtime, "", (),
    {
        if(m_timed && getclientmap() && (maplimit >= 0 || intermission))
        {
            int secs = max(maplimit-lastmillis, 0)/1000;
            result(tempformatstring("%d:%02d", secs/60, secs%60));
        }
    });

    ICOMMAND(getteamscore, "i", (int *team),
    {
        if(m_teammode && validteam(*team))
        {
            if(cmode && cmode->hidefrags()) intret(cmode->getteamscore(*team));
            else intret(teaminfos[*team-1].frags);
        }
    });
}

