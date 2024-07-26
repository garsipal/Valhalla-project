/*
 * ========================================================================================================================
 * monster.cpp
 * Implements AI for monsters and NPCs
 * ========================================================================================================================
 *
 * Monster functionalities:
 * - Extend the "gameent" entity for managing monster-specific attributes.
 * - Manage various aspects of NPC behavior, including state transitions, attacks, movement and interactions.
 * - Handle actions such as targeting, pathfinding and decision-making based on the monster's current state.
 * - Support behaviors like teleportation, melee attacks, ranged attacks, detonation and death animations.
 * - Support configuration parameters such as skill level and entity attributes.
 *
 * Note: monsters are currently implemented for client-side gameplay only.
 *
 * ========================================================================================================================
 */

#include "game.h"

namespace game
{
    static vector<int> teleports;

    VAR(skill, 1, 8, 10);
    VAR(killsendsp, 0, 1, 1);

    bool monsterhurt;
    vec monsterhurtpos;

    struct monster : gameent
    {
        int monsterstate; // one of MS_*, MS_NONE means it's not an NPC

        int mtype, tag; // see monstertypes table
        gameent *enemy; // monster wants to kill this entity
        float targetyaw; // monster wants to look in this direction
        int trigger; // millis at which transition to another monsterstate takes place
        vec attacktarget;
        int anger; // how many times already hit by fellow monster
        physent *stacked;
        vec stackpos, orient;
        bool halted, canmove, exploding;
        int lastunblocked, detonating;
        int bursting, shots;

        monster(int _type, int _yaw, int _tag, int _canmove, int _state, int _trigger, int _move) :
            monsterstate(_state), tag(_tag),
            stacked(NULL),
            stackpos(0, 0, 0)
        {
            type = ENT_AI;
            respawn();
            if(_type>=NUMMONSTERS || _type < 0)
            {
                conoutf(CON_WARN, "warning: unknown monster in spawn: %d", _type);
                _type = 0;
            }
            mtype = _type;
            const monstertype &t = monstertypes[mtype];
            eyeheight = 8.0f;
            aboveeye = 7.0f;
            radius *= t.bscale/10.0f;
            xradius = yradius = radius;
            eyeheight = max(9.5f, eyeheight * t.bscale/10.f);
            aboveeye *= t.bscale/10.0f;
            weight = t.weight;
            if(_state!=MS_SLEEP) spawnplayer(this);
            trigger = lastmillis+_trigger;
            targetyaw = yaw = (float)_yaw;
            canmove = _canmove;
            if(canmove) move = _move;
            enemy = self;
            gunselect = attacks[t.atk].gun;
            speed = (float)t.speed*4;
            health = t.health;
            shield = 0;
            loopi(NUMGUNS) ammo[i] = 10000;
            pitch = 0;
            roll = 0;
            state = CS_ALIVE;
            anger = 0;
            orient = headpos();
            copystring(name, t.name);
            halted = exploding = false;
            lastunblocked = detonating = 0;
            bursting = shots = 0;

            spawneffect(this);
        }

        void normalize_yaw(float angle)
        {
            while(yaw<angle-180.0f) yaw += 360.0f;
            while(yaw>angle+180.0f) yaw -= 360.0f;
        }

        /* monster AI is sequenced using transitions: they are in a particular state where
         * they execute a particular behaviour until the trigger time is hit, and then they
         * reevaluate their situation based on the current state, the environment etc., and
         * transition to the next state. Transition timeframes are parametrized by difficulty
         * level (skill), faster transitions means quicker decision making means tougher AI.
         */

        void transition(int _state, int _moving, int n, int r) // n = at skill 0, n/2 = at skill 10, r = added random factor
        {
            monsterstate = _state;
            if(canmove) move = _moving;
            n = n*130/100;
            trigger = lastmillis+n-skill*(n/16)+rnd(r+1);
        }

        void burst(bool on)
        {
            if(on)
            {
                bursting = lastmillis;
                crouching = -1;
            }
            else
            {
                bursting = shots = 0;
                crouching = 1;
            }
        }

        void emitattacksound()
        {
            int attacksound = monstertypes[mtype].attacksound;
            if (validsound(attacksound))
            {
                playsound(attacksound, this); // battle cry: announcing the attack
            }
        }

        void alert(bool on)
        {
            int alertsound = on ? monstertypes[mtype].haltsound : monstertypes[mtype].unhaltsound;
            if (validsound(alertsound))
            {
                playsound(alertsound, this);
            }
            halted = on;
        }

        void monsteraction(int curtime) // main AI thinking routine, called every frame for every monster
        {
            if(enemy->state==CS_DEAD)
            {
                enemy = self;
                anger = 0;
            }
            normalize_yaw(targetyaw);
            if(targetyaw>yaw) // slowly turn monster towards his target
            {
                yaw += curtime*0.5f;
                if(targetyaw<yaw) yaw = targetyaw;
            }
            else
            {
                yaw -= curtime*0.5f;
                if(targetyaw>yaw) yaw = targetyaw;
            }
            float dist = enemy->o.dist(o);
            if(monsterstate!=MS_SLEEP) pitch = asin((enemy->o.z - o.z) / dist) / RAD;

            if(blocked) // special case: if we run into scenery
            {
                //blocked = false;
                if((lastmillis - lastunblocked) > 3000 || !rnd(20000/monstertypes[mtype].speed)) // try to jump over obstacle (rare)
                {
                    jumping = true;
                    lastunblocked = lastmillis;
                }
                else if(trigger<lastmillis && (monsterstate!=MS_HOME || !rnd(5))) // search for a way around (common)
                {
                    targetyaw += 90+rnd(180); // patented "random walk" AI path-finding (TM) ;)
                    transition(MS_SEARCH, 1, 100, 1000);
                }
            }
            else
            {
                lastunblocked = lastmillis;
            }

            float enemyyaw = -atan2(enemy->o.x - o.x, enemy->o.y - o.y)/RAD;

            int meleeatk = monstertypes[mtype].meleeatk;
            bool meleerange = validatk(meleeatk) && dist <= attacks[meleeatk].range;

            vec target = vec(0, 0, 0);
            orient = headpos();

            switch(monsterstate)
            {
                case MS_PAIN:
                case MS_ATTACKING:
                case MS_SEARCH:
                {
                    if(trigger<lastmillis && canmove) transition(MS_HOME, 1, 100, 200);
                    if(!halted && monsterstate == MS_SEARCH && raycubelos(o, enemy->o, target))
                    {
                        alert(true);
                    }
                    burst(false); // reset burst shots and rage status
                    break;
                }

                case MS_SLEEP: // state classic monsters start in, wait for visual contact
                {
                    if(editmode || !canmove) break;
                    normalize_yaw(enemyyaw);
                    float angle = (float)fabs(enemyyaw-yaw);
                    if(dist<128 // the better the angle to the player, the further the monster can see/hear
                    ||(dist<256 && angle<135)
                    ||(dist<512 && angle<90)
                    ||(dist<1024 && angle<45)
                    || angle<10
                    || (monsterhurt && o.dist(monsterhurtpos)<512))
                    {
                        if(raycubelos(o, enemy->o, target))
                        {
                            transition(MS_HOME, 1, 500, 200);
                            alert(true);
                        }
                    }
                    break;
                }

                case MS_AIMING: // this state is the delay between wanting to shoot and actually firing
                {
                    bool burstfire = monstertypes[mtype].burstshots > 0 && !meleerange;

                    if(burstfire)
                    {
                        if(raycubelos(o, enemy->o, target)) targetyaw = enemyyaw;
                        if(gunwait) break;
                        if(!bursting)
                        {
                            burst(true);
                            emitattacksound();
                        }
                        if(lastmillis - bursting < 1500) break; // delay before starting to burst!
                    }

                    if(trigger < lastmillis)
                    {
                        int atk = monstertypes[mtype].atk;
                        if(!burstfire || (burstfire && bursting))
                        {
                            ai::findorientation(orient, yaw, pitch, attacktarget);
                            if(attacktarget.dist(o) <= attacks[atk].exprad) goto stopfiring;
                            lastaction = 0;
                            if(meleerange && attacks[atk].action != ACT_MELEE) atk = meleeatk;
                            attacking = attacks[atk].action;
                            shoot(this, attacktarget);

                            if(burstfire) shots++;
                            bool burstcomplete = shots >= monstertypes[mtype].burstshots;
                            if(!burstfire || (burstfire && burstcomplete))
                            {
                                if(!burstfire && atk != meleeatk) emitattacksound();
                                goto stopfiring;
                            }
                        }
                        break;
                        stopfiring: transition(MS_ATTACKING, 0, 600, 0); burst(false);
                    }
                    break;
                }

                case MS_HOME: // monster has visual contact, heads straight for player and may want to shoot at any time
                {
                    if(!detonating) targetyaw = enemyyaw;
                    if(trigger<lastmillis)
                    {
                        if(!raycubelos(o, enemy->o, target)) // no visual contact anymore, let monster gets as close as possible then search for the player
                        {
                            transition(MS_HOME, 1, 800, 500);
                            if(halted)
                            {
                                alert(false);
                            }
                            if(monstertypes[mtype].isexplosive)
                            {
                                if(health <= monstertypes[mtype].health / 2)
                                {
                                    preparedetonation();
                                }
                            }
                        }
                        else if(!exploding && !detonating)
                        {
                            bool melee = false, longrange = false;
                            switch(monstertypes[mtype].atk)
                            {
                                case ATK_MELEE: case ATK_MELEE2: melee = true; break;
                                case ATK_RAIL1: longrange = true; break;
                            }
                            if(meleerange) melee = true;
                            // the closer the monster is the more likely he wants to shoot
                            if((!melee || dist<20) && !rnd(longrange ? (int)dist/12+1 : min((int)dist/12+1,6)) && enemy->state==CS_ALIVE)  // get ready to fire
                            {
                                ai::findorientation(orient, yaw, pitch, attacktarget);
                                transition(MS_AIMING, 0, monstertypes[mtype].lag, 10);
                            }
                            else // track player some more
                            {
                                transition(MS_HOME, 1, monstertypes[mtype].rate, 0);
                            }
                        }
                    }
                    break;
                }

            }

            if(move || maymove() || (stacked && (stacked->state!=CS_ALIVE || stackpos != stacked->o)))
            {
                vec pos = feetpos();
                loopv(teleports) // equivalent of player entity touch, but only teleports are used
                {
                    entity &e = *entities::ents[teleports[i]];
                    float dist = e.o.dist(pos);
                    if(dist<16) entities::teleport(teleports[i], this);
                }

                if(physics::physsteps > 0) stacked = NULL;
                physics::moveplayer(this, 1, true); // use physics to move monster
                physics::crouchplayer(this, 1, true);
            }
        }

        void preparedetonation()
        {
            if(exploding) return;
            exploding = true;
            speed += monstertypes[mtype].speedbonus; // increase movement to get to the player and explode in their face faster
            int haltsound = monstertypes[mtype].haltsound;
            if (validsound(haltsound))
            {
                playsound(haltsound, this);
            }
        }

        void detonate()
        {
            if(detonating) return;
            detonating = lastmillis;
            exploding = false;
            move = strafe = 0;
            playsound(S_WEAPON_DETONATE, this);
        }

        void monsterdeath(bool forceexplosion = false, int flags = 0)
        {
            state = CS_DEAD;
            lastpain = lastmillis;
            exploding = false;
            detonating = 0;
            stopownersounds(this);
            if(gibbed() || forceexplosion)
            {
                if(!gibbed()) health = -50;
                if(gore) gibeffect(max(-health, 0), vel, this);
                int atk = monstertypes[mtype].atk;
                // if(monstertypes[mtype].isexplosive) game::explode(true, this, o, vel, NULL, attacks[atk].damage, atk, true);
            }
            else if (!(flags & HIT_HEAD))
            {
                int diesound = monstertypes[mtype].diesound;
                if (validsound(diesound))
                {
                    playsound(diesound, this);
                }
            }
            monsterkilled(flags & HIT_HEAD ? KILL_HEADSHOT : 0);
        }

        void monsterpain(int damage, gameent *d, int atk, int flags)
        {
            monster *m = (monster *)d;
            if(d->type == ENT_AI) // a monster hit us
            {
                if(monstertypes[mtype].isefficient && mtype == m->mtype) return; // efficient monsters don't hurt themselves
                if(this != d) // guard for RL guys shooting themselves :)
                {
                    anger++; // don't attack straight away, first get angry
                    int _anger = d->type == ENT_AI && mtype == m->mtype ? anger / 2 : anger;
                    if(_anger>=monstertypes[mtype].loyalty)
                    {
                        enemy = d; // monster infight if very angry
                        checkefficientenemy(this, enemy);
                    }
                }
                else if(monstertypes[mtype].isexplosive) return;
            }
            else if(d->type == ENT_PLAYER) // player hit us
            {
                anger = 0;
                enemy = d;
                monsterhurt = true;
                monsterhurtpos = o;
            }
            health -= damage;
            if(health <= 0 || (m_insta(mutators) && d->type != ENT_AI))
            {
                if(atk == ATK_PISTOL_COMBO) deathtype = DEATH_DISRUPT;
                monsterdeath(m_insta(mutators), flags);
            }
            else
            {
                if(!exploding) // if the monster is in kamikaze mode, ignore the pain
                {
                    if(!bursting) transition(MS_PAIN, 0, monstertypes[mtype].pain, 200); // in this state monster won't attack
                    if(health > 0 && lastmillis - lastyelp > 600)
                    {
                        int painsound = monstertypes[mtype].painsound;
                        if (validsound(painsound))
                        {
                            playsound(painsound, this);
                        }
                        lastyelp = lastmillis;
                    }
                }
            }
            if(!bursting) lastpain = lastmillis;
        }
    };

    int getbloodcolor(dynent *d)
    {
        if(d->type == ENT_AI)
        {
            return monstertypes[((monster *)d)->mtype].bloodcolor;
        }
        return getplayermodelinfo((gameent *)d).bloodcolor;
    }

    void stackmonster(monster *d, physent *o)
    {
        d->stacked = o;
        d->stackpos = o->o;
    }

    int nummonsters(int tag, int state)
    {
        int n = 0;
        loopv(monsters) if(monsters[i]->tag==tag && (monsters[i]->state==CS_ALIVE ? state!=1 : state>=1)) n++;
        return n;
    }
    ICOMMAND(nummonsters, "ii", (int *tag, int *state), intret(nummonsters(*tag, *state)));

    void preloadmonsters()
    {
        loopi(NUMMONSTERS)
        {
            preloadmodel(monstertypes[i].mdlname);
            if(monstertypes[i].worldgunmodel) preloadmodel(monstertypes[i].worldgunmodel);
        }
    }

    vector<monster *> monsters;

    int nextmonster, spawnremain, numkilled, monstertotal, mtimestart, remain;

    void spawnmonster() // spawn a random monster according to freq distribution in DMSP
    {
        int n = rnd(TOTMFREQ), type;
        for(int i = 1; ; i++)
        {
            if((n -= monstertypes[i].freq)<0)
            {
                type = i;
                break;
            }
        }
        monsters.add(new monster(type, rnd(360), 0, true, MS_SEARCH, 1000, 1));
    }

    void healmonsters()
    {
        loopv(monsters)
        {
            if(monsters[i]->state==CS_ALIVE)
            {
                // heal monsters when player dies
                monster *m = monsters[i];
                m->health = min(m->health + monstertypes[m->mtype].healthbonus, monstertypes[m->mtype].health);
                if(m->detonating) m->detonating = 0; // reset explosion timer
            }
        }
    }

    void clearmonsters() // called after map start or when toggling edit mode to reset/spawn all monsters to initial state
    {
        removetrackedparticles();
        removetrackeddynlights();
        loopv(monsters) delete monsters[i];
        cleardynentcache();
        monsters.shrink(0);
        numkilled = 0;
        monstertotal = 0;
        spawnremain = 0;
        remain = 0;
        monsterhurt = false;
        if(m_invasion)
        {
            nextmonster = mtimestart = lastmillis+10000;
            monstertotal = spawnremain = skill*10;
        }
        else if(m_story || m_edit)
        {
            mtimestart = lastmillis;
            loopv(entities::ents)
            {
                extentity &e = *entities::ents[i];
                if(e.type != TARGET) continue;
                monster *m = new monster(e.attr1, e.attr2, e.attr3, e.attr4, MS_SLEEP, 100, 0);
                monsters.add(m);
                m->o = e.o;
                entinmap(m);
                updatedynentcache(m);
                monstertotal++;
            }
        }
        teleports.setsize(0);
        loopv(entities::ents) if(entities::ents[i]->type==TELEPORT) teleports.add(i);
    }

    void endsp(bool allkilled)
    {
        conoutf(CON_GAMEINFO, "\f2You have cleared the map!");
        monstertotal = 0;
        forceintermission();
    }
    ICOMMAND(endsp, "", (), endsp(false));

    void monsterkilled(int flags)
    {
        if(flags) checkannouncements(self, flags);
        if(!m_invasion && !m_story) return;
        numkilled++;
        self->frags = numkilled;
        remain = monstertotal-numkilled;
        if(remain>0 && remain<=5) conoutf(CON_GAMEINFO, "\f2%d monster(s) remaining", remain);
        if(remain == 5 || remain == 1)
        {
            playsound(remain == 5 ? S_ANNOUNCER_5_KILLS : S_ANNOUNCER_1_KILL, NULL, NULL, NULL, SND_ANNOUNCER);
        }
    }

    void checkefficientenemy(gameent *that, gameent *enemy)
    {
        monster *m = (monster *)that;
        bool issameenemy = false;
        if(monstertypes[m->mtype].isefficient) issameenemy = true;
        loopv(monsters)
        {
            if(!monstertypes[monsters[i]->mtype].isefficient) continue;
            if(issameenemy)
            {
                monsters[i]->enemy = enemy;
                continue;
            }
            m->enemy = that;
        }
    }

    void updatemonsters(int curtime)
    {
        if(m_invasion && spawnremain && lastmillis>nextmonster)
        {
            if(spawnremain--==monstertotal)
            {
                conoutf(CON_GAMEINFO, "\f2The invasion has begun!");
                playsound(S_INFECTION);
            }
            nextmonster = lastmillis+2000;
            spawnmonster();
        }

        if(m_invasion && killsendsp && monstertotal && !spawnremain && numkilled==monstertotal) endsp(true);

        bool monsterwashurt = monsterhurt;

        loopv(monsters)
        {
            monster *m = monsters[i];
            if(m->state==CS_ALIVE)
            {
                m->monsteraction(curtime);
                if(lastmillis - m->lastaction >= m->gunwait) m->gunwait = 0;
                if(m->exploding)
                {
                    regular_particle_flame(PART_FLAME, m->o, 6.5f, 1.5f, 0x903020, 1, 2.0f);
                    regular_particle_flame(PART_SMOKE, m->o, 5.0f, 2.5f, 0x303020, 2, 4.0f, 100.0f);
                    int atk = monstertypes[m->mtype].atk;
                    if(m->enemy->state == CS_ALIVE && m->o.dist(m->enemy->o) <= attacks[atk].exprad)  // close enough to the enemy
                    {
                        m->detonate();
                    }
                }
                if(m->detonating)
                {
                    if(lastmillis - m->detonating >= MONSTER_DETONATION_DELAY)
                    {
                        m->monsterdeath(true); // detonate monster through regular death with forced gore/explosion
                    }
                }
            }
            else if(m->state==CS_DEAD)
            {
                if(lastmillis - m->lastpain < 2000)
                {
                    m->move = m->strafe = 0;
                    physics::moveplayer(m, 1, true);
                }
                if(m->ragdoll) moveragdoll(m);
            }
            else if(m->ragdoll) cleanragdoll(m);
        }

        if(monsterwashurt) monsterhurt = false;
    }

    void rendermonsters()
    {
        loopv(monsters)
        {
            monster &m = *monsters[i];
            if(m.gibbed()) continue;
            if(m.state != CS_DEAD || lastmillis-m.lastpain<10000)
            {
                modelattach a[4];
                int ai = 0;
                a[ai++] = modelattach("tag_weapon", monstertypes[m.mtype].worldgunmodel, ANIM_VWEP_IDLE|ANIM_LOOP, 0);
                if(m.state == CS_ALIVE)
                {
                    a[ai++] = modelattach("tag_head", &m.head);
                    a[ai++] = modelattach("tag_muzzle", &m.muzzle);
                }
                float fade = 1;
                if(m.state==CS_DEAD) fade -= clamp(float(lastmillis - (m.lastpain + 9000))/1000, 0.0f, 1.0f);
                int attackanimation = 0;
                if(m.monsterstate == MS_ATTACKING || m.bursting)
                {
                    if(m.attacking > ACT_MELEE) attackanimation = ANIM_SHOOT;
                    else attackanimation = ANIM_MELEE;
                }
                renderai(&m, monstertypes[m.mtype].mdlname, a, 0, -attackanimation, 300, m.lastaction, m.lastpain, fade, monstertypes[m.mtype].hasragdoll);
            }
        }
    }

    void suicidemonster(monster *m)
    {
        m->deathtype = mapdeath;
        m->monsterdeath();
    }

    void hitmonster(int damage, monster *m, gameent *at, int atk, int flags)
    {
        m->monsterpain(damage, at, atk, flags);
    }

    void spsummary(int accuracy)
    {
        int pen, score = 0;
        pen = ((lastmillis-maptime)*100)/game::scaletime(1000); score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2Time taken: %d seconds (%d simulated seconds)", pen, (lastmillis-maptime)/1000);
        pen = self->deaths*60; score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2Time penalty for %d deaths (1 minute each): %d seconds", self->deaths, pen);
        pen = remain*10;          score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2Time penalty for %d monsters remaining (10 seconds each): %d seconds", remain, pen);
        pen = (10-skill)*20;      score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2Time penalty for lower skill level (20 seconds each): %d seconds", pen);
        pen = 100-accuracy;       score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2Time penalty for missed shots (1 second each %%): %d seconds", pen);
        defformatstring(aname, "bestscore_%s", getclientmap());
        const char *bestsc = getalias(aname);
        int bestscore = *bestsc ? parseint(bestsc) : score;
        if(score<bestscore) bestscore = score;
        defformatstring(nscore, "%d", bestscore);
        alias(aname, nscore);
        conoutf(CON_GAMEINFO, "\f2Total score (time + time penalties): %d seconds (best so far: %d seconds)", score, bestscore);
    }
}

