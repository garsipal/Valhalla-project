// renderparticles.cpp

#include "engine.h"

Shader *particleshader = NULL, *particlenotextureshader = NULL, *particlesoftshader = NULL, *particletextshader = NULL;

VARP(particlelayers, 0, 1, 1);
FVARP(particlebright, 0, 2, 100);
VARP(particlesize, 20, 100, 500);

VARP(softparticles, 0, 1, 1);
VARP(softparticleblend, 1, 8, 64);

// Check canemitparticles() to limit the rate that paricles can be emitted for models/sparklies
// Automatically stops particles being emitted when paused or in reflective drawing
VARP(emitmillis, 1, 17, 1000);
static int lastemitframe = 0, emitoffset = 0;
static bool canemit = false, regenemitters = false, canstep = false;

static bool canemitparticles()
{
    return canemit || emitoffset;
}

VARP(showparticles, 0, 1, 1);
VAR(cullparticles, 0, 1, 1);
VAR(replayparticles, 0, 1, 1);
VARN(seedparticles, seedmillis, 0, 3000, 10000);
VAR(dbgpcull, 0, 0, 1);
VAR(dbgpseed, 0, 0, 1);

struct particleemitter
{
    extentity *ent;
    vec bbmin, bbmax;
    vec center;
    float radius;
    ivec cullmin, cullmax;
    int maxfade, lastemit, lastcull;

    particleemitter(extentity *ent)
        : ent(ent), bbmin(ent->o), bbmax(ent->o), maxfade(-1), lastemit(0), lastcull(0)
    {}

    void finalize()
    {
        center = vec(bbmin).add(bbmax).mul(0.5f);
        radius = bbmin.dist(bbmax)/2;
        cullmin = ivec::floor(bbmin);
        cullmax = ivec::ceil(bbmax);
        if(dbgpseed) conoutf(CON_DEBUG, "radius: %f, maxfade: %d", radius, maxfade);
    }

    void extendbb(const vec &o, float size = 0)
    {
        bbmin.x = min(bbmin.x, o.x - size);
        bbmin.y = min(bbmin.y, o.y - size);
        bbmin.z = min(bbmin.z, o.z - size);
        bbmax.x = max(bbmax.x, o.x + size);
        bbmax.y = max(bbmax.y, o.y + size);
        bbmax.z = max(bbmax.z, o.z + size);
    }

    void extendbb(float z, float size = 0)
    {
        bbmin.z = min(bbmin.z, z - size);
        bbmax.z = max(bbmax.z, z + size);
    }
};

static vector<particleemitter> emitters;
static particleemitter *seedemitter = NULL;

void clearparticleemitters()
{
    emitters.setsize(0);
    regenemitters = true;
}

void addparticleemitters()
{
    emitters.setsize(0);
    const vector<extentity *> &ents = entities::getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_PARTICLES) continue;
        emitters.add(particleemitter(&e));
    }
    regenemitters = false;
}

enum
{
    PT_PART = 0,
    PT_TAPE,
    PT_TRAIL,
    PT_TEXT,
    PT_TEXTUP,
    PT_METER,
    PT_METERVS,
    PT_FIREBALL,
    PT_LIGHTNING,
    PT_FLARE,

    PT_MOD       = 1<<8,
    PT_RND4      = 1<<9,
    PT_LERP      = 1<<10, // use very sparingly - order of blending issues
    PT_TRACK     = 1<<11,
    PT_BRIGHT    = 1<<12,
    PT_SOFT      = 1<<13,
    PT_HFLIP     = 1<<14,
    PT_VFLIP     = 1<<15,
    PT_ROT       = 1<<16,
    PT_CULL      = 1<<17,
    PT_FEW       = 1<<18,
    PT_ICON      = 1<<19,
    PT_NOTEX     = 1<<20,
    PT_SHADER    = 1<<21,
    PT_NOLAYER   = 1<<22,
    PT_COLLIDE   = 1<<23,
    PT_HUD       = 1<<24,
    PT_FLIP      = PT_HFLIP | PT_VFLIP | PT_ROT
};

const char *partnames[] = { "part", "tape", "trail", "text", "textup", "meter", "metervs", "fireball", "lightning", "flare" };

struct particle
{
    vec o, d;
    int gravity, fade, millis;
    bvec color;
    uchar flags;
    float initsize, size, maxsize;
    union
    {
        struct
        {
            const char *text;
            const char *font;
            const char *language;
        };
        float val;
        physent *owner;
        struct
        {
            uchar color2[3];
            uchar progress;
        };
    };
};

struct partvert
{
    vec pos;
    bvec4 color;
    vec2 tc;
};

#define COLLIDERADIUS 8.0f
#define COLLIDEERROR 1.0f

struct partrenderer
{
    Texture *tex;
    const char *texname;
    int texclamp;
    uint type;
    int stain, stainsize;
    int part, partsize;
    string info;

    partrenderer(const char *texname, int texclamp, int type, int stain = -1, int stainsize = 0, int part = -1, int partsize = 0)
        : tex(NULL), texname(texname), texclamp(texclamp), type(type), stain(stain), stainsize(stainsize), part(part), partsize(partsize)
    {
    }
    partrenderer(int type, int stain = -1, int stainsize = 0, int part = -1, int partsize = 0)
        : tex(NULL), texname(NULL), texclamp(0), type(type), stain(stain), stainsize(stainsize), part(part), partsize(partsize)
    {
    }
    virtual ~partrenderer()
    {
    }

    virtual void init(int n) { }
    virtual void reset() = 0;
    virtual void resettracked(physent *owner) { }
    virtual particle *addpart(const vec &o, const vec &d, int fade, int color, float size, int gravity = 0, float maxsize = 0) = 0;
    virtual void update() { }
    virtual void render() = 0;
    virtual bool haswork() = 0;
    virtual int count() = 0; //for debug
    virtual void cleanup() {}

    virtual void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity)
    {
    }

    virtual void preload()
    {
        if(texname && !tex) tex = textureload(texname, texclamp);
    }

    //blend = 0 => remove it
    void calc(particle *p, int &blend, int &ts, vec &o, vec &d, bool step = true)
    {
        o = p->o;
        d = p->d;
        if(type&PT_TRACK && p->owner) game::trackparticles(p->owner, o, d);
        if(p->fade <= 5)
        {
            ts = 1;
            blend = 255;
        }
        else
        {
            ts = lastmillis - p->millis;
            blend = max(255 - (ts << 8) / p->fade, 0);
            if(p->maxsize)
            {
                const float growth = clamp(ts / (float)p->fade, 0.0f, 1.0f);
                p->size = lerp(p->initsize, p->maxsize, growth);
            }
            if(p->gravity)
            {
                if(ts > p->fade) ts = p->fade;
                float t = ts;
                o.add(vec(d).mul(t / 5000.0f));
                o.z -= t * t / (2.0f * 5000.0f * p->gravity);
            }
            if(type&PT_COLLIDE && o.z < p->val && step)
            {
                if(stain >= 0)
                {
                    vec surface;
                    float floorz = rayfloor(vec(o.x, o.y, p->val), surface, RAY_CLIPMAT|RAY_LIQUIDMAT, COLLIDERADIUS);
                    float collidez = floorz<0 ? o.z-COLLIDERADIUS : p->val - floorz;
                    if(o.z >= collidez+COLLIDEERROR)
                        p->val = collidez+COLLIDEERROR;
                    else
                    {
                        vec collidepos = vec(o.x, o.y, collidez);
                        addstain(stain, collidepos, vec(p->o).sub(o).normalize(), p->size * (stainsize ? stainsize : 1), p->color, type&PT_RND4 ? (p->flags>>5)&3 : 0);
                        if (part >= 0)
                        {
                            particle_splash(part, 3, 150, collidepos, p->color.tohexcolor(), p->size * (partsize ? partsize : 1), 100, 2);
                        }
                        blend = 0;
                    }
                }
                else blend = 0;
            }
        }
        if(type & PT_HUD)
        {
            p->size = p->size / 100.0f * sqrt(o.dist(camera1->o));
        }
    }

    void debuginfo()
    {
        formatstring(info, "%d\t%s(", count(), partnames[type&0xFF]);
        if(type&PT_LERP) concatstring(info, "l,");
        if(type&PT_MOD) concatstring(info, "m,");
        if(type&PT_RND4) concatstring(info, "r,");
        if(type&PT_TRACK) concatstring(info, "t,");
        if(type&PT_FLIP) concatstring(info, "f,");
        if(type&PT_COLLIDE) concatstring(info, "c,");
        int len = strlen(info);
        info[len-1] = info[len-1] == ',' ? ')' : '\0';
        if(texname)
        {
            const char *title = strrchr(texname, '/');
            if(title) concformatstring(info, ": %s", title+1);
        }
    }
};

struct listparticle : particle
{
    listparticle *next;
};

VARP(outlinemeters, 0, 0, 1);

struct listrenderer : partrenderer
{
    static listparticle *parempty;
    listparticle *list;

    listrenderer(const char *texname, int texclamp, int type, int stain = -1, int stainsize = 0, int part = -1, int partsize = 0)
        : partrenderer(texname, texclamp, type, stain, stainsize, part, partsize), list(NULL)
    {
    }
    listrenderer(int type, int stain = -1, int stainsize = 0, int part = -1, int partsize = 0)
        : partrenderer(type, stain, stainsize, part, partsize), list(NULL)
    {
    }

    virtual ~listrenderer()
    {
    }

    virtual void killpart(listparticle *p)
    {
    }

    void reset()
    {
        if(!list) return;
        listparticle *p = list;
        for(;;)
        {
            killpart(p);
            if(p->next) p = p->next;
            else break;
        }
        p->next = parempty;
        parempty = list;
        list = NULL;
    }

    void resettracked(physent *owner)
    {
        if(!(type&PT_TRACK)) return;
        for(listparticle **prev = &list, *cur = list; cur; cur = *prev)
        {
            if(!owner || cur->owner==owner)
            {
                *prev = cur->next;
                cur->next = parempty;
                parempty = cur;
            }
            else prev = &cur->next;
        }
    }

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, int gravity, float maxsize)
    {
        if(!parempty)
        {
            listparticle *ps = new listparticle[256];
            loopi(255) ps[i].next = &ps[i+1];
            ps[255].next = parempty;
            parempty = ps;
        }
        listparticle *p = parempty;
        parempty = p->next;
        p->next = list;
        list = p;
        p->o = o;
        p->d = d;
        p->gravity = gravity;
        p->fade = fade;
        p->millis = lastmillis + emitoffset;
        p->color = bvec::hexcolor(color);
        p->initsize = p->size = size;
        p->maxsize = maxsize;
        p->owner = NULL;
        p->flags = 0;
        return p;
    }

    int count()
    {
        int num = 0;
        listparticle *lp;
        for(lp = list; lp; lp = lp->next) num++;
        return num;
    }

    bool haswork()
    {
        return (list != NULL);
    }

    virtual void startrender() = 0;
    virtual void endrender() = 0;
    virtual void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts) = 0;

    bool renderpart(listparticle *p)
    {
        vec o, d;
        int blend, ts;
        calc(p, blend, ts, o, d, canstep);
        if(blend <= 0) return false;
        renderpart(p, o, d, blend, ts);
        return p->fade > 5;
    }

    void render()
    {
        startrender();
        if(tex) setusedtexture(tex);
        if(canstep) for(listparticle **prev = &list, *p = list; p; p = *prev)
        {
            if(renderpart(p)) prev = &p->next;
            else
            { // remove
                *prev = p->next;
                p->next = parempty;
                killpart(p);
                parempty = p;
            }
        }
        else for(listparticle *p = list; p; p = p->next) renderpart(p);
        endrender();
    }
};

listparticle *listrenderer::parempty = NULL;

struct meterrenderer : listrenderer
{
    meterrenderer(int type)
        : listrenderer(type|PT_NOTEX|PT_LERP|PT_NOLAYER)
    {}

    void startrender()
    {
         glDisable(GL_BLEND);
         gle::defvertex();
    }

    void endrender()
    {
         glEnable(GL_BLEND);
    }

    void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts)
    {
        int basetype = type&0xFF;
        float scale = FONTH*p->size/80.0f, right = 8, left = p->progress/100.0f*right;
        matrix4x3 m(camright, vec(camup).neg(), vec(camdir).neg(), o);
        m.scale(scale);
        m.translate(-right/2.0f, 0, 0);

        if(outlinemeters)
        {
            gle::colorf(0, 0.8f, 0);
            gle::begin(GL_TRIANGLE_STRIP);
            loopk(10)
            {
                const vec2 &sc = sincos360[k*(180/(10-1))];
                float c = (0.5f + 0.1f)*sc.y, s = 0.5f - (0.5f + 0.1f)*sc.x;
                gle::attrib(m.transform(vec2(-c, s)));
                gle::attrib(m.transform(vec2(right + c, s)));
            }
            gle::end();
        }

        if(basetype==PT_METERVS) gle::colorub(p->color2[0], p->color2[1], p->color2[2]);
        else gle::colorf(0, 0, 0);
        gle::begin(GL_TRIANGLE_STRIP);
        loopk(10)
        {
            const vec2 &sc = sincos360[k*(180/(10-1))];
            float c = 0.5f*sc.y, s = 0.5f - 0.5f*sc.x;
            gle::attrib(m.transform(vec2(left + c, s)));
            gle::attrib(m.transform(vec2(right + c, s)));
        }
        gle::end();

        if(outlinemeters)
        {
            gle::colorf(0, 0.8f, 0);
            gle::begin(GL_TRIANGLE_FAN);
            loopk(10)
            {
                const vec2 &sc = sincos360[k*(180/(10-1))];
                float c = (0.5f + 0.1f)*sc.y, s = 0.5f - (0.5f + 0.1f)*sc.x;
                gle::attrib(m.transform(vec2(left + c, s)));
            }
            gle::end();
        }

        gle::color(p->color);
        gle::begin(GL_TRIANGLE_STRIP);
        loopk(10)
        {
            const vec2 &sc = sincos360[k*(180/(10-1))];
            float c = 0.5f*sc.y, s = 0.5f - 0.5f*sc.x;
            gle::attrib(m.transform(vec2(-c, s)));
            gle::attrib(m.transform(vec2(left + c, s)));
        }
        gle::end();
    }
};
static meterrenderer meters(PT_METER), metervs(PT_METERVS);

struct textrenderer : listrenderer
{

    textrenderer(int type = 0)
        : listrenderer(type|PT_TEXT|PT_LERP|PT_SHADER|PT_NOLAYER)
    {}

    void startrender()
    {
        textshader = particletextshader;
    }

    void endrender()
    {
        textshader = NULL;
    }

    void killpart(listparticle *p)
    {
        if(p->flags&1)
        {
            if(p->text) delete[] p->text;
            if(p->font) delete[] p->font;
            if(p->language) delete[] p->language;
        }
    }

    void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts)
    {
        pushfont();
        setfont(p->font);
        setfontsize(hudh / PARTICLETEXTROWS);

        const text::Label& label = text::prepare_for_particle(p->text, p->color, FONTH / 32.f, bvec4(0, 0, 0, 255), p->language);
        if(!label.valid())
        {
            popfont();
            return;
        }
        float scale = p->size/80.0f, xoff = -label.width()/2, yoff = -label.width()/2;
        if((type&0xFF)==PT_TEXTUP) { xoff += detrnd((size_t)p, 100)-50; yoff -= detrnd((size_t)p, 101); }

        matrix4x3 m(camright, vec(camup).neg(), vec(camdir).neg(), o);
        m.scale(scale);
        m.translate(xoff, yoff, 50);

        textmatrix = &m;
        label.draw(0, 0, blend);
        popfont();
        textmatrix = NULL;
    }
};
static textrenderer texts;

template<int T>
static inline void modifyblend(const vec &o, int &blend)
{
    blend = min(blend<<2, 255);
}

template<>
inline void modifyblend<PT_TAPE>(const vec &o, int &blend)
{
}

template<int T>
static inline void genpos(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs)
{
    vec udir = vec(camup).sub(camright).mul(size);
    vec vdir = vec(camup).add(camright).mul(size);
    vs[0].pos = vec(o.x + udir.x, o.y + udir.y, o.z + udir.z);
    vs[1].pos = vec(o.x + vdir.x, o.y + vdir.y, o.z + vdir.z);
    vs[2].pos = vec(o.x - udir.x, o.y - udir.y, o.z - udir.z);
    vs[3].pos = vec(o.x - vdir.x, o.y - vdir.y, o.z - vdir.z);
}

template<>
inline void genpos<PT_TAPE>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec dir1 = vec(d).sub(o), dir2 = vec(d).sub(camera1->o), c;
    c.cross(dir2, dir1).normalize().mul(size);
    vs[0].pos = vec(d.x-c.x, d.y-c.y, d.z-c.z);
    vs[1].pos = vec(o.x-c.x, o.y-c.y, o.z-c.z);
    vs[2].pos = vec(o.x+c.x, o.y+c.y, o.z+c.z);
    vs[3].pos = vec(d.x+c.x, d.y+c.y, d.z+c.z);
}

template<>
inline void genpos<PT_TRAIL>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec e = d;
    if(grav) e.z -= float(ts)/grav;
    e.div(-75.0f).add(o);
    genpos<PT_TAPE>(o, e, size, ts, grav, vs);
}

template<int T>
static inline void genrotpos(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs, int rot)
{
    genpos<T>(o, d, size, grav, ts, vs);
}

#define ROTCOEFFS(n) { \
    vec2(-1,  1).rotate_around_z(n*2*M_PI/32.0f), \
    vec2( 1,  1).rotate_around_z(n*2*M_PI/32.0f), \
    vec2( 1, -1).rotate_around_z(n*2*M_PI/32.0f), \
    vec2(-1, -1).rotate_around_z(n*2*M_PI/32.0f) \
}
static const vec2 rotcoeffs[32][4] =
{
    ROTCOEFFS(0),  ROTCOEFFS(1),  ROTCOEFFS(2),  ROTCOEFFS(3),  ROTCOEFFS(4),  ROTCOEFFS(5),  ROTCOEFFS(6),  ROTCOEFFS(7),
    ROTCOEFFS(8),  ROTCOEFFS(9),  ROTCOEFFS(10), ROTCOEFFS(11), ROTCOEFFS(12), ROTCOEFFS(13), ROTCOEFFS(14), ROTCOEFFS(15),
    ROTCOEFFS(16), ROTCOEFFS(17), ROTCOEFFS(18), ROTCOEFFS(19), ROTCOEFFS(20), ROTCOEFFS(21), ROTCOEFFS(22), ROTCOEFFS(7),
    ROTCOEFFS(24), ROTCOEFFS(25), ROTCOEFFS(26), ROTCOEFFS(27), ROTCOEFFS(28), ROTCOEFFS(29), ROTCOEFFS(30), ROTCOEFFS(31),
};

template<>
inline void genrotpos<PT_PART>(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs, int rot)
{
    const vec2 *coeffs = rotcoeffs[rot];
    vs[0].pos = vec(o).madd(camright, coeffs[0].x*size).madd(camup, coeffs[0].y*size);
    vs[1].pos = vec(o).madd(camright, coeffs[1].x*size).madd(camup, coeffs[1].y*size);
    vs[2].pos = vec(o).madd(camright, coeffs[2].x*size).madd(camup, coeffs[2].y*size);
    vs[3].pos = vec(o).madd(camright, coeffs[3].x*size).madd(camup, coeffs[3].y*size);
}

template<int T>
static inline void seedpos(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    if(grav)
    {
        float t = fade;
        vec end = vec(o).madd(d, t/5000.0f);
        end.z -= t*t/(2.0f * 5000.0f * grav);
        pe.extendbb(end, size);

        float tpeak = d.z*grav;
        if(tpeak > 0 && tpeak < fade) pe.extendbb(o.z + 1.5f*d.z*tpeak/5000.0f, size);
    }
}

template<>
inline void seedpos<PT_TAPE>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    pe.extendbb(d, size);
}

template<>
inline void seedpos<PT_TRAIL>(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int grav)
{
    vec e = d;
    if(grav) e.z -= float(fade)/grav;
    e.div(-75.0f).add(o);
    pe.extendbb(e, size);
}

template<int T>
struct varenderer : partrenderer
{
    partvert *verts;
    particle *parts;
    int maxparts, numparts, lastupdate, rndmask;
    GLuint vbo;

    varenderer(const char *texname, int type, int stain = -1, int stainsize = 0, int part = -1, int partsize = 0)
        : partrenderer(texname, 3, type, stain, stainsize, part, partsize),
          verts(NULL), parts(NULL), maxparts(0), numparts(0), lastupdate(-1), rndmask(0), vbo(0)
    {
        if(type & PT_HFLIP) rndmask |= 0x01;
        if(type & PT_VFLIP) rndmask |= 0x02;
        if(type & PT_ROT) rndmask |= 0x1F<<2;
        if(type & PT_RND4) rndmask |= 0x03<<5;
    }

    void cleanup()
    {
        if(vbo) { glDeleteBuffers_(1, &vbo); vbo = 0; }
    }

    void init(int n)
    {
        DELETEA(parts);
        DELETEA(verts);
        parts = new particle[n];
        verts = new partvert[n*4];
        maxparts = n;
        numparts = 0;
        lastupdate = -1;
    }

    void reset()
    {
        numparts = 0;
        lastupdate = -1;
    }

    void resettracked(physent *owner)
    {
        if(!(type&PT_TRACK)) return;
        loopi(numparts)
        {
            particle *p = parts+i;
            if(!owner || (p->owner == owner)) p->fade = -1;
        }
        lastupdate = -1;
    }

    int count()
    {
        return numparts;
    }

    bool haswork()
    {
        return (numparts > 0);
    }

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size, int gravity, float maxsize)
    {
        particle *p = parts + (numparts < maxparts ? numparts++ : rnd(maxparts)); //next free slot, or kill a random kitten
        p->o = o;
        p->d = d;
        p->gravity = gravity;
        p->fade = fade;
        p->millis = lastmillis + emitoffset;
        p->color = bvec::hexcolor(color);
        p->initsize = p->size = size;
        p->maxsize = maxsize;
        p->owner = NULL;
        p->flags = 0x80 | (rndmask ? rnd(0x80) & rndmask : 0);
        lastupdate = -1;
        return p;
    }

    void seedemitter(particleemitter &pe, const vec &o, const vec &d, int fade, float size, int gravity)
    {
        pe.maxfade = max(pe.maxfade, fade);
        size *= SQRT2;
        pe.extendbb(o, size);

        seedpos<T>(pe, o, d, fade, size, gravity);
        if(!gravity) return;

        vec end(o);
        float t = fade;
        end.add(vec(d).mul(t/5000.0f));
        end.z -= t*t/(2.0f * 5000.0f * gravity);
        pe.extendbb(end, size);

        float tpeak = d.z*gravity;
        if(tpeak > 0 && tpeak < fade) pe.extendbb(o.z + 1.5f*d.z*tpeak/5000.0f, size);
    }

    void genverts(particle *p, partvert *vs, bool regen)
    {
        vec o, d;
        int blend, ts;

        calc(p, blend, ts, o, d);
        if(blend <= 1 || p->fade <= 5) p->fade = -1; //mark to remove on next pass (i.e. after render)

        modifyblend<T>(o, blend);

        if(regen)
        {
            p->flags &= ~0x80;

            #define SETTEXCOORDS(u1c, u2c, v1c, v2c, body) \
            { \
                float u1 = u1c, u2 = u2c, v1 = v1c, v2 = v2c; \
                body; \
                vs[0].tc = vec2(u1, v1); \
                vs[1].tc = vec2(u2, v1); \
                vs[2].tc = vec2(u2, v2); \
                vs[3].tc = vec2(u1, v2); \
            }
            if(type&PT_RND4)
            {
                float tx = 0.5f*((p->flags>>5)&1), ty = 0.5f*((p->flags>>6)&1);
                SETTEXCOORDS(tx, tx + 0.5f, ty, ty + 0.5f,
                {
                    if(p->flags&0x01) swap(u1, u2);
                    if(p->flags&0x02) swap(v1, v2);
                });
            }
            else if(type&PT_ICON)
            {
                float tx = 0.25f*(p->flags&3), ty = 0.25f*((p->flags>>2)&3);
                SETTEXCOORDS(tx, tx + 0.25f, ty, ty + 0.25f, {});
            }
            else SETTEXCOORDS(0, 1, 0, 1, {});

            #define SETCOLOR(r, g, b, a) \
            do { \
                bvec4 col(r, g, b, a); \
                loopi(4) vs[i].color = col; \
            } while(0)
            #define SETMODCOLOR SETCOLOR((p->color.r*blend)>>8, (p->color.g*blend)>>8, (p->color.b*blend)>>8, 255)
            if(type&PT_MOD) SETMODCOLOR;
            else SETCOLOR(p->color.r, p->color.g, p->color.b, blend);
        }
        else if(type&PT_MOD) SETMODCOLOR;
        else loopi(4) vs[i].color.a = blend;

        if(type&PT_ROT) genrotpos<T>(o, d, p->size, ts, p->gravity, vs, (p->flags>>2)&0x1F);
        else genpos<T>(o, d, p->size, ts, p->gravity, vs);
    }

    void genverts()
    {
        loopi(numparts)
        {
            particle *p = &parts[i];
            partvert *vs = &verts[i*4];
            if(p->fade < 0)
            {
                do
                {
                    --numparts;
                    if(numparts <= i) return;
                }
                while(parts[numparts].fade < 0);
                *p = parts[numparts];
                genverts(p, vs, true);
            }
            else genverts(p, vs, (p->flags&0x80)!=0);
        }
    }

    void genvbo()
    {
        if(lastmillis == lastupdate && vbo) return;
        lastupdate = lastmillis;

        genverts();

        if(!vbo) glGenBuffers_(1, &vbo);
        gle::bindvbo(vbo);
        glBufferData_(GL_ARRAY_BUFFER, maxparts*4*sizeof(partvert), NULL, GL_STREAM_DRAW);
        glBufferSubData_(GL_ARRAY_BUFFER, 0, numparts*4*sizeof(partvert), verts);
        gle::clearvbo();
    }

    void render()
    {
        genvbo();

        setusedtexture(tex);

        gle::bindvbo(vbo);
        const partvert *ptr = 0;
        gle::vertexpointer(sizeof(partvert), ptr->pos.v);
        gle::texcoord0pointer(sizeof(partvert), ptr->tc.v);
        gle::colorpointer(sizeof(partvert), ptr->color.v);
        gle::enablevertex();
        gle::enabletexcoord0();
        gle::enablecolor();
        gle::enablequads();

        gle::drawquads(0, numparts);

        gle::disablequads();
        gle::disablevertex();
        gle::disabletexcoord0();
        gle::disablecolor();
        gle::clearvbo();
    }
};
typedef varenderer<PT_PART> quadrenderer;
typedef varenderer<PT_TAPE> taperenderer;
typedef varenderer<PT_TRAIL> trailrenderer;

#include "explosion.h"
#include "lensflare.h"
#include "lightning.h"

struct softquadrenderer : quadrenderer
{
    softquadrenderer(const char *texname, int type, int stain = -1)
        : quadrenderer(texname, type|PT_SOFT, stain)
    {
    }
};

static partrenderer *parts[] =
{
    new quadrenderer("<grey>data/texture/particle/blood01.png", PT_PART|PT_FLIP|PT_MOD|PT_RND4|PT_COLLIDE, STAIN_BLOOD),           // blood spats (note: rgb is inverted)
    new quadrenderer("<grey>data/texture/particle/blood02.png", PT_PART|PT_FLIP|PT_MOD|PT_RND4, STAIN_BLOOD),                      // blood drops
    new trailrenderer("data/texture/particle/water.png", PT_TRAIL|PT_COLLIDE, STAIN_RAIN, 10, PART_SPLASH, 5),                     // water
    new quadrenderer("data/texture/particle/glass.png", PT_PART|PT_FLIP|PT_BRIGHT|PT_RND4),                                        // glass
    new quadrenderer("<grey>data/texture/particle/smoke.png", PT_PART|PT_RND4|PT_FLIP|PT_LERP),                                    // smoke
    new quadrenderer("<grey>data/texture/particle/steam.png", PT_PART|PT_FLIP),                                                    // steam
    new quadrenderer("<grey>data/texture/particle/flames.png", PT_PART|PT_HFLIP|PT_RND4|PT_BRIGHT),                                // flame
    new quadrenderer("data/texture/particle/snow.png", PT_PART|PT_FLIP|PT_RND4|PT_COLLIDE, STAIN_SNOW),                            // colliding snow
    new taperenderer("data/texture/particle/trail.png", PT_TAPE|PT_BRIGHT),                                                        // bullet trail
    new taperenderer("data/texture/particle/trail_projectile.png", PT_TAPE|PT_FEW|PT_BRIGHT),                                      // projectile trail
    new taperenderer("data/texture/particle/trail_straight.png", PT_TAPE|PT_FEW|PT_BRIGHT),                                        // straight, continuous trail
    &lightnings,                                                                                                                   // lightning
    &explosions,                                                                                                                   // bland explosion
    &energybursts,                                                                                                                 // pulse/plasma/energy burst
    &fireballs,                                                                                                                    // fire ball
    new quadrenderer("data/texture/particle/spark01.png", PT_PART|PT_FLIP|PT_BRIGHT),                                              // sparks
    new trailrenderer("data/texture/particle/spark02.png", PT_TRAIL|PT_BRIGHT),                                                    // spark trail
    new quadrenderer("<animate:10,8,8>data/texture/particle/explosion01.png", PT_PART|PT_FLIP|PT_BRIGHT),                          // explosion sprite
    new quadrenderer("<animate:10,8,8>data/texture/particle/explosion02.png", PT_PART|PT_FLIP|PT_BRIGHT),                          // explosion sprite
    new quadrenderer("<animate:10,8,8>data/texture/particle/explosion03.png", PT_PART|PT_FLIP|PT_BRIGHT),                          // explosion sprite
    new quadrenderer("data/texture/particle/explosion04.png", PT_PART|PT_FLIP|PT_BRIGHT),                                          // explosion
    new quadrenderer("<animate:100,2,2>data/texture/particle/electricity.png", PT_PART|PT_FEW|PT_FLIP|PT_BRIGHT|PT_TRACK),         // electric explosion sprite
    new quadrenderer("data/texture/particle/base.png", PT_PART|PT_FLIP|PT_BRIGHT),                                                 // edit mode entities
    new quadrenderer("data/texture/particle/orb.png", PT_PART|PT_FLIP|PT_FEW|PT_BRIGHT),                                           // energy orb
    new quadrenderer("data/texture/particle/ring.png", PT_PART|PT_FLIP|PT_BRIGHT),                                                 // energy ring
    new quadrenderer("data/texture/particle/splash.png", PT_PART|PT_FLIP),                                                         // liquid splash
    new quadrenderer("data/texture/particle/bubble.png", PT_PART | PT_FLIP),                                                       // liquid bubble
    new quadrenderer("data/texture/particle/muzzle01.png", PT_PART|PT_FEW|PT_FLIP|PT_BRIGHT|PT_TRACK),                             // muzzle flash 1
    new quadrenderer("<animate:50,4,2,1>data/texture/particle/muzzle02.png", PT_PART|PT_FEW|PT_FLIP|PT_BRIGHT|PT_TRACK),           // muzzle flash 2
    new quadrenderer("data/texture/particle/muzzle03.png", PT_PART|PT_FEW|PT_FLIP|PT_BRIGHT|PT_TRACK),                             // muzzle flash 3
    new quadrenderer("data/texture/particle/muzzle04.png", PT_PART|PT_FEW|PT_FLIP|PT_BRIGHT|PT_TRACK),                             // muzzle flash 4
    new quadrenderer("<animate:25,4,0,1>data/texture/particle/muzzle05.png", PT_PART|PT_FEW|PT_FLIP|PT_BRIGHT|PT_TRACK),           // muzzle flash 5
    new quadrenderer("<grey><animate:50,2,3,1>data/texture/particle/muzzle_smoke.png", PT_PART|PT_FEW|PT_FLIP|PT_LERP|PT_TRACK),   // muzzle smoke
    new quadrenderer("<animate:30,2,2,1>data/texture/particle/sparks.png", PT_PART|PT_FEW|PT_FLIP|PT_BRIGHT|PT_TRACK),             // muzzle sparks
    new quadrenderer("data/texture/particle/comics.png", PT_PART|PT_LERP|PT_NOLAYER),                                              // comics effect
    new quadrenderer("data/interface/particle/game_icons.png", PT_PART|PT_FEW|PT_ICON|PT_HUD|PT_LERP|PT_NOLAYER),                  // game icons
    new quadrenderer("data/interface/particle/editor_icons.png", PT_PART|PT_ICON|PT_LERP|PT_NOLAYER),                              // editor icons
    &texts,                                                                                                                        // text
    &meters,                                                                                                                       // meter
    &metervs,                                                                                                                      // meter vs.
    &flares                                                                                                                        // lens flares - must be done last
};

VARFP(maxparticles, 10, 4000, 10000, initparticles());
VARFP(fewparticles, 10, 100, 10000, initparticles());

void initparticles()
{
    if(initing) return;
    if(!particleshader) particleshader = lookupshaderbyname("particle");
    if(!particlenotextureshader) particlenotextureshader = lookupshaderbyname("particlenotexture");
    if(!particlesoftshader) particlesoftshader = lookupshaderbyname("particlesoft");
    if(!particletextshader) particletextshader = lookupshaderbyname("particletext");
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->init(parts[i]->type&PT_FEW ? min(fewparticles, maxparticles) : maxparticles);
    loopi(sizeof(parts)/sizeof(parts[0]))
    {
        loadprogress = float(i+1)/(sizeof(parts)/sizeof(parts[0]));
        parts[i]->preload();
    }
    loadprogress = 0;
}

void clearparticles()
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->reset();
    clearparticleemitters();
}

void cleanupparticles()
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->cleanup();
}

void removetrackedparticles(physent *owner)
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->resettracked(owner);
}

VARN(debugparticles, dbgparts, 0, 0, 1);

void debugparticles()
{
    if(!dbgparts) return;
    int n = sizeof(parts)/sizeof(parts[0]);
    pushhudmatrix();
    hudmatrix.ortho(0, FONTH*n*2*vieww/float(viewh), FONTH*n*2, 0, -1, 1); // squeeze into top-left corner
    flushhudmatrix();
    loopi(n) text::draw(parts[i]->info, FONTH, (i+n/2)*FONTH);
    pophudmatrix();
}

void renderparticles(int layer)
{
    canstep = layer != PL_UNDER;

    //want to debug BEFORE the lastpass render (that would delete particles)
    if(dbgparts && (layer == PL_ALL || layer == PL_UNDER)) loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->debuginfo();

    bool rendered = false;
    uint lastflags = PT_LERP|PT_SHADER,
         flagmask = PT_LERP|PT_MOD|PT_BRIGHT|PT_NOTEX|PT_SOFT|PT_SHADER,
         excludemask = layer == PL_ALL ? ~0 : (layer != PL_NOLAYER ? PT_NOLAYER : 0);

    loopi(sizeof(parts)/sizeof(parts[0]))
    {
        partrenderer *p = parts[i];
        if((p->type&PT_NOLAYER) == excludemask || !p->haswork()) continue;

        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            if(p->type & PT_HUD) glDisable(GL_DEPTH_TEST);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glActiveTexture_(GL_TEXTURE2);
            if(msaalight) glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msdepthtex);
            else glBindTexture(GL_TEXTURE_RECTANGLE, gdepthtex);
            glActiveTexture_(GL_TEXTURE0);
        }

        uint flags = p->type & flagmask, changedbits = flags ^ lastflags;
        if(changedbits)
        {
            if(changedbits&PT_LERP) { if(flags&PT_LERP) resetfogcolor(); else zerofogcolor(); }
            if(changedbits&(PT_LERP|PT_MOD))
            {
                if(flags&PT_LERP) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                else if(flags&PT_MOD) glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                else glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }
            if(!(flags&PT_SHADER))
            {
                if(changedbits&(PT_LERP|PT_SOFT|PT_NOTEX|PT_SHADER))
                {
                    if(flags&PT_SOFT && softparticles)
                    {
                        particlesoftshader->set();
                        LOCALPARAMF(softparams, -1.0f/softparticleblend, 0, 0);
                    }
                    else if(flags&PT_NOTEX) particlenotextureshader->set();
                    else particleshader->set();
                }
                if(changedbits&(PT_MOD|PT_BRIGHT|PT_SOFT|PT_NOTEX|PT_SHADER))
                {
                    float colorscale = flags&PT_MOD ? 1 : ldrscale;
                    if(flags&PT_BRIGHT) colorscale *= particlebright;
                    LOCALPARAMF(colorscale, colorscale, colorscale, colorscale, 1);
                }
            }
            lastflags = flags;
        }
        p->render();
    }

    if(rendered)
    {
        if(lastflags & PT_HUD) glEnable(GL_DEPTH_TEST);
        if(lastflags & (PT_LERP|PT_MOD)) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        if(!(lastflags & PT_LERP)) resetfogcolor();
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

static int addedparticles = 0;

static inline particle *newparticle(const vec &o, const vec &d, int fade, int type, int color, float size, int gravity = 0, float maxsize = 0)
{
    static particle dummy;
    if(seedemitter)
    {
        parts[type]->seedemitter(*seedemitter, o, d, fade, size, gravity);
        return &dummy;
    }
    if(fade + emitoffset < 0) return &dummy;
    addedparticles++;
    return parts[type]->addpart(o, d, fade, color, size, gravity, maxsize);
}

VARP(maxparticledistance, 256, 2048, 4096);

static void splash(int type, int color, int radius, int num, int fade, const vec &p, float size, int gravity, float maxsize)
{
    if(camera1->o.dist(p) > maxparticledistance && !seedemitter) return;
    float collidez = parts[type]->type&PT_COLLIDE ? p.z - raycube(p, vec(0, 0, -1), COLLIDERADIUS, RAY_CLIPMAT) + (parts[type]->stain >= 0 ? COLLIDEERROR : 0) : -1;
    int fmin = 1;
    int fmax = fade*3;
    loopi(num)
    {
        int x, y, z;
        do
        {
            x = rnd(radius*2)-radius;
            y = rnd(radius*2)-radius;
            z = rnd(radius*2)-radius;
        }
        while(x*x+y*y+z*z>radius*radius);
        vec tmp = vec((float)x, (float)y, (float)z);
        int f = (num < 10) ? (fmin + rnd(fmax)) : (fmax - (i*(fmax-fmin))/(num-1)); //help deallocater by using fade distribution rather than random
        newparticle(p, tmp, f, type, color, size, gravity, maxsize)->val = collidez;
    }
}

static void regularsplash(int type, int color, int radius, int num, int fade, const vec &p, float size, int gravity, int delay = 0, float maxsize = 0)
{
    if(!canemitparticles() || (delay > 0 && rnd(delay) != 0)) return;
    splash(type, color, radius, num, fade, p, size, gravity, maxsize);
}

bool canaddparticles()
{
    return !minimized;
}

void regular_particle_splash(int type, int num, int fade, const vec &p, int color, float size, int radius, int gravity, int delay, float maxsize)
{
    if(!canaddparticles()) return;
    regularsplash(type, color, radius, num, fade, p, size, gravity, delay, maxsize);
}

void particle_splash(int type, int num, int fade, const vec &p, int color, float size, int radius, int gravity, float maxsize)
{
    if(!canaddparticles()) return;
    splash(type, color, radius, num, fade, p, size, gravity, maxsize);
}

VARP(maxtrail, 1, 500, 10000);

void particle_trail(int type, int fade, const vec &s, const vec &e, int color, float size, int gravity, float maxsize)
{
    if(!canaddparticles()) return;
    vec v;
    float d = e.dist(s, v);
    int steps = clamp(int(d*2), 1, maxtrail);
    v.div(steps);
    vec p = s;
    loopi(steps)
    {
        p.add(v);
        vec tmp = vec(float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5));
        newparticle(p, tmp, rnd(fade)+fade, type, color, size, gravity, maxsize);
    }
}

VARP(particletext, 0, 1, 1);
VARP(maxparticletextdistance, 0, 64, 10000);

void particle_text(const vec &s, const char *t, int type, int fade, int color, float size, int gravity, const char *font, const char *language)
{
    if(!canaddparticles()) return;
    if(!particletext || camera1->o.dist(s) > maxparticletextdistance) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size, gravity);
    p->text = t;
    p->font = font;
    p->language = language;
}

void particle_textcopy(const vec &s, const char *t, int type, int fade, int color, float size, int gravity, const char *font, const char *language)
{
    if(!canaddparticles()) return;
    if(!particletext || camera1->o.dist(s) > maxparticletextdistance) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size, gravity);
    p->text = newstring(t);
    p->font = newstring(font);
    p->language = newstring(language ? language : "");
    p->flags = 1;
}

void particle_icon(const vec &s, int ix, int iy, int type, int fade, int color, float size, int gravity)
{
    if(!canaddparticles()) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size, gravity);
    p->flags |= ix | (iy<<2);
}

VARP(hudmarkmindist, 0, 128, 512);
VARP(hudmarkmaxdist, 0, 1024, 4096);

void particle_hud_mark(const vec &s, int ix, int iy, int type, int fade, int color, float size)
{
    if(!canaddparticles()) return;
    vec o = vec(0, 0, 0);
    if((camera1->o.dist(s) <= hudmarkmindist && raycubelos(s, camera1->o, o)) || camera1->o.dist(s) > hudmarkmaxdist)
    {
        return;
    }
    o = s;
    vec camera = camera1->o;
    o.sub(camera).normalize();
    particle *p = newparticle(camera.add(o), vec(0, 0, 1), fade, type, color, size, 0);
    p->flags |= ix | (iy<<2);
}

void particle_hud_text(const vec &s, const char *t, int type, int fade, int color, float size, const char *font, const char *language)
{
    if(!canaddparticles()) return;
    vec o;
    if((camera1->o.dist(s) <= hudmarkmindist && raycubelos(s, camera1->o, o)) || camera1->o.dist(s) > hudmarkmaxdist)
    {
        return;
    }
    o = s;
    vec camera = camera1->o;
    o.sub(camera).normalize();
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size * camera1->o.dist(s) / 80, 0);
    p->text = t;
    p->font = font;
    p->language = language;
}

void particle_meter(const vec &s, float val, int type, int fade, int color, int color2, float size)
{
    if(!canaddparticles()) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size);
    p->color2[0] = color2>>16;
    p->color2[1] = (color2>>8)&0xFF;
    p->color2[2] = color2&0xFF;
    p->progress = clamp(int(val*100), 0, 100);
}

void particle_flare(const vec &p, const vec &dest, int fade, int type, int color, float size, physent *owner, float maxsize)
{
    if(!canaddparticles()) return;
    newparticle(p, dest, fade, type, color, size, 0, maxsize)->owner = owner;
}

void particle_fireball(const vec &dest, float maxsize, int type, int fade, int color, float size)
{
    if(!canaddparticles()) return;
    float growth = maxsize - size;
    if(fade < 0) fade = int(growth * 20);
    newparticle(dest, vec(0, 0, 1), fade, type, color, size)->val = growth;
}

//dir = 0..6 where 0=up
static inline vec offsetvec(vec o, int dir, int dist)
{
    vec v = vec(o);
    v[(2+dir)%3] += (dir>2)?(-dist):dist;
    return v;
}

//converts a 16bit color to 24bit
static inline int colorfromattr(int attr)
{
    return (((attr&0xF)<<4) | ((attr&0xF0)<<8) | ((attr&0xF00)<<12)) + 0x0F0F0F;
}

/* Experiments in shapes...
 * dir: (where dir%3 is similar to offsetvec with 0=up)
 * 0..2 circle
 * 3.. 5 cylinder shell
 * 6..11 cone shell
 * 12..14 plane volume
 * 15..20 line volume, i.e. wall
 * 21 sphere
 * 24..26 flat plane
 * +32 to inverse direction
 */
static void regularshape(int type, int radius, int color, int dir, int num, int fade, const vec &p, float size, int gravity, int vel = 200)
{
    if(!canemitparticles()) return;

    int basetype = parts[type]->type&0xFF;
    bool flare = (basetype == PT_TAPE) || (basetype == PT_LIGHTNING),
         inv = (dir&0x20)!=0, taper = (dir&0x40)!=0 && !seedemitter;
    dir &= 0x1F;
    loopi(num)
    {
        vec to, from;
        if(dir < 12)
        {
            const vec2 &sc = sincos360[rnd(360)];
            to[dir%3] = sc.y*radius;
            to[(dir+1)%3] = sc.x*radius;
            to[(dir+2)%3] = 0.0;
            to.add(p);
            if(dir < 3) //circle
                from = p;
            else if(dir < 6) //cylinder
            {
                from = to;
                to[(dir+2)%3] += radius;
                from[(dir+2)%3] -= radius;
            }
            else //cone
            {
                from = p;
                to[(dir+2)%3] += (dir < 9)?radius:(-radius);
            }
        }
        else if(dir < 15) //plane
        {
            to[dir%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
            to[(dir+1)%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
            to[(dir+2)%3] = radius;
            to.add(p);
            from = to;
            from[(dir+2)%3] -= 2*radius;
        }
        else if(dir < 21) //line
        {
            if(dir < 18)
            {
                to[dir%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
                to[(dir+1)%3] = 0.0;
            }
            else
            {
                to[dir%3] = 0.0;
                to[(dir+1)%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
            }
            to[(dir+2)%3] = 0.0;
            to.add(p);
            from = to;
            to[(dir+2)%3] += radius;
        }
        else if(dir < 24) //sphere
        {
            to = vec(2*M_PI*float(rnd(1000))/1000.0, M_PI*float(rnd(1000)-500)/1000.0).mul(radius);
            to.add(p);
            from = p;
        }
        else if(dir < 27) // flat plane
        {
            to[dir%3] = float(rndscale(2*radius)-radius);
            to[(dir+1)%3] = float(rndscale(2*radius)-radius);
            to[(dir+2)%3] = 0.0;
            to.add(p);
            from = to;
        }
        else from = to = p;

        if(inv) swap(from, to);

        if(taper)
        {
            float dist = clamp(from.dist2(camera1->o)/maxparticledistance, 0.0f, 1.0f);
            if(dist > 0.2f)
            {
                dist = 1 - (dist - 0.2f)/0.8f;
                if(rnd(0x10000) > dist*dist*0xFFFF) continue;
            }
        }

        if(flare)
            newparticle(from, to, rnd(fade*3)+1, type, color, size, gravity);
        else
        {
            vec d = vec(to).sub(from).rescale(vel); //velocity
            particle *n = newparticle(from, d, rnd(fade*3)+1, type, color, size, gravity);
            if(parts[type]->type&PT_COLLIDE)
                n->val = from.z - raycube(from, vec(0, 0, -1), parts[type]->stain >= 0 ? COLLIDERADIUS : max(from.z, 0.0f), RAY_CLIPMAT) + (parts[type]->stain >= 0 ? COLLIDEERROR : 0);
        }
    }
}

static void regularflame(int type, const vec &p, float radius, float height, int color, int density = 3, float scale = 2.0f, float speed = 200.0f, float fade = 600.0f, int gravity = -15)
{
    if(!canemitparticles()) return;

    float size = scale * min(radius, height);
    vec v(0, 0, min(1.0f, height)*speed);
    loopi(density)
    {
        vec s = p;
        s.x += rndscale(radius*2.0f)-radius;
        s.y += rndscale(radius*2.0f)-radius;
        newparticle(s, v, rnd(max(int(fade*height), 1))+1, type, color, size, gravity);
    }
}

void regular_particle_flame(int type, const vec &p, float radius, float height, int color, int density, float scale, float speed, float fade, int gravity)
{
    if(!canaddparticles()) return;
    regularflame(type, p, radius, height, color, density, scale, speed, fade, gravity);
}

static void makeparticles(entity &e)
{
    switch(e.attr1)
    {
        case 0: //fire and smoke -  <radius> <height> <rgb> - 0 values default to compat for old maps
        {
            float radius = e.attr2 ? float(e.attr2)/100.0f : 1.5f,
                  height = e.attr3 ? float(e.attr3)/100.0f : radius/3;
            regularflame(PART_FLAME, e.o, radius, height, e.attr4 ? colorfromattr(e.attr4) : 0x903020, 3, 2.0f);
            regularflame(PART_SMOKE, vec(e.o.x, e.o.y, e.o.z + 4.0f*min(radius, height)), radius, height, 0x303020, 1, 4.0f, 100.0f, 2000.0f, -20);
            break;
        }
        case 1: //steam vent - <dir>
        {
            regularsplash(PART_STEAM, 0x897661, 50, 1, 200, offsetvec(e.o, e.attr2, rnd(10)), 2.4f, -20);
            break;
        }
        case 2: //water fountain - <dir>
        {
            int color;
            if(e.attr3 > 0) color = colorfromattr(e.attr3);
            else
            {
                int mat = MAT_WATER + clamp(-e.attr3, 0, 3);
                color = getwaterfallcolor(mat).tohexcolor();
                if(!color) color = getwatercolor(mat).tohexcolor();
            }
            regularsplash(PART_WATER, color, 150, 4, 200, offsetvec(e.o, e.attr2, rnd(10)), 0.6f, 2);
            break;
        }
        case 3: //fire ball - <size> <rgb>
            newparticle(e.o, vec(0, 0, 1), 1, PART_EXPLOSION1, colorfromattr(e.attr3), 4.0f)->val = 1+e.attr2;
            break;
        case 4:  //tape - <dir> <length> <rgb>
        case 7:  //lightning
        case 9:  //steam
        case 10: //water
        case 13: //snow
        case 14: //spark
        {
            static const int typemap[]   = { PART_TRAIL, -1, -1, PART_LIGHTNING, -1, PART_STEAM, PART_WATER, -1, -1, PART_SNOW,  PART_SPARK };
            static const float sizemap[] = { 0.28f, 0.0f, 0.0f, 1.0f, 0.0f, 2.4f, 0.1f, 0.0f, 0.0f, 0.5f, 0.4f };
            static const int gravmap[] = { 0, 0, 0, 0, 0, -20, 2, 0, 0, 20, 20};
            int type = typemap[e.attr1-4];
            float size = sizemap[e.attr1-4];
            int gravity = gravmap[e.attr1-4];
            if(e.attr2 >= 256) regularshape(type, max(1+e.attr3, 1), colorfromattr(e.attr4), e.attr2-256, 5, e.attr5 > 0 ? min(int(e.attr5), 10000) : 200, e.o, size, gravity);
            else newparticle(e.o, offsetvec(e.o, e.attr2, max(1+e.attr3, 0)), 1, type, colorfromattr(e.attr4), size, gravity);
            break;
        }
        case 5: //meter, metervs - <percent> <rgb> <rgb2>
        case 6:
        {
            particle *p = newparticle(e.o, vec(0, 0, 1), 1, e.attr1==5 ? PART_METER : PART_METER_VS, colorfromattr(e.attr3), 2.0f);
            int color2 = colorfromattr(e.attr4);
            p->color2[0] = color2>>16;
            p->color2[1] = (color2>>8)&0xFF;
            p->color2[2] = color2&0xFF;
            p->progress = clamp(int(e.attr2), 0, 100);
            break;
        }
        case 11: // flame <radius> <height> <rgb> - radius=100, height=100 is the classic size
        {
            regularflame(PART_FLAME, e.o, float(e.attr2)/100.0f, float(e.attr3)/100.0f, colorfromattr(e.attr4), 3, 2.0f);
            break;
        }
        case 12: // smoke plume <radius> <height> <rgb>
        {
            regularflame(PART_SMOKE, e.o, float(e.attr2)/100.0f, float(e.attr3)/100.0f, colorfromattr(e.attr4), 1, 4.0f, 100.0f, 2000.0f, -20);
            break;
        }
        case 32: //lens flares - plain/sparkle/sun/sparklesun <red> <green> <blue>
        case 33:
        case 34:
        case 35:
        {
            flares.addflare(e.o, e.attr2, e.attr3, e.attr4, (e.attr1&0x02)!=0, (e.attr1&0x01)!=0);
            break;
        }
        case 36: // Temporary way to trigger light effects
        {
            extentity* ent = (extentity*)&e;
            if(!ent->lasttrigger || lastmillis - ent->lasttrigger >= e.attr2 * 1000)
            {
                const int color = colorfromattr(e.attr4);
                adddynlight(e.o, e.attr3, vec::hexcolor(color), e.attr5, 0, L_NOSHADOW);
                ent->lasttrigger = lastmillis;
            }
            break;
        }
        default:
        {
            if(!editmode)
            {
                defformatstring(ds, "particles %d?", e.attr1);
                particle_textcopy(e.o, ds, PART_TEXT, 1, 0x6496FF, 2.0f);
            }
            break;
        }
    }
}

bool printparticles(extentity &e, char *buf, int len)
{
    switch(e.attr1)
    {
        case 0: case 4: case 7: case 8: case 9: case 10: case 11: case 12: case 13:
            nformatstring(buf, len, "%s %d %d %d 0x%.3hX %d", entities::entname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
        case 3:
            nformatstring(buf, len, "%s %d %d 0x%.3hX %d %d", entities::entname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
        case 5: case 6:
            nformatstring(buf, len, "%s %d %d 0x%.3hX 0x%.3hX %d", entities::entname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);
            return true;
    }
    return false;
}

void seedparticles()
{
    renderprogress(0, "Sprinkling particles...");
    addparticleemitters();
    canemit = true;
    loopv(emitters)
    {
        particleemitter &pe = emitters[i];
        extentity &e = *pe.ent;
        seedemitter = &pe;
        for(int millis = 0; millis < seedmillis; millis += min(emitmillis, seedmillis/10))
            makeparticles(e);
        seedemitter = NULL;
        pe.lastemit = -seedmillis;
        pe.finalize();
    }
}

VARP(entityicons, 0, 1, 1);
VARP(entityiconscolor, 0, 0, 1);

void updateparticles()
{
    if(regenemitters) addparticleemitters();

    if(minimized) { canemit = false; return; }

    if(lastmillis - lastemitframe >= emitmillis)
    {
        canemit = true;
        lastemitframe = lastmillis - (lastmillis%emitmillis);
    }
    else canemit = false;

    loopi(sizeof(parts)/sizeof(parts[0]))
    {
        parts[i]->update();
    }

    if(!editmode || showparticles)
    {
        int emitted = 0, replayed = 0;
        addedparticles = 0;
        loopv(emitters)
        {
            particleemitter &pe = emitters[i];
            extentity &e = *pe.ent;
            if(e.o.dist(camera1->o) > maxparticledistance) { pe.lastemit = lastmillis; continue; }
            if(cullparticles && pe.maxfade >= 0)
            {
                if(isfoggedsphere(pe.radius, pe.center)) { pe.lastcull = lastmillis; continue; }
                if(pvsoccluded(pe.cullmin, pe.cullmax)) { pe.lastcull = lastmillis; continue; }
            }
            makeparticles(e);
            emitted++;
            if(replayparticles && pe.maxfade > 5 && pe.lastcull > pe.lastemit)
            {
                for(emitoffset = max(pe.lastemit + emitmillis - lastmillis, -pe.maxfade); emitoffset < 0; emitoffset += emitmillis)
                {
                    makeparticles(e);
                    replayed++;
                }
                emitoffset = 0;
            }
            pe.lastemit = lastmillis;
        }
        if(dbgpcull && (canemit || replayed) && addedparticles) conoutf(CON_DEBUG, "%d emitters, %d particles", emitted, addedparticles);
    }
    if(editmode) // show sparkly thingies for map entities in edit mode
    {
        const vector<extentity *> &ents = entities::getents();
        static const int entcolor[] = { 0xFFFFFF, 0xFFFFFF, 0xFF8C00, 0x90EE90, 0xBF40BF, 0xFF7F7F, 0xFFFF00, 0xFFC0FB, 0x0096FF }; // from ET_EMPTY to ET_DECAL (9 entities)
        // note: order matters in this case as particles of the same type are drawn in the reverse order that they are added
        loopv(entgroup)
        {
            entity &e = *ents[entgroup[i]];
            particle_textcopy(e.o, entities::entnameinfo(e), PART_TEXT, 1, 0xA9A9A9, 2.0f);
        }
        loopv(ents)
        {
            entity &e = *ents[i];
            if(e.type==ET_EMPTY) continue;
            if(e.type >= ET_LIGHT && e.type <= ET_DECAL)
            {
                particle_textcopy(e.o, entities::entnameinfo(e), PART_TEXT, 1, entcolor[e.type], 2.0f);
                if(entityicons) particle_icon(e.o, e.type%4, e.type/4, PART_EDITOR_ICONS, 1, entityiconscolor ? entcolor[e.type] : 0xFFFFFF, 2.0f, 0);
            }
            else
            {
                particle_textcopy(e.o, entities::entnameinfo(e), PART_TEXT, 1, 0x00FFFF, 2.0f);
                regular_particle_splash(PART_EDIT, 2, 80, e.o, 0x00FFFF, 0.16f*particlesize/100.0f);
            }
        }
    }
}
