// hit information to send
struct hitmsg
{
    int target, lifesequence, info1, info2, flags;
    ivec dir;
};

// Hit flags: context on how the player has been hit by a weapon.
enum
{
    HIT_TORSO    = 1<<0,
    HIT_LEGS     = 1<<1,
    HIT_HEAD     = 1<<2,
    HIT_MATERIAL = 1<<3,
    HIT_DIRECT   = 1<<4
};

// Kill flags: information on how the player died, announcements, etc.
enum
{
    KILL_NONE        = 1 << 0,
    KILL_FIRST       = 1 << 1,
    KILL_SPREE       = 1 << 2,
    KILL_SAVAGE      = 1 << 3,
    KILL_UNSTOPPABLE = 1 << 4,
    KILL_LEGENDARY   = 1 << 5,
    KILL_HEADSHOT    = 1 << 6,
    KILL_BERSERKER   = 1 << 7,
    KILL_TRAITOR     = 1 << 8,
    KILL_DIRECT      = 1 << 9
};

// Death flags: effects for each player death.
enum
{
    DEATH_FIST = 0,
    DEATH_DEFAULT,
    DEATH_GIB,
    DEATH_FALL,
    DEATH_DISRUPT,
    DEATH_HEADSHOT,
    DEATH_SHOCK,
    DEATH_ONFIRE,
    DEATH_HEADLESS
};

// Attack types (primary, secondary, melee) depend on player actions.
enum
{
    ACT_IDLE = 0,
    ACT_MELEE,     // use melee attack, currently the same for all weapons
    ACT_PRIMARY,   // either use the weapon primary fire
    ACT_SECONDARY, // or use the secondary fire
    NUMACTS
};
inline bool validact(int act) { return act >= 0 && act < NUMACTS; }

// Weapon attacks (primary, secondary, melee).
enum
{
    ATK_MELEE = 0, ATK_MELEE2,

    ATK_SCATTER1, ATK_SCATTER2,
    ATK_SMG1, ATK_SMG2,
    ATK_PULSE1, ATK_PULSE2,
    ATK_ROCKET1, ATK_ROCKET2,
    ATK_RAIL1, ATK_RAIL2,
    ATK_GRENADE1, ATK_GRENADE2,
    ATK_PISTOL1, ATK_PISTOL2, ATK_PISTOL_COMBO,

    ATK_INSTA, ATK_ZOMBIE,

    NUMATKS
};
inline bool validatk(int atk) { return atk >= 0 && atk < NUMATKS; }

enum
{
    // Main weapons (always present).
    GUN_SCATTER = 0, GUN_SMG, GUN_PULSE, GUN_ROCKET, GUN_RAIL, GUN_GRENADE, GUN_PISTOL,

    // Special weapons (only used in certain modes).
    GUN_INSTA, GUN_ZOMBIE,

    NUMGUNS
};
inline bool validgun(int gun) { return gun >= 0 && gun < NUMGUNS; }

const int HEALTH_GIB = -50; // if health equals or falls below this threshold, the player bursts into a bloody mist

const int GUN_MAXRAYS = 20; // maximum rays a player can shoot, we can't change that

const int DELAY_ENVDAM = 500; // environmental damage is dealt again after a specific number of milliseconds
const int DELAY_RESPAWN = 1500; // spawn is possible after a specific number of milliseconds has elapsed

const int DAM_ALLYDIV = 2;  // divide damage dealt to self or allies
const int DAM_ENV = 5;  // environmental damage like lava, damage material and fall damage

const float EXP_SELFPUSH = 2.5f; // how much our player is going to be pushed from our own projectiles
const float EXP_DISTSCALE = 1.5f; // explosion damage is going to be scaled by distance

static const struct attackinfo
{
    int gun, action, projectile, attackdelay, damage, headshotdam, spread, margin, projspeed, kickamount, range, rays, hitpush, exprad, lifetime, use;
    float gravity, elasticity;
    bool isloop, isfullauto;
    int anim, vwepanim, hudanim, sound, sound2, impactsound, hitsound;
} attacks[NUMATKS] =
{
    // melee: default melee for all weapons
    { NULL,        ACT_MELEE,     -1,                  650,  60,  0,   0, 2,    0,  0,   14,  1,  50,  0,    0, 0,    0,    0, false, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_MELEE,         -1,         S_IMPACT_MELEE,    S_HIT_MELEE   },
    { NULL,        ACT_MELEE,     -1,                  420,  25,  0,   0, 1,    0,  0,   16,  1,  50,  0,    0, 0,    0,    0, false, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_MELEE,         -1,         S_IMPACT_MELEE,    S_HIT_MELEE   },
    // shotgun
    { GUN_SCATTER, ACT_PRIMARY,   Projectile_Bullet,   880,   5,  5, 260, 0, 1200, 20, 1000, 20,  60,  0,    0, 1,    0,    0, false, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SG1_A,         S_SG1_B,    S_IMPACT_SG,       S_HIT_WEAPON  },
    { GUN_SCATTER, ACT_SECONDARY, Projectile_Bullet,   980,   6,  5, 120, 0, 1200, 25, 1000, 10,  60,  0,    0, 1,    0,    0, false, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SG2_A,         S_SG1_B,    S_IMPACT_SG,       S_HIT_WEAPON  },
    // smg
    { GUN_SMG,     ACT_PRIMARY,   Projectile_Bullet,   110,  16, 14,  84, 0, 1500,  7, 1000,  1,  60,  0,    0, 1,    0,    0, false, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SMG,           -1,         S_IMPACT_SMG,      S_HIT_WEAPON  },
    { GUN_SMG,     ACT_SECONDARY, Projectile_Bullet,   160,  17, 15,  30, 0, 1500, 10, 1000,  1,  80,  0,    0, 1,    0,    0, false, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_SMG,           -1,         S_IMPACT_SMG,      S_HIT_WEAPON  },
    // pulse
    { GUN_PULSE,   ACT_PRIMARY,   Projectile_Pulse,    180,  22,  0,   0, 1, 1000,  8, 2048,  1,  80, 18, 3000, 2,    0,    0, false, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_PULSE1,        -1,         S_PULSE_EXPLODE,   S_HIT_WEAPON  },
    { GUN_PULSE,   ACT_SECONDARY, -1,                   80,  14,  0,   0, 0,    0,  2,  200,  1, 150,  0,    0, 1,    0,    0, true,  true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT2, S_PULSE2_A,      S_PULSE2_B, S_IMPACT_PULSE,    S_HIT_WEAPON},
    // rocket
    { GUN_ROCKET,  ACT_PRIMARY,   Projectile_Rocket,   920, 110,  0,   0, 0,  300,  0, 2048,  1, 110, 33, 5000, 1,    0,    0, false, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_ROCKET1,       -1,         S_ROCKET_EXPLODE,  S_HIT_WEAPON  },
    { GUN_ROCKET,  ACT_SECONDARY, Projectile_Rocket2,  920, 110,  0,   0, 0,  300,  0, 2048,  1, 110, 33, 2000, 1, 0.6f, 0.7f, false, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_ROCKET2,       -1,         S_ROCKET_EXPLODE,  S_HIT_WEAPON  },
    // railgun
    { GUN_RAIL,    ACT_PRIMARY,   Projectile_Bullet,  1200,  70, 30,   0, 0, 2000, 30, 5000,  1, 100,  0,    0, 1,    0,    0, false, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_RAIL_A,        S_RAIL_B,   S_IMPACT_RAILGUN,  S_HIT_RAILGUN },
    { GUN_RAIL,    ACT_SECONDARY, Projectile_Bullet,  1400, 100, 10,   0, 0, 2000, 30, 5000,  1, 100,  0,    0, 1,    0,    0, false, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_RAIL_A,        S_RAIL_B,   S_IMPACT_RAILGUN,  S_HIT_RAILGUN },
    // grenade launcher
    { GUN_GRENADE, ACT_PRIMARY,   Projectile_Grenade,  650,  90,  0,   0, 0,  200, 10, 2024,  1, 250, 45, 1500, 1, 0.7f, 0.8f, false, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_GRENADE,       -1,         S_GRENADE_EXPLODE, S_HIT_WEAPON  },
    { GUN_GRENADE, ACT_SECONDARY, Projectile_Grenade2, 750,  90,  0,   0, 0,  190, 10, 2024,  1, 200, 35, 2000, 1, 1.0f,    0, false, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_GRENADE,       -1,         S_GRENADE_EXPLODE, S_HIT_WEAPON  },
    // pistol
    { GUN_PISTOL,  ACT_PRIMARY,   Projectile_Bullet,   300,  18, 17,  60, 0, 1500, 12, 1000,  1, 180,  0,    0, 1,    0,    0, false, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_PISTOL1,       -1,         S_IMPACT_PULSE,    S_HIT_WEAPON  },
    { GUN_PISTOL,  ACT_SECONDARY, Projectile_Plasma,   600,  15,  0,   0, 5,  400, 15, 2048,  1, 500,  8, 2000, 2,    0,    0, false, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_PISTOL2,       -1,         S_IMPACT_PULSE,    S_HIT_WEAPON  },
    { GUN_PISTOL,  ACT_SECONDARY, -1,                 1000,  80,  0,   0, 0,  400,  0, 2048,  1, 350, 50,    0, 0,    0,    0, false, false, ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  -1,              -1,         S_IMPACT_PISTOL,   S_HIT_RAILGUN },
    // instagib
    { GUN_INSTA,   ACT_PRIMARY,   -1,                 1200,   1,  0,   0, 0,    0, 36, 4000,  1,   1,  0,    0, 0,    0,    0, false, true,  ANIM_SHOOT, ANIM_VWEP_SHOOT, ANIM_GUN_SHOOT,  S_RAIL_INSTAGIB, S_RAIL_B,   S_IMPACT_RAILGUN,  S_HIT_WEAPON  },
    // zombie
    { GUN_ZOMBIE,  ACT_PRIMARY,   -1,                  600, 100,  0,   0, 4,    0,  0,   15,  1,  20,  0,    0, 0,    0,    0, false, false, ANIM_MELEE, ANIM_VWEP_MELEE, ANIM_GUN_MELEE,  S_ZOMBIE,        -1,         S_IMPACT_MELEE,    S_HIT_MELEE   }
};

static const struct guninfo
{
    const char *name, *model, *worldmodel;
    int attacks[NUMACTS], zoom, switchsound;
} guns[NUMGUNS] =
{
    { "scattergun", "scattergun", "weapon/scattergun/world", { -1, ATK_MELEE,  ATK_SCATTER1, ATK_SCATTER2 }, ZOOM_NONE,   S_SCATTERGUN_SWITCH },
    { "smg",        "smg",        "weapon/smg/world",        { -1, ATK_MELEE,  ATK_SMG1,     ATK_SMG2     }, ZOOM_SHADOW, S_SCATTERGUN_SWITCH },
    { "pulse",      "pulserifle", "weapon/pulserifle/world", { -1, ATK_MELEE,  ATK_PULSE1,   ATK_PULSE2   }, ZOOM_NONE,   S_PULSE_SWITCH      },
    { "rocket",     "rocket",     "weapon/rocket/world",     { -1, ATK_MELEE,  ATK_ROCKET1,  ATK_ROCKET2  }, ZOOM_NONE,   S_ROCKET_SWITCH     },
    { "railgun",    "railgun",    "weapon/railgun/world",    { -1, ATK_MELEE,  ATK_RAIL1,    ATK_RAIL2    }, ZOOM_SCOPE,  S_RAILGUN_SWITCH    },
    { "grenade",    "grenade",    "weapon/grenade/world",    { -1, ATK_MELEE,  ATK_GRENADE1, ATK_GRENADE2 }, ZOOM_NONE,   S_GRENADE_SWITCH    },
    { "pistol",     "pistol",     "weapon/pistol/world",     { -1, ATK_MELEE,  ATK_PISTOL1,  ATK_PISTOL2  }, ZOOM_NONE,   S_PISTOL_SWITCH     },
    { "instagun",   "instagun",   "weapon/railgun/world",    { -1, ATK_MELEE,  ATK_INSTA,    ATK_INSTA    }, ZOOM_SCOPE,  S_RAILGUN_SWITCH    },
    { "zombie",     "zombie",     NULL,                      { -1, ATK_ZOMBIE, ATK_ZOMBIE,   ATK_ZOMBIE   }, ZOOM_SHADOW, -1                  }
};
