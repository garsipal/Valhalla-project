// the interface the engine uses to run the gameplay module

// entities
namespace entities
{
    extern void editent(int i, bool local);
    extern void writeent(entity &e, char *buf);
    extern void readent(entity &e, char *buf, int ver);
    extern void fixentity(extentity &e);
    extern void entradius(extentity &e, bool color);
    extern void deleteentity(extentity *e);
    extern void clearents();
    extern void animatemapmodel(const extentity &e, int &anim, int &basetime);

    extern bool mayattach(extentity &e);
    extern bool attachent(extentity &e, extentity &a);
    extern bool printent(extentity &e, char *buf, int len);

    extern const char *entnameinfo(entity &e);
    extern const char *entname(int type);
    extern const char *entmodel(const entity &e);

    extern int extraentinfosize();

    extern float dropheight(entity &e);

    extern extentity *newentity();
    extern vector<extentity *> &getents();
}

// physics
namespace physics
{
    extern void triggerphysicsevent(physent* pl, int event, int material = 0, vec origin = vec(0, 0, 0));
    extern void collidewithdynamicentity(physent* d, physent* o, const vec& dir);
    extern void updatevertex(dynent* pl, vec pos, vec& dpos, float gravity, float ts);

    extern bool shouldmoveragdoll(dynent* pl, vec eye);
}

// game
enum
{
    ZOOM_NONE = 0,
    ZOOM_SHADOW,
    ZOOM_SCOPE
};

enum
{
    TEXEFFECT_GENERIC = 0,
    TEXEFFECT_DIRT,
    TEXEFFECT_METAL,
    TEXEFFECT_WOOD,
    TEXEFFECT_DUCT,
    TEXEFFECT_SILKY,
    TEXEFFECT_SNOW,
    TEXEFFECT_ORGANIC,
    TEXEFFECT_GLASS,
    TEXEFFECT_WATER
};

static const struct textureeffect
{
    const char *name;
    int id;
    bool hascrouchfootsteps;
    int footstepsound;
} textureeffects[] =
{
    {"generic", TEXEFFECT_GENERIC, true,  S_FOOTSTEP         },
    {"dirt",    TEXEFFECT_DIRT,    true,  S_FOOTSTEP_DIRT    },
    {"metal",   TEXEFFECT_METAL,   true,  S_FOOTSTEP_METAL   },
    {"wood",    TEXEFFECT_WOOD,    true,  S_FOOTSTEP_WOOD    },
    {"duct",    TEXEFFECT_DUCT,    false, S_FOOTSTEP_DUCT    },
    {"silky",   TEXEFFECT_SILKY,   true,  S_FOOTSTEP_SILKY   },
    {"snow",    TEXEFFECT_SNOW,    true,  S_FOOTSTEP_SNOW    },
    {"organic", TEXEFFECT_ORGANIC, true,  S_FOOTSTEP_ORGANIC },
    {"glass",   TEXEFFECT_GLASS,   true,  S_FOOTSTEP_GLASS   },
    {"water",   TEXEFFECT_WATER,   false, S_FOOTSTEP_WATER   }
};

namespace game
{
    extern void parseoptions(vector<const char *> &args);
    extern void gamedisconnect(bool cleanup, bool force = false);
    extern void parsepacketclient(int chan, packetbuf &p);
    extern void connectattempt(const char *name, const char *password, const ENetAddress &address);
    extern void connectfail();
    extern void gameconnect(bool _remote);
    extern void edittoggled(bool on);
    extern void writeclientinfo(stream *f);
    extern void toserver(char *text);
    extern void changemap(const char *name);
    extern void forceedit(const char *name);
    extern void loadconfigs();
    extern void updateworld();
    extern void initclient();
    extern void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0, const VSlot *vs = NULL);
    extern void vartrigger(ident *id);
    extern void resetgamestate();
    extern void newmap(int size);
    extern void startmap(const char *name);
    extern void preload();
    extern void setupcamera();
    extern void rendergame();
    extern void renderavatar();
    extern void renderplayerpreview(int model, int color, int team, int weap);
    extern void findanims(const char *pattern, vector<int> &anims);
    extern void writegamedata(vector<char> &extras);
    extern void readgamedata(vector<char> &extras);
    extern void particletrack(physent *owner, vec &o, vec &d);
    extern void dynlighttrack(physent *owner, vec &o, vec &hud);
    extern void voicecom(int sound, char *text, bool isteam);
    extern void adddynamiclights();

    extern bool allowedittoggle();
    extern bool editing();
    extern bool ispaused();
    extern bool allowmouselook();
    extern bool intermission;
    extern bool needminimap();
    extern bool isfirstpersondeath();
    extern bool allowthirdperson();
    extern bool detachcamera();
    extern bool collidecamera();

    extern const char *gameident();
    extern const char *gameconfig();
    extern const char *savedconfig();
    extern const char *defaultconfig();
    extern const char *autoexec();
    extern const char *savedservers();
    extern const char *getclientmap();
    extern const char *getmapinfo();
    extern const char *getscreenshotinfo();
    extern const char *defaultcrosshair(int index);

    extern int numdynents();
    extern int scaletime(int t);
    extern int numanims();
    extern int selectcrosshair(vec &col);
    extern int maxsoundradius(int n);
    extern int mapdeath;
    extern int checkzoom();

    extern float abovegameplayhud(int w, int h);
    extern float clipconsole(float w, float h);
    extern float calcradarscale();
    extern float minimapalpha;

    extern dynent *iterdynents(int i);
}

// server
namespace server
{
    extern void *newclientinfo();
    extern void deleteclientinfo(void *ci);
    extern void serverinit();
    extern void clientdisconnect(int n);
    extern void localdisconnect(int n);
    extern void localconnect(int n);
    extern void recordpacket(int chan, void *data, int len);
    extern void parsepacket(int sender, int chan, packetbuf &p);
    extern void sendservmsg(const char *s);
    extern void serverinforeply(ucharbuf &req, ucharbuf &p);
    extern void serverupdate();
    extern void processmasterinput(const char *cmd, int cmdlen, const char *args);
    extern void masterconnected();
    extern void masterdisconnected();

    extern bool allowbroadcast(int n);
    extern bool sendpackets(bool force = false);
    extern bool ispaused();

    extern const char *defaultmaster();

    extern int reserveclients();
    extern int numchannels();
    extern int clientconnect(int n, uint ip);
    extern int protocolversion();
    extern int laninfoport();
    extern int serverport();
    extern int masterport();
    extern int scaletime(int t);
}
