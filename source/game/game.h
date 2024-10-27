#ifndef __GAME_H__
#define __GAME_H__

#include "cube.h"

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
};

// console message types

enum
{
    CON_CHAT     = 1<<8,
    CON_TEAMCHAT = 1<<9,
    CON_GAMEINFO = 1<<10,
    CON_FRAGINFO = 1<<11
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

// network messages codes, c2s, c2c, s2c
enum
{
    N_CONNECT = 0, N_SERVINFO, N_WELCOME, N_INITCLIENT, N_POS, N_TEXT, N_SOUND, N_CDIS,
    N_SHOOT, N_EXPLODE, N_SUICIDE,
    N_DIED, N_DAMAGE, N_HITPUSH, Net_DamageProjectile, N_SHOTEVENT, N_SHOTFX, N_EXPLODEFX, N_REGENERATE, N_REPAMMO,
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
    NUMMSG
};

static const int msgsizes[] =               // size inclusive message token, 0 for variable or not-checked sizes
{
    N_CONNECT, 0, N_SERVINFO, 0, N_WELCOME, 1, N_INITCLIENT, 0, N_POS, 0, N_TEXT, 0, N_SOUND, 2, N_CDIS, 2,
    N_SHOOT, 0, N_EXPLODE, 0, N_SUICIDE, 1,
    N_DIED, 7, N_DAMAGE, 11, N_HITPUSH, 7, Net_DamageProjectile, 9, N_SHOTEVENT, 3, N_SHOTFX, 11, N_EXPLODEFX, 6, N_REGENERATE, 2, N_REPAMMO, 3,
    N_TRYSPAWN, 1, N_SPAWNSTATE, 9, N_SPAWN, 3, N_FORCEDEATH, 2,
    N_GUNSELECT, 2, N_TAUNT, 1,
    N_ANNOUNCE, 3,
    N_MAPCHANGE, 0, N_MAPVOTE, 0, N_TEAMINFO, 0, N_ITEMSPAWN, 2, N_ITEMPICKUP, 2, N_ITEMACC, 3,
    N_PING, 2, N_PONG, 2, N_CLIENTPING, 2,
    N_TIMEUP, 2, N_FORCEINTERMISSION, 1,
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
    -1
};

#define VALHALLA_SERVER_PORT 21217
#define VALHALLA_LANINFO_PORT 21216
#define VALHALLA_MASTER_PORT 21215
#define PROTOCOL_VERSION 1 // bump when protocol changes
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
    int gunselect, gunwait, primary;
    int ammo[NUMGUNS];
    int aitype, skill;
    int poweruptype, powerupmillis;
    int role;

    gamestate() : maxhealth(100), aitype(AI_NONE), skill(0) {}

    bool canpickup(int type)
    {
        if(!validitem(type) || role == ROLE_BERSERKER || role == ROLE_ZOMBIE) return false;
        itemstat &is = itemstats[type-I_AMMO_SG];
        switch(type)
        {
            case I_HEALTH:
            case I_SUPERHEALTH:
            case I_MEGAHEALTH:
                return health<is.max;

            case I_YELLOWSHIELD:
            case I_REDSHIELD:
                return shield<is.max;

            case I_DDAMAGE:
            case I_HASTE:
            case I_ARMOR:
            case I_UAMMO:
            case I_AGILITY:
            case I_INVULNERABILITY:
                if(powerupmillis < is.info || poweruptype == is.info)
                {
                    return true;
                }
                return false;

            default:
                return ammo[is.info]<is.max;
        }
    }

    void pickup(int type)
    {
        if(!validitem(type) || role == ROLE_BERSERKER || role == ROLE_ZOMBIE) return;
        itemstat &is = itemstats[type-I_AMMO_SG];
        switch(type)
        {
            case I_HEALTH:
            case I_SUPERHEALTH:
            case I_MEGAHEALTH:
                health = min(health+is.add, is.max);
                break;

            case I_YELLOWSHIELD:
            case I_REDSHIELD:
                shield = min(shield+is.add, is.max);
                break;

            case I_DDAMAGE:
            case I_ARMOR:
            case I_HASTE:
            case I_UAMMO:
            case I_AGILITY:
            case I_INVULNERABILITY:
                poweruptype = is.info;
                powerupmillis = min(powerupmillis+is.add, is.max);
                break;

            default:
                ammo[is.info] = min(ammo[is.info]+is.add, is.max);
                break;
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

    void infect()
    {
        role = ROLE_ZOMBIE;
        maxhealth = health = 1000;
        resetitems();
        resetweapons();
        ammo[GUN_ZOMBIE] = 1;
        gunselect = GUN_ZOMBIE;
    }

    void makeberserker()
    {
        role = ROLE_BERSERKER;
        maxhealth = health = maxhealth*2;
        loopi(NUMGUNS)
        {
            if(!ammo[i] || i == GUN_INSTA || i == GUN_ZOMBIE) continue;
            if(i == GUN_PISTOL) ammo[i] = 100;
            else ammo[i] = max(itemstats[i-GUN_SCATTER].max, itemstats[i-GUN_SCATTER].add*5);
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
            maxhealth = health = 1;
            gunselect = GUN_INSTA;
            ammo[GUN_INSTA] = 1;
        }
        else if(m_effic(mutators))
        {
            maxhealth = health = 200;
            loopi(NUMGUNS-3) baseammo(i);
            gunselect = GUN_SMG;
        }
        else if(m_tactics(mutators))
        {
            maxhealth = health = 150;
            ammo[GUN_PISTOL] = 100;
            int spawngun1 = rnd(5)+1, spawngun2;
            gunselect = spawngun1;
            baseammo(spawngun1, m_noitems(mutators) ? 5 : 3);
            do spawngun2 = rnd(5)+1; while(spawngun1==spawngun2);
            baseammo(spawngun2, m_noitems(mutators) ? 5 : 3);
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

    // subtract damage/shield points and apply damage here
    int dodamage(int damage, bool environment = false)
    {
        if(shield && !environment)
        { // only if the player has shield points and damage is not caused by the environment
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

const int MAXNAMELEN = 15;

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

struct gameent : dynent, gamestate
{
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, privilege, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int respawned, suicided;
    int lastpain, lasthurt;
    int lastaction, lastattack, lasthit;
    int deathtype;
    int attacking;
    int lasttaunt, lastfootstep, lastland, lastyelp, lastswitch;
    int lastpickup, lastpickupmillis, flagpickup;
    int frags, flags, deaths, points, totaldamage, totalshots, lives, holdingflag;
    editinfo *edit;
    float deltayaw, deltapitch, deltaroll, newyaw, newpitch, newroll, pitchrecoil;
    int smoothmillis;

    int attackchan, idlechan, powerupchan;
    int attacksound, idlesound, powerupsound;

    string name, info;
    int team, playermodel, playercolor;
    ai::aiinfo *ai;
    int ownernum, lastnode;
    bool respawnqueued, ghost;

    vec muzzle, eject;

    gameent() : weight(100),
                clientnum(-1), privilege(PRIV_NONE), lastupdate(0), plag(0), ping(0),
                lifesequence(0), respawned(-1), suicided(-1),
                lastpain(0), lasthurt(0),
                lastfootstep(0), lastland(0), lastyelp(0), lastswitch(0),
                frags(0), flags(0), deaths(0), points(0), totaldamage(0), totalshots(0), lives(3), holdingflag(0),
                edit(NULL), pitchrecoil(0), smoothmillis(-1),
                attackchan(-1), idlechan(-1), powerupchan(-1),
                team(0), playermodel(-1), playercolor(0), ai(NULL), ownernum(-1),
                muzzle(-1, -1, -1), eject(-1, -1, -1)
    {
        name[0] = info[0] = 0;
        ghost = false;
        respawn();
    }
    ~gameent()
    {
        freeeditinfo(edit);
        if(ai) delete ai;
        if(attackchan >= 0) stopsound(attacksound, attackchan);
        if(idlechan >= 0) stopsound(idlesound, idlechan);
        if(powerupchan >= 0) stopsound(powerupsound, powerupchan);
    }

    void hitpush(int damage, const vec &dir, gameent *actor, int atk)
    {
        vec push(dir);
        if (isweaponprojectile(attacks[atk].projectile))
        {
            falling.z = 1; // projectiles reset gravity while falling so trick jumps are more rewarding and players are pushed further
        }
        if(role == ROLE_ZOMBIE) damage *= 3; // zombies are pushed "a bit" more
        push.mul((actor==this && attacks[atk].exprad ? EXP_SELFPUSH : 1.0f)*attacks[atk].hitpush*damage/weight);
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

    void stopweaponsound()
    {
        if(attackchan >= 0) stopsound(attacksound, attackchan, 300);
        attacksound = attackchan = -1;
    }

    void stopidlesound()
    {
        if(idlechan >= 0) stopsound(idlesound, idlechan, 400);
        idlesound = idlechan = -1;
    }

    void respawn()
    {
        dynent::reset();
        gamestate::respawn();
        respawned = suicided = -1;
        lastaction = 0;
        lastattack = -1;
        deathtype = 0;
        attacking = ACT_IDLE;
        lasttaunt = 0;
        lastpickup = -1;
        lastpickupmillis = 0;
        flagpickup = 0;
        lastnode = -1;
        lasthit = 0;
        stopweaponsound();
        stoppowerupsound();
        respawnqueued = false;
    }

    void stoppowerupsound()
    {
        if(powerupchan >= 0)
        {
            stopsound(powerupsound, powerupchan, 500);
            powerupchan = -1;
        }
    }

    bool gibbed()
    {
        return (state == CS_DEAD && health <= HEALTH_GIB) || deathtype == DEATH_GIB;
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
    extern vector<extentity *> ents;

    extern void preloadentities();
    extern void renderentities();
    extern void checkitems(gameent *d);
    extern void updatepowerups(int time, gameent *d);
    extern void resetspawns();
    extern void spawnitems(bool force = false);
    extern void putitems(packetbuf &p);
    extern void setspawn(int i, bool on);
    extern void teleport(int n, gameent *d);
    extern void pickupeffects(int n, gameent *d);
    extern void teleporteffects(gameent *d, int tp, int td, bool local = true);
    extern void jumppadeffects(gameent *d, int jp, bool local = true);
}

namespace physics
{
    extern void moveplayer(gameent* pl, int moveres, bool local);
    extern void crouchplayer(gameent* pl, int moveres, bool local);
    extern void physicsframe();
    extern void updatephysstate(gameent* d);

    extern bool hasbounced(projectile* proj, float secs, float elasticity, float waterfric, float gravity);
    extern bool isbouncing(projectile* proj, float elasticity, float waterfric, float gravity);
    extern bool allowmove(gameent* d);

    extern int physsteps;
    extern int liquidtransition(physent* d, int material, bool isinwater);
}

namespace game
{
    extern int gamemode, mutators;

    struct clientmode
    {
        virtual ~clientmode() {}

        virtual void preload() {}
        virtual float clipconsole(float w, float h) { return 0; }
        virtual void drawhud(gameent *d, int w, int h) {}
        virtual void rendergame() {}
        virtual void respawned(gameent *d) {}
        virtual void setup() {}
        virtual void checkitems(gameent *d) {}
        virtual int respawnwait(gameent *d, bool seconds = false) { return 0; }
        virtual void pickspawn(gameent *d) { findplayerspawn(d, -1, m_teammode ? d->team : 0); }
        virtual void senditems(packetbuf &p) {}
        virtual void removeplayer(gameent *d) {}
        virtual void gameover() {}
        virtual bool hidefrags() { return false; }
        virtual int getteamscore(int team) { return 0; }
        virtual void getteamscores(vector<teamscore> &scores) {}
        virtual bool canfollow(gameent *s, gameent *f) { return true; }
        virtual void aifind(gameent *d, ai::aistate &b, vector<ai::interest> &interests) {}
        virtual bool aicheck(gameent *d, ai::aistate &b) { return false; }
        virtual bool aidefend(gameent *d, ai::aistate &b) { return false; }
        virtual bool aipursue(gameent *d, ai::aistate &b) { return false; }
    };

    extern clientmode *cmode;
    extern void setclientmode();

    // game.cpp
    extern int vooshgun;
    extern string clientmap;
    extern int maptime, maprealtime, maplimit;
    extern gameent *self;
    extern vector<gameent *> players, clients;
    extern int lastspawnattempt;
    extern int following;
    extern int smoothmove, smoothdist;
    extern int gore;

    extern bool clientoption(const char *arg);
    extern bool gamewaiting, betweenrounds, hunterchosen;
    extern bool isally(gameent *a, gameent *b);
    extern bool isinvulnerable(gameent *target, gameent *actor);

    extern gameent *getclient(int cn);
    extern gameent *newclient(int cn);
    extern const char *colorname(gameent *d, const char *name = NULL, const char *alt = NULL, const char *color = "");
    extern const char *teamcolorname(gameent *d, const char *alt = NULL);
    extern const char *teamcolor(const char *prefix, const char *suffix, int team, const char *alt);
    extern const char *chatcolor(gameent *d);
    extern gameent *pointatplayer();
    extern gameent *hudplayer();
    extern gameent *followingplayer(gameent *fallback = NULL);
    extern void taunt(gameent *d);
    extern void stopfollowing();
    extern void checkfollow();
    extern void nextfollow(int dir = 1);
    extern void clientdisconnected(int cn, bool notify = true);
    extern void clearclients(bool notify = true);
    extern void startgame();
    extern void spawnplayer(gameent *d);
    extern void spawneffect(gameent *d);
    extern void respawn();
    extern void deathstate(gameent *d, bool restore = false);
    extern void damagehud(int damage, gameent *d, gameent *actor);
    extern void damageentity(int damage, gameent *d, gameent *actor, int atk, int flags = 0, bool local = true);
    extern void writeobituary(gameent *d, gameent *actor, int atk, int flags = 0);
    extern void checkannouncements(gameent *actor, int flags);
    extern void kill(gameent *d, gameent *actor, int atk, int flags = KILL_NONE);
    extern void timeupdate(int timeremain);
    extern void msgsound(int n, physent *d = NULL);
    extern void doaction(int act);
    extern void addroll(gameent *d, float amount);
    extern void hurt(gameent* d);
    extern void suicide(gameent* d);
    const char *mastermodecolor(int n, const char *unknown);
    const char *mastermodeicon(int n, const char *unknown);

    // client.cpp
    extern bool connected, remote, demoplayback;
    extern bool isignored(int cn);

    extern string servdesc;

    extern vector<uchar> messages;

    extern int parseplayer(const char *arg);
    extern int gamespeed;

    extern void ignore(int cn);
    extern void unignore(int cn);
    extern bool addmsg(int type, const char *fmt = NULL, ...);
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

    // weapon.cpp
    extern vector<hitmsg> hits;

    extern int getweapon(const char *name);
    extern void shoot(gameent *d, const vec &targ);
    extern void shoteffects(int atk, const vec &from, const vec &to, gameent *d, bool local, int id, int prevaction, bool hit = false);
    extern void damageeffect(int damage, dynent *d, vec hit, int atk, bool headshot = false);
    extern void gibeffect(int damage, const vec &vel, gameent *d, bool force = false);
    extern int calcdamage(int damage, gameent *target, gameent *actor, int atk, int flags = HIT_TORSO);
    extern float intersectdist;
    extern bool isintersecting(dynent *d, const vec &from, const vec &to, float margin = 0, float &dist = intersectdist);
    extern dynent *intersectclosest(const vec &from, const vec &to, gameent *at, float margin = 0, float &dist = intersectdist);
    extern bool intersecthead(dynent *d, const vec &from, const vec &to, float &dist = intersectdist);
    extern void updateweapons(int curtime);
    extern void playswitchsound(gameent* d, int gun);
    extern void gunselect(int gun, gameent *d);
    extern void weaponswitch(gameent *d);
    extern void removeprojectiles(gameent* owner = NULL);
    extern void applyhiteffects(int damage, dynent* target, gameent* at, const vec hit, int atk, int flags, bool local);
    extern void registerhit(int damage, dynent *target, gameent* at, const vec hit, const vec& velocity, int atk, float dist, int rays = 1, int flags = HIT_TORSO);

    // projectile.cpp
    extern vector<projectile*> projectiles;

    extern projectile* getprojectile(int id, gameent* owner = NULL);

    extern void updateprojectiles(int time);
    extern void removeprojectiles(gameent* owner);
    extern void renderprojectiles();
    extern void preloadprojectiles();
    extern void makeprojectile(gameent* owner, const vec& from, const vec& to, bool islocal, int id, int atk, int type, int lifetime, int speed, float gravity = 0, float elasticity = 0);
    extern void spawnbouncer(const vec& from, gameent* d, int type);
    extern void bounce(physent* d, const vec& surface);
    extern void collidewithentity(physent* bouncer, physent* collideentity);
    extern void explodeeffects(int atk, gameent* d, bool islocal, int id = 0);
    extern void avoidprojectiles(ai::avoidset& obstacles, float radius);

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
    extern void hitmonster(int damage, monster *m, gameent *at, int atk, int flags = 0);
    extern void monsterkilled(int flags = 0);
    extern void checkefficientenemy(gameent *that, gameent *enemy);
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
        int bloodcolor, painsound, diesound, tauntsound;
    };

    extern void saveragdoll(gameent *d);
    extern void clearragdolls();
    extern void moveragdolls();
    extern const playermodelinfo &getplayermodelinfo(gameent *d);
    extern int getplayercolor(gameent *d, int team);
    extern int chooserandomplayermodel(int seed);
    extern int getplayermodel(gameent *d);
    extern void syncplayer();
    extern void swayhudgun(int curtime);
    extern void renderai(dynent *d, const char *mdlname, modelattach *attachments, int hold, int attack, int attackdelay, int lastaction, int lastpain, float fade = 1, bool ragdoll = false);
    extern vec hudgunorigin(int gun, const vec &from, const vec &to, gameent *d);
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

