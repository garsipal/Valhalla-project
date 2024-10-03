/*
 * =====================================================================
 * weapon.cpp
 * Manage weapons and shooting effects.
 * =====================================================================
 *
 * This file implements the behavior and mechanics of weapons, such as:
 * - Weapon selection.
 * - Shooting mechanics and effects.
 * - Hit detection.
 * - Damage calculation.
 *
 * =====================================================================
 */

#include "game.h"

namespace game
{
    vector<hitmsg> hits;

    vec rays[GUN_MAXRAYS];

    ICOMMAND(getweapon, "", (), intret(self->gunselect));

    void playswitchsound(gameent* d, int gun)
    {
        if (validsound(guns[gun].switchsound))
        {
            playsound(guns[gun].switchsound, d);
        }
        else playsound(S_WEAPON_LOAD, d);
    }

    void gunselect(int gun, gameent *d)
    {
        if(gun == d->gunselect || lastmillis - d->lastswitch < 100)
        {
            return;
        }
        addmsg(N_GUNSELECT, "rci", d, gun);
        d->gunselect = gun;
        d->lastswitch = lastmillis;
        d->lastattack = -1;
        if(d == self) disablezoom();
        playswitchsound(d, gun);
    }

    void nextweapon(int dir, bool force = false)
    {
        if(self->state!=CS_ALIVE) return;
        dir = (dir < 0 ? NUMGUNS-1 : 1);
        int gun = self->gunselect;
        loopi(NUMGUNS)
        {
            gun = (gun + dir)%NUMGUNS;
            if(force || self->ammo[gun]) break;
        }
        if(gun != self->gunselect) gunselect(gun, self);
    }
    ICOMMAND(nextweapon, "ii", (int *dir, int *force), nextweapon(*dir, *force!=0));

    int getweapon(const char *name)
    {
        if(isdigit(name[0])) return parseint(name);
        else
        {
            int len = strlen(name);
            loopi(sizeof(guns)/sizeof(guns[0])) if(!strncasecmp(guns[i].name, name, len)) return i;
        }
        return -1;
    }

    void setweapon(const char *name, bool force = false)
    {
        int gun = getweapon(name);
        if(self->state!=CS_ALIVE || !validgun(gun)) return;
        if(force || self->ammo[gun]) gunselect(gun, self);
    }
    ICOMMAND(setweapon, "si", (char *name, int *force), setweapon(name, *force!=0));

    void cycleweapon(int numguns, int *guns, bool force = false)
    {
        if(numguns<=0 || self->state!=CS_ALIVE) return;
        int offset = 0;
        loopi(numguns) if(guns[i] == self->gunselect) { offset = i+1; break; }
        loopi(numguns)
        {
            int gun = guns[(i+offset)%numguns];
            if(gun>=0 && gun<NUMGUNS && (force || self->ammo[gun]))
            {
                gunselect(gun, self);
                return;
            }
        }
    }
    ICOMMAND(cycleweapon, "V", (tagval *args, int numargs),
    {
         int numguns = min(numargs, 3);
         int guns[3];
         loopi(numguns) guns[i] = getweapon(args[i].getstr());
         cycleweapon(numguns, guns);
    });

    void weaponswitch(gameent *d)
    {
        if(d->state!=CS_ALIVE) return;
        int s = d->gunselect;
        if(s!=GUN_SCATTER && d->ammo[GUN_SCATTER])
        {
            s = GUN_SCATTER;
        }
        else if(s!=GUN_SMG && d->ammo[GUN_SMG])
        {
            s = GUN_SMG;
        }
        else if(s!=GUN_PULSE && d->ammo[GUN_PULSE])
        {
            s = GUN_PULSE;
        }
        else if(s!=GUN_ROCKET && d->ammo[GUN_ROCKET])
        {
            s = GUN_ROCKET;
        }
        else if(s!=GUN_RAIL && d->ammo[GUN_RAIL])
        {
            s = GUN_RAIL;
        }
        else if(s!=GUN_GRENADE && d->ammo[GUN_GRENADE])
        {
            s = GUN_GRENADE;
        }
        gunselect(s, d);
    }

    ICOMMAND(weapon, "V", (tagval *args, int numargs),
    {
        if(self->state!=CS_ALIVE) return;
        loopi(3)
        {
            const char *name = i < numargs ? args[i].getstr() : "";
            if(name[0])
            {
                int gun = getweapon(name);
                if(validgun(gun) && gun != self->gunselect && self->ammo[gun]) { gunselect(gun, self); return; }
            } else { weaponswitch(self); return; }
        }
        playsound(S_WEAPON_NOAMMO);
    });

    void offsetray(const vec &from, const vec &to, int spread, float range, vec &dest, gameent *d)
    {
        vec offset;
        do offset = vec(rndscale(1), rndscale(1), rndscale(1)).sub(0.5f);
        while(offset.squaredlen() > 0.5f * 0.5f);
        offset.mul((to.dist(from) / 1024) * spread / (d->crouched() && d->crouching ? 1.5f : 1));
        offset.z /= 2;
        dest = vec(offset).add(to);
        if(dest != from)
        {
            vec dir = vec(dest).sub(from).normalize();
            raycubepos(from, dir, dest, range, RAY_CLIPMAT|RAY_ALPHAPOLY);
        }
    }

    VARP(blood, 0, 1, 1);
    VARP(goreeffect, 0, 0, 2);
    VARP(playheadshotsound, 0, 1, 1);

    void damageeffect(int damage, dynent *d, vec p, int atk, int color, bool headshot)
    {
        gameent *f = (gameent *)d, *hud = followingplayer(self);
        if(f == hud)
        {
            p.z += 0.6f*(d->eyeheight + d->aboveeye) - d->eyeheight;
        }
        if(f->haspowerup(PU_INVULNERABILITY) || f->shield)
        {
            particle_splash(PART_SPARK2, 100, 150, p, f->haspowerup(PU_INVULNERABILITY) ? getplayercolor(f, f->team) : 0xFFFF66, 0.50f);
            if(f->haspowerup(PU_INVULNERABILITY))
            {
                playsound(S_ACTION_INVULNERABILITY, f);
                return;
            }
        }
        if(blood && color != -1)
        {
            particle_splash(PART_BLOOD, damage/10, 1000, p, color, 2.60f);
            particle_splash(PART_BLOOD2, 200, 250, p, color, 0.50f);
        }
        else
        {
            particle_flare(p, p, 100, PART_MUZZLE_FLASH3, 0xFFFF66, 3.5f);
            particle_splash(PART_SPARK2, damage/5, 500, p, 0xFFFF66, 0.5f, 300);
        }
        if(f->health > 0 && lastmillis-f->lastyelp > 600)
        {
            if(f != hud && f->shield) playsound(S_SHIELD_HIT, f);
            if(f->type == ENT_PLAYER)
            {
                int painsound = getplayermodelinfo(f).painsound;
                if (validsound(painsound))
                {
                    playsound(painsound, f);
                }
                f->lastyelp = lastmillis;
            }
        }
        if(validatk(atk))
        {
            if(headshot && playheadshotsound)
            {
                playsound(S_HIT_WEAPON_HEAD, NULL, &f->o);
            }
            else
            {
                int hitsound = attacks[atk].hitsound;
                if (validsound(hitsound))
                {
                    playsound(attacks[atk].hitsound, NULL, &f->o);
                }
            }
        }
        else playsound(S_PLAYER_DAMAGE, NULL, &f->o);
        if(f->haspowerup(PU_ARMOR)) playsound(S_ACTION_ARMOR, NULL, &f->o);
    }

    void gibeffect(int damage, const vec &vel, gameent *d, bool force)
    {
        if(!gore) return;
        if(force)
        {
            d->health = HEALTH_GIB;
            damage = d->maxhealth;
        }
        vec from = d->abovehead();
        if(goreeffect <= 0)
        {
            loopi(min(damage, 8)+1) spawnbouncer(from, d, Projectile_Gib);
            if(blood)
            {
                particle_splash(PART_BLOOD, 3, 180, d->o, getbloodcolor(d), 3.0f + rndscale(5.0f), 150, 0);
                particle_splash(PART_BLOOD2, damage, 300, d->o, getbloodcolor(d), 0.89f, 300, 5);
            }
        }
        playsound(S_GIB, d);
    }

    VARP(monsterdeadpush, 1, 5, 20);

    void hit(int damage, dynent *d, gameent *at, const vec &vel, int atk, float info1, int info2, int flags)
    {
        gameent *f = (gameent *)d;
        if(f->type == ENT_PLAYER && !isinvulnerable(f, at)) f->lastpain = lastmillis;
        if(at->type==ENT_PLAYER && f!=at && !isally(f, at))
        {
            at->totaldamage += damage;
        }
        if(at == self && d != at)
        {
            extern int hitsound;
            if(hitsound && at->lasthit != lastmillis) {
                playsound(isally(f, at) ? S_HIT_ALLY : S_HIT);
            }
            at->lasthit = lastmillis;
        }
        if(f->type != ENT_AI && (!m_mp(gamemode) || f==at))
        {
            f->hitpush(damage, vel, at, atk);
        }
        if(f->type == ENT_AI)
        {
            hitmonster(damage, (monster *)f, at, atk, flags);
            f->hitpush(damage * (f->health<=0 ? monsterdeadpush : 1), vel, at, atk);
        }
        else if(!m_mp(gamemode))
        {
            damaged(damage, f->o, f, at, atk, flags);
        }
        else
        {
            hitmsg &h = hits.add();
            h.target = f->clientnum;
            h.lifesequence = f->lifesequence;
            h.info1 = int(info1*DMF);
            h.info2 = info2;
            h.flags = flags;
            h.dir = f==at ? ivec(0, 0, 0) : ivec(vec(vel).mul(DNF));

            if (at == self && f == at)
            {
                damagehud(damage, f, at);
            }
        }
    }

    int calcdamage(int damage, gameent *target, gameent *actor, int atk, int flags)
    {
        if(target != actor && isinvulnerable(target, actor))
        {
            return 0;
        }
        if (!(flags & HIT_MATERIAL))
        {
            if (attacks[atk].headshotdam && !isweaponprojectile(attacks[atk].projectile)) // weapons deal locational damage only if headshot damage is specified (except for projectiles)
            {
                if (flags & HIT_HEAD)
                {
                    if(m_mayhem(mutators)) return (damage = target->health); // force death if it's a blow to the head when the Mayhem mutator is enabled
                    else damage += attacks[atk].headshotdam;
                }
                if (flags & HIT_LEGS) damage /= 2;
            }
            if (actor->haspowerup(PU_DAMAGE) || actor->role == ROLE_BERSERKER) damage *= 2;
            if (isally(target, actor) || target == actor) damage /= DAM_ALLYDIV;
        }
        if (target->haspowerup(PU_ARMOR) || target->role == ROLE_BERSERKER) damage /= 2;
        if(!damage) damage = 1;
        return damage;
    }

    void calcpush(int damage, dynent *d, gameent *at, vec &from, vec &to, int atk, int rays, int flags)
    {
        if(betweenrounds) return;
        vec velocity = vec(to).sub(from).safenormalize();
        hit(damage, d, at, velocity, atk, from.dist(to), rays, flags);
    }

    void playimpactsound(int sound, vec to)
    {
        if (!validsound(sound)) return;
        playsound(sound, NULL, &to);
    }

    void impacteffects(int atk, gameent *d, const vec &from, const vec &to, bool hit = false)
    {
        if(!validatk(atk) || isemptycube(to) || from.dist(to) > attacks[atk].range) return;
        vec dir = vec(from).sub(to).safenormalize();
        int material = lookupmaterial(to);
        bool iswater = (material & MATF_VOLUME) == MAT_WATER, isglass = (material & MATF_VOLUME) == MAT_GLASS;
        switch(atk)
        {
            case ATK_SCATTER1:
            case ATK_SCATTER2:
            {
                adddynlight(vec(to).madd(dir, 4), 6, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if(hit || iswater || isglass) break;
                particle_splash(PART_SPARK2, 10, 80+rnd(380), to, 0xFFC864, 0.1f, 250);
                particle_splash(PART_SMOKE, 10, 150, to, 0x606060, 1.8f + rndscale(2.2f), 100, 100);
                addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.30f + rndscale(0.80f), rnd(4));
            }
            break;

            case ATK_SMG1:
            case ATK_SMG2:
            {
                adddynlight(vec(to).madd(dir, 4), 15, vec(0.5f, 0.375f, 0.25f), 140, 10);
                if(hit || iswater || isglass) break;
                particle_fireball(to, 0.5f, PART_EXPLOSION1, 120, 0xFFC864, 2.0f);
                particle_splash(PART_EXPLODE, 50, 40, to, 0xFFC864, 1.0f);
                particle_splash(PART_SPARK2, 30, 150, to, 0xFFC864, 0.05f + rndscale(0.09f), 250);
                particle_splash(PART_SMOKE, 30, 180, to, 0x444444, 2.20f, 80, 100);
                addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.30f + rndscale(0.80f), rnd(4));
                break;
            }

            case ATK_PULSE2:
            {
                adddynlight(vec(to).madd(dir, 4), 80, vec(1.0f, 0.50f, 1.0f), 20);
                if(hit)
                {
                    particle_flare(to, to, 120, PART_ELECTRICITY, 0xEE88EE, 5.0f);
                    break;
                }
                if(iswater) break;
                
                particle_splash(PART_SPARK2, 10, 300, to, 0xEE88EE, 0.01f + rndscale(0.10f), 350, 2);
                particle_splash(PART_SMOKE, 20, 150, to, 0x777777, 2.0f, 100, 50);
                addstain(STAIN_PULSE_SCORCH, to, vec(from).sub(to).normalize(), 1.0f + rndscale(1.10f));
                playimpactsound(attacks[atk].impactsound, to);
                break;
            }

            case ATK_RAIL1:
            case ATK_RAIL2:
            case ATK_INSTA:
            {
                bool insta = attacks[atk].gun == GUN_INSTA;
                adddynlight(vec(to).madd(dir, 4), 60, !insta ? vec(0.25f, 1.0f, 0.75f) :  vec(0.25f, 0.75f, 1.0f), 180, 75, DL_EXPAND);
                if(hit)
                {
                    if(insta) particle_flare(to, to, 200, PART_ELECTRICITY, 0x50CFE5, 6.0f);
                    break;
                }
                if(iswater || isglass) break;
                particle_splash(PART_EXPLODE, 80, 80, to, !insta ? 0x77DD77 : 0x50CFE5, 1.25f, 100, 80);
                particle_splash(PART_SPARK2, 5 + rnd(20), 200 + rnd(380), to, !insta ? 0x77DD77 : 0x50CFE5, 0.1f + rndscale(0.3f), 200, 3);
                particle_splash(PART_SMOKE, 20, 180, to, 0x808080, 2.0f, 60, 80);
                addstain(STAIN_RAIL_HOLE, to, dir, 3.5f, 0xFFFFFF, rnd(4));
                addstain(STAIN_RAIL_GLOW, to, dir, 3.0f, !insta ? 0x77DD77 : 0x50CFE5);
                break;
            }

            case ATK_PISTOL1:
            {
                adddynlight(vec(to).madd(dir, 4), 30, vec(0.25, 1.0f, 1.0f), 200, 10, DL_SHRINK);
                if(hit || iswater || isglass) break;
                particle_fireball(to, 2.2f, PART_EXPLOSION1, 140, 0x00FFFF, 0.1f);
                particle_splash(PART_SPARK2, 50, 180, to, 0x00FFFF, 0.08f+rndscale(0.18f));
                addstain(STAIN_PULSE_SCORCH, to, vec(from).sub(to).normalize(), 0.80f+rndscale(1.0f));
                addstain(STAIN_RAIL_GLOW, to, dir, 1.50f, 0x00FFFF);
                break;
            }

            default: break;
        }
        if (hit || atk == ATK_PULSE2) return;
        int impactsound = attacks[atk].impactsound;
        if(iswater)
        {
            addstain(STAIN_RAIL_HOLE, to, vec(from).sub(to).normalize(), 0.30f + rndscale(0.80f));
            impactsound = S_IMPACT_WATER;

        }
        else if(isglass)
        {
            particle_splash(PART_GLASS, 20, 200, to, 0xFFFFFF, 0.10 + rndscale(0.20f));
            addstain(STAIN_GLASS_HOLE, to, vec(from).sub(to).normalize(), 0.30f + rndscale(1.0f));
            impactsound = S_IMPACT_GLASS;
        }
        if (!(attacks[atk].rays > 1 && d == hudplayer()))
        {
            playimpactsound(impactsound, to);
        }
    }

    void playweaponsounds(gameent* d, int atk, int prevaction)
    {
        bool isloop = attacks[atk].isloop, islooping = false;
        int sound = attacks[atk].sound;
        if (d->attacksound >= 0 && d->attacksound != sound) d->stopweaponsound();
        if (d->idlesound >= 0) d->stopidlesound();
        if (validsound(sound))
        {
            if (isloop && attacks[atk].gun != GUN_SMG)
            {
                if (validsound(d->attacksound)) islooping = true;
                d->attacksound = sound;
                d->attackchan = playsound(sound, NULL, &d->o, NULL, 0, -1, 100, d->attackchan);
            }
            else playsound(sound, NULL, d == hudplayer() ? NULL : &d->o);
        }
        int sound2 = attacks[atk].sound2;
        if (validsound(sound2))
        {
            bool isloopstarting = lastmillis - prevaction > 200 && !islooping;
            if (isloopstarting || (!isloop && d == followingplayer(self)))
            {
                playsound(sound2, d);
            }
        }
        if (lastmillis - prevaction > 200 && !islooping)
        {
            if (d->role == ROLE_BERSERKER)
            {
                playsound(S_BERSERKER_ACTION, d);
                return;
            }
            if (d->haspowerup(PU_DAMAGE) || d->haspowerup(PU_HASTE) || d->haspowerup(PU_AMMO))
            {
                playsound(S_ACTION_DAMAGE + d->poweruptype - 1, d);
            }
        }
    }

    VARP(muzzleflash, 0, 1, 1);

    void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction, bool hit)     // create visual effect from a shot
    {
        int gun = attacks[atk].gun;
        float dist = from.dist(to);
        bool shouldeject = d->eject.x >= 0 && d == followingplayer(self);
        vec up = vec(0, 0, 0);
        switch(atk)
        {
            case ATK_SCATTER1:
            case ATK_SCATTER2:
            {
                if(d->muzzle.x >= 0 && muzzleflash)
                {
                    particle_flare(d->muzzle, d->muzzle, 70, PART_MUZZLE_FLASH, 0xEFE598, 2.4f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 60, vec(0.5f, 0.375f, 0.25f), 110, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(shouldeject) spawnbouncer(d->eject, d, Projectile_Eject);
                if(!local)
                {
                    loopi(attacks[atk].rays)
                    {
                        offsetray(from, to, attacks[atk].spread, attacks[atk].range, rays[i], d);
                        impacteffects(atk, d, from, rays[i], hit);
                    }
                }
                loopi(attacks[atk].rays)
                {
                    particle_flare(hudgunorigin(gun, from, rays[i], d), rays[i], 80, PART_TRAIL, 0xFFC864, 1.2f);
                }
                break;
            }

            case ATK_SMG1:
            case ATK_SMG2:
            {
                if(d->muzzle.x >= 0 && muzzleflash)
                {
                    particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH3, 0xEFE898, 1.5f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 60, vec(0.5f, 0.375f, 0.25f), atk == ATK_SMG1 ? 70 : 110, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                if(shouldeject) spawnbouncer(d->eject, d, Projectile_Eject);
                if(atk == ATK_SMG2) particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_TRAIL, 0xFFC864, 1.2f);
                if(!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_PULSE1:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 115, PART_MUZZLE_FLASH2, 0xDD88DD, 1.8f, d);
                }
                break;
            }
            case ATK_PULSE2:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                     particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH2, 0xDD88DD, 1.6f, d);
                     adddynlight(hudgunorigin(gun, d->o, to, d), 30, vec(1.0f, 0.50f, 1.0f), 80, 10, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_LIGHTNING, 0xEE88EE, 1.0f, d);
                particle_fireball(to, 1.0f, PART_EXPLOSION2, 100, 0xDD88DD, 3.0f);
                if(!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_ROCKET1:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH4, 0xEFE898, 3.0f, d);
                }
                break;
            }

            case ATK_RAIL1:
            case ATK_RAIL2:
            {
                if(d->muzzle.x >= 0 && muzzleflash)
                {
                    particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH, 0x77DD77, 2.75f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 60, vec(0.25f, 1.0f, 0.75f), 150, 75, DL_SHRINK, 0, vec(0, 0, 0), d);
                }
                if(shouldeject) spawnbouncer(d->eject, d, Projectile_Eject);
                if(atk == ATK_RAIL2) particle_trail(PART_SMOKE, 350, hudgunorigin(gun, from, to, d), to, 0xDEFFDE, 0.3f, 50);
                particle_flare(hudgunorigin(gun, from, to, d), to, 600, PART_TRAIL, 0x55DD55, 0.50f);
                if(!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_GRENADE1:
            case ATK_GRENADE2:
            {
                if(d->muzzle.x >= 0 && muzzleflash)
                {
                    particle_flare(d->muzzle, d->muzzle, 80, PART_MUZZLE_FLASH5, 0x74BCF9, 2.8f, d);
                }
                up = to;
                up.z += dist / (atk == ATK_GRENADE1 ? 8 : 16);
                break;
            }

            case ATK_PISTOL1:
            case ATK_PISTOL2:
            {
                if(muzzleflash && d->muzzle.x >= 0)
                {
                   particle_flare(d->muzzle, d->muzzle, 50, PART_MUZZLE_FLASH3, 0x00FFFF, 2.50f, d);
                   adddynlight(hudgunorigin(attacks[atk].gun, d->o, to, d), 30, vec(0.25f, 1.0f, 1.0f), 60, 20, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(attacks[atk].gun, from, to, d), to, 80, PART_TRAIL, 0x00FFFF, 2.0f);
                if(!local) impacteffects(atk, d, from, to, hit);
                break;
            }

            case ATK_INSTA:

                if(muzzleflash && d->muzzle.x >= 0)
                {
                    particle_flare(d->muzzle, d->muzzle, 100, PART_MUZZLE_FLASH, 0x50CFE5, 2.75f, d);
                    adddynlight(hudgunorigin(gun, d->o, to, d), 60, vec(0.25f, 0.75f, 1.0f), 75, 75, DL_FLASH, 0, vec(0, 0, 0), d);
                }
                particle_flare(hudgunorigin(gun, from, to, d), to, 100, PART_LIGHTNING, 0x50CFE5, 1.0f);
                particle_flare(hudgunorigin(gun, from, to, d), to, 500, PART_TRAIL, 0x50CFE5, 1.0f);
                break;

            default: break;
        }
        if (isvalidprojectile(attacks[atk].projectile))
        {
            int attackrays = attacks[atk].rays;
            if (attackrays <= 1)
            {
                vec aim = up.iszero() ? to : up;
                makeprojectile(d, from, aim, local, id, atk, attacks[atk].projectile, attacks[atk].lifetime, attacks[atk].projspeed, attacks[atk].gravity, attacks[atk].elasticity);
            }
            else loopi(attackrays)
            {
                makeprojectile(d, from, rays[i], local, id, atk, attacks[atk].projectile, attacks[atk].lifetime, attacks[atk].projspeed, attacks[atk].gravity, attacks[atk].elasticity);
            }
        }
        playweaponsounds(d, atk, prevaction);
    }

    void particletrack(physent *owner, vec &o, vec &d)
    {
        if(owner->type != ENT_PLAYER && owner->type != ENT_AI) return;
        gameent *pl = (gameent *)owner;
        if(pl->muzzle.x < 0 || pl->lastattack < 0 || attacks[pl->lastattack].gun != pl->gunselect) return;
        float dist = o.dist(d);
        o = pl->muzzle;
        if(dist <= 0) d = o;
        else
        {
            vecfromyawpitch(owner->yaw, owner->pitch, 1, 0, d);
            float newdist = raycube(owner->o, d, dist, RAY_CLIPMAT|RAY_ALPHAPOLY);
            d.mul(min(newdist, dist)).add(owner->o);
        }
    }

    void dynlighttrack(physent *owner, vec &o, vec &hud)
    {
        if(owner->type!=ENT_PLAYER && owner->type != ENT_AI) return;
        gameent *pl = (gameent *)owner;
        if(pl->muzzle.x < 0 || pl->lastattack < 0 || attacks[pl->lastattack].gun != pl->gunselect) return;
        o = pl->muzzle;
        hud = owner == followingplayer(self) ? vec(pl->o).add(vec(0, 0, 2)) : pl->muzzle;
    }

    float intersectdist = 1e16f;

    bool isheadhitbox(dynent *d, const vec &from, const vec &to, float dist)
    {
        vec bottom(d->head), top(d->head);
        top.z += d->headradius;
        return linecylinderintersect(from, to, bottom, top, d->headradius, dist);
    }

    bool islegshitbox(dynent *d, const vec &from, const vec &to, float dist)
    {
        vec bottom(d->o), top(d->o);
        bottom.z -= d->eyeheight;
        top.z -= d->eyeheight/2.5f;
        return linecylinderintersect(from, to, bottom, top, d->legsradius, dist);
    }

    bool isintersecting(dynent *d, const vec &from, const vec &to, float margin, float &dist)   // if lineseg hits entity bounding box
    {
        vec bottom(d->o), top(d->o);
        bottom.z -= d->eyeheight + margin;
        top.z += d->aboveeye + margin;
        return linecylinderintersect(from, to, bottom, top, d->radius + margin, dist);
    }

    dynent *intersectclosest(const vec &from, const vec &to, gameent *at, float margin, float &bestdist)
    {
        dynent *best = NULL;
        bestdist = 1e16f;
        loopi(numdynents())
        {
            dynent *o = iterdynents(i);
            if(o==at || o->state!=CS_ALIVE) continue;
            float dist;
            if(!isintersecting(o, from, to, margin, dist)) continue;
            if(dist<bestdist)
            {
                best = o;
                bestdist = dist;
            }
        }
        return best;
    }

    void shorten(const vec &from, vec &target, float dist)
    {
        target.sub(from).mul(min(1.0f, dist)).add(from);
    }

    void hitscan(vec &from, vec &to, gameent *d, int atk)
    {
        int maxrays = attacks[atk].rays;
        dynent *o;
        float dist;
        int margin = attacks[atk].margin, damage = attacks[atk].damage, flags = HIT_TORSO;
        bool hitlegs = false, hithead = false;
        if(attacks[atk].rays > 1)
        {
            dynent *hits[GUN_MAXRAYS];
            loopi(maxrays)
            {
                if(!betweenrounds && (hits[i] = intersectclosest(from, rays[i], d, margin, dist)))
                {
                    hitlegs = islegshitbox(hits[i], from, rays[i], dist);
                    hithead = isheadhitbox(hits[i], from, rays[i], dist);
                    shorten(from, rays[i], dist);
                    impacteffects(atk, d, from, rays[i], true);
                }
                else impacteffects(atk, d, from, rays[i]);
            }
            if(betweenrounds) return;
            loopi(maxrays) if(hits[i])
            {
                o = hits[i];
                hits[i] = NULL;
                int numhits = 1;
                for(int j = i+1; j < maxrays; j++) if(hits[j] == o)
                {
                    hits[j] = NULL;
                    numhits++;
                }
                if(attacks[atk].headshotdam) // if an attack does not have headshot damage, then it does not deal locational damage
                {
                    if(hithead) flags |= HIT_HEAD;
                    if(hitlegs) flags |= HIT_LEGS;
                }
                damage = calcdamage(damage, (gameent *)o, d, atk, flags);
                calcpush(numhits*damage, o, d, from, to, atk, numhits, flags);
                damageeffect(damage, o, rays[i], atk, getbloodcolor(o), hithead);
            }
        }
        else
        {
            if(!betweenrounds && (o = intersectclosest(from, to, d, margin, dist)))
            {
                hitlegs = islegshitbox(o, from, to, dist);
                hithead = isheadhitbox(o, from, to, dist);
                shorten(from, to, dist);
                impacteffects(atk, d, from, to, true);
                if(attacks[atk].headshotdam) // if an attack does not have headshot damage, then it does not deal locational damage
                {
                    if(hithead) flags = HIT_HEAD;
                    else if(hitlegs) flags = HIT_LEGS;
                }
                damage = calcdamage(damage, (gameent *)o, d, atk, flags);
                calcpush(damage, o, d, from, to, atk, 1, flags);
                damageeffect(damage, o, to, atk, getbloodcolor(o), hithead);
                if(d == followingplayer(self) && attacks[atk].action == ACT_MELEE)
                {
                    addroll(d, damage / 2.0f);
                }
            }
            else
            {
                impacteffects(atk, d, from, to);
            }
        }
    }

    bool canshoot(gameent* d, int atk, int gun, int projectile)
    {
        if (attacks[atk].action != ACT_MELEE && (!d->ammo[gun] || attacks[atk].use > d->ammo[gun]))
        {
            return false;
        }
        if (isvalidprojectile(projectile))
        {
            bool isinwater = ((lookupmaterial(d->o)) & MATF_VOLUME) == MAT_WATER;
            if (isinwater && projs[projectile].flags & ProjFlag_Quench)
            {
                return false;
            }
        }
        return true;
    }

    void shoot(gameent *d, const vec &targ)
    {
        int prevaction = d->lastaction, attacktime = lastmillis - prevaction;
        if(attacktime<d->gunwait) return;
        d->gunwait = 0;
        if(!d->attacking) return;
        int gun = d->gunselect, act = d->attacking, atk = guns[gun].attacks[act], projectile = attacks[atk].projectile;
        d->lastaction = lastmillis;
        d->lastattack = atk;
        if(!canshoot(d, atk, gun, projectile))
        {
            if(d==self)
            {
                msgsound(S_WEAPON_NOAMMO, d);
                d->gunwait = 600;
                d->lastattack = -1;
                if(!d->ammo[gun]) weaponswitch(d);
            }
            return;
        }
        if(!d->haspowerup(PU_AMMO)) d->ammo[gun] -= attacks[atk].use;

        vec from = d->o, to = targ, dir = vec(to).sub(from).safenormalize();
        float dist = to.dist(from);
        int kickamount = attacks[atk].kickamount;
        if(d->haspowerup(PU_DAMAGE)) kickamount *= 2;
        if(kickamount && !(d->physstate >= PHYS_SLOPE && d->crouching && d->crouched()))
        {
            vec kickback = vec(dir).mul(kickamount*-2.5f);
            d->vel.add(kickback);
        }
        float shorten = attacks[atk].range && dist > attacks[atk].range ? attacks[atk].range : 0,
              barrier = raycube(d->o, dir, dist, RAY_CLIPMAT|RAY_ALPHAPOLY);
        if(barrier > 0 && barrier < dist && (!shorten || barrier < shorten))
            shorten = barrier;
        if(shorten) to = vec(dir).mul(shorten).add(from);

        if(attacks[atk].rays > 1)
        {
            loopi(attacks[atk].rays)
            {
                offsetray(from, to, attacks[atk].spread, attacks[atk].range, rays[i], d);
            }
        }
        else if(attacks[atk].spread)
        {
            offsetray(from, to, attacks[atk].spread, attacks[atk].range, to, d);
        }

        hits.setsize(0);

        if(!isweaponprojectile(attacks[atk].projectile)) hitscan(from, to, d, atk);

        shoteffects(atk, from, to, d, true, 0, prevaction);

        if(d==self || d->ai)
        {
            addmsg(N_SHOOT, "rci2i6iv", d, lastmillis-maptime, atk,
                   (int)(from.x*DMF), (int)(from.y*DMF), (int)(from.z*DMF),
                   (int)(to.x*DMF), (int)(to.y*DMF), (int)(to.z*DMF),
                   hits.length(), hits.length()*sizeof(hitmsg)/sizeof(int), hits.getbuf());
        }
        if(!attacks[atk].isfullauto) d->attacking = ACT_IDLE;
        int gunwait = attacks[atk].attackdelay;
        if(d->haspowerup(PU_HASTE) || d->role == ROLE_BERSERKER) gunwait /= 2;
        d->gunwait = gunwait;
        if(d->gunselect == GUN_PISTOL && d->ai) d->gunwait += int(d->gunwait*(((101-d->skill)+rnd(111-d->skill))/100.f));
        d->totalshots += attacks[atk].damage*attacks[atk].rays;
        d->pitchrecoil = kickamount * 0.10f;
    }

    void updaterecoil(gameent *d, int curtime)
    {
        if(!d->pitchrecoil || !curtime) return;
        const float amount = d->pitchrecoil * (curtime / 1000.0f) * d->speed * 0.12f;
        d->pitch += amount;
        float friction = 4.0f / curtime * 30.0f;
        d->pitchrecoil = d->pitchrecoil * (friction - 2.8f) / friction;
        fixcamerarange();
    }

    void checkattacksound(gameent *d, bool local)
    {
        int atk = guns[d->gunselect].attacks[d->attacking];
        switch(d->attacksound)
        {
            case S_PULSE2_A: atk = ATK_PULSE2; break;
            default: return;
        }
        if(atk >= 0 && atk < NUMATKS &&
           d->clientnum >= 0 && d->state == CS_ALIVE &&
           d->lastattack == atk && lastmillis - d->lastaction < attacks[atk].attackdelay + 50)
        {
            d->attackchan = playsound(d->attacksound, NULL, local ? NULL : &d->o, NULL, 0, -1, -1, d->attackchan);
            if(d->attackchan < 0) d->attacksound = -1;
        }
        else
        {
            d->stopweaponsound();
        }
    }

    void checkidlesound(gameent *d, bool local)
    {
        int sound = -1;
        if (d->clientnum >= 0 && d->state == CS_ALIVE && d->attacksound < 0)
        {
            switch (d->gunselect)
            {
                case GUN_ZOMBIE:
                {
                    sound = S_ZOMBIE_IDLE;
                    break;
                }
            }
        }
        if (d->idlesound != sound)
        {
            if(d->idlesound >= 0) d->stopidlesound();
            if(sound >= 0)
            {
                d->idlechan = playsound(sound, NULL, local ? NULL : &d->o, NULL, 0, -1, 1200, d->idlechan, 150);
                if(d->idlechan >= 0) d->idlesound = sound;
            }
        }
        else if(sound >= 0)
        {
            d->idlechan = playsound(sound, NULL, local ? NULL : &d->o, NULL, 0, -1, 1200, d->idlechan, 500);
            if(d->idlechan < 0) d->idlesound = -1;
        }
    }

    void updateweapons(int curtime)
    {
        if (self->clientnum >= 0 && self->state == CS_ALIVE)
        {
            shoot(self, worldpos); // only shoot when connected to server
        }
        updateprojectiles(curtime); // need to do this after the player shoots so projectiles don't end up inside player's BB next frame
        updaterecoil(self, curtime);
        gameent *following = followingplayer();
        if(!following) following = self;
        loopv(players)
        {
            gameent *d = players[i];
            checkattacksound(d, d==following);
            checkidlesound(d, d==following);
        }
    }

};
