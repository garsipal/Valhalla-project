#include "game.h"

extern int maxparticletextdistance;

namespace game
{
    VARP(ragdoll, 0, 1, 1);
    VARP(ragdollmillis, 0, 10000, 300000);
    VARP(ragdollfade, 0, 380, 5000);
    VARP(forceplayermodels, 0, 0, 1);
    VARP(showdeadplayers, 0, 1, 1);

    extern int playermodel;

    vector<gameent *> ragdolls;

    void saveragdoll(gameent *d)
    {
        if(!d->ragdoll || !ragdollmillis || (!ragdollfade && lastmillis > d->lastpain + ragdollmillis)) return;
        gameent *r = new gameent(*d);
        r->lastupdate = ragdollfade && lastmillis > d->lastpain + max(ragdollmillis - ragdollfade, 0) ? lastmillis - max(ragdollmillis - ragdollfade, 0) : d->lastpain;
        r->edit = NULL;
        r->ai = NULL;
        if(d==self) r->playermodel = playermodel;
        ragdolls.add(r);
        d->ragdoll = NULL;
    }

    void clearragdolls()
    {
        ragdolls.deletecontents();
    }

    void moveragdolls()
    {
        loopv(ragdolls)
        {
            gameent *d = ragdolls[i];
            if(lastmillis > d->lastupdate + ragdollmillis)
            {
                delete ragdolls.remove(i--);
                continue;
            }
            moveragdoll(d);
        }
    }

    static const int playercolors[] =
    {
        0xA12020,
        0xA15B28,
        0xB39D52,
        0x3E752F,
        0x3F748C,
        0x214C85,
        0xB3668C,
        0x523678,
        0xB3ADA3
    };

    static const int playercolorsblue[] =
    {
        0x27508A,
        0x3F748C,
        0x3B3B80,
        0x5364B5
    };

    static const int playercolorsred[] =
    {
        0xAC2C2A,
        0x992417,
        0x802438,
        0xA3435B
    };

    extern void changedplayercolor();
    VARFP(playercolor, 0, 4, sizeof(playercolors)/sizeof(playercolors[0])-1, changedplayercolor());
    VARFP(playercolorblue, 0, 0, sizeof(playercolorsblue)/sizeof(playercolorsblue[0])-1, changedplayercolor());
    VARFP(playercolorred, 0, 0, sizeof(playercolorsred)/sizeof(playercolorsred[0])-1, changedplayercolor());
    SVARP(customflag, "");

    static const playermodelinfo playermodels[5] =
    {
        { 
            "player/bones", "player/bones/arm", { "cosmetic/skull", "cosmetic/cowboy", "cosmetic/helmet",  "cosmetic/wizard", "cosmetic/wings" }, true,
            0x60FFFFF, S_PAIN_MALE, { S_DIE_MALE1, -1, S_DIE_MALE2, -1, -1, S_DIE_MALE3, -1, S_DIE_MALE4 }, S_TAUNT_MALE
        },
        { 
            "player/bonnie", "player/bones/arm", { "cosmetic/skull", "cosmetic/cowboy", "cosmetic/helmet",  "cosmetic/wizard", "cosmetic/wings" }, true,
            0x60FFFFF, S_PAIN_FEMALE, { S_DIE_FEMALE1, -1, S_DIE_FEMALE2, -1, -1, S_DIE_FEMALE3, -1, S_DIE_MALE4 }, S_TAUNT_FEMALE
        },
        {
            "player/bones/zombie",  "player/bones/arm", { NULL, NULL, NULL, NULL, NULL }, true,
            0xFF90FF, S_PAIN_ZOMBIE_MALE, { S_DIE_ZOMBIE_MALE, -1, S_DIE_ZOMBIE_MALE, -1, -1, S_DIE_ZOMBIE_MALE, -1,  S_DIE_ZOMBIE_MALE }, S_TAUNT_ZOMBIE_MALE
        },
        { 
            "player/bonnie/zombie", "player/bones/arm", { NULL, NULL, NULL, NULL, NULL }, true,
            0xFF90FF, S_PAIN_ZOMBIE_FEMALE, { S_DIE_ZOMBIE_FEMALE, -1, S_DIE_ZOMBIE_FEMALE, -1, -1, S_DIE_ZOMBIE_FEMALE, -1, S_DIE_ZOMBIE_FEMALE }, S_TAUNT_ZOMBIE_FEMALE
        },
        { 
            "player/berserker", NULL, { "player/berserker/helmet",  NULL, NULL, NULL, NULL }, true,
            0x60FFFFF, S_PAIN_MALE, { S_DIE_MALE1, -1, S_DIE_MALE2, -1, -1, S_DIE_MALE3, -1, S_DIE_MALE4 }, S_TAUNT_MALE
        }
    };

    extern void changedplayermodel();
    VARFP(playermodel, 0, 0, 1, changedplayermodel());

    int chooserandomplayermodel(int seed)
    {
        int sizeofplayermodels = sizeof(playermodels) / sizeof(playermodels[0]);
        return (seed&0xFFFF) % (sizeofplayermodels - 3);
    }

    const playermodelinfo *getplayermodelinfo(int n)
    {
        if(size_t(n) >= sizeof(playermodels)/sizeof(playermodels[0])) return NULL;
        return &playermodels[n];
    }

    int getplayermodel(gameent *d)
    {
        int model = d==self || forceplayermodels ? playermodel : d->playermodel;
        if(d->role == ROLE_BERSERKER) return 4;
        else if(d->role == ROLE_ZOMBIE) return model == 0 ? 2 : 3;
        else return model;
    }

    const playermodelinfo &getplayermodelinfo(gameent *d)
    {
        const playermodelinfo *mdl = getplayermodelinfo(getplayermodel(d));
        if(!mdl) mdl = getplayermodelinfo(playermodel);
        return *mdl;
    }

    int getplayercolor(int team, int color)
    {
        #define GETPLAYERCOLOR(playercolors) \
            return playercolors[color%(sizeof(playercolors)/sizeof(playercolors[0]))];
        switch(team)
        {
            case 1: GETPLAYERCOLOR(playercolorsblue)
            case 2: GETPLAYERCOLOR(playercolorsred)
            default: GETPLAYERCOLOR(playercolors)
        }
    }

    ICOMMAND(getplayercolor, "ii", (int *color, int *team), intret(getplayercolor(*team, *color)));

    int getplayercolor(gameent *d, int team)
    {
        if(d==self) switch(team)
        {
            case 1: return getplayercolor(1, playercolorblue);
            case 2: return getplayercolor(2, playercolorred);
            default: return getplayercolor(0, playercolor);
        }
        else return getplayercolor(team, (d->playercolor>>(5*team))&0x1F);
    }

    void changedplayermodel()
    {
        if(self->clientnum < 0) self->playermodel = playermodel;
        if(self->ragdoll) cleanragdoll(self);
        loopv(ragdolls)
        {
            gameent *d = ragdolls[i];
            if(!d->ragdoll) continue;
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl) continue;
            }
            cleanragdoll(d);
        }
        loopv(players)
        {
            gameent *d = players[i];
            if(d == self || !d->ragdoll) continue;
            if(!forceplayermodels)
            {
                const playermodelinfo *mdl = getplayermodelinfo(d->playermodel);
                if(mdl) continue;
            }
            cleanragdoll(d);
        }
    }

    void changedplayercolor()
    {
        if(self->clientnum < 0) self->playercolor = playercolor | (playercolorblue<<5) | (playercolorred<<10);
    }

    void syncplayer()
    {
        if(self->playermodel != playermodel)
        {
            self->playermodel = playermodel;
            addmsg(N_SWITCHMODEL, "ri", self->playermodel);
        }

        int col = playercolor | (playercolorblue<<5) | (playercolorred<<10);
        if(self->playercolor != col)
        {
            self->playercolor = col;
            addmsg(N_SWITCHCOLOR, "ri", self->playercolor);
        }
        if(strcmp(self->preferred_flag, customflag) != 0)
        {
            filtertext(self->preferred_flag, customflag, T_NONE, MAXCOUNTRYCODELEN);
            addmsg(N_COUNTRY, "rs", self->preferred_flag);
        }
    }

    void preloadplayermodel()
    {
        loopi(sizeof(playermodels)/sizeof(playermodels[0]))
        {
            const playermodelinfo *pm = getplayermodelinfo(i);
            if(!pm) break;
            preloadmodel(pm->directory);
            loopj(5) if(pm->powerup[j]) preloadmodel(pm->powerup[j]);
        }
    }

    int numanims() { return NUMANIMS; }

    void findanims(const char *pattern, vector<int> &anims)
    {
        loopi(sizeof(animnames)/sizeof(animnames[0])) if(matchanim(animnames[i], pattern)) anims.add(i);
    }

    float updatetransparency(gameent* d, float fade)
    {
        float transparency = 1;
        switch (d->state)
        {
            case CS_ALIVE:
            {
                if (camera1->o.dist(d->o) <= d->radius)
                {
                    transparency = 0.1f;
                }
                else if(lastmillis - d->lastspawn <= DURATION_SPAWN)
                {
                    transparency = clamp((lastmillis - d->lastspawn) / 1000.0f, 0.0f, 1.0f);
                }
                break;
            }

            case CS_LAGGED:
            {
                transparency = 0.5f;
                break;
            }
        
            case CS_DEAD:
            {
                if (d->deathstate == Death_Disrupt)
                {
                    transparency -= clamp(float(lastmillis - d->lastpain) / 2000, 0.0f, 1.0f);
                    if (transparency <= 0)
                    {
                        d->deathstate = Death_Gib;
                    }
                }
                else if (d->deathstate != Death_Fall)
                {
                    transparency = fade;
                }
                break;
            }
        }
        return transparency;
    }

    VAR(animoverride, -1, 0, NUMANIMS-1);
    VAR(testanims, 0, 0, 1);
    VAR(testpitch, -90, 0, 90);

    void renderplayer(gameent *d, const playermodelinfo &playermodel, int color, int team, float fade, int flags = 0, bool mainpass = true)
    {
        if (d->deathstate == Death_Gib)
        {
            return;
        }

        int lastaction = d->lastaction, anim = ANIM_IDLE|ANIM_LOOP, attack = ANIM_SHOOT, delay = 0;
        if(d->state==CS_ALIVE)
        {
            if(d->lastattack >= 0)
            {
                attack = attacks[d->lastattack].anim;
                delay = attacks[d->lastattack].attackdelay+50;
            }
            if(d->lasttaunt && lastmillis-d->lasttaunt<1000 && lastmillis-d->lastaction>delay)
            {
                lastaction = d->lasttaunt;
                anim = attack = ANIM_TAUNT;
                delay = 1000;
            }
            if(d->lastswitch && lastmillis-d->lastswitch <= 600)
            {
                lastaction = d->lastswitch;
                anim = attack = ANIM_SWITCH;
                delay = 600;
            }
        }
        modelattach a[9];
        int ai = 0;
        if(guns[d->gunselect].worldmodel)
        {
            int vanim = ANIM_VWEP_IDLE|ANIM_LOOP, vtime = 0;
            if(lastaction && d->lastattack >= 0 && attacks[d->lastattack].gun==d->gunselect && lastmillis < lastaction + delay)
            {
                vanim = attacks[d->lastattack].vwepanim;
                vtime = lastaction;
            }
            a[ai++] = modelattach("tag_weapon", guns[d->gunselect].worldmodel, vanim, vtime);
        }
        if(mainpass && !(flags&MDL_ONLYSHADOW))
        {
            d->muzzle = d->eject = vec(-1, -1, -1);
            if(guns[d->gunselect].worldmodel)
            {
                a[ai++] = modelattach("tag_muzzle", &d->muzzle);
                a[ai++] = modelattach("tag_eject", &d->eject);
            }
        }
        if(d->state == CS_ALIVE)
        {
            a[ai++] = modelattach("tag_head", &d->head);
            a[ai++] = modelattach("tag_rfoot", &d->rfoot);
            a[ai++] = modelattach("tag_lfoot", &d->lfoot);
        }
        if(d->state != CS_SPECTATOR)
        {
            if(d->role == ROLE_BERSERKER)
            {
                a[ai++] = modelattach("tag_hat", playermodel.powerup[0], ANIM_MAPMODEL|ANIM_LOOP, 0);
            }
            else if(d->powerupmillis && !d->haspowerup(PU_INVULNERABILITY))
            {
                int type = clamp(d->poweruptype, (int)PU_DAMAGE, (int)PU_AGILITY);
                a[ai++] = modelattach(d->haspowerup(PU_AGILITY) ? "tag_back" : "tag_hat", playermodel.powerup[type-1], ANIM_MAPMODEL|ANIM_LOOP, 0);
            }
        }
        float yaw = testanims && d==self ? 0 : d->yaw,
              pitch = testpitch && d==self ? testpitch : d->pitch;
        vec o = d->feetpos();
        int basetime = 0;
        if(animoverride) anim = (animoverride<0 ? ANIM_ALL : animoverride)|ANIM_LOOP;
        else if(d->state==CS_DEAD)
        {
            anim = ANIM_DYING|ANIM_NOPITCH;
            basetime = d->lastpain;
            if(ragdoll && playermodel.ragdoll) anim |= ANIM_RAGDOLL;
            else if(lastmillis-basetime>1000) anim = ANIM_DEAD|ANIM_LOOP|ANIM_NOPITCH;
        }
        else if(d->state==CS_EDITING || d->state==CS_SPECTATOR) anim = ANIM_EDIT|ANIM_LOOP;
        else if(d->state==CS_LAGGED)                            anim = ANIM_LAG|ANIM_NOPITCH|ANIM_LOOP;
        else
        {
            if(lastmillis-d->lastpain < 300)
            {
                anim = ANIM_PAIN;
                basetime = d->lastpain;
            }
            else if(d->lastpain < lastaction && lastmillis-lastaction < delay)
            {
                anim = attack;
                basetime = lastaction;
            }
            bool canmove = physics::canmove(d);
            if (d->inwater && d->physstate <= PHYS_FALL) anim |= (((canmove && (d->move || d->strafe)) || d->vel.z + d->falling.z > 0 ? ANIM_SWIM : ANIM_SINK) | ANIM_LOOP) << ANIM_SECONDARY;
            else
            {
                static const int dirs[9] =
                {
                    ANIM_RUN_SE, ANIM_RUN_S, ANIM_RUN_SW,
                    ANIM_RUN_E,  0,          ANIM_RUN_W,
                    ANIM_RUN_NE, ANIM_RUN_N, ANIM_RUN_NW
                };
                int dir = dirs[(d->move+1)*3 + (d->strafe+1)];
                if(d->timeinair>100) anim |= ((dir && canmove ? dir+ANIM_JUMP_N-ANIM_RUN_N : ANIM_JUMP) | ANIM_END) << ANIM_SECONDARY;
                else if(dir && canmove) anim |= (dir | ANIM_LOOP) << ANIM_SECONDARY;
            }

            if(d->crouching) switch((anim>>ANIM_SECONDARY)&ANIM_INDEX)
            {
                case ANIM_IDLE: anim &= ~(ANIM_INDEX<<ANIM_SECONDARY); anim |= ANIM_CROUCH<<ANIM_SECONDARY; break;
                case ANIM_JUMP: anim &= ~(ANIM_INDEX<<ANIM_SECONDARY); anim |= ANIM_CROUCH_JUMP<<ANIM_SECONDARY; break;
                case ANIM_SWIM: anim &= ~(ANIM_INDEX<<ANIM_SECONDARY); anim |= ANIM_CROUCH_SWIM<<ANIM_SECONDARY; break;
                case ANIM_SINK: anim &= ~(ANIM_INDEX<<ANIM_SECONDARY); anim |= ANIM_CROUCH_SINK<<ANIM_SECONDARY; break;
                case 0: anim |= (ANIM_CROUCH|ANIM_LOOP)<<ANIM_SECONDARY; break;
                case ANIM_RUN_N: case ANIM_RUN_NE: case ANIM_RUN_E: case ANIM_RUN_SE: case ANIM_RUN_S: case ANIM_RUN_SW: case ANIM_RUN_W: case ANIM_RUN_NW:
                    anim += (ANIM_CROUCH_N - ANIM_RUN_N) << ANIM_SECONDARY;
                    break;
                case ANIM_JUMP_N: case ANIM_JUMP_NE: case ANIM_JUMP_E: case ANIM_JUMP_SE: case ANIM_JUMP_S: case ANIM_JUMP_SW: case ANIM_JUMP_W: case ANIM_JUMP_NW:
                    anim += (ANIM_CROUCH_JUMP_N - ANIM_JUMP_N) << ANIM_SECONDARY;
                    break;
            }

            if((anim&ANIM_INDEX)==ANIM_IDLE && (anim>>ANIM_SECONDARY)&ANIM_INDEX) anim >>= ANIM_SECONDARY;
        }
        if(!((anim>>ANIM_SECONDARY)&ANIM_INDEX)) anim |= (ANIM_IDLE|ANIM_LOOP)<<ANIM_SECONDARY;
        if(d!=self) flags |= MDL_CULL_VFC | MDL_CULL_OCCLUDED | MDL_CULL_QUERY;
        if(d->type==ENT_PLAYER) flags |= MDL_FULLBRIGHT;
        else flags |= MDL_CULL_DIST;
        if(!mainpass) flags &= ~(MDL_FULLBRIGHT | MDL_CULL_VFC | MDL_CULL_OCCLUDED | MDL_CULL_QUERY | MDL_CULL_DIST);
        d->transparency = updatetransparency(d, fade);
        rendermodel(playermodel.directory, anim, o, yaw, pitch, 0, flags, d, a[0].tag ? a : NULL, basetime, 0, fade, vec4(vec::hexcolor(color), d->transparency));
    }

    void rendermonster(dynent *d, const char *mdlname, modelattach *attachments, const int attack, const int attackdelay, const int lastaction, const int lastpain, const float fade, const bool ragdoll)
    {
        if (d->type != ENT_AI)
        {
            return;
        }

        int anim = ANIM_IDLE|ANIM_LOOP;
        vec o = d->feetpos();
        int basetime = 0;
        if (animoverride)
        {
            anim = (animoverride < 0 ? ANIM_ALL : animoverride) | ANIM_LOOP;
        }
        else if (d->state==CS_DEAD)
        {
            anim = ANIM_DYING | ANIM_NOPITCH;
            basetime = lastpain;
            if(ragdoll) anim |= ANIM_RAGDOLL;
            else if (lastmillis - basetime > 1000)
            {
                anim = ANIM_DEAD | ANIM_LOOP | ANIM_NOPITCH;
            }
        }
        else
        {
            if (lastmillis - lastpain < 300)
            {
                anim = ANIM_PAIN;
                basetime = lastpain;
            }
            else if (lastpain < lastaction && attack < 0)
            {
                anim = attack < 0 ? -attack : attack;
                basetime = lastaction;
            }
            if (d->inwater && d->physstate <= PHYS_FALL)
            {
                anim |= (((d->move || d->strafe) || d->vel.z + d->falling.z > 0 ? ANIM_SWIM : ANIM_SINK) | ANIM_LOOP) << ANIM_SECONDARY;
            }
            else if (d->timeinair > 100)
            {
                anim |= (ANIM_JUMP | ANIM_END) << ANIM_SECONDARY;
            }
            else if (d->move || d->strafe)
            {
                if(d->move>0) anim |= (ANIM_RUN_N|ANIM_LOOP) << ANIM_SECONDARY;
                else if(d->strafe)
                {
                    if(d->move<0) anim |= ((d->strafe>0 ? ANIM_RUN_E : ANIM_RUN_W)|ANIM_REVERSE|ANIM_LOOP) << ANIM_SECONDARY;
                    else anim |= ((d->strafe>0 ? ANIM_RUN_W : ANIM_RUN_E)|ANIM_LOOP) << ANIM_SECONDARY;
                }
                else if(d->move<0) anim |= (ANIM_RUN_S|ANIM_LOOP) << ANIM_SECONDARY;
            }

            if ((anim & ANIM_INDEX) == ANIM_IDLE && (anim >> ANIM_SECONDARY) & ANIM_INDEX)
            {
                anim >>= ANIM_SECONDARY;
            }
        }
        if(!((anim>>ANIM_SECONDARY)&ANIM_INDEX)) anim |= (ANIM_IDLE|ANIM_LOOP) << ANIM_SECONDARY;
        int flags = MDL_CULL_VFC | MDL_CULL_OCCLUDED | MDL_CULL_QUERY |  MDL_CULL_DIST;
        gameent* m = (gameent*)d;
        m->transparency = updatetransparency(m, fade);
        rendermodel(mdlname, anim, o, d->yaw, d->pitch, 0, flags, d, attachments, basetime, 0, fade, vec4(1, 1, 1, m->transparency));
    }

    static inline void renderplayer(gameent *d, float fade = 1, int flags = 0)
    {
        int team = m_teammode && validteam(d->team) ? d->team : 0;
        renderplayer(d, getplayermodelinfo(d), getplayercolor(d, team), team, fade, flags);
    }

    void booteffect(gameent *d)
    {
        if(d == followingplayer(self) && !camera::isthirdperson()) return;
        if(d->timeinair > 650 && (d->haspowerup(PU_AGILITY) || d->role == ROLE_BERSERKER || d->role == ROLE_ZOMBIE))
        {
            if(d->lastfootright.z >= 0) particle_flare(d->lastfootright, d->rfoot, 220, PART_TRAIL_STRAIGHT, getplayercolor(d, d->team), 0.3f);
            if(d->lastfootleft.z >= 0) particle_flare(d->lastfootleft, d->lfoot, 220, PART_TRAIL_STRAIGHT, getplayercolor(d, d->team), 0.3f);
            d->lastfootright = d->rfoot;
            d->lastfootleft = d->lfoot;
        }
    }

    VARP(statusbars, 0, 1, 1);
    FVARP(statusbarscale, 0, 1, 2);

    int gethealthcolor(gameent* d)
    {
        int maxhealth = d->maxhealth;
        if (d->health <= maxhealth / 4) return 0xFF0000;
        if (d->health <= maxhealth / 2) return 0xFF8000;
        if (d->health <= maxhealth) return 0x00FF80;
        return 0xD0F0FF;
    }

    float renderstatusbars(gameent* d)
    {
        if (!isally(self, d) || (d->state != CS_ALIVE && d->state != CS_LAGGED)) return 0;

        vec pos = d->abovehead().msub(camdir, 50 / 80.0f).msub(camup, 2.0f);
        float offset = 0;
        float size = statusbarscale;
        if (d->shield > 0)
        {
            int shieldlimit = 200;
            offset += size;
            float shieldfill = float(d->shield) / shieldlimit;
            int shieldcolor = 0xFFC040;
            particle_meter(vec(pos).madd(camup, offset), shieldfill, PART_METER, 1, shieldcolor, 0, size);
        }
        offset += size;
        float healthfill = float(d->health) / d->maxhealth;
        particle_meter(vec(pos).madd(camup, offset), healthfill, PART_METER, 1, gethealthcolor(d), 0, size);

        return offset;
    }

    inline bool hidenames()
    {
        return m_betrayal && self->state == CS_DEAD;
    }

    void renderplayereffects(gameent* d)
    {
        if (d->state != CS_ALIVE && d->state != CS_DEAD)
        {
            return;
        }

        float offset = renderstatusbars(d);
        gameent* hud = followingplayer(self);
        vec pos = d->abovehead().madd(camup, offset), hitpos;
        if (hud->o.dist(d->o) > maxparticletextdistance || !raycubelos(pos, camera1->o, hitpos))
        {
            if (isally(hud, d))
            {
                if (d->state == CS_ALIVE) particle_hud_mark(pos, 2, 0, PART_GAME_ICONS, 1, 0xFFFFFF, 2.0f);
                else if (d->deaths)
                {
                    /* Mark the location of death only if the player died during the game (deaths > 0),
                     * otherwise it means they are in a "fake death state" immediately after joining the game.
                     */
                    particle_hud_mark(pos, 3, 0, PART_GAME_ICONS, 1, 0xFFFFFF, 2.0f);
                }
            }
            if (m_berserker && d->role == ROLE_BERSERKER)
            {
                particle_hud_mark(pos, 1, 1, PART_GAME_ICONS, 1, 0xFFFFFF, 2.0f);
            }
            if (d->haspowerup(PU_INVULNERABILITY))
            {
                particle_hud_mark(pos, 0, 1, PART_GAME_ICONS, 1, 0xFFFFFF, 2.0f);
            }
        }
        else if (d->state == CS_ALIVE && !hidenames())
        {
            int team = m_teammode && validteam(d->team) ? d->team : 0;
            particle_text(pos, d->info, PART_TEXT, 1, teamtextcolor[team], 2.0f, 0, "wide");
        }
        booteffect(d);
    }

    void rendergame()
    {
        ai::render();

        bool isthirdPerson = camera::isthirdperson();
        gameent *f = followingplayer(), *exclude = isthirdPerson ? NULL : f;
        loopv(players)
        {
            gameent *d = players[i];
            if(d == self || d->state==CS_SPECTATOR || d->state==CS_SPAWNING || d->lifesequence < 0 || d == exclude || (d->state==CS_DEAD && !showdeadplayers)) continue;
            renderplayer(d);
            copystring(d->info, colorname(d));
            renderplayereffects(d);
        }
        loopv(ragdolls)
        {
            gameent *d = ragdolls[i];
            float fade = 1.0f;
            if (d->deathstate == Death_Fall)
            {
                fade -= clamp(float(lastmillis - d->lastpain) / 1000, 0.0f, 1.0f);
            }
            else if (ragdollmillis && ragdollfade)
            {
                fade -= clamp(float(lastmillis - (d->lastupdate + max(ragdollmillis - ragdollfade, 0))) / min(ragdollmillis, ragdollfade), 0.0f, 1.0f);
            }
            renderplayer(d, fade);
        }
        if (exclude)
        {
            renderplayer(exclude, 1, MDL_ONLYSHADOW);
        }
        else if (!f && (self->state == CS_ALIVE || (self->state == CS_EDITING && isthirdPerson) || (self->state == CS_DEAD && showdeadplayers)) && camera::camera.zoomstate.progress < 1)
        {
            float fade = 1.0f;
            if (self->deathstate == Death_Fall)
            {
                fade -= clamp(float(lastmillis - self->lastpain) / 1000, 0.0f, 1.0f);
            }
            renderplayer(self, fade, isthirdPerson ? 0 : MDL_ONLYSHADOW);
        }
            
        booteffect(self);
        entities::renderentities();
        renderprojectiles();
        rendermonsters();
        if(cmode) cmode->rendergame();
    }

    void drawhudmodel(gameent *d, int anim, int basetime)
    {
        const char *file = guns[d->gunselect].model;
        if(!file) return;

        sway.update(d);
        const playermodelinfo &playermodel = getplayermodelinfo(d);
        int team = m_teammode && validteam(d->team) ? d->team : 0, color = getplayercolor(d, team);
        defformatstring(gunname, "%s/%s", playermodel.armdirectory, file);
        d->muzzle = d->eject = vec(-1, -1, -1);
        modelattach a[3];
        int ai = 0;
        a[ai++] = modelattach("tag_muzzle", &d->muzzle);
        a[ai++] = modelattach("tag_eject", &d->eject);
        if (d->attacking == ACT_SECONDARY && d->gunselect == GUN_PULSE)
        {
            anim |= ANIM_LOOP;
            basetime = 0;
        }
        int flags = MDL_NOBATCH;
        if (lastmillis - d->lastspawn <= DURATION_SPAWN)
        {
            flags |= MDL_FORCETRANSPARENT;
        }
        rendermodel(gunname, anim, sway.o, sway.yaw, sway.pitch, sway.roll, flags, &sway.interpolation, a, basetime, 0, 1, vec4(vec::hexcolor(color)));

        if(d->muzzle.x >= 0) d->muzzle = calcavatarpos(d->muzzle, 12);
    }

    void drawhudgun()
    {
        gameent *d = hudplayer();
        extern int hudgun;
        if(d->state == CS_DEAD || d->state == CS_SPECTATOR || d->state == CS_EDITING || !hudgun || editmode)
        {
            d->muzzle = self->muzzle = d->eject = self->eject = vec(-1, -1, -1);
            return;
        }

        int anim = ANIM_GUN_IDLE|ANIM_LOOP, basetime = 0;
        if(validatk(d->lastattack))
        {
            bool animateattack = d->lastattack == ATK_MELEE || attacks[d->lastattack].gun == d->gunselect;
            if(animateattack && d->lastaction && lastmillis - d->lastaction < attacks[d->lastattack].attackdelay)
            {
                if(anim >= 0) anim = attacks[d->lastattack].hudanim;
                basetime = d->lastaction;
                d->lastswitch = 0;
            }
        }
        if(d->lastswitch && lastmillis - d->lastswitch <= 600)
        {
            if(anim >= 0) anim = ANIM_GUN_SWITCH;
            basetime = d->lastswitch;
        }
        if(d->lasttaunt && lastmillis-d->lasttaunt < 1000)
        {
            if(anim >= 0) anim = ANIM_GUN_TAUNT;
            basetime = d->lasttaunt;
        }
        drawhudmodel(d, anim, basetime);
    }

    void renderavatar()
    {
        drawhudgun();
    }

    void renderplayerpreview(int model, int color, int team, int weap)
    {
        static gameent *previewent = NULL;
        if(!previewent)
        {
            previewent = new gameent;
            loopi(NUMGUNS) previewent->ammo[i] = 1;
        }
        float height = previewent->eyeheight + previewent->aboveeye,
              zrad = height/2;
        vec2 xyrad = vec2(previewent->xradius, previewent->yradius).max(height/4);
        previewent->o = calcmodelpreviewpos(vec(xyrad, zrad), previewent->yaw).addz(previewent->eyeheight - zrad);
        previewent->gunselect = validgun(weap) ? weap : GUN_RAIL;
        const playermodelinfo *mdlinfo = getplayermodelinfo(model);
        if(!mdlinfo) return;
        renderplayer(previewent, *mdlinfo, getplayercolor(team, color), team, 1, 0, false);
    }

    void preloadweapons()
    {
        const playermodelinfo &playermodel = getplayermodelinfo(self);
        loopi(NUMGUNS)
        {
            const char *file = guns[i].model;
            if(!file) continue;
            string fname;
            formatstring(fname, "%s/%s", playermodel.armdirectory, file);
            preloadmodel(fname);
            if(guns[i].worldmodel) preloadmodel(guns[i].worldmodel);
        }
    }

    void preloadsounds()
    {
        for(int i = S_JUMP1; i <= S_INTERMISSION_WIN; i++) preloadsound(i);
    }

    void preload()
    {
        preloadweapons();
        preloadprojectiles();
        preloadplayermodel();
        preloadsounds();
        entities::preloadentities();
        preloadmonsters();
    }

}

