/*
 * =====================================================================
 * projectile.cpp
 * Manages the creation and behavior of projectiles in the game.
 * =====================================================================
 *
 * Projectiles include:
 * - Rockets and grenades fired by weapons to attack players.
 * - Objects affected by physics, such as debris or giblets.
 *
 * Each projectile type has unique properties and behaviors:
 * - Trajectory and movement patterns.
 * - Interaction with physics, such as bouncing or sticking to surfaces.
 * - Specific effects on impact or over time (e.g. explosions/particle effects).
 * - Health and countdown values that determine durability and lifespan.
 * - Potential for players to pick them up.
 *
 * Projectiles are implemented using a specialized structure.
 * This is based on the "physent" entity, from which all dynamic entities are derived.
 * This allows for precise control over their dynamics and interactions in the game world,
 * considering that physics do not impact every entity in the same way.
 *
 * =====================================================================
 */

#include "game.h"

namespace game
{
    vector<projectile *> projectiles;

    void makeprojectile(gameent *owner, const vec &from, const vec &to, bool islocal, int id, int atk, int type, int lifetime, int speed, float gravity, float elasticity)
    {
        projectile &proj = *projectiles.add(new projectile);
        proj.owner = owner;
        proj.projtype = type;
        proj.setflags();
        proj.o = from;
        proj.from = from;
        proj.to = to;
        proj.setradius();
        proj.islocal = islocal;
        proj.id = islocal ? lastmillis : id;
        proj.atk = atk;
        proj.lifetime = lifetime;
        proj.speed = speed;
        proj.gravity = gravity;
        proj.elasticity = elasticity;

        proj.setvariant();

        vec dir(to);
        dir.sub(from).safenormalize();
        proj.vel = dir;
        if(proj.flags & ProjFlag_Bounce) proj.vel.mul(speed);

        avoidcollision(&proj, dir, owner, 0.1f);

        if (proj.flags & ProjFlag_Weapon)
        {
            proj.offset = hudgunorigin(attacks[proj.atk].gun, from, to, owner);
        }
        if(proj.flags & ProjFlag_Bounce)
        {
            if (proj.flags & ProjFlag_Weapon)
            {
                if (owner == hudplayer() && !isthirdperson())
                {
                    proj.offset.sub(owner->o).rescale(16).add(owner->o);
                }
            }
            else proj.offset = from;
        }

        vec o = proj.flags & ProjFlag_Bounce ? proj.o : from;
        proj.offset.sub(o);

        proj.offsetmillis = OFFSETMILLIS;

        if(proj.flags & ProjFlag_Bounce) proj.resetinterp();

        proj.lastposition = owner->o;

        proj.checkliquid();
        proj.setsounds();
    }

    extern int blood;

    void applybounceeffects(projectile* proj, vec surface)
    {
        if (proj->inwater) return;

        if (proj->vel.magnitude() > 5.0f)
        {
            if (validsound(proj->bouncesound))
            {
                playsound(proj->bouncesound, NULL, &proj->o, NULL, 0, 0, 0, -1);
            }
        }
        if (proj->projtype == Projectile_Rocket2)
        {
            particle_splash(PART_SPARK2, 20, 150, proj->o, 0xFFC864, 0.3f, 250, 1);
        }
        if (blood && proj->projtype == Projectile_Gib)
        {
            addstain(STAIN_BLOOD, vec(proj->o).sub(vec(surface).mul(proj->radius)), surface, 2.96f / proj->bounces, getbloodcolor(proj->owner), rnd(4));
        }
    }

    void bounce(physent* d, const vec& surface)
    {
        if (d->type != ENT_PROJECTILE) return;

        projectile* proj = (projectile*)d;
        proj->bounces++;

        int maxbounces = projs[proj->projtype].maxbounces;
        if ((maxbounces && proj->bounces > maxbounces) || lastmillis - proj->lastbounce < 100) return;

        applybounceeffects(proj, surface);
        proj->lastbounce = lastmillis;
    }

    void collidewithentity(physent* bouncer, physent* collideentity)
    {
        projectile* proj = (projectile*)bouncer;
        proj->isdirect = true;
    }

    float projectiledistance(dynent* o, vec& dir, const vec& v, const vec& vel)
    {
        vec middle = o->o;
        middle.z += (o->aboveeye - o->eyeheight) / 2;
        dir = vec(middle).sub(v).add(vec(vel).mul(5)).safenormalize();

        float low = min(o->o.z - o->eyeheight + o->radius, middle.z),
            high = max(o->o.z + o->aboveeye - o->radius, middle.z);
        vec closest(o->o.x, o->o.y, clamp(v.z, low, high));
        return max(closest.dist(v) - o->radius, 0.0f);
    }

    void stain(vec dir, const vec& pos, int atk)
    {
        vec negdir = vec(dir).neg();
        float radius = attacks[atk].exprad * 0.75f;
        addstain(STAIN_PULSE_SCORCH, pos, negdir, radius);
        if (lookupmaterial(pos) & MAT_WATER) return; // no glow in water
        int gun = attacks[atk].gun;
        if (gun != GUN_ROCKET)
        {
            int color = 0x00FFFF;
            if (gun == GUN_PULSE) color = 0xEE88EE;
            else if (gun == GUN_GRENADE) color = 0x74BCF9;
            addstain(STAIN_PULSE_GLOW, pos, negdir, radius / (gun == GUN_GRENADE ? 2 : 1), color);
        }
    }

    bool candealdamage(dynent* o, projectile& proj, const vec& v, int damage)
    {
        if (betweenrounds || o->state != CS_ALIVE) return false;
        if (!isintersecting(o, proj.o, v, attacks[proj.atk].margin)) return false;
        if (isweaponprojectile(proj.projtype))
        {
            vec dir;
            projectiledistance(o, dir, v, proj.vel);
            gameent* f = (gameent*)o;
            int cdamage = calcdamage(damage, f, proj.owner, proj.atk);
            int flags = HIT_TORSO | HIT_DIRECT;
            registerhit(cdamage, o, proj.owner, o->o, dir, proj.atk, 0, 1, flags);
        }
        return true;
    }

    VARP(maxdebris, 10, 60, 1000);

    void addexplosioneffects(gameent* owner, int atk, vec v)
    {
        playsound(attacks[atk].impactsound, NULL, &v);
        vec dynlight = vec(1.0f, 3.0f, 4.0f);
        int explosioncolor = 0x50CFE5, explosiontype = PART_EXPLOSION1;
        bool iswater = (lookupmaterial(v) & MATF_VOLUME) == MAT_WATER;
        switch (atk)
        {
            case ATK_ROCKET1:
            case ATK_ROCKET2:
            {
                explosioncolor = 0xC8E66B;
                dynlight = vec(0.5f, 0.375f, 0.25f);
                if (iswater) break;
                explosiontype = PART_EXPLOSION3;
                particle_splash(PART_EXPLODE, 30, 180, v, 0xF3A612, 6.0f + rndscale(9.0f), 180, 50);
                particle_splash(PART_SPARK2, 100, 250, v, 0xFFC864, 0.10f + rndscale(0.50f), 600, 1);
                particle_splash(PART_SMOKE, 50, 280, v, 0x444444, 10.0f, 250, 200);
                break;
            }
            case ATK_PULSE1:
            {
                explosioncolor = 0xEE88EE;
                if (iswater)
                {
                    particle_flare(v, v, 280, PART_ELECTRICITY, explosioncolor, 12.0f);
                    return;
                }
                dynlight = vec(1.0f, 0.50f, 1.0f);
                explosiontype = PART_EXPLOSION2;
                particle_splash(PART_SPARK2, 5 + rnd(20), 200, v, explosioncolor, 0.08f + rndscale(0.35f), 400, 2);
                particle_splash(PART_EXPLODE, 30, 80, v, explosioncolor, 1.5f + rndscale(2.8f), 120, 40);
                particle_splash(PART_SMOKE, 60, 180, v, 0x222222, 2.5f + rndscale(3.8f), 120, 60);
                break;
            }
            case ATK_GRENADE1:
            case ATK_GRENADE2:
            {
                explosioncolor = 0x74BCF9;
                dynlight = vec(0, 0.25f, 1.0f);
                if (iswater) break;
                explosiontype = PART_EXPLOSION2;
                particle_flare(v, v, 280, PART_ELECTRICITY, explosioncolor, 30.0f);
                break;
            }
            case ATK_PISTOL2:
            case ATK_PISTOL_COMBO:
            {
                explosioncolor = 0x00FFFF;
                if (atk == ATK_PISTOL2 && iswater)
                {
                    particle_flare(v, v, 280, PART_ELECTRICITY, explosioncolor, 12.0f);
                    return;
                }
                dynlight = vec(0.25f, 1.0f, 1.0f);
                particle_fireball(v, 1.0f, PART_EXPLOSION2, atk == ATK_PISTOL2 ? 200 : 500, 0x00FFFF, attacks[atk].exprad);
                particle_splash(PART_SPARK2, 50, 180, v, 0x00FFFF, 0.18f, 380);
                break;
            }
            default: break;
        }
        particle_fireball(v, 1.15f * attacks[atk].exprad, explosiontype, atk == ATK_GRENADE1 || atk == ATK_GRENADE2 ? 200 : 400, explosioncolor, 0.10f);
        adddynlight(v, 2 * attacks[atk].exprad, dynlight, 350, 40, 0, attacks[atk].exprad / 2, vec(0.5f, 1.5f, 2.0f));
        if (!iswater) // no debris in water
        {
            int numdebris = rnd(maxdebris - 5) + 5;
            vec debrisvel = vec(owner->o).sub(v).safenormalize(), debrisorigin(v);
            if (atk == ATK_ROCKET1)
            {
                debrisorigin.add(vec(debrisvel).mul(8));
            }
            if (numdebris && (attacks[atk].gun == GUN_ROCKET || attacks[atk].gun == GUN_SCATTER))
            {
                loopi(numdebris)
                {
                    spawnbouncer(debrisorigin, owner, Projectile_Debris);
                }
            }
        }
    }

    void applyradialeffect(dynent* o, const vec& v, const vec& vel, int damage, gameent* at, int atk, bool isdirect)
    {
        if (o->state != CS_ALIVE) return;
        vec dir;
        float dist = projectiledistance(o, dir, v, vel);
        if (dist < attacks[atk].exprad)
        {
            float radiusdamage = damage * (1 - dist / EXP_DISTSCALE / attacks[atk].exprad), damage = calcdamage(radiusdamage, (gameent*)o, at, atk);
            int flags = HIT_TORSO;
            if (isdirect) flags |= HIT_DIRECT;
            registerhit(damage, o, at, o->o, dir, atk, dist, 1, flags);
        }
    }

    void explodeprojectile(projectile& proj, const vec& v, dynent* safe, int damage, bool islocal)
    {
        stain(proj.vel, proj.flags & ProjFlag_Linear ? v : proj.offsetposition(), proj.atk);
        vec pos = proj.flags & ProjFlag_Linear ? v : proj.o;
        vec offset = proj.flags & ProjFlag_Linear ? v : proj.offsetposition();
        addexplosioneffects(proj.owner, proj.atk, pos);

        if (betweenrounds || !islocal) return;

        int numdyn = numdynents();
        loopi(numdyn)
        {
            dynent* o = iterdynents(i);
            if (o->o.reject(pos, o->radius + attacks[proj.atk].exprad) || o == safe) continue;
            applyradialeffect(o, pos, proj.vel, damage, proj.owner, proj.atk, proj.isdirect);
        }
    }

    void explodeeffects(int atk, gameent* d, bool islocal, int id)
    {
        if (islocal) return;
        switch (atk)
        {
            case ATK_ROCKET2:
            case ATK_GRENADE1:
            case ATK_GRENADE2:
            case ATK_PULSE1:
            case ATK_ROCKET1:
            case ATK_PISTOL2:
            case ATK_PISTOL_COMBO:
            {
                loopv(projectiles)
                {
                    projectile& proj = *projectiles[i];
                    if (proj.owner == d && proj.id == id && !proj.islocal)
                    {
                        if (atk == ATK_PISTOL_COMBO) proj.atk = atk;
                        else if (proj.atk != atk) continue;
                        vec pos = proj.flags & ProjFlag_Bounce ? proj.offsetposition() : vec(proj.offset).mul(proj.offsetmillis / float(OFFSETMILLIS)).add(proj.o);
                        explodeprojectile(proj, pos, NULL, 0, proj.islocal);
                        delete projectiles.remove(i);
                        break;
                    }
                }
                break;
            }
            default: break;
        }
    }

    void handleliquidtransitions(projectile& proj)
    {
        bool isinwater = ((lookupmaterial(proj.o) & MATF_VOLUME) == MAT_WATER);
        if (isinwater && projs[proj.projtype].flags & ProjFlag_Quench)
        {
            proj.isdestroyed = true;
        }
        int transition = physics::liquidtransition(&proj, lookupmaterial(proj.o), isinwater);
        if (transition > 0)
        {
            particle_splash(PART_WATER, 200, 250, proj.o, 0xFFFFFF, 0.09f, 500, 1);
            particle_splash(PART_SPLASH, 10, 80, proj.o, 0xFFFFFF, 7.0f, 250, -1);
            if (transition == LiquidTransition_In)
            {
                playsound(S_IMPACT_WATER_PROJ, NULL, &proj.o);
            }
            proj.lastposition = proj.o;
        }
    }

    void checkloopsound(projectile* proj)
    {
        if (!validsound(proj->loopsound)) return;
        if (!proj->isdestroyed)
        {
            proj->loopchan = playsound(proj->loopsound, NULL, &proj->o, NULL, 0, -1, 100, proj->loopchan);
        }
        else
        {
            stopsound(proj->loopsound, proj->loopchan);
        }
    }

    void addprojectileeffects(projectile* proj, vec pos)
    {
        int tailc = 0xFFFFFF, tails = 2.0f;
        bool hasenoughvelocity = proj->vel.magnitude() > 50.0f;
        if (proj->inwater)
        {
            if (hasenoughvelocity || proj->flags & ProjFlag_Linear)
            {
                regular_particle_splash(PART_BUBBLE, 1, 200, pos, 0xFFFFFF, 1.0f, 8, 50, 1);
            }
            return;
        }
        else switch (proj->projtype)
        {
            case Projectile_Grenade:
            case Projectile_Grenade2:
            {
                if (hasenoughvelocity) regular_particle_splash(PART_RING, 1, 200, pos, 0x74BCF9, 1.0f, 1, 500);
                if (proj->projtype == Projectile_Grenade2)
                {
                    if (proj->lifetime < attacks[proj->atk].lifetime - 100) particle_flare(proj->lastposition, pos, 500, PART_TRAIL_STRAIGHT, 0x74BCF9, 0.4f);
                }
                proj->lastposition = pos;
                break;
            }

            case Projectile_Rocket:
            {
                tailc = 0xFFC864; tails = 1.5f;
                if (proj->lifetime <= attacks[proj->atk].lifetime / 2)
                {
                    tails *= 4;
                }
                else regular_particle_splash(PART_SMOKE, 3, 300, pos, 0x303030, 2.4f, 50, -20);
                particle_flare(pos, pos, 1, PART_MUZZLE_FLASH3, tailc, 1.0f + rndscale(tails * 2));
                break;
            }

            case Projectile_Rocket2:
            {
                if (hasenoughvelocity) regular_particle_splash(PART_SMOKE, 5, 200, pos, 0x555555, 1.60f, 10, 500);
                if (proj->lifetime < attacks[proj->atk].lifetime - 100)
                {
                    particle_flare(proj->lastposition, pos, 500, PART_TRAIL_STRAIGHT, 0xFFC864, 0.4f);
                }
                proj->lastposition = pos;
                break;
            }

            case Projectile_Pulse:
            {
                tailc = 0xDD88DD;
                particle_flare(pos, pos, 1, PART_ORB, tailc, 1.0f + rndscale(tails));
                break;
            }

            case Projectile_Plasma:
            {
                tails = 6.0f; tailc = 0x00FFFF;
                particle_flare(pos, pos, 1, PART_ORB, tailc, proj->owner == self || proj->owner->type == ENT_AI ? tails : tails - 2.0f);
                break;
            }

            case Projectile_Gib:
            {
                if (blood && hasenoughvelocity)
                {
                    regular_particle_splash(PART_BLOOD, 0 + rnd(4), 400, pos, getbloodcolor(proj->owner), 0.80f, 25);
                }
                break;
            }

            case Projectile_Debris:
            {
                if (!hasenoughvelocity) break;
                regular_particle_splash(PART_SMOKE, 5, 100, pos, 0x555555, 1.80f, 30, 500);
                regular_particle_splash(PART_SPARK, 1, 40, pos, 0xF83B09, 1.20f, 10, 500);
                particle_flare(proj->o, proj->o, 1, PART_EDIT, 0xFFC864, 0.5 + rndscale(1.5f));
                break;
            }

            case Projectile_Bullet:
            {
                tailc = 0xFFC864; tails = 1.0f;
                if (attacks[proj->atk].gun == GUN_PISTOL) tailc = 0x00FFFF;
                else if (attacks[proj->atk].gun == GUN_RAIL) tailc = 0x77DD77;
                break;
            }
        }
        if (proj->flags & ProjFlag_Linear)
        {
            float len = min(80.0f, vec(proj->offset).add(proj->from).dist(pos));
            vec dir = vec(proj->dv).normalize(), tail = vec(dir).mul(-len).add(pos), head = vec(dir).mul(2.4f).add(pos);
            particle_flare(tail, head, 1, PART_TRAIL_PROJECTILE, tailc, tails);
        }
    }

    void checklifetime(projectile& proj, int time)
    {
        if (isweaponprojectile(proj.projtype))
        {
            if ((proj.lifetime -= time) < 0)
            {
                proj.isdestroyed = true;
            }
        }
        else if(proj.flags & ProjFlag_Junk)
        {
            // Cheaper variable rate physics for debris, gibs and other "junk" projectiles.
            for (int rtime = time; rtime > 0;)
            {
                int qtime = min(80, rtime);
                rtime -= qtime;
                if ((proj.lifetime -= qtime) < 0 || (proj.flags & ProjFlag_Bounce && physics::hasbounced(&proj, qtime / 1000.0f, 0.5f, 0.4f, 0.7f)))
                {
                    proj.isdestroyed = true;
                }
            }
        }
    }

    void handleprojectileeffects(projectile& proj, vec pos)
    {
        handleliquidtransitions(proj);
        vec v = proj.flags & ProjFlag_Bounce ? pos : vec(proj.offset).mul(proj.offsetmillis / float(OFFSETMILLIS)).add(pos);
        addprojectileeffects(&proj, v);
    }

    void updateprojectiles(int time)
    {
        if (projectiles.empty()) return;
        loopv(projectiles)
        {
            projectile& proj = *projectiles[i];

            vec pos = proj.updateposition(time);
            vec old(proj.o);

            if (proj.flags & ProjFlag_Linear)
            {
                hits.setsize(0);
                if (proj.islocal)
                {
                    vec halfdv = vec(proj.dv).mul(0.5f), bo = vec(proj.o).add(halfdv);
                    float br = max(fabs(halfdv.x), fabs(halfdv.y)) + 1 + attacks[proj.atk].margin;
                    if (!betweenrounds) loopj(numdynents())
                    {
                        dynent* o = iterdynents(j);
                        if (proj.owner == o || o->o.reject(bo, o->radius + br)) continue;
                        if (candealdamage(o, proj, pos, attacks[proj.atk].damage))
                        {
                            proj.isdestroyed = true;
                            proj.isdirect = true;
                            break;
                        }
                    }
                }
            }
            if (!proj.isdestroyed)
            {
                checklifetime(proj, time);
                if ((lookupmaterial(proj.o) & MATF_VOLUME) == MAT_LAVA)
                {
                    proj.isdestroyed = true;
                }
                if (proj.flags & ProjFlag_Linear)
                {
                    if (proj.flags & ProjFlag_Impact && proj.dist < 4)
                    {
                        if (proj.o != proj.to) // If original target was moving, re-evaluate endpoint.
                        {
                            if (raycubepos(proj.o, proj.vel, proj.to, 0, RAY_CLIPMAT | RAY_ALPHAPOLY) >= 4) continue;
                        }
                        proj.isdestroyed = true;
                    }
                }
                if (isweaponprojectile(proj.projtype))
                {
                    if (proj.flags & ProjFlag_Bounce)
                    {
                        bool isbouncing = physics::isbouncing(&proj, proj.elasticity, 0.5f, proj.gravity);
                        bool hasbounced = projs[proj.projtype].maxbounces && proj.bounces >= projs[proj.projtype].maxbounces;
                        if (!isbouncing || hasbounced)
                        {
                            proj.isdestroyed = true;
                        }
                    }
                }
                handleprojectileeffects(proj, pos);
            }
            checkloopsound(&proj);
            if (proj.isdestroyed)
            {
                if (isweaponprojectile(proj.projtype))
                {
                    explodeprojectile(proj, pos, NULL, attacks[proj.atk].damage, proj.islocal);
                    if (proj.islocal)
                    {
                        addmsg(N_EXPLODE, "rci3iv", proj.owner, lastmillis - maptime, proj.atk, proj.id - maptime, hits.length(), hits.length() * sizeof(hitmsg) / sizeof(int), hits.getbuf());
                    }
                }
                delete projectiles.remove(i--);
            }
            else
            {
                if (proj.flags & ProjFlag_Bounce)
                {
                    proj.roll += old.sub(proj.o).magnitude() / (4 * RAD);
                    proj.offsetmillis = max(proj.offsetmillis - time, 0);
                    proj.limitoffset();
                }
                else
                {
                    proj.o = pos;
                }
            }
        }
    }

    void spawnbouncer(const vec& from, gameent* d, int type)
    {
        vec to(rnd(100) - 50, rnd(100) - 50, rnd(100) - 50);
        float elasticity = 0.6f;
        if (type == Projectile_Eject)
        {
            to = vec(-50, 1, rnd(30) - 15);
            to.rotate_around_z(d->yaw * RAD);
            elasticity = 0.4f;
        }
        if (to.iszero()) to.z += 1;
        to.normalize();
        to.add(from);
        makeprojectile(d, from, to, true, 0, -1, type, type == Projectile_Debris ? 400 : rnd(1000) + 1000, rnd(100) + 20, 0.3f + rndscale(0.8f), elasticity);
    }

    bool scanprojectiles(vec &from, vec &to, gameent *d, int atk)
    {
        if(betweenrounds) return false;
        vec stepv;
        float dist = to.dist(from, stepv);
        int steps = clamp(int(dist * 2), 1, 200);
        stepv.div(steps);
        vec point = from;
        loopi(steps)
        {
            point.add(stepv);
            loopv(projectiles)
            {
                projectile &proj = *projectiles[i];
                if (proj.projtype != Projectile_Plasma || (d != proj.owner && proj.owner->type != ENT_AI)) continue;
                if (attacks[atk].gun == GUN_PISTOL && proj.o.dist(point) <= attacks[proj.atk].margin)
                {
                    proj.atk = ATK_PISTOL_COMBO;
                    explodeprojectile(proj, proj.o, NULL, attacks[proj.atk].damage, proj.islocal);
                    if(d == self || d->ai)
                    {
                        addmsg(N_EXPLODE, "rci3iv", proj.owner, lastmillis-maptime, proj.atk, proj.id-maptime,
                               hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
                    }
                    projectiles.remove(i--);
                    return true;
                }
            }
        }
        return false;
    }

    void adddynlights()
    {
        loopv(projectiles)
        {
            projectile &proj = *projectiles[i];
            vec pos(proj.o);
            pos.add(vec(proj.offset).mul(proj.offsetmillis/float(OFFSETMILLIS)));
            switch(proj.projtype)
            {
                case Projectile_Pulse:
                {
                    adddynlight(pos, 25, vec(2.0f, 1.5f, 2.0));
                    break;
                }

                case Projectile_Rocket:
                {
                    adddynlight(pos, 50, vec(2, 1.5f, 1), 0, 0, 0, 10, vec(0.5f, 0.375f, 0.25f));
                    break;
                }

                case Projectile_Plasma:
                {
                    adddynlight(pos, 20, vec(0, 1.50f, 1.50f));
                    break;
                }

                case Projectile_Grenade:
                case Projectile_Grenade2:
                {
                    adddynlight(pos, 8, vec(0.25f, 0.25f, 1));
                    break;
                }
            }
        }
    }

    static const char * const projectilenames[6] = { "projectile/grenade", "projectile/grenade", "projectile/rocket", "projectile/eject/01", "projectile/eject/02", "projectile/eject/03" };
    static const char * const gibnames[5] = { "projectile/gib/gib01", "projectile/gib/gib02", "projectile/gib/gib03", "projectile/gib/gib04", "projectile/gib/gib05" };

    void preloadprojectiles()
    {
        loopi(sizeof(projectilenames)/sizeof(projectilenames[0])) preloadmodel(projectilenames[i]);
        loopi(sizeof(gibnames)/sizeof(gibnames[0])) preloadmodel(gibnames[i]);
    }

    vec modelmanipulation(projectile& proj, float& yaw, float& pitch)
    {
        if (proj.flags & ProjFlag_Bounce)
        {
            vec pos = proj.offsetposition();
            vec vel(proj.vel);
            if (vel.magnitude() <= 25.0f)
            {
                yaw = proj.lastyaw;
            }
            else
            {
                vectoyawpitch(vel, yaw, pitch);
                yaw += 90;
                proj.lastyaw = yaw;
            }
            return pos;
        }
        else
        {
            float dist = min(proj.o.dist(proj.to) / 32.0f, 1.0f);
            vec pos = vec(proj.o).add(vec(proj.offset).mul(dist * proj.offsetmillis / float(OFFSETMILLIS)));
            vec v = dist < 1e-6f ? proj.vel : vec(proj.to).sub(pos).normalize();
            vectoyawpitch(v, yaw, pitch); // The amount of distance in front of the smoke trail needs to change if the model does.
            v.mul(3);
            v.add(pos);
            return v;
        }
    }

    void renderprojectiles()
    {
        float yaw, pitch;
        loopv(projectiles)
        {
            projectile& proj = *projectiles[i];
            const char* model = projs[proj.projtype].directory;
            if (proj.variant)
            {
                model = gibnames[proj.variant];
            }
            if (model == NULL || model[0] == '\0') continue;

            vec pos = modelmanipulation(proj, yaw, pitch);
            int cull = MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED;
            float fade = 1;
            if (proj.flags & ProjFlag_Junk)
            {
                if (proj.lifetime < 400) fade = proj.lifetime / 400.0f;
            }
            rendermodel(model, ANIM_MAPMODEL | ANIM_LOOP, pos, yaw, pitch, proj.roll, cull, NULL, NULL, 0, 0, fade);
        }
    }

    void removeprojectiles(gameent* owner)
    {
        if (!owner)
        {
            projectiles.deletecontents();
        }
        else
        {
            loopv(projectiles) if (projectiles[i]->owner == owner)
            {
                delete projectiles[i];
                projectiles.remove(i--);
            }
        }
    }

    void avoidprojectiles(ai::avoidset &obstacles, float radius)
    {
        loopv(projectiles)
        {
            projectile &proj = *projectiles[i];
            obstacles.avoidnear(NULL, proj.o.z + attacks[proj.atk].exprad + 1, proj.o, radius + attacks[proj.atk].exprad);
        }
    }

    void adddynamiclights()
    {
        loopv(projectiles)
        {
            projectile& proj = *projectiles[i];
            if (proj.flags & ProjFlag_Junk) continue;
            vec pos(proj.o);
            pos.add(vec(proj.offset).mul(proj.offsetmillis / float(OFFSETMILLIS)));
            switch (proj.projtype)
            {
                case Projectile_Pulse:
                {
                    adddynlight(pos, 25, vec(2.0f, 1.5f, 2.0));
                    break;
                }

                case Projectile_Rocket:
                case Projectile_Rocket2:
                {
                    adddynlight(pos, 50, vec(2, 1.5f, 1), 0, 0, 0, 10, vec(0.5f, 0.375f, 0.25f));
                    break;
                }

                case Projectile_Plasma:
                {
                    adddynlight(pos, 20, vec(0, 1.50f, 1.50f));
                    break;
                }
                case Projectile_Grenade:
                case Projectile_Grenade2:
                {
                    adddynlight(pos, 8, vec(0.25f, 0.25f, 1));
                    break;
                }
            }
        }
    }
};
