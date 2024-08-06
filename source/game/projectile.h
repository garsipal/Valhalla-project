#include "game.h"

struct gameent;

const int OFFSETMILLIS = 500;

enum
{
    ProjFlag_Weapon   = 1 << 0, // Empty.
    ProjFlag_Junk     = 1 << 1, // Projectiles that are not used by weapons.
    ProjFlag_Bounce   = 1 << 2, // Projectile bounces off surfaces.
    ProjFlag_Linear   = 1 << 3, // Projectile moves in a straight line.
    ProjFlag_Impact   = 1 << 4, // Projectile detonates on collision.
    ProjFlag_Quench   = 1 << 5, // Projectile is destroyed upon contact with water.
    ProjFlag_Immortal = 1 << 6  // Projectile cannot be destroyed by players.
};

enum
{
    Projectile_Grenade = 0,
    Projectile_Grenade2,
    Projectile_Rocket,
    Projectile_Rocket2,
    Projectile_Pulse,
    Projectile_Plasma,
    Projectile_Gib,
    Projectile_Debris,
    Projectile_Eject,
    Projectile_Bullet,
    Projectile_Max
};
inline bool isvalidprojectile(int type) { return type >= 0 && type < Projectile_Max; }

static const struct projectileinfo
{
    int type, flags;
    const char* directory;
    int bouncesound, loopsound, maxbounces, variants;
    float radius;
} projs[Projectile_Max] =
{
    { Projectile_Grenade,  ProjFlag_Weapon | ProjFlag_Bounce,                                                         "projectile/grenade",  S_BOUNCE_GRENADE, -1,             0, 0, 1.4f },
    { Projectile_Grenade2, ProjFlag_Weapon | ProjFlag_Bounce | ProjFlag_Impact,                                       "projectile/grenade",  S_BOUNCE_GRENADE, -1,             0, 0, 1.4f },
    { Projectile_Rocket,   ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Impact,                                       "projectile/rocket",   -1,               S_ROCKET_LOOP,  0, 0, 1.4f },
    { Projectile_Rocket2,  ProjFlag_Weapon | ProjFlag_Bounce,                                                         "projectile/rocket",   S_BOUNCE_ROCKET,  -1,             2, 0, 2.0f },
    { Projectile_Pulse,    ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Immortal, NULL,                  S_BOUNCE_ROCKET,  S_PULSE_LOOP,   0, 0, 1.0f },
    { Projectile_Plasma,   ProjFlag_Weapon | ProjFlag_Linear | ProjFlag_Quench | ProjFlag_Impact | ProjFlag_Immortal, NULL,                  S_BOUNCE_ROCKET,  S_PISTOL_LOOP,  0, 0, 1.0f },
    { Projectile_Gib,      ProjFlag_Junk | ProjFlag_Bounce,                                                           "projectile/gib",      -1,               -1,             2, 5, 1.5f },
    { Projectile_Debris,   ProjFlag_Junk | ProjFlag_Bounce,                                                           NULL,                  -1,               -1,             0, 0, 1.8f },
    { Projectile_Eject,    ProjFlag_Junk | ProjFlag_Bounce,                                                           "projectile/eject/01", S_BOUNCE_EJECT,   -1,             2, 0, 0.4f },
    { Projectile_Bullet,   ProjFlag_Junk | ProjFlag_Linear,                                                           NULL,                  -1,               -1,             0, 0, 0.4f }
};
inline bool isweaponprojectile(int projectile) { return isvalidprojectile(projectile) && projs[projectile].flags & ProjFlag_Weapon; }

struct projectile : physent
{
    gameent* owner;

    vec offset, lastposition;

    bool islocal, isdestroyed, isdirect;

    float lastyaw, roll, gravity, elasticity, offsetheight, dist;

    int health, id, atk, projtype, flags, lifetime;
    int variant, bounces, offsetmillis;
    int lastbounce, bouncesound, loopchan, loopsound;

    vec dv, from, to;

    projectile() : isdestroyed(false), isdirect(false), roll(0), variant(0), bounces(0), lastbounce(0), bouncesound(-1), loopchan(-1), loopsound(-1)
    {
        type = ENT_BOUNCE;
        collidetype = COLLIDE_ELLIPSE;
    }
    ~projectile()
    {
        if (loopchan >= 0) stopsound(loopsound, loopchan);
        loopsound = loopchan = -1;
    }

    void limitoffset()
    {
        if (flags & ProjFlag_Weapon && offsetmillis > 0 && offset.z < 0)
        {
            offsetheight = raycube(vec(o.x + offset.x, o.y + offset.y, o.z), vec(0, 0, -1), -offset.z);
        }
        else offsetheight = -1;
    }

    void setradius()
    {
        float radius = projs[projtype].radius;
        this->radius = radius;
        xradius = yradius = eyeheight = aboveeye = this->radius;
    }

    void setvariant()
    {
        int variants = projs[projtype].variants;
        if (variants > 0)
        {
            variant = rnd(variants);
        }
    }

    void setsounds()
    {
        int bouncesound = projs[projtype].bouncesound;
        if (validsound(bouncesound))
        {
            this->bouncesound = bouncesound;
        }
        int loopsound = projs[projtype].loopsound;
        if (validsound(loopsound))
        {
            this->loopsound = loopsound;
        }
    }

    void setflags()
    {
        int flags = projs[projtype].flags;
        this->flags = flags;
    }

    void checkliquid()
    {
        int material = lookupmaterial(o);
        bool isinwater = isliquidmaterial(material & MATF_VOLUME);
        inwater = isinwater ? material & MATF_VOLUME : MAT_AIR;
    }

    vec offsetposition()
    {
        vec pos(o);
        if (offsetmillis > 0)
        {
            pos.add(vec(offset).mul(offsetmillis / float(OFFSETMILLIS)));
            if (offsetheight >= 0) pos.z = max(pos.z, o.z - max(offsetheight - eyeheight, 0.0f));
        }
        return pos;
    }

    vec updateposition(int time)
    {
        if (flags & ProjFlag_Linear)
        {
            offsetmillis = max(offsetmillis - time, 0);
            dist = to.dist(o, dv);
            dv.mul(time / max(dist * 1000 / speed, float(time)));
            vec v = vec(o).add(dv);
            return v;
        }
        else
        {
            vec pos(o);
            pos.add(vec(offset).mul(offsetmillis / float(OFFSETMILLIS)));
            return pos;
        }
    }
};
