// the interface the game uses to access the engine

extern int curtime;                     // current frame time
extern int lastmillis;                  // last time
extern int elapsedtime;                 // elapsed frame time
extern int totalmillis;                 // total elapsed time
extern uint totalsecs;

enum
{
    MATF_INDEX_SHIFT  = 0,
    MATF_VOLUME_SHIFT = 2,
    MATF_CLIP_SHIFT   = 5,
    MATF_FLAG_SHIFT   = 8,

    MATF_INDEX  = 3 << MATF_INDEX_SHIFT,
    MATF_VOLUME = 7 << MATF_VOLUME_SHIFT,
    MATF_CLIP   = 7 << MATF_CLIP_SHIFT,
    MATF_FLAGS  = 0xFF << MATF_FLAG_SHIFT
};

enum // cube empty-space materials
{
    MAT_AIR      = 0,                      // the default, fill the empty space with air
    MAT_WATER    = 1 << MATF_VOLUME_SHIFT, // fill with water, showing waves at the surface
    MAT_LAVA     = 2 << MATF_VOLUME_SHIFT, // fill with lava
    MAT_GLASS    = 3 << MATF_VOLUME_SHIFT, // behaves like clip but is blended blueish

    MAT_NOCLIP   = 1 << MATF_CLIP_SHIFT,   // collisions always treat cube as empty
    MAT_CLIP     = 2 << MATF_CLIP_SHIFT,   // collisions always treat cube as solid
    MAT_GAMECLIP = 3 << MATF_CLIP_SHIFT,   // game specific clip material

    MAT_DEATH    = 1  << MATF_FLAG_SHIFT,  // force player suicide
    MAT_NOGI     = 2  << MATF_FLAG_SHIFT,  // disable global illumination
    MAT_ALPHA    = 4  << MATF_FLAG_SHIFT,  // alpha blended
    MAT_DAMAGE   = 8  << MATF_FLAG_SHIFT,  // damage player
    MAT_CLIMB    = 16 << MATF_FLAG_SHIFT   // ladder-like behaviour
};

inline bool isliquidmaterial(int mat)  { return mat == MAT_WATER || mat == MAT_LAVA;  }
inline bool issolidmaterial(int mat)   { return mat == MAT_GLASS;                     }
inline bool isdeadlymaterial(int mat)  { return mat == MAT_LAVA;                      }

enum { RAY_BB = 1, RAY_POLY = 3, RAY_ALPHAPOLY = 7, RAY_ENTS = 9, RAY_CLIPMAT = 16, RAY_LIQUIDMAT = 32, RAY_SKIPFIRST = 64, RAY_EDITMAT = 128, RAY_PASS = 256 };

extern float raycube   (const vec &o, const vec &ray,     float radius = 0, int mode = RAY_CLIPMAT, int size = 0, extentity *t = 0);
extern float raycubepos(const vec &o, const vec &ray, vec &hit, float radius = 0, int mode = RAY_CLIPMAT, int size = 0);
extern float rayfloor  (const vec &o, vec &floor, int mode = 0, float radius = 0);
extern bool  raycubelos(const vec &o, const vec &dest, vec &hitpos);

// octaedit

enum { EDIT_FACE = 0, EDIT_TEX, EDIT_MAT, EDIT_FLIP, EDIT_COPY, EDIT_PASTE, EDIT_ROTATE, EDIT_REPLACE, EDIT_DELCUBE, EDIT_CALCLIGHT, EDIT_REMIP, EDIT_VSLOT, EDIT_UNDO, EDIT_REDO };

struct selinfo
{
    int corner;
    int cx, cxs, cy, cys;
    ivec o, s;
    int grid, orient;
    selinfo() : corner(0), cx(0), cxs(0), cy(0), cys(0), o(0, 0, 0), s(0, 0, 0), grid(8), orient(0) {}
    int size() const    { return s.x*s.y*s.z; }
    int us(int d) const { return s[d]*grid; }
    bool operator==(const selinfo &sel) const { return o==sel.o && s==sel.s && grid==sel.grid && orient==sel.orient; }
    bool validate()
    {
        extern int worldsize;
        if(grid <= 0 || grid >= worldsize) return false;
        if(o.x >= worldsize || o.y >= worldsize || o.z >= worldsize) return false;
        if(o.x < 0) { s.x -= (grid - 1 - o.x)/grid; o.x = 0; }
        if(o.y < 0) { s.y -= (grid - 1 - o.y)/grid; o.y = 0; }
        if(o.z < 0) { s.z -= (grid - 1 - o.z)/grid; o.z = 0; }
        s.x = clamp(s.x, 0, (worldsize - o.x)/grid);
        s.y = clamp(s.y, 0, (worldsize - o.y)/grid);
        s.z = clamp(s.z, 0, (worldsize - o.z)/grid);
        return s.x > 0 && s.y > 0 && s.z > 0;
    }
};

struct editinfo;
extern editinfo *localedit;

extern bool editmode;

extern int shouldpacktex(int index);
extern bool packeditinfo(editinfo *e, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackeditinfo(editinfo *&e, const uchar *inbuf, int inlen, int outlen);
extern void freeeditinfo(editinfo *&e);
extern void pruneundos(int maxremain = 0);
extern bool packundo(int op, int &inlen, uchar *&outbuf, int &outlen);
extern bool unpackundo(const uchar *inbuf, int inlen, int outlen);
extern bool noedit(bool view = false, bool msg = true);
extern void toggleedit(bool force = true);
extern void mpeditface(int dir, int mode, selinfo &sel, bool local);
extern void mpedittex(int tex, int allfaces, selinfo &sel, bool local);
extern bool mpedittex(int tex, int allfaces, selinfo &sel, ucharbuf &buf);
extern void mpeditmat(int matid, int filter, selinfo &sel, bool local);
extern void mpflip(selinfo &sel, bool local);
extern void mpcopy(editinfo *&e, selinfo &sel, bool local);
extern void mppaste(editinfo *&e, selinfo &sel, bool local);
extern void mprotate(int cw, selinfo &sel, bool local);
extern void mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, bool local);
extern bool mpreplacetex(int oldtex, int newtex, bool insel, selinfo &sel, ucharbuf &buf);
extern void mpdelcube(selinfo &sel, bool local);
extern void mpremip(bool local);
extern bool mpeditvslot(int delta, int allfaces, selinfo &sel, ucharbuf &buf);
extern void mpcalclight(bool local);
extern float rayent(const vec& o, const vec& ray, float radius, int mode, int size, int& orient, int& ent);

// command
extern int variable(const char *name, int min, int cur, int max, int *storage, identfun fun, int flags);
extern float fvariable(const char *name, float min, float cur, float max, float *storage, identfun fun, int flags);
extern char *svariable(const char *name, const char *cur, char **storage, identfun fun, int flags);
extern void setvar(const char *name, int i, bool dofunc = true, bool doclamp = true);
extern void setfvar(const char *name, float f, bool dofunc = true, bool doclamp = true);
extern void setsvar(const char *name, const char *str, bool dofunc = true);
extern void setvarchecked(ident *id, int val);
extern void setfvarchecked(ident *id, float val);
extern void setsvarchecked(ident *id, const char *val);
extern void touchvar(const char *name);
extern int getvar(const char *name);
extern int getvarmin(const char *name);
extern int getvarmax(const char *name);
extern bool identexists(const char *name);
extern ident *getident(const char *name);
extern ident *newident(const char *name, int flags = 0);
extern ident *readident(const char *name);
extern ident *writeident(const char *name, int flags = 0);
extern bool addcommand(const char *name, identfun fun, const char *narg, int type = ID_COMMAND);
template<class F> static inline bool addcommand(const char *name, F *fun, const char *narg, int type = ID_COMMAND) { return ::addcommand(name, (identfun)fun, narg, type); }
extern uint *compilecode(const char *p);
extern void keepcode(uint *p);
extern void freecode(uint *p);
extern void executeret(const uint *code, tagval &result = *commandret);
extern void executeret(const char *p, tagval &result = *commandret);
extern void executeret(ident *id, tagval *args, int numargs, bool lookup = false, tagval &result = *commandret);
extern char *executestr(const uint *code);
extern char *executestr(const char *p);
extern char *executestr(ident *id, tagval *args, int numargs, bool lookup = false);
extern char *execidentstr(const char *name, bool lookup = false);
extern int execute(const uint *code);
extern int execute(const char *p);
extern int execute(ident *id, tagval *args, int numargs, bool lookup = false);
extern int execident(const char *name, int noid = 0, bool lookup = false);
extern float executefloat(const uint *code);
extern float executefloat(const char *p);
extern float executefloat(ident *id, tagval *args, int numargs, bool lookup = false);
extern float execidentfloat(const char *name, float noid = 0, bool lookup = false);
extern bool executebool(const uint *code);
extern bool executebool(const char *p);
extern bool executebool(ident *id, tagval *args, int numargs, bool lookup = false);
extern bool execidentbool(const char *name, bool noid = false, bool lookup = false);
extern bool execfile(const char *cfgfile, bool msg = true);
extern void alias(const char *name, const char *action);
extern void alias(const char *name, tagval &v);
extern const char *getalias(const char *name);
extern const char *escapestring(const char *s);
extern const char *escapeid(const char *s);
static inline const char *escapeid(ident &id) { return escapeid(id.name); }
extern bool validateblock(const char *s);
extern void explodelist(const char *s, vector<char *> &elems, int limit = -1);
extern char *indexlist(const char *s, int pos);
extern int listlen(const char *s);
extern void printvar(ident *id);
extern void printvar(ident *id, int i);
extern void printfvar(ident *id, float f);
extern void printsvar(ident *id, const char *s);
extern int clampvar(ident *id, int i, int minval, int maxval);
extern float clampfvar(ident *id, float f, float minval, float maxval);
extern void loopiter(ident *id, identstack &stack, const tagval &v);
extern void loopend(ident *id, identstack &stack);

#define loopstart(id, stack) if((id)->type != ID_ALIAS) return; identstack stack;
static inline void loopiter(ident *id, identstack &stack, int i) { tagval v; v.setint(i); loopiter(id, stack, v); }
static inline void loopiter(ident *id, identstack &stack, float f) { tagval v; v.setfloat(f); loopiter(id, stack, v); }
static inline void loopiter(ident *id, identstack &stack, const char *s) { tagval v; v.setstr(newstring(s)); loopiter(id, stack, v); }

// console

enum
{
    CON_INFO     = 1<<0,
    CON_WARN     = 1<<1,
    CON_ERROR    = 1<<2,
    CON_DEBUG    = 1<<3,
    CON_INIT     = 1<<4,
    CON_ECHO     = 1<<5,
    CON_NOTICE   = 1<<6,

    CON_CHAT     = 1<<8,
    CON_TEAMCHAT = 1<<9,
    CON_GAMEINFO = 1<<10,
    CON_FRAGINFO = 1<<11,

    CON_FLAGS = 0xFFFF,
    CON_TAG_SHIFT = 16,
    CON_TAG_MASK = (0x7FFF << CON_TAG_SHIFT)
};

extern void conoutf(const char *s, ...) PRINTFARGS(1, 2);
extern void conoutf(int type, const char *s, ...) PRINTFARGS(2, 3);
extern void conoutf(int type, int tag, const char *s, ...) PRINTFARGS(3, 4);
extern void conoutfv(int type, const char *fmt, va_list args);

extern FILE *getlogfile();
extern void setlogfile(const char *fname);
extern void closelogfile();
extern void logoutfv(const char *fmt, va_list args);
extern void logoutf(const char *fmt, ...) PRINTFARGS(1, 2);

// octa
extern bool isemptycube(vec v);
extern int lookupmaterial(const vec &v);
extern int lookuptextureeffect(const vec &v);

static inline bool insideworld(const vec &o)
{
    extern int worldsize;
    return o.x>=0 && o.x<worldsize && o.y>=0 && o.y<worldsize && o.z>=0 && o.z<worldsize;
}

static inline bool insideworld(const ivec &o)
{
    extern int worldsize;
    return uint(o.x)<uint(worldsize) && uint(o.y)<uint(worldsize) && uint(o.z)<uint(worldsize);
}

// world
extern bool emptymap(int factor, bool force, const char *mname = "", bool usecfg = true);
extern bool enlargemap(bool force);
extern int findentity(int type, int index = 0, int attr1 = -1, int attr2 = -1);
extern void findents(int low, int high, bool notspawned, const vec &pos, const vec &radius, vector<int> &found);
extern void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, int attr5, bool local);
extern vec getselpos();
extern int getworldsize();
extern int getmapversion();
extern void renderentcone(const extentity &e, const vec &dir, float radius, float angle);
extern void renderentarrow(const extentity &e, const vec &dir, float radius);
extern void renderentattachment(const extentity &e);
extern void renderentsphere(const extentity &e, float radius);
extern void renderentring(const extentity &e, float radius, int axis = 0);

// main
extern void fatal(const char *s, ...) PRINTFARGS(1, 2);
extern void drawquad(float x, float y, float w, float h, float tx1 = 0, float ty1 = 0, float tx2 = 1, float ty2 = 1, bool flipx = false, bool flipy = false);
extern int currentversion;

// texture

struct VSlot;
extern void packvslot(vector<uchar> &buf, int index);
extern void packvslot(vector<uchar> &buf, const VSlot *vs);
extern bool settexture(const char* name, int clamp = 0);

// renderlights

enum { L_NOSHADOW = 1<<0, L_NODYNSHADOW = 1<<1, L_VOLUMETRIC = 1<<2, L_NOSPEC = 1<<3, L_SMALPHA = 1<<4 };
void addgamelight(vec o, vec color, float radius);

// dynlight
enum
{
    DL_SHRINK = 1<<8,
    DL_EXPAND = 1<<9,
    DL_FLASH  = 1<<10
};

extern void adddynlight(const vec &o, float radius, const vec &color, int fade = 0, int peak = 0, int flags = 0, float initradius = 0, const vec &initcolor = vec(0, 0, 0), physent *owner = NULL, const vec &dir = vec(0, 0, 0), int spot = 0);
extern void removetrackeddynlights(physent *owner = NULL);

// rendergl
extern physent *camera1;
extern vec worldpos, camdir, camright, camup;

extern vec calcavatarpos(const vec &pos, float dist);
extern vec calcmodelpreviewpos(const vec &radius, float &yaw);

extern vec minimapcenter, minimapradius, minimapscale;
extern void bindminimap();

extern matrix4 hudmatrix;
extern void resethudmatrix();
extern void pushhudmatrix();
extern void flushhudmatrix(bool flushparams = true);
extern void pophudmatrix(bool flush = true, bool flushparams = true);
extern void pushhudscale(float sx, float sy = 0);
extern void pushhudtranslate(float tx, float ty, float sx = 0, float sy = 0);
extern void resethudshader();

// renderparticles
enum
{
    PART_BLOOD = 0, PART_BLOOD2,
    PART_WATER, PART_GLASS, PART_SMOKE, PART_STEAM, PART_FLAME, PART_SNOW,
    PART_TRAIL, PART_TRAIL_PROJECTILE, PART_TRAIL_STRAIGHT,
    PART_LIGHTNING,
    PART_EXPLOSION1, PART_EXPLOSION2, PART_EXPLOSION3,
    PART_SPARK, PART_SPARK2, PART_EXPLODE1, PART_EXPLODE2, PART_EXPLODE3, PART_EXPLODE4, PART_ELECTRICITY, PART_EDIT, PART_ORB, PART_RING,
    PART_SPLASH, PART_BUBBLE,
    PART_MUZZLE_FLASH, PART_MUZZLE_FLASH2, PART_MUZZLE_FLASH3, PART_MUZZLE_FLASH4, PART_MUZZLE_FLASH5,
    PART_MUZZLE_SMOKE, PART_SPARKS, PART_COMICS,
    PART_GAME_ICONS, PART_EDITOR_ICONS,
    PART_TEXT,
    PART_METER, PART_METER_VS,
    PART_LENS_FLARE
};

extern bool canaddparticles();
extern void regular_particle_splash(int type, int num, int fade, const vec &p, int color = 0xFFFFFF, float size = 1.0f, int radius = 150, int gravity = 2, int delay = 0, float maxsize = 0);
extern void regular_particle_flame(int type, const vec &p, float radius, float height, int color, int density = 3, float scale = 2.0f, float speed = 200.0f, float fade = 600.0f, int gravity = -15);
extern void particle_splash(int type, int num, int fade, const vec &p, int color = 0xFFFFFF, float size = 1.0f, int radius = 150, int gravity = 2, float maxsize = 0);
extern void particle_trail(int type, int fade, const vec &from, const vec &to, int color = 0xFFFFFF, float size = 1.0f, int gravity = 20, float maxsize = 0);
extern void particle_text(const vec &s, const char *t, int type, int fade = 2000, int color = 0xFFFFFF, float size = 2.0f, int gravity = 0, const char *font = "wide", const char *language = NULL);
extern void particle_textcopy(const vec &s, const char *t, int type, int fade = 2000, int color = 0xFFFFFF, float size = 2.0f, int gravity = 0, const char *font = "wide", const char *language = NULL);
extern void particle_icon(const vec &s, int ix, int iy, int type, int fade = 2000, int color = 0xFFFFFF, float size = 2.0f, int gravity = 0);
extern void particle_hud_mark(const vec &s, int ix, int iy, int type, int fade, int color, float size);
extern void particle_hud_text(const vec &s, const char *t, int type, int fade, int color, float size, const char *font, const char *language = NULL);
extern void particle_meter(const vec &s, float val, int type, int fade = 1, int color = 0xFFFFFF, int color2 = 0xFFFFF, float size = 2.0f);
extern void particle_flare(const vec &p, const vec &dest, int fade, int type, int color = 0xFFFFFF, float size = 0.28f, physent *owner = NULL, float maxsize = 0);
extern void particle_fireball(const vec &dest, float max, int type, int fade = -1, int color = 0xFFFFFF, float size = 4.0f);
extern void removetrackedparticles(physent *owner = NULL);

// stain
enum
{
    STAIN_BLOOD = 0,
    STAIN_BULLETHOLE_SMALL, STAIN_BULLETHOLE_BIG, STAIN_PUNCH_HOLE, STAIN_GLASS_HOLE,
    STAIN_SCORCH, STAIN_GLOW1, STAIN_GLOW2,
    STAIN_SNOW, STAIN_RAIN
};

extern void addstain(int type, const vec &center, const vec &surface, float radius, const bvec &color = bvec(0xFF, 0xFF, 0xFF), int info = 0);

static inline void addstain(int type, const vec &center, const vec &surface, float radius, int color, int info = 0)
{
    addstain(type, center, surface, radius, bvec::hexcolor(color), info);
}

// worldio
extern bool load_world(const char *mname, const char *cname = NULL);
extern bool save_world(const char *mname, bool nolms = false);
extern void fixmapname(char *name);
extern uint getmapcrc();
extern void clearmapcrc();
extern bool loadents(const char *fname, vector<entity> &ents, uint *crc = NULL);

// physics
enum
{
    PHYSEVENT_JUMP = 0,
    PHYSEVENT_LAND_LIGHT,
    PHYSEVENT_LAND_HEAVY,
    PHYSEVENT_FOOTSTEP,
    PHYSEVENT_RAGDOLL_COLLIDE,
    PHYSEVENT_LIQUID_IN,
    PHYSEVENT_LIQUID_OUT,
};

extern vec collidewall;
extern int collideinside;
extern physent *collideplayer;

extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0.0f, bool playercol = true, bool insideplayercol = false);
extern void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space);
extern bool overlapsdynent(const vec &o, float radius);
extern bool movecamera(physent *pl, const vec &dir, float dist, float stepdist);
extern void dropenttofloor(entity *e);
extern bool droptofloor(vec &o, float radius, float height);

extern void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m);
extern void vectoyawpitch(const vec &v, float &yaw, float &pitch);
extern void cleardynentcache();
extern void updatedynentcache(physent *d);
extern bool entinmap(dynent *d, bool avoidplayers = false);
extern void findplayerspawn(dynent *d, int forceent = -1, int tag = 0);

// rendermodel
enum
{
    MDL_CULL_VFC = 1<<0, MDL_CULL_DIST = 1<<1, MDL_CULL_OCCLUDED = 1<<2,
    MDL_CULL_QUERY = 1<<3, MDL_FULLBRIGHT = 1<<4, MDL_NORENDER = 1<<5,
    MDL_MAPMODEL = 1<<6, MDL_NOBATCH = 1<<7, MDL_ONLYSHADOW = 1<<8,
    MDL_NOSHADOW = 1<<9, MDL_FORCESHADOW = 1<<10, MDL_FORCETRANSPARENT = 1<<11
};

struct model;
struct modelattach
{
    const char *tag, *name;
    int anim, basetime;
    vec *pos;
    model *m;

    modelattach() : tag(NULL), name(NULL), anim(-1), basetime(0), pos(NULL), m(NULL) {}
    modelattach(const char *tag, const char *name, int anim = -1, int basetime = 0) : tag(tag), name(name), anim(anim), basetime(basetime), pos(NULL), m(NULL) {}
    modelattach(const char *tag, vec *pos) : tag(tag), name(NULL), anim(-1), basetime(0), pos(pos), m(NULL) {}
};

extern void rendermodel(const char *mdl, int anim, const vec &o, float yaw = 0, float pitch = 0, float roll = 0, int cull = MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, int basetime2 = 0, float size = 1, const vec4 &color = vec4(1, 1, 1, 1));
extern int intersectmodel(const char *mdl, int anim, const vec &pos, float yaw, float pitch, float roll, const vec &o, const vec &ray, float &dist, int mode = 0, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, int basetime2 = 0, float size = 1);
extern void abovemodel(vec &o, const char *mdl);
extern void renderclient(dynent *d, const char *mdlname, modelattach *attachments, int hold, int attack, int attackdelay, int lastaction, int lastpain, float scale = 1, bool ragdoll = false, float trans = 1);
extern void interpolateorientation(dynent *d, float &interpyaw, float &interppitch);
extern void setbbfrommodel(dynent *d, const char *mdl);
extern const char *mapmodelname(int i);
extern model *loadmodel(const char *name, int i = -1, bool msg = false);
extern void preloadmodel(const char *name);
extern void flushpreloadedmodels(bool msg = true);
extern bool matchanim(const char *name, const char *pattern);

// UI

namespace UI
{
    void holdui(const char* name, bool on);

    bool showui(const char *name);
    bool hideui(const char *name);
    bool toggleui(const char *name);
    bool uivisible(const char *name);
}

// ragdoll

extern void moveragdoll(dynent *d);
extern void cleanragdoll(dynent *d);

// server
#define MAXCLIENTS 128                 // DO NOT set this any higher
#define MAXTRANS 5000                  // max amount of data to swallow in 1 go

extern int maxclients;

enum { DISC_NONE = 0, DISC_EOP, DISC_LOCAL, DISC_KICK, DISC_MSGERR, DISC_IPBAN, DISC_PRIVATE, DISC_MAXCLIENTS, DISC_TIMEOUT, DISC_OVERFLOW, DISC_PASSWORD, DISC_NUM };

extern void *getclientinfo(int i);
extern ENetPeer *getclientpeer(int i);
extern ENetPacket *sendf(int cn, int chan, const char *format, ...);
extern ENetPacket *sendfile(int cn, int chan, stream *file, const char *format = "", ...);
extern void sendpacket(int cn, int chan, ENetPacket *packet, int exclude = -1);
extern void flushserver(bool force);
extern int getservermtu();
extern int getnumclients();
extern uint getclientip(int n);
extern void localconnect();
extern const char *disconnectreason(int reason);
extern void disconnect_client(int n, int reason);
extern void kicknonlocalclients(int reason = DISC_NONE);
extern bool hasnonlocalclients();
extern bool haslocalclients();
extern void sendserverinforeply(ucharbuf &p);
extern bool requestmaster(const char *req);
extern bool requestmasterf(const char *fmt, ...) PRINTFARGS(1, 2);
extern bool isdedicatedserver();

// serverbrowser

struct servinfo
{
    string name, map, desc;
    int protocol, numplayers, maxplayers, gameversion, ping;
    vector<int> attr;

    servinfo() : protocol(INT_MIN), numplayers(0), maxplayers(0), gameversion(0)
    {
        name[0] = map[0] = desc[0] = '\0';
    }
};

extern servinfo *getservinfo(int i);

#define GETSERVINFO(idx, si, body) do { \
    servinfo *si = getservinfo(idx); \
    if(si) \
    { \
        body; \
    } \
} while(0)
#define GETSERVINFOATTR(idx, aidx, aval, body) \
    GETSERVINFO(idx, si, { if(si->attr.inrange(aidx)) { int aval = si->attr[aidx]; body; } })

// client
extern void sendclientpacket(ENetPacket *packet, int chan);
extern void flushclient();
extern void disconnect(bool async = false, bool cleanup = true);
extern bool isconnected(bool attempt = false, bool local = true);
extern const ENetAddress *connectedpeer();
extern bool multiplayer(bool msg = true);
extern void neterr(const char *s, bool disc = true);
extern void gets2c();
extern void notifywelcome();

// crypto
extern void genprivkey(const char *seed, vector<char> &privstr, vector<char> &pubstr);
extern bool calcpubkey(const char *privstr, vector<char> &pubstr);
extern bool hashstring(const char *str, char *result, int maxlen);
extern bool answerchallenge(const char *privstr, const char *challenge, vector<char> &answerstr);
extern void *parsepubkey(const char *pubstr);
extern void freepubkey(void *pubkey);
extern void *genchallenge(void *pubkey, const void *seed, int seedlen, vector<char> &challengestr);
extern void freechallenge(void *answer);
extern bool checkchallenge(const char *answerstr, void *correct);

