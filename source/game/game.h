#ifndef __GAME_H__
#define __GAME_H__

#include "cube.h"
#include "unicode.h"

// animations

enum
{
    ANIM_DEAD = ANIM_GAMESPECIFIC, ANIM_DYING,
    ANIM_IDLE, ANIM_RUN_N, ANIM_RUN_NE, ANIM_RUN_E, ANIM_RUN_SE, ANIM_RUN_S, ANIM_RUN_SW, ANIM_RUN_W, ANIM_RUN_NW,
    ANIM_JUMP, ANIM_JUMP_N, ANIM_JUMP_NE, ANIM_JUMP_E, ANIM_JUMP_SE, ANIM_JUMP_S, ANIM_JUMP_SW, ANIM_JUMP_W, ANIM_JUMP_NW,
    ANIM_SINK, ANIM_SWIM,
    ANIM_CROUCH, ANIM_CROUCH_N, ANIM_CROUCH_NE, ANIM_CROUCH_E, ANIM_CROUCH_SE, ANIM_CROUCH_S, ANIM_CROUCH_SW, ANIM_CROUCH_W, ANIM_CROUCH_NW,
    ANIM_CROUCH_JUMP, ANIM_CROUCH_JUMP_N, ANIM_CROUCH_JUMP_NE, ANIM_CROUCH_JUMP_E, ANIM_CROUCH_JUMP_SE, ANIM_CROUCH_JUMP_S, ANIM_CROUCH_JUMP_SW, ANIM_CROUCH_JUMP_W, ANIM_CROUCH_JUMP_NW,
    ANIM_CROUCH_SINK, ANIM_CROUCH_SWIM,
    ANIM_SHOOT, ANIM_MELEE, ANIM_SWITCH,
    ANIM_PAIN,
    ANIM_EDIT, ANIM_LAG, ANIM_TAUNT, ANIM_WIN, ANIM_LOSE,
    ANIM_GUN_IDLE, ANIM_GUN_SHOOT, ANIM_GUN_SHOOT2, ANIM_GUN_MELEE, ANIM_GUN_SWITCH, ANIM_GUN_TAUNT,
    ANIM_VWEP_IDLE, ANIM_VWEP_SHOOT, ANIM_VWEP_MELEE,
    ANIM_TRIGGER,
    NUMANIMS
};

static const char * const animnames[] =
{
    "mapmodel",
    "dead", "dying",
    "idle", "run N", "run NE", "run E", "run SE", "run S", "run SW", "run W", "run NW",
    "jump", "jump N", "jump NE", "jump E", "jump SE", "jump S", "jump SW", "jump W", "jump NW",
    "sink", "swim",
    "crouch", "crouch N", "crouch NE", "crouch E", "crouch SE", "crouch S", "crouch SW", "crouch W", "crouch NW",
    "crouch jump", "crouch jump N", "crouch jump NE", "crouch jump E", "crouch jump SE", "crouch jump S", "crouch jump SW", "crouch jump W", "crouch jump NW",
    "crouch sink", "crouch swim",
    "shoot", "melee", "switch",
    "pain",
    "edit", "lag", "taunt", "win", "lose",
    "gun idle", "gun shoot", "gun shoot 2", "gun melee", "gun switch", "gun taunt",
    "vwep idle", "vwep shoot", "vwep melee",
    "trigger",
};

// network quantization scale
const float DMF   = 16.0f;  // for world locations
const float DNF   = 100.0f; // for normalized vectors
const float DVELF = 1.0f;   // for velocity vectors based on player's speed

struct gameentity : extentity
{
};

// physics stuff
enum
{
    LiquidTransition_None = 0,
    LiquidTransition_In,
    LiquidTransition_Out
};

enum { MM_AUTH = -1, MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE, MM_PASSWORD, MM_START = MM_AUTH, MM_INVALID = MM_START - 1 };

static const char * const mastermodenames[] =  { "Default",        "Open",        "Veto",        "Locked",        "Private",        "Password" };
static const char * const mastermodecolors[] = {       "",          "\f0",         "\f2",           "\f4",            "\f3",             "\f6" };
static const char * const mastermodeicons[] =  { "server",  "server_open", "server_veto", "server_locked", "server_private", "server_password" };

// server privileges
enum { PRIV_NONE = 0, PRIV_MASTER, PRIV_ADMIN, PRIV_AUTH };
static const int privilegecolors[4]            = {   0xFFFFFF, 0x40FF80,        0xFF8000, 0xFF00FF };
static const char * const privilegetextcode[4] = {         "",    "\f0",           "\f6",    "\f5" };
static const char * const privilegenames[4]    = {  "unknown", "master", "administrator",   "auth" };
inline bool validprivilege(int privilege) { return privilege > PRIV_NONE && privilege <= PRIV_AUTH; }

// round states
enum
{
    ROUND_START  = 1<<0,
    ROUND_END    = 1<<1,
    ROUND_RESET  = 1<<2,
    ROUND_WAIT   = 1<<3,
    ROUND_UNWAIT = 1<<4
};

// Time updates.
enum
{
    TimeUpdate_Match = 0,
    TimeUpdate_Overtime,
    TimeUpdate_Intermission
};

enum
{
    Zoom_None = 0,
    Zoom_Shadow,
    Zoom_Scope
};

// network messages codes, c2s, c2c, s2c
enum
{
    N_CONNECT = 0, N_SERVINFO, N_WELCOME, N_INITCLIENT, N_POS, N_TEXT, N_SOUND, N_CDIS,
    N_SHOOT, N_EXPLODE, N_SUICIDE,
    N_DIED, N_DAMAGE, N_HITPUSH, N_SHOTEVENT, N_SHOTFX, N_EXPLODEFX, N_REGENERATE, N_REPAMMO,
    N_TRYSPAWN, N_SPAWNSTATE, N_SPAWN, N_FORCEDEATH,
    N_GUNSELECT, N_TAUNT,
    N_ANNOUNCE,
    N_MAPCHANGE, N_MAPVOTE, N_TEAMINFO, N_ITEMSPAWN, N_ITEMPICKUP, N_ITEMACC, N_TELEPORT, N_JUMPPAD,
    N_PING, N_PONG, N_CLIENTPING,
    N_TIMEUP, N_FORCEINTERMISSION,
    N_SERVMSG, N_ITEMLIST, N_RESUME,
    N_EDITMODE, N_EDITENT, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE, N_ROTATE, N_REPLACE, N_DELCUBE, N_CALCLIGHT, N_REMIP, N_EDITVSLOT, N_UNDO, N_REDO, N_NEWMAP, N_GETMAP, N_SENDMAP, N_CLIPBOARD, N_EDITVAR,
    N_MASTERMODE, N_MUTE, N_KICK, N_CLEARBANS, N_CURRENTMASTER, N_SPECTATOR, N_SETMASTER, N_SETTEAM,
    N_LISTDEMOS, N_SENDDEMOLIST, N_GETDEMO, N_SENDDEMO,
    N_DEMOPLAYBACK, N_RECORDDEMO, N_STOPDEMO, N_CLEARDEMOS,
    N_TAKEFLAG, N_RETURNFLAG, N_RESETFLAG, N_TRYDROPFLAG, N_DROPFLAG, N_SCOREFLAG, N_INITFLAGS,
    N_ROUND, N_ROUNDSCORE, N_ASSIGNROLE, N_SCORE, N_VOOSH,
    N_SAYTEAM, N_WHISPER,
    N_CLIENT,
    N_AUTHTRY, N_AUTHKICK, N_AUTHCHAL, N_AUTHANS, N_REQAUTH,
    N_PAUSEGAME, N_GAMESPEED,
    N_ADDBOT, N_DELBOT, N_INITAI, N_FROMAI, N_BOTLIMIT, N_BOTBALANCE,
    N_MAPCRC, N_CHECKMAPS,
    N_SWITCHNAME, N_SWITCHMODEL, N_SWITCHCOLOR, N_SWITCHTEAM,
    N_SERVCMD,
    N_DEMOPACKET,
    N_COUNTRY,
    NUMMSG
};

static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
    N_CONNECT, 0, N_SERVINFO, 0, N_WELCOME, 1, N_INITCLIENT, 0, N_POS, 0, N_TEXT, 0, N_SOUND, 2, N_CDIS, 2,
    N_SHOOT, 0, N_EXPLODE, 0, N_SUICIDE, 1,
    N_DIED, 7, N_DAMAGE, 11, N_HITPUSH, 7, N_SHOTEVENT, 3, N_SHOTFX, 11, N_EXPLODEFX, 6, N_REGENERATE, 2, N_REPAMMO, 3,
    N_TRYSPAWN, 1, N_SPAWNSTATE, 9, N_SPAWN, 3, N_FORCEDEATH, 2,
    N_GUNSELECT, 2, N_TAUNT, 1,
    N_ANNOUNCE, 3,
    N_MAPCHANGE, 0, N_MAPVOTE, 0, N_TEAMINFO, 0, N_ITEMSPAWN, 2, N_ITEMPICKUP, 2, N_ITEMACC, 3,
    N_PING, 2, N_PONG, 2, N_CLIENTPING, 2,
    N_TIMEUP, 3, N_FORCEINTERMISSION, 1,
    N_SERVMSG, 0, N_ITEMLIST, 0, N_RESUME, 0,
    N_EDITMODE, 2, N_EDITENT, 11, N_EDITF, 16, N_EDITT, 16, N_EDITM, 16, N_FLIP, 14, N_COPY, 14, N_PASTE, 14, N_ROTATE, 15, N_REPLACE, 17, N_DELCUBE, 14, N_CALCLIGHT, 1, N_REMIP, 1, N_EDITVSLOT, 16, N_UNDO, 0, N_REDO, 0, N_NEWMAP, 2, N_GETMAP, 1, N_SENDMAP, 0, N_EDITVAR, 0,
    N_MASTERMODE, 2, N_MUTE, 0, N_KICK, 0, N_CLEARBANS, 1, N_CURRENTMASTER, 0, N_SPECTATOR, 4, N_SETMASTER, 0, N_SETTEAM, 0,
    N_LISTDEMOS, 1, N_SENDDEMOLIST, 0, N_GETDEMO, 3, N_SENDDEMO, 0,
    N_DEMOPLAYBACK, 3, N_RECORDDEMO, 2, N_STOPDEMO, 1, N_CLEARDEMOS, 2,
    N_TAKEFLAG, 3, N_RETURNFLAG, 4, N_RESETFLAG, 3, N_TRYDROPFLAG, 1, N_DROPFLAG, 7, N_SCOREFLAG, 9, N_INITFLAGS, 0,
    N_ROUND, 0, N_ROUNDSCORE, 0, N_ASSIGNROLE, 4, N_SCORE, 3, N_VOOSH, 3,
    N_SAYTEAM, 0, N_WHISPER, 0,
    N_CLIENT, 0,
    N_AUTHTRY, 0, N_AUTHKICK, 0, N_AUTHCHAL, 0, N_AUTHANS, 0, N_REQAUTH, 0,
    N_PAUSEGAME, 0, N_GAMESPEED, 0,
    N_ADDBOT, 2, N_DELBOT, 1, N_INITAI, 0, N_FROMAI, 2, N_BOTLIMIT, 2, N_BOTBALANCE, 2,
    N_MAPCRC, 0, N_CHECKMAPS, 1,
    N_SWITCHNAME, 0, N_SWITCHMODEL, 2, N_SWITCHCOLOR, 2,  N_SWITCHTEAM, 2,
    N_SERVCMD, 0,
    N_DEMOPACKET, 0,
    N_COUNTRY, 0,
    -1
};

#define VALHALLA_SERVER_PORT 21217
#define VALHALLA_LANINFO_PORT 21216
#define VALHALLA_MASTER_PORT 21215
#define PROTOCOL_VERSION 2 // bump when protocol changes
#define DEMO_VERSION 1  // bump when demo format changes
#define DEMO_MAGIC "VALHALLA_DEMO\0\0"

struct demoheader
{
    char magic[16];
    int version, protocol;
};

#include "projectile.h"
#include "weapon.h"
#include "ai.h"
#include "gamemode.h"
#include "entity.h"

// inherited by gameent and server clients
struct gamestate
{
    int health, maxhealth, shield;
    int gunselect, gunwait;
    int aitype, skill;
    int poweruptype, powerupmillis;
    int role;
    int ammo[NUMGUNS];

    gamestate() : health(100), maxhealth(100), shield(0), gunselect(GUN_PISTOL), gunwait(0), aitype(AI_NONE), skill(0), poweruptype(PU_NONE), powerupmillis(0), role(ROLE_NONE)
    {
        loopi(NUMGUNS)
        {
            ammo[i] = 0;
        }
    }

    bool canpickup(int type)
    {
        if(!validitem(type) || role == ROLE_BERSERKER || role == ROLE_ZOMBIE) return false;
        itemstat &is = itemstats[type-I_AMMO_SG];
        switch(type)
        {
            case I_HEALTH:
            case I_MEGAHEALTH:
            case I_ULTRAHEALTH:
            {
                return health < is.max;
            }

            case I_YELLOWSHIELD:
            case I_REDSHIELD:
            {
                return shield < is.max;
            }

            case I_DDAMAGE:
            case I_HASTE:
            case I_ARMOR:
            case I_INFINITEAMMO:
            case I_AGILITY:
            case I_INVULNERABILITY:
            {
                if(powerupmillis < is.info || poweruptype == is.info)
                {
                    return true;
                }
                return false;
            }

            default:
            {
                return ammo[is.info] < is.max;
            }
        }
        return false;
    }

    void pickup(int type)
    {
        if(!validitem(type) || role == ROLE_BERSERKER || role == ROLE_ZOMBIE) return;
        itemstat &is = itemstats[type-I_AMMO_SG];
        switch(type)
        {
            case I_HEALTH:
            case I_MEGAHEALTH:
            case I_ULTRAHEALTH:
            {
                health = min(health+is.add, is.max);
                break;
            }

            case I_YELLOWSHIELD:
            case I_REDSHIELD:
            {
                shield = min(shield+is.add, is.max);
                break;
            }

            case I_DDAMAGE:
            case I_ARMOR:
            case I_HASTE:
            case I_INFINITEAMMO:
            case I_AGILITY:
            case I_INVULNERABILITY:
            {
                poweruptype = is.info;
                powerupmillis = min(powerupmillis+is.add, is.max);
                break;
            }

            default:
            {
                ammo[is.info] = min(ammo[is.info]+is.add, is.max);
                break;
            }
        }
    }

    void baseammo(int gun, int k = 3)
    {
        ammo[gun] = itemstats[gun-GUN_SCATTER].add*k;
    }

    void addammo(int gun, int k = 1, int scale = 1)
    {
        itemstat &is = itemstats[gun-GUN_SCATTER];
        ammo[gun] = min(ammo[gun] + (is.add*k)/scale, is.max);
    }

    bool hasmaxammo(int type)
    {
       const itemstat &is = itemstats[type-I_AMMO_SG];
       return ammo[type-I_AMMO_SG+GUN_SCATTER]>=is.max;
    }

    void resetitems()
    {
        shield = 0;
        poweruptype = PU_NONE;
        powerupmillis = 0;
    }

    void resetweapons()
    {
        loopi(NUMGUNS) ammo[i] = 0;
        gunwait = 0;
    }

    void respawn()
    {
        maxhealth = 100;
        health = maxhealth;
        resetitems();
        resetweapons();
        gunselect = GUN_PISTOL;
        role = ROLE_NONE;
    }

    void assignrole(int newrole)
    {
        role = newrole;
        if (role == ROLE_ZOMBIE)
        {
            maxhealth = health = 1000;
            resetitems();
            resetweapons();
            ammo[GUN_ZOMBIE] = 1;
            gunselect = GUN_ZOMBIE;
        }
        else if (role == ROLE_BERSERKER)
        {
            maxhealth = health = maxhealth * 2;
            loopi(NUMGUNS)
            {
                if (!ammo[i] || i == GUN_INSTA || i == GUN_ZOMBIE) continue;
                if (i == GUN_PISTOL) ammo[i] = 100;
                else ammo[i] = max(itemstats[i - GUN_SCATTER].max, itemstats[i - GUN_SCATTER].add * 5);
            }
        }
    }

    void voosh(int gun)
    {
        loopi(NUMGUNS)
        {
            ammo[i] = 0;
        }
        baseammo(gun);
        gunselect = gun;
    }

    void spawnstate(int gamemode, int mutators, int forcegun)
    {
        if(m_insta(mutators))
        {
            gunselect = GUN_INSTA;
            ammo[GUN_INSTA] = 1;
        }
        else if(m_effic(mutators))
        {
            maxhealth = health = 200;
            loopi(NUMGUNS - 3) // Exclude the pistol and special weapons (last three).
            {
                baseammo(i);
            }
            gunselect = GUN_SMG;
        }
        else if(m_tactics(mutators))
        {
            maxhealth = health = 150;
            ammo[GUN_PISTOL] = 100;
            int primaryweapon = rnd(5) + 1, secondaryweapon;
            gunselect = primaryweapon;
            int ammomultiplier = m_noitems(mutators) ? 5 : 3;
            baseammo(primaryweapon, ammomultiplier);
            do secondaryweapon = rnd(5) + 1;
            while (primaryweapon == secondaryweapon); // Make sure we do not spawn with an identical secondary and primary weapon.
            baseammo(secondaryweapon, ammomultiplier);
        }
        else if(m_voosh(mutators))
        {
            voosh(forcegun);
        }
        else
        {
            gunselect = GUN_PISTOL;
            ammo[GUN_PISTOL] = 100;
            if(!m_story) ammo[GUN_GRENADE] = 1;
        }
    }

    void instantkill(int extradamage)
    {
        shield = 0;
        health -= health + extradamage;
    }

    // Subtract damage/shield points and apply damage here.
    int dodamage(int damage, bool environment = false)
    {
        if(shield && !environment)
        { // Only if the player has shield points and damage is not caused by the environment.
            int ad = round(damage / 3.0f * 2.0f);
            if(ad > shield) ad = shield;
            shield -= ad;
            damage -= ad;
        }
        health -= damage;
        return damage;
    }

    int hasammo(int gun, int exclude = -1)
    {
        return validgun(gun) && gun != exclude && ammo[gun] > 0;
    }

    bool haspowerup(int powerup)
    {
        return powerupmillis && poweruptype == powerup;
    }
};

#include "monster.h"

const int MAXNAMELEN = 60; // bytes
const int MAXNAMEUNILEN = 15; // unicode characters
const int MAXCOUNTRYCODELEN = 8;

const int MAXTEAMS = 2;
inline bool validteam(int team) { return team >= 1 && team <= MAXTEAMS; }
static const char * const teamnames[1+MAXTEAMS] = { "", "Aesir", "Vanir" };
static const char * const teamtextcode[1+MAXTEAMS] = { "\ff", "\f1", "\f3" };
static const char * const teamblipcolor[1+MAXTEAMS] = { "_neutral", "_blue", "_red" };
inline const char *teamname(int team) { return teamnames[validteam(team) ? team : 0]; }
static inline int teamnumber(const char *name) { loopi(MAXTEAMS) if(!strcmp(teamnames[1+i], name)) return 1+i; return 0; }
static const int teamtextcolor[1+MAXTEAMS] = { 0xFFFFFF, 0x6496FF, 0xFF4B19 };
static const int teamscoreboardcolor[1+MAXTEAMS] = { 0, 0x3030C0, 0xC03030 };
static const int teameffectcolor[1+MAXTEAMS] = { 0xFFFFFF, 0x2020FF, 0xFF2020 };

const int TAUNT_DELAY = 1000;
const int VOICECOM_DELAY = 2800;

enum
{
    // Reserved sound channels.
    Chan_Idle = 0,
    Chan_Attack,
    Chan_Weapon,
    Chan_PowerUp,
    Chan_LowHealth,
    Chan_Num
};

struct gameent : dynent, gamestate
{
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, privilege, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int respawned, suicided;
    int lastpain, lasthurt, lastspawn;
    int lastaction, lastattack, lastattacker, lasthit;
    int deathstate;
    int attacking;
    int lasttaunt, lastfootstep, lastyelp, lastswitch, lastswitchattempt, lastroll;
    int lastpickup, lastpickupmillis, flagpickup;
    int frags, flags, deaths, points, totaldamage, totalshots, lives, holdingflag;
    editinfo *edit;
    float deltayaw, deltapitch, deltaroll, newyaw, newpitch, newroll, pitchrecoil;
    int smoothmillis;

    int chan[Chan_Num], chansound[Chan_Num];

    float transparency;

    string name, info;
    int team, playermodel, playercolor;
    ai::aiinfo *ai;
    int ownernum, lastnode;
    bool respawnqueued, ghost;
    char country_code[MAXCOUNTRYCODELEN+1], preferred_flag[MAXCOUNTRYCODELEN+1];
    string country_name;

    vec muzzle, eject;

    gameent() : weight(100),
                clientnum(-1), privilege(PRIV_NONE), lastupdate(0), plag(0), ping(0),
                lifesequence(0), respawned(-1), suicided(-1),
                lastpain(0), lasthurt(0), lastspawn(0),
                lastfootstep(0), lastyelp(0), lastswitch(0), lastswitchattempt(0), lastroll(0),
                frags(0), flags(0), deaths(0), points(0), totaldamage(0), totalshots(0), lives(3), holdingflag(0),
                edit(NULL), pitchrecoil(0), smoothmillis(-1),
                transparency(1),
                team(0), playermodel(-1), playercolor(0), ai(NULL), ownernum(-1),
                muzzle(-1, -1, -1), eject(-1, -1, -1)
    {
        loopi(Chan_Num)
        {
            chan[i] = chansound[i] = -1;
        }
        name[0] = info[0] = 0;
        ghost = false;
        country_code[0] = country_name[0] = preferred_flag[0] = 0;
        respawn();
    }
    ~gameent()
    {
        freeeditinfo(edit);
        if(ai) delete ai;
        loopi(Chan_Num)
        {
            if (chan[i] >= 0)
            {
                stopsound(chansound[i], chan[i]);
            }
        }
    }

    void hitpush(int damage, const vec &dir, gameent *actor, int atk)
    {
        vec push(dir);
        bool istrickjump = actor == this && isweaponprojectile(attacks[atk].projectile) && attacks[atk].exprad;
        if (istrickjump)
        {
            // Projectiles reset gravity while falling so trick jumps are more rewarding and players are pushed further.
            falling.z = 1;
        }
        if (actor != this && physstate < PHYS_SLOPE)
        {
            // While in mid-air, push is stronger.
            damage *= GUN_AIR_PUSH;
        }
        if (role == ROLE_ZOMBIE)
        {
            // Zombies are pushed "a bit" more.
            damage *= GUN_ZOMBIE_PUSH;
        }
        push.mul((istrickjump ? EXP_SELFPUSH : 1.0f) * attacks[atk].hitpush * damage / weight);
        vel.add(push);
    }

    void startgame()
    {
        frags = flags = deaths = points = 0;
        totaldamage = totalshots = 0;
        lives = 3;
        holdingflag = 0;
        maxhealth = 100;
        shield = 0;
        lifesequence = -1;
        respawned = suicided = -2;
        lasthit = 0;
        ghost = false;
    }

    void respawn()
    {
        dynent::reset();
        gamestate::respawn();
        respawned = suicided = -1;
        lastaction = 0;
        lastattack = lastattacker = -1;
        deathstate = Death_Default;
        attacking = ACT_IDLE;
        lasttaunt = 0;
        lastpickup = -1;
        lastpickupmillis = 0;
        flagpickup = 0;
        lastnode = -1;
        lasthit = 0;
        respawnqueued = false;
        loopi(Chan_Num)
        {
            stopchannelsound(i);
        }
    }

    void playchannelsound(int type, int sound, int fade = 0, bool isloop = false)
    {
        chansound[type] = sound;
        chan[type] = playsound(chansound[type], this, NULL, NULL, 0, isloop ? -1 : 0, fade, chan[type]);
    }

    void stopchannelsound(int type, int fade = 0)
    {
        if (chan[type] >= 0)
        {
            stopsound(chansound[type], chan[type], fade);
        }
        chansound[type] = chan[type] = -1;
    }

    bool haslowhealth()
    {
        return state == CS_ALIVE && health <= maxhealth / 4;
    }

    bool shouldgib()
    {
        return health <= THRESHOLD_GIB;
    }
};

struct teamscore
{
    int team, score;
    teamscore() {}
    teamscore(int team, int n) : team(team), score(n) {}

    static bool compare(const teamscore &x, const teamscore &y)
    {
        if(x.score > y.score) return true;
        if(x.score < y.score) return false;
        return x.team < y.team;
    }
};

static inline uint hthash(const teamscore &t) { return hthash(t.team); }
static inline bool htcmp(int team, const teamscore &t) { return team == t.team; }

struct teaminfo
{
    int frags;

    teaminfo() { reset(); }

    void reset() { frags = 0; }
};

namespace entities
{
    extern void preloadentities();
    extern void renderentities();
    extern void checkitems(gameent *d);
    extern void updatepowerups(int time, gameent *d);
    extern void resetspawns();
    extern void spawnitems(bool force = false);
    extern void putitems(packetbuf &p);
    extern void setspawn(int i, bool shouldspawn, bool isforced = false);
    extern void teleport(int n, gameent *d);
    extern void pickupeffects(int n, gameent *d);
    extern void teleporteffects(gameent *d, int tp, int td, bool local = true);
    extern void jumppadeffects(gameent *d, int jp, bool local = true);
    extern void resettriggers();

    extern int respawnent;

    extern vector<extentity*> ents;
}

namespace physics
{
    extern void moveplayer(gameent* pl, int moveres, bool local);
    extern void crouchplayer(gameent* pl, int moveres, bool local);
    extern void physicsframe();
    extern void updatephysstate(gameent* d);
    extern void addroll(gameent* d, float amount);

    extern bool canmove(gameent* d);
    extern bool hasbounced(projectile* proj, float secs, float elasticity, float waterfric, float gravity);
    extern bool isbouncing(projectile* proj, float elasticity, float waterfric, float gravity);
    extern bool hascamerapitchmovement(gameent* d);

    extern int physsteps;
    extern int liquidtransition(physent* d, int material, bool isinwater);
}

// checks if two strings are visually identical or very similar
static inline bool duplicatestring(const char *a, const char *b)
{
    if(uni_strlen(a) != uni_strlen(b)) return false;
    uint c, d;
    size_t s = uni_getchar(a, c), z = uni_getchar(b, d);
    for(const char *p = a, *q = b; c; p += s, q += z, s = uni_getchar(p, c), z = uni_getchar(q, d))
    {
        if(homoglyph(c) != homoglyph(d)) return false;
    }
    return true;
}

namespace game
{
    extern int gamemode, mutators;

    struct clientmode
    {
        virtual ~clientmode() {}

        virtual void preload() {}
        virtual void drawhud(gameent *d, int w, int h) {}
        virtual void rendergame() {}
        virtual void respawned(gameent *d) {}
        virtual void setup() {}
        virtual void checkitems(gameent *d) {}
        virtual void pickspawn(gameent *d) { findplayerspawn(d, -1, m_teammode ? d->team : 0); }
        virtual void senditems(packetbuf &p) {}
        virtual void removeplayer(gameent *d) {}
        virtual void gameover() {}
        virtual void getteamscores(vector<teamscore> &scores) {}
        virtual void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests) {}
        virtual bool hidefrags() { return false; }
        virtual bool canfollow(gameent* s, gameent* f) { return true; }
        virtual bool aicheck(gameent *d, ai::aistate &b) { return false; }
        virtual bool aidefend(gameent *d, ai::aistate &b) { return false; }
        virtual bool aipursue(gameent *d, ai::aistate &b) { return false; }
        virtual int getteamscore(int team) { return 0; }
        virtual int respawnwait(gameent* d, bool seconds = false) { return 0; }
        virtual float ratespawn(gameent* d, const extentity& e) { return 1.0f; }
        virtual float clipconsole(float w, float h) { return 0; }
    };

    extern clientmode *cmode;
    extern void setclientmode();

    // game.cpp
    extern void taunt(gameent *d);
    extern void stopfollowing();
    extern void checkfollow();
    extern void nextfollow(int dir = 1);
    extern void clientdisconnected(int cn, bool notify = true);
    extern void clearclients(bool notify = true);
    extern void cleargame();
    extern void startgame();
    extern void pickgamespawn(gameent* d);
    extern void spawnplayer(gameent *d);
    extern void spawneffect(gameent *d);
    extern void respawn();
    extern void setdeathstate(gameent *d, bool restore = false);
    extern void writeobituary(gameent *d, gameent *actor, int atk, int flags = 0);
    extern void kill(gameent *d, gameent *actor, int atk, int flags = KILL_NONE);
    extern void updatetimer(int time, int type);
    extern void msgsound(int n, physent *d = NULL);
    extern void doaction(int act);
    extern void hurt(gameent* d);
    extern void suicide(gameent* d);
    extern void managedeatheffects(gameent* d);

    extern bool clientoption(const char* arg);
    extern bool gamewaiting, betweenrounds, hunterchosen;
    extern bool isally(const gameent* a, const gameent* b);
    extern bool isinvulnerable(gameent* target, gameent* actor);

    extern int vooshgun;
    extern int maptime, maprealtime, maplimit;
    extern int lastspawnattempt;
    extern int following, specmode;
    extern int smoothmove, smoothdist;
    extern int deathscream;
    extern int getdeathstate(gameent* d, int atk, int flags);

    extern const char* colorname(gameent* d, const char* name = NULL, const char* alt = NULL, const char* color = "");
    extern const char* teamcolorname(gameent* d, const char* alt = NULL);
    extern const char* teamcolor(const char* prefix, const char* suffix, int team, const char* alt);
    extern const char* chatcolor(gameent* d);
    const char *mastermodecolor(int n, const char *unknown);
    const char *mastermodeicon(int n, const char *unknown);

    extern string clientmap;

    extern gameent* pointatplayer();
    extern gameent* hudplayer();
    extern gameent* followingplayer(gameent* fallback = NULL);
    extern gameent* getclient(int cn);
    extern gameent* newclient(int cn);
    extern gameent* self;

    extern vector<gameent*> players, clients;

    // client.cpp
    extern void ignore(int cn);
    extern void unignore(int cn);
    extern void switchname(const char *name);
    extern void switchteam(const char *name);
    extern void switchplayermodel(int playermodel);
    extern void switchplayercolor(int playercolor);
    extern void sendmapinfo();
    extern void stopdemo();
    extern void changemap(const char *name, int mode, int muts);
    extern void c2sinfo(bool force = false);
    extern void sendposition(gameent *d, bool reliable = false);
    extern void forceintermission();

    extern bool connected, remote, demoplayback;
    extern bool addmsg(int type, const char* fmt = NULL, ...);
    extern bool isignored(int cn);

    extern int parseplayer(const char* arg);
    extern int gamespeed;

    extern string servdesc;

    extern vector<uchar> messages;

    // projectile.cpp
    extern vector<projectile*> projectiles;

    extern void updateprojectiles(int time);
    extern void updateprojectilelights();
    extern void removeprojectiles(gameent* owner = NULL);
    extern void renderprojectiles();
    extern void preloadprojectiles();
    extern void makeprojectile(gameent* owner, const vec& from, const vec& to, bool islocal, const int id, const int atk, const int type, const int lifetime, const int speed, const float gravity = 0, const float elasticity = 0);
    extern void spawnbouncer(const vec& from, gameent* d, int type);
    extern void bounce(physent* d, const vec& surface);
    extern void collidewithentity(physent* bouncer, physent* collideentity);
    extern void explodeeffects(const int atk, gameent* d, const bool islocal, const int id = 0);
    extern void avoidprojectiles(ai::avoidset& obstacles, const float radius);
    extern void explode(gameent* owner, const int atk, const vec& position, const vec& velocity);

    // weapon.cpp
    enum
    {
        SwayEvent_Land = 0,
        SwayEvent_LandHeavy,
        SwayEvent_Crouch,
        SwayEvent_Switch
    };

    struct swayinfo
    {
        float fade, speed, dist;
        float yaw, pitch, pitchadd, roll;
        vec o, dir;

        struct hudent : dynent
        {
            hudent()
            {
                type = ENT_CAMERA;
            }
        } interpolation;

        struct swayEvent
        {
            int type, millis, duration, factor;
        };
        vector<swayEvent> events;

        swayinfo() : fade(0), speed(0), dist(0), yaw(0), pitch(0), pitchadd(0), roll(0), o(0, 0, 0), dir(0, 0, 0)
        {
        }
        ~swayinfo()
        {
        }

        void updatedirection(gameent* owner);
        void update(gameent* owner);
        void addevent(gameent* owner, int type, int duration, int factor);
        void processevents();
    };

    extern swayinfo sway;

    extern void shoot(gameent *d, const vec &targ);
    extern void updaterecoil(gameent* d, int curtime);
    extern void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction, bool hit = false);
    extern void scanhit(vec& from, vec& to, gameent* d, int atk);
    extern void gibeffect(int damage, const vec &vel, gameent *d);
    extern void updateweapons(int curtime);
    extern void clearweapons();
    extern void gunselect(int gun, gameent* d);
    extern void doweaponchangeffects(gameent* d, int gun = -1);
    extern void weaponswitch(gameent* d);
    extern void autoswitchweapon(int type);
    extern void damageentity(int damage, gameent* d, gameent* actor, int atk, int flags = 0, bool local = true);
    extern void applyhiteffects(int damage, gameent* target, gameent* actor, const vec& hitposition, int atk, int flags, bool local);
    extern void registerhit(dynent* target, gameent* actor, const vec& hitposition, const vec& velocity, int damage, int atk, float dist, int rays = 1, int flags = Hit_Torso);

    extern float intersectdist;

    extern bool isintersecting(dynent *d, const vec &from, const vec &to, float margin = 0, float &dist = intersectdist);

    extern int getweapon(const char* name);
    extern int calculatedamage(int damage, gameent* target, gameent* actor, int atk, int flags = Hit_Torso);
    extern int checkweaponzoom();

    extern vec hudgunorigin(int gun, const vec& from, const vec& to, gameent* d);

    extern dynent* intersectclosest(const vec& from, const vec& to, gameent* at, float margin = 0, float& dist = intersectdist);

    extern vector<hitmsg> hits;

    // monster.cpp
    struct monster;
    extern vector<monster *> monsters;

    extern void clearmonsters();
    extern void preloadmonsters();
    extern void stackmonster(monster *d, physent *o);
    extern void updatemonsters(int curtime);
    extern void rendermonsters();
    extern void suicidemonster(monster *m);
    extern void healmonsters();
    extern void hitmonster(int damage, monster *m, gameent *at, int atk, const vec& velocity, int flags = 0);
    extern void monsterkilled(gameent* monster, int id, int flags = 0);
    extern void checkmonsterinfight(monster *that, gameent *enemy);
    extern void endsp(bool allkilled);
    extern void spsummary(int accuracy);

    extern int getbloodcolor(dynent *d);

    // scoreboard.cpp
    extern void getbestplayers(vector<gameent *> &best);
    extern void getbestteams(vector<int> &best);
    extern void clearteaminfo();
    extern void setteaminfo(int team, int frags);
    extern void removegroupedplayer(gameent *d);

    // render.cpp
    struct playermodelinfo
    {
        const char *directory, *armdirectory, *powerup[5];
        bool ragdoll;
        const int bloodcolor, painsound, diesound[Death_Num], tauntsound;
    };

    extern void saveragdoll(gameent *d);
    extern void clearragdolls();
    extern void moveragdolls();
    extern void syncplayer();
    extern void rendermonster(dynent* d, const char* mdlname, modelattach* attachments, const int attack, const int attackdelay, const int lastaction, const int lastpain, const float fade = 1, const bool ragdoll = false);

    extern int getplayercolor(gameent* d, int team);
    extern int chooserandomplayermodel(int seed);
    extern int getplayermodel(gameent* d);

    extern const playermodelinfo &getplayermodelinfo(gameent *d);

    // hud.cpp
    extern void drawradar(const float x, const float y, const float s);
    extern void setbliptex(const int team, const char* type = "");
    extern void managelowhealthscreen();
    extern void damageblend(const int damage, const int factor = 0);
    extern void setdamagehud(const int damage, gameent* d, gameent* actor);
    extern void addbloodsplatter(const int amount, const int color);
    extern void addscreenflash(const int amount);
    extern void checkitem(int type);

    extern int lowhealthscreen;

    namespace camera
    {
        enum
        {
            CameraEvent_Land = 0,
            CameraEvent_Shake,
            CameraEvent_Spawn,
            CameraEvent_Teleport
        };

        struct camerainfo
        {
            bool isdetached;
            float yaw, pitch, roll, fov;
            float bobfade, bobspeed, bobdist;
            vec direction;

            struct CameraEvent
            {
                int type, millis, duration;
                float factor;
            };
            vector<CameraEvent> events;

            struct ShakeEvent
            {
                int factor, millis, duration, elapsed;
                float intensity;
            };
            vector<ShakeEvent> shakes;

            struct zoominfo
            {
                float progress;

                zoominfo() : progress(0)
                {
                }
                ~zoominfo()
                {
                }

                void update();
                void disable();

                bool isenabled();

                bool isinprogress()
                {
                    return progress > 0;
                }
            };
            zoominfo zoomstate;

            camerainfo() : isdetached(false), yaw(0), pitch(0), roll(0), fov(1), bobfade(0), bobspeed(0), bobdist(0)
            {
                direction = vec(0, 0, 0);
            }
            ~camerainfo()
            {
            }

            void update();
            void addevent(gameent* owner, int type, int duration, float factor = 0);
            void processevents();
            void addshake(int factor);
            void updateshake();

            vec getposition();
        };

        extern camerainfo camera;

        extern void update();
        extern void reset();
        extern void restore(const bool shouldIgnorePitch = false);
        extern void fixrange();

        extern bool allowthirdperson();

        extern int thirdperson;
        extern int zoom;
    }

    namespace announcer
    {
        extern void parseannouncements(gameent* d, gameent* actor, int flags);
        extern void update();
        extern void reset();
    }
}

namespace server
{
    extern const char *modename(int n, const char *unknown = "unknown");
    extern const char *modedesc(int n, const char *unknown = "unknown");
    extern const char *modeprettyname(int n, const char *unknown = "unknown");
    extern const char *mastermodename(int n, const char *unknown = "unknown");
    extern const char *getdemofile(const char *file, bool init);

    extern void startintermission();
    extern void stopdemo();
    extern void timeupdate(int secs);
    extern void forcemap(const char *map, int mode, int muts);
    extern void forcepaused(bool paused);
    extern void forcegamespeed(int speed);
    extern void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen = MAXSTRLEN);
    extern void updateroundstate(int state, bool send = true);
    extern void checkroundwait();
    extern void endround();
    extern void checkplayers(bool timeisup = false);

    extern bool serveroption(const char *arg);
    extern bool delayspawn(int type);
    extern bool checkovertime(bool timeisup = false);
    extern bool gamewaiting;

    extern int msgsizelookup(int msg);
}

#endif

