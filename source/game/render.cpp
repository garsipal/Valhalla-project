#include "game.h"

extern int maxparticletextdistance, zoom;

namespace game
{
    VARP(ragdoll, 0, 1, 1);
    VARP(ragdollmillis, 0, 10000, 300000);
    VARP(ragdollfade, 0, 400, 5000);
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

    static const playermodelinfo playermodels[5] =
    {
        { "player/bones",         "player/bones/arm", { "cosmetic/skull",           "cosmetic/cowboy", "cosmetic/helmet",  "cosmetic/wizard", "cosmetic/wings" }, true, 0x60FFFFF, S_PAIN_MALE,          S_DIE_MALE,          S_TAUNT_MALE,         },
        { "player/bonnie",        "player/bones/arm", { "cosmetic/skull",           "cosmetic/cowboy", "cosmetic/helmet",  "cosmetic/wizard", "cosmetic/wings" }, true, 0x60FFFFF, S_PAIN_FEMALE,        S_DIE_FEMALE,        S_TAUNT_FEMALE        },
        { "player/bones/zombie",  "player/bones/arm", { NULL,                       NULL,              NULL,               NULL,              NULL             }, true, 0xFF90FF,  S_PAIN_ZOMBIE_MALE,   S_DIE_ZOMBIE_MALE,   S_TAUNT_ZOMBIE_MALE   },
        { "player/bonnie/zombie", "player/bones/arm", { NULL,                       NULL,              NULL,               NULL,              NULL,            }, true, 0xFF90FF,  S_PAIN_ZOMBIE_FEMALE, S_DIE_ZOMBIE_FEMALE, S_TAUNT_ZOMBIE_FEMALE },
        { "player/berserker",    NULL,                { "player/berserker/helmet",  NULL,              NULL,               NULL,              NULL,            }, true, 0x60FFFFF, S_PAIN_MALE,          S_DIE_MALE,          S_TAUNT_MALE          }
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

    VAR(animoverride, -1, 0, NUMANIMS-1);
    VAR(testanims, 0, 0, 1);
    VAR(testpitch, -90, 0, 90);

    void renderplayer(gameent *d, const playermodelinfo &playermodel, int color, int team, float fade, int flags = 0, bool mainpass = true)
    {
        if(gore && d->gibbed()) return;
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

            if(d->inwater && d->physstate<=PHYS_FALL) anim |= (((physics::allowmove(d) && (d->move || d->strafe)) || d->vel.z+d->falling.z>0 ? ANIM_SWIM : ANIM_SINK)|ANIM_LOOP)<<ANIM_SECONDARY;
            else
            {
                static const int dirs[9] =
                {
                    ANIM_RUN_SE, ANIM_RUN_S, ANIM_RUN_SW,
                    ANIM_RUN_E,  0,          ANIM_RUN_W,
                    ANIM_RUN_NE, ANIM_RUN_N, ANIM_RUN_NW
                };
                int dir = dirs[(d->move+1)*3 + (d->strafe+1)];
                if(d->timeinair>100) anim |= ((dir && physics::allowmove(d) ? dir+ANIM_JUMP_N-ANIM_RUN_N : ANIM_JUMP) | ANIM_END) << ANIM_SECONDARY;
                else if(dir && physics::allowmove(d)) anim |= (dir | ANIM_LOOP) << ANIM_SECONDARY;
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
        float trans = 1;
        if(d->state == CS_LAGGED) trans = 0.5f;
        else if(d->state == CS_ALIVE && camera1->o.dist(d->o) < d->radius) trans = 0.1f;
        else if(d->deathtype == DEATH_DISRUPT) trans = 0.8f;
        rendermodel(playermodel.directory, anim, o, yaw, pitch, 0, flags, d, a[0].tag ? a : NULL, basetime, 0, fade, vec4(vec::hexcolor(color), trans));
    }

    void renderai(dynent *d, const char *mdlname, modelattach *attachments, int hold, int attack, int attackdelay, int lastaction, int lastpain, float fade, bool ragdoll)
    {
        if(d->type != ENT_AI) return;
        int anim = hold ? hold : ANIM_IDLE|ANIM_LOOP;
        vec o = d->feetpos();
        int basetime = 0;
        if(animoverride) anim = (animoverride<0 ? ANIM_ALL : animoverride)|ANIM_LOOP;
        else if(d->state==CS_DEAD)
        {
            anim = ANIM_DYING|ANIM_NOPITCH;
            basetime = lastpain;
            if(ragdoll) anim |= ANIM_RAGDOLL;
            else if(lastmillis-basetime>1000) anim = ANIM_DEAD|ANIM_LOOP|ANIM_NOPITCH;
        }
        else
        {
            if(lastmillis-lastpain < 300)
            {
                anim = ANIM_PAIN;
                basetime = lastpain;
            }
            else if(lastpain < lastaction && attack < 0)
            {
                anim = attack < 0 ? -attack : attack;
                basetime = lastaction;
            }

            if(d->inwater && d->physstate<=PHYS_FALL) anim |= (((d->move || d->strafe) || d->vel.z+d->falling.z>0 ? ANIM_SWIM : ANIM_SINK)|ANIM_LOOP)<<ANIM_SECONDARY;
            else if(d->timeinair>100) anim |= (ANIM_JUMP|ANIM_END)<<ANIM_SECONDARY;
            else if(d->move || d->strafe)
            {
                if(d->move>0) anim |= (ANIM_RUN_N|ANIM_LOOP)<<ANIM_SECONDARY;
                else if(d->strafe)
                {
                    if(d->move<0) anim |= ((d->strafe>0 ? ANIM_RUN_E : ANIM_RUN_W)|ANIM_REVERSE|ANIM_LOOP)<<ANIM_SECONDARY;
                    else anim |= ((d->strafe>0 ? ANIM_RUN_W : ANIM_RUN_E)|ANIM_LOOP)<<ANIM_SECONDARY;
                }
                else if(d->move<0) anim |= (ANIM_RUN_S|ANIM_LOOP)<<ANIM_SECONDARY;
            }

            if((anim&ANIM_INDEX)==ANIM_IDLE && (anim>>ANIM_SECONDARY)&ANIM_INDEX) anim >>= ANIM_SECONDARY;
        }
        if(!((anim>>ANIM_SECONDARY)&ANIM_INDEX)) anim |= (ANIM_IDLE|ANIM_LOOP)<<ANIM_SECONDARY;
        int flags = MDL_CULL_VFC | MDL_CULL_OCCLUDED | MDL_CULL_QUERY |  MDL_CULL_DIST;
        rendermodel(mdlname, anim, o, d->yaw+90, d->pitch, 0, flags, d, attachments, basetime, 0, fade);
    }

    static inline void renderplayer(gameent *d, float fade = 1, int flags = 0)
    {
        int team = m_teammode && validteam(d->team) ? d->team : 0;
        renderplayer(d, getplayermodelinfo(d), getplayercolor(d, team), team, fade, flags);
    }

    void booteffect(gameent *d)
    {
        if(d == followingplayer(self) && !isthirdperson()) return;
        if(d->timeinair > 650 && (d->haspowerup(PU_AGILITY) || d->role == ROLE_BERSERKER || d->role == ROLE_ZOMBIE))
        {
            if(d->lastfootright.z >= 0) particle_flare(d->lastfootright, d->rfoot, 220, PART_TRAIL_STRAIGHT, getplayercolor(d, d->team), 0.3f);
            if(d->lastfootleft.z >= 0) particle_flare(d->lastfootleft, d->lfoot, 220, PART_TRAIL_STRAIGHT, getplayercolor(d, d->team), 0.3f);
            d->lastfootright = d->rfoot;
            d->lastfootleft = d->lfoot;
        }
    }

    inline bool hidenames()
    {
        return m_betrayal && self->state == CS_DEAD;
    }

    void rendergame()
    {
        ai::render();

        bool third = isthirdperson();
        gameent *f = followingplayer(), *exclude = third ? NULL : f;
        loopv(players)
        {
            gameent *d = players[i];
            if(d == self || d->state==CS_SPECTATOR || d->state==CS_SPAWNING || d->lifesequence < 0 || d == exclude || (d->state==CS_DEAD && !showdeadplayers)) continue;
            renderplayer(d);
            copystring(d->info, colorname(d));
            if(d->state == CS_ALIVE || d->state == CS_DEAD)
            {
                int team = m_teammode && validteam(d->team) ? d->team : 0;
                gameent *hud = followingplayer(self);
                if(hud->o.dist(d->o) > maxparticletextdistance)
                {
                    if(isally(hud, d))
                    {
                        if(d->state == CS_ALIVE) particle_icon_mark(d->abovehead(), 2, 0, PART_GAME_ICONS, 1, 0xFFFFFF, 2.0f);
                        else if(d->deaths)
                        {
                            particle_icon_mark(d->abovehead(), 3, 0, PART_GAME_ICONS, 1, 0xFFFFFF, 2.0f);
                        }
                    }
                    else if(d->role == ROLE_BERSERKER) particle_icon_mark(d->abovehead(), 1, 1, PART_GAME_ICONS, 1, 0xFFFFFF, 3.0f);
                    if(d->haspowerup(PU_INVULNERABILITY)) particle_icon_mark(d->o, 0, 1, PART_GAME_ICONS, 1, 0xF9B303, 4.0f);
                }
                else if(d->state == CS_ALIVE && !hidenames()) particle_text(d->abovehead(), d->info, PART_TEXT, 1, teamtextcolor[team], 2.0f);
            }
            booteffect(d);
        }
        loopv(ragdolls)
        {
            gameent *d = ragdolls[i];
            float fade = 1.0f;
            if(ragdollmillis && ragdollfade)
                fade -= clamp(float(lastmillis - (d->lastupdate + max(ragdollmillis - ragdollfade, 0)))/min(ragdollmillis, ragdollfade), 0.0f, 1.0f);
            renderplayer(d, fade);
        }
        if(exclude)
            renderplayer(exclude, 1, MDL_ONLYSHADOW);
        else if(!f && (self->state==CS_ALIVE || (self->state==CS_EDITING && third) || (self->state == CS_DEAD && showdeadplayers)))
            renderplayer(self, 1, third ? 0 : MDL_ONLYSHADOW);
        booteffect(self);
        entities::renderentities();
        renderprojectiles();
        rendermonsters();
        if(cmode) cmode->rendergame();
    }

    VARP(hudgun, 0, 1, 1);
    VARP(hudgunsway, 0, 1, 1);

    FVAR(swaystep, 1, 36.8f, 100);
    FVAR(swayside, 0, 0.03f, 1);
    FVAR(swayup, -1, 0.02f, 1);
    FVAR(swayrollfactor, 1, 4.2f, 30);
    FVAR(swaydecay, 0.1f, 0.996f, 0.9999f);
    FVAR(swayinertia, 0.0f, 0.04f, 1.0f);
    FVAR(swaymaxinertia, 0.0f, 15.0f, 1000.0f);

    float swayfade = 0, swayspeed = 0, swaydist = 0, swayyaw = 0, swaypitch = 0, swaylandpitch = 0;
    vec swaydir(0, 0, 0);

    // FP weapon sway by Q009, ported from RE
    static void updatesway(gameent* d, vec& sway, int curtime)
    {
        vec sidedir = vec((d->yaw + 90)*RAD, 0.0f), trans = vec(0, 0, 0);

        float steplen = swaystep;
        float steps = swaydist / steplen * M_PI;

        // Magic floats to generate the animation cycle
        float f1 = cosf(steps) + 1,
            f2 = sinf(steps * 2.0f) + 1,
            f3 = (f1 * f1 * 0.25f) - 0.5f,
            f4 = (f2 * f2 * 0.25f) - 0.5f,
            f5 = sinf(lastmillis * 0.001f); // Low frequency detail

        vec dirforward = vec(d->yaw*RAD, 0.0f), dirside = vec((d->yaw + 90)*RAD, 0.0f);
        float rotyaw = 0, rotpitch = 0;

        // Walk cycle animation
        trans.add(vec(dirforward).mul(swayside * f4 * 2.0f));
        trans.add(vec(dirside).mul(swayside * f5 * 2.0f));
        trans.add(vec(sway).mul(-4.0f));
        trans.z += swayup * f2 * 1.5f;
        rotyaw += swayside * f3 * 24.0f;
        rotpitch += swayup * f2 * -10.0f;

        // "Look-around" animation
        static int lastsway = 0;
        static vec2 lastcam = vec2(camera1->yaw, camera1->pitch);
        static vec2 camvel = vec2(0, 0);

        if(lastmillis != lastsway) // Prevent running the inertia math multiple times in the same frame
        {
            vec2 curcam = vec2(camera1->yaw, camera1->pitch);
            vec2 camrot = vec2(lastcam).sub(curcam);

            if (camrot.x > 180.0f) camrot.x -= 360.0f;
            else if (camrot.x < -180.0f) camrot.x += 360.0f;

            camvel.mul(powf(swaydecay, curtime));
            camvel.add(vec2(camrot).mul(swayinertia));
            camvel.clamp(-swaymaxinertia, swaymaxinertia);

            lastcam = curcam;
            lastsway = lastmillis;
        }
        trans.add(sidedir.mul(camvel.x * 0.06f));
        trans.z += camvel.y * 0.045f;
        rotyaw += camvel.x * -0.3f;
        rotpitch += camvel.y * -0.3f;
        sway.add(trans); // add the trans to the swaydir vector, where the weapon model is at
        swayyaw = rotyaw;
        swaypitch = rotpitch;
    }

    void swayhudgun(int curtime)
    {
        gameent *d = hudplayer();
        if(d->state == CS_SPECTATOR || d->state == CS_EDITING) return;

        if(d->physstate >= PHYS_SLOPE)
        {
            swayspeed = min(sqrtf(d->vel.x*d->vel.x + d->vel.y*d->vel.y), d->speed);
            swaydist += swayspeed*curtime/1000.0f;
            swaydist = fmod(swaydist, 2*swaystep);
            swayfade = 1;
        }
        else if(swayfade > 0)
        {
            swaydist += swayspeed*swayfade*curtime/1000.0f;
            swaydist = fmod(swaydist, 2*swaystep);
            swayfade -= 0.5f*(curtime*d->speed)/(swaystep*1000.0f);
        }
        if(lastmillis - d->lastland <= 350)
        {
            float progress = clamp((lastmillis - d->lastland) / (float)350, 0.0f, 1.0f);
            swaylandpitch = -10 * sinf(progress * PI);
        }
        else if(d->lastswitch > 0 && lastmillis - d->lastswitch <= 500)
        {
            float progress = clamp((lastmillis - d->lastswitch) / (float)500, 0.0f, 1.0f);
            swaylandpitch = -15 * sinf(progress * PI);
        }
        else swaylandpitch = 0;

        float k = pow(0.7f, curtime/10.0f);
        swaydir.mul(k);
        vec vel(d->vel);
        vel.add(d->falling);
        swaydir.add(vec(vel).mul((1-k)/(8*max(vel.magnitude(), d->speed))));
    }

    struct hudent : dynent
    {
        hudent() { type = ENT_CAMERA; }
    } guninterp;

    void drawhudmodel(gameent *d, int anim, int basetime)
    {
        const char *file = guns[d->gunselect].model;
        if(!file) return;

        vec sway;
        vecfromyawpitch(d->yaw, 0, 0, 1, sway);
        float steps = swaydist/swaystep*M_PI;
        sway.mul(swayside*sinf(steps));
        updatesway(d, sway, curtime);
        sway.add(swaydir).add(d->o);
        if(!hudgunsway) sway = d->o;

        const playermodelinfo &playermodel = getplayermodelinfo(d);
        int team = m_teammode && validteam(d->team) ? d->team : 0, color = getplayercolor(d, team);
        defformatstring(gunname, "%s/%s", playermodel.armdirectory, file);
        d->muzzle = d->eject = vec(-1, -1, -1);
        modelattach a[3];
        int ai = 0;
        a[ai++] = modelattach("tag_muzzle", &d->muzzle);
        a[ai++] = modelattach("tag_eject", &d->eject);
        swaypitch += swaylandpitch;
        float yaw = d->yaw + swayyaw, pitch = d->pitch + swaypitch, roll = d->roll * swayrollfactor;
        if (d->attacking && attacks[guns[d->gunselect].attacks[d->attacking]].isloop)
        {
            anim |= ANIM_LOOP;
            basetime = 0;
        }
        rendermodel(gunname, anim, sway, yaw, pitch, roll, MDL_NOBATCH, &guninterp, a, basetime, 0, 1, vec4(vec::hexcolor(color), 1));
        if(d->muzzle.x >= 0) d->muzzle = calcavatarpos(d->muzzle, 12);
    }

    void drawhudgun()
    {
        gameent *d = hudplayer();
        if(d->state == CS_DEAD || d->state==CS_SPECTATOR || d->state==CS_EDITING || !hudgun || editmode)
        {
            d->muzzle = self->muzzle = vec(-1, -1, -1);
            return;
        }

        int anim = ANIM_GUN_IDLE|ANIM_LOOP, basetime = 0;
        bool animateattack = d->lastattack == ATK_MELEE || attacks[d->lastattack].gun == d->gunselect;
        if(animateattack && d->lastaction && d->lastattack >= 0 && lastmillis - d->lastaction < attacks[d->lastattack].attackdelay)
        {
            if(anim >= 0) anim = attacks[d->lastattack].hudanim;
            basetime = d->lastaction;
            d->lastswitch = 0;
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

    vec hudgunorigin(int gun, const vec &from, const vec &to, gameent *d)
    {
        if(zoom && d == self) return d->feetpos(4);
        else if(d->muzzle.x >= 0) return d->muzzle;
        vec offset(from);
        if(d!=hudplayer() || isthirdperson())
        {
            vec front, right;
            vecfromyawpitch(d->yaw, d->pitch, 1, 0, front);
            offset.add(front.mul(d->radius));
            if(d->type != ENT_AI)
            {
                offset.z += (d->aboveeye + d->eyeheight)*0.75f - d->eyeheight;
                vecfromyawpitch(d->yaw, 0, 0, -1, right);
                offset.add(right.mul(0.5f*d->radius));
                offset.add(front);
            }
            return offset;
        }
        offset.add(vec(to).sub(from).normalize().mul(2));
        if(hudgun)
        {
            offset.sub(vec(camup).mul(1.0f));
            offset.add(vec(camright).mul(0.8f));
        }
        else offset.sub(vec(camup).mul(0.8f));
        return offset;
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

