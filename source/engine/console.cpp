// console.cpp: the console buffer, its display, and command line control

#include "unicode.h"
#include "engine.h"

#define MAXCONLINES 1000
struct cline { char *line; int type, outtime; float fontsize, w; text::Label label; };
reversequeue<cline, MAXCONLINES> conlines;

int commandmillis = -1, inputmillis = 0;
string commandbuf;
char *commandaction = NULL, *commandprompt = NULL;
enum { CF_COMPLETE = 1<<0, CF_EXECUTE = 1<<1 };
int commandflags = 0, commandpos = -1;

VARFP(maxcon, 10, 200, MAXCONLINES,
{
    while(conlines.length() > maxcon)
    {
        cline &cl = conlines.pop();
        delete[] cl.line;
    }
});

#define CONSTRLEN 512

VARP(contags, 0, 3, 3);

const char *getprefix(int type)
{
    const char *prefix = "";
    switch(type)
    {
        case CON_CHAT:
        case CON_TEAMCHAT:
        case CON_GAMEINFO:
        case CON_FRAGINFO: break;
        case CON_INFO:     prefix = "\f0INFO";    break;
        case CON_WARN:     prefix = "\f5WARNING"; break;
        case CON_ERROR:    prefix = "\f3ERROR";   break;
        case CON_DEBUG:    prefix = "\f6DEBUG";   break;
        case CON_INIT:     prefix = "\f8INIT";    break;
        case CON_ECHO:     prefix = "\f1ECHO";    break;
        case CON_NOTICE:   prefix = "\f2NOTICE";  break;
        default:           prefix = "\f4UNKNOWN"; break;
    }
    if(prefix[0] != '\0') return tempformatstring("[\fs%s\fr] ", prefix);
    return "";
}

void conline(int type, const char *sf) // add a line to the console buffer
{
    char *buf = NULL;
    if(type&CON_TAG_MASK) for(int i = conlines.length()-1; i >= max(conlines.length()-contags, 0); i--)
    {
        int prev = conlines.removing(i).type;
        if(!(prev&CON_TAG_MASK)) break;
        if(type == prev)
        {
            buf = conlines[i].line;
            //buf = conlines.remove(i).line;
            conlines.remove(i);
            break;
        }
    }
    if(!buf) buf = conlines.length() >= maxcon ? conlines.remove().line : newstring("", CONSTRLEN-1);
    cline &cl = conlines.add();
    cl.label.clear();
    cl.line = buf;
    cl.type = type;
    cl.outtime = totalmillis; // for how long to keep line on screen
    cl.fontsize = cl.w = 0;
    defformatstring(prefixedsf, "%s%s", getprefix(type), sf);
    copystring(cl.line, prefixedsf, CONSTRLEN);
}

void conoutfv(int type, const char *fmt, va_list args)
{
    static char buf[CONSTRLEN];
    vformatstring(buf, fmt, args, sizeof(buf));
    conline(type, buf);
    logoutf("%s", buf);
}

ICOMMAND(fullconsole, "iN$", (int *val, int *numargs, ident *id),
{
    if(*numargs > 0) UI::holdui("fullconsole", *val!=0);
    else
    {
        int vis = UI::uivisible("fullconsole") ? 1 : 0;
        if(*numargs < 0) intret(vis);
        else printvar(id, vis);
    }
});
ICOMMAND(toggleconsole, "", (), UI::toggleui("fullconsole"));

float rendercommand(float x, float y, float w)
{
    if(commandmillis < 0) return 0;

    char buf[CONSTRLEN];
    const char *prompt = commandprompt ? commandprompt : ">";
    formatstring(buf, "%s %s", prompt, commandbuf);

    pushfont();
    setfont("default");
    const text::Label label = text::prepare_for_console(buf, w, commandpos>=0 ? commandpos+1 + strlen(prompt) : strlen(buf));
    y -= label.height();
    
    label.draw_as_console(x, y);
    
    popfont();
    return label.height();
}

VARP(consize, 0, 5, 100);
VARP(miniconsize, 0, 5, 100);
VARP(miniconwidth, 0, 40, 100);
VARP(confade, 0, 30, 60);
VARP(miniconfade, 0, 30, 60);
VARP(fullconsize, 0, 75, 100);
HVARP(confilter, 0, 0xFF24C0, 0xFFFFFF);
HVARP(fullconfilter, 0, 0xFFFFFF, 0xFFFFFF);
HVARP(miniconfilter, 0, 0, 0xFFFFFF);

int conskip = 0, miniconskip = 0;

void setconskip(int &skip, int filter, int n)
{
    filter &= CON_FLAGS;
    int offset = abs(n), dir = n < 0 ? -1 : 1;
    skip = clamp(skip, 0, conlines.length()-1);
    while(offset)
    {
        skip += dir;
        if(!conlines.inrange(skip))
        {
            skip = clamp(skip, 0, conlines.length()-1);
            return;
        }
        if(conlines[skip].type&filter) --offset;
    }
}

ICOMMAND(conskip, "i", (int *n), setconskip(conskip, UI::uivisible("fullconsole") ? fullconfilter : confilter, *n));
ICOMMAND(miniconskip, "i", (int *n), setconskip(miniconskip, miniconfilter, *n));

ICOMMAND(clearconsole, "", (),
{
    while(conlines.length())
    {
        cline &cl = conlines.pop();
        delete[] cl.line;
        cl.label.clear();
    }
});

// free conline textures, necessary when changing font settings or calling `resetgl()`
void clearconsoletextures()
{
    loopv(conlines) conlines[i].label.clear();
}

float drawconlines(int conskip, int confade, float conwidth, float conheight, float conoff, int maxlines, bool full, int filter, float y = 0, int dir = 1)
{
    filter &= CON_FLAGS;
    int numl = conlines.length(), offset = min(conskip, numl);

    if(confade)
    {
        if(!conskip)
        {
            numl = 0;
            loopvrev(conlines) if(totalmillis-conlines[i].outtime < confade*1000) { numl = i+1; break; }
        }
        else offset--;
    }

    int totalheight = 0;
    int n = 0;
    loopi(numl) //determine visible height
    {
        // shuffle backwards to fill if necessary
        int idx = offset+i < numl ? offset+i : --offset;
        if(!(conlines[idx].type&filter)) continue;
        char *line = conlines[idx].line;
        int width, height;
        const text::Label& label = conlines[idx].label;
        if(conlines[idx].w != conwidth || conlines[idx].fontsize != fontsize || !label.valid())
        {
            text::measure(line, conwidth, width, height);
        }
        else
        {
            width = label.width();
            height = label.height();
        }
        if(maxlines > 0) { if(++n > maxlines) { numl = i; if(offset == idx) ++offset; break; } }
        else if(totalheight + height > conheight) { numl = i; if(offset == idx) ++offset; break; }
        totalheight += height;
    }
    if(dir > 0) y = conoff;
    loopi(numl)
    {
        int idx = offset + (dir > 0 ? numl-i-1 : i);
        if(!(conlines[idx].type&filter)) continue;
        char *line = conlines[idx].line;
        text::Label& label = conlines[idx].label;
        if(conlines[idx].w != conwidth || conlines[idx].fontsize != fontsize || !label.valid())
        {
            if(!full) label = text::prepare_for_console(line, conwidth, -1);
            else label = text::prepare(line, conwidth, bvec(255, 255, 255), -1);
            conlines[idx].w = conwidth;
            conlines[idx].fontsize = fontsize;
        }
        if(dir <= 0) y -= label.height();
        if(label.valid())
        {
            if(!full) label.draw_as_console(conoff, y);
            else label.draw(conoff, y);
        }
        if(dir > 0) y += label.height();
    }
    return y+conoff;
}

float renderfullconsole(float w, float h)
{
    pushfont();
    setfont("default");
    setfontsize(hudh * conscale / CONSOLETEXTROWS);
    float conpad = FONTH*1.5/2,
          conheight = h - 2*conpad,
          conwidth = w - 2*conpad;
    drawconlines(conskip, 0, conwidth, conheight, conpad, 0, true, fullconfilter);
    popfont();
    return conheight + 2*conpad;
}

float renderconsole(float w, float h, float abovehud)
{
    float conpad = FONTH*1.5/2,
          conheight = min(float(FONTH*1.5*consize), h - 2*conpad),
          conwidth = w - 2*conpad - game::clipconsole(w, h);
    pushfont();
    setfont("default");
    setfontsize(hudh * conscale / CONSOLETEXTROWS);
    float y = drawconlines(conskip, confade, conwidth, conheight, conpad, consize, false, confilter);
    if(miniconsize && miniconwidth)
        drawconlines(miniconskip, miniconfade, (miniconwidth*(w - 2*conpad))/100, min(float(FONTH*1.5*miniconsize), abovehud - y), conpad, miniconsize, false, miniconfilter, abovehud, -1);
    popfont();
    return y;
}

// keymap is defined externally in keymap.cfg

struct keym
{
    enum
    {
        ACTION_DEFAULT = 0,
        ACTION_SPECTATOR,
        ACTION_EDITING,
        ACTION_UI,
        NUMACTIONS
    };

    int code;
    char *name;
    char *actions[NUMACTIONS], *window;;
    bool pressed;

    keym() : code(-1), name(NULL), pressed(false)
    {
        loopi(NUMACTIONS) actions[i] = newstring("");
        window = newstring("");
    }
    ~keym()
    { 
        DELETEA(name);
        loopi(NUMACTIONS) DELETEA(actions[i]);
        DELETEA(window);
    }

    void clear(int type);
    void clear() { loopi(NUMACTIONS) clear(i); }
};

hashtable<int, keym> keyms(128);

void keymap(int *code, char *key)
{
    if(identflags&IDF_OVERRIDDEN) { conoutf(CON_ERROR, "cannot override keymap %d", *code); return; }
    keym &km = keyms[*code];
    km.code = *code;
    DELETEA(km.name);
    km.name = newstring(key);
}

COMMAND(keymap, "is");

keym *keypressed = NULL;
char *keyaction = NULL;

const char *getkeyname(int code)
{
    keym *km = keyms.access(code);
    return km ? km->name : NULL;
}

void searchbinds(char *action, int type)
{
    vector<char> names;
    enumerate(keyms, keym, km,
    {
        if(!strcmp(km.actions[type], action))
        {
            if(names.length()) names.add(' ');
            names.put(km.name, strlen(km.name));
        }
    });
    names.add('\0');
    result(names.getbuf());
}

keym *findbind(char *key)
{
    enumerate(keyms, keym, km,
    {
        if(!strcasecmp(km.name, key)) return &km;
    });
    return NULL;
}

void getbind(char *key, int type)
{
    keym *km = findbind(key);
    result(km ? km->actions[type] : "");
}

void bindkey(char *key, char *action, int state, const char *cmd, const char* window = NULL)
{
    if(identflags&IDF_OVERRIDDEN) { conoutf(CON_ERROR, "cannot override %s \"%s\"", cmd, key); return; }
    keym *km = findbind(key);
    if(!km)
    { 
        conoutf(CON_ERROR, "unknown key \"%s\"", key);
        return;
    }
    if(window)
    {
        km->window = newstring(window);
    }
    char *&binding = km->actions[state];
    if(!keypressed || keyaction!=binding) delete[] binding;
    // trim white-space to make searchbinds more reliable
    while(iscubespace(*action)) action++;
    int len = strlen(action);
    while(len>0 && iscubespace(action[len-1])) len--;
    binding = newstring(action, len);
}

void binduikey(char* key, char* action, const char* window)
{
    bindkey(key, action, keym::ACTION_UI, "uibind", window);
}

ICOMMAND(bind,     "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_DEFAULT, "bind"));
ICOMMAND(specbind, "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_SPECTATOR, "specbind"));
ICOMMAND(editbind, "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_EDITING, "editbind"));
ICOMMAND(getbind,     "s", (char *key), getbind(key, keym::ACTION_DEFAULT));
ICOMMAND(getspecbind, "s", (char *key), getbind(key, keym::ACTION_SPECTATOR));
ICOMMAND(geteditbind, "s", (char *key), getbind(key, keym::ACTION_EDITING));
ICOMMAND(searchbinds,     "s", (char *action), searchbinds(action, keym::ACTION_DEFAULT));
ICOMMAND(searchspecbinds, "s", (char *action), searchbinds(action, keym::ACTION_SPECTATOR));
ICOMMAND(searcheditbinds, "s", (char *action), searchbinds(action, keym::ACTION_EDITING));

void keym::clear(int type)
{
    char *&binding = actions[type];
    if(binding[0])
    {
        if(!keypressed || keyaction!=binding) delete[] binding;
        binding = newstring("");
    }
    if(window[0] && type == keym::ACTION_UI)
    {
        delete[] window;
        window = newstring("");
    }
}
ICOMMAND(clearbinds, "", (), enumerate(keyms, keym, km, km.clear(keym::ACTION_DEFAULT)));
ICOMMAND(clearspecbinds, "", (), enumerate(keyms, keym, km, km.clear(keym::ACTION_SPECTATOR)));
ICOMMAND(cleareditbinds, "", (), enumerate(keyms, keym, km, km.clear(keym::ACTION_EDITING)));
ICOMMAND(clearallbinds, "", (), enumerate(keyms, keym, km, km.clear()));

void clearuibinds(const char* window)
{
    enumerate(keyms, keym, km,
    {
        if(!window || strcmp(km.window, window)) km.clear(keym::ACTION_UI));
    }
}
ICOMMAND(clearuibinds, "", (), clearuibinds());

bool isfullconsoletoggled = false;
ICOMMAND(togglefullconsole, "", (),
{
    isfullconsoletoggled = UI::toggleui("fullconsole");
});
VARP(fullconsolecommand, 0, 1, 1);

void inputcommand(char *init, char *action = NULL, char *prompt = NULL, char *flags = NULL) // turns input to the command line on or off
{
    inputmillis = totalmillis;
    commandmillis = init ? totalmillis : -1;
    textinput(commandmillis >= 0, TI_CONSOLE);
    keyrepeat(commandmillis >= 0, KR_CONSOLE);
    copystring(commandbuf, init ? init : "");
    DELETEA(commandaction);
    DELETEA(commandprompt);
    commandpos = -1;
    if(action && action[0]) commandaction = newstring(action);
    if(prompt && prompt[0]) commandprompt = newstring(prompt);
    commandflags = 0;
    if(flags) while(*flags) switch(*flags++)
    {
        case 'c': commandflags |= CF_COMPLETE; break;
        case 'x': commandflags |= CF_EXECUTE; break;
        case 's': commandflags |= CF_COMPLETE|CF_EXECUTE; break;
    }
    else if(init) commandflags |= CF_COMPLETE|CF_EXECUTE;
    if(!isfullconsoletoggled && fullconsolecommand)
    {
        if(init) UI::showui("fullconsole");
        else UI::hideui("fullconsole");
    }
}

ICOMMAND(saycommand, "C", (char *init), inputcommand(init));
COMMAND(inputcommand, "ssss");

struct hline
{
    char *buf, *action, *prompt;
    int flags;

    hline() : buf(NULL), action(NULL), prompt(NULL), flags(0) {}
    ~hline()
    {
        DELETEA(buf);
        DELETEA(action);
        DELETEA(prompt);
    }

    void restore()
    {
        inputmillis = totalmillis;
        copystring(commandbuf, buf);
        if(commandpos >= (int)strlen(commandbuf)) commandpos = -1;
        DELETEA(commandaction);
        DELETEA(commandprompt);
        if(action) commandaction = newstring(action);
        if(prompt) commandprompt = newstring(prompt);
        commandflags = flags;
    }

    bool shouldsave()
    {
        return strcmp(commandbuf, buf) ||
               (commandaction ? !action || strcmp(commandaction, action) : action!=NULL) ||
               (commandprompt ? !prompt || strcmp(commandprompt, prompt) : prompt!=NULL) ||
               commandflags != flags;
    }

    void save()
    {
        buf = newstring(commandbuf);
        if(commandaction) action = newstring(commandaction);
        if(commandprompt) prompt = newstring(commandprompt);
        flags = commandflags;
    }

    void run()
    {
        if(flags&CF_EXECUTE && buf[0]=='/') execute(buf+1);
        else if(action)
        {
            alias("commandbuf", buf);
            execute(action);
        }
        else game::toserver(buf);
    }
};
vector<hline *> history;
int histpos = 0;

VARP(maxhistory, 0, 1000, 10000);

void history_(int *n)
{
    static bool inhistory = false;
    if(!inhistory && history.inrange(*n))
    {
        inhistory = true;
        history[history.length()-*n-1]->run();
        inhistory = false;
    }
}

COMMANDN(history, history_, "i");

struct releaseaction
{
    keym *key;
    union
    {
        char *action;
        ident *id;
    };
    int numargs;
    tagval args[3];
};
vector<releaseaction> releaseactions;

const char *addreleaseaction(char *s)
{
    if(!keypressed) { delete[] s; return NULL; }
    releaseaction &ra = releaseactions.add();
    ra.key = keypressed;
    ra.action = s;
    ra.numargs = -1;
    return keypressed->name;
}

tagval *addreleaseaction(ident *id, int numargs)
{
    if(!keypressed || numargs > 3) return NULL;
    releaseaction &ra = releaseactions.add();
    ra.key = keypressed;
    ra.id = id;
    ra.numargs = numargs;
    return ra.args;
}

void onrelease(const char *s)
{
    addreleaseaction(newstring(s));
}

COMMAND(onrelease, "s");

void execbind(keym &k, bool isdown)
{
    loopv(releaseactions)
    {
        releaseaction &ra = releaseactions[i];
        if(ra.key==&k)
        {
            if(ra.numargs < 0)
            {
                if(!isdown) execute(ra.action);
                delete[] ra.action;
            }
            else execute(isdown ? NULL : ra.id, ra.args, ra.numargs);
            releaseactions.remove(i--);
        }
    }
    if(isdown)
    {
        int state = keym::ACTION_DEFAULT;
        if(k.window[0] && UI::uivisible(k.window))
        {
            state = keym::ACTION_UI;
        }
        else if(!mainmenu)
        {
            if(editmode) state = keym::ACTION_EDITING;
            else if(player->state==CS_SPECTATOR) state = keym::ACTION_SPECTATOR;
        }
        char *&action = k.actions[state][0] ? k.actions[state] : k.actions[keym::ACTION_DEFAULT];
        keyaction = action;
        keypressed = &k;
        execute(keyaction);
        keypressed = NULL;
        if(keyaction!=action) delete[] keyaction;
    }
    k.pressed = isdown;
}

bool iskeyheld(char *key)
{
    const keym *km = findbind(key);
    if(!km) return false;
    const SDL_Scancode scancode = SDL_GetScancodeFromKey(km->code);
    return SDL_GetKeyboardState(NULL)[scancode];
}
ICOMMAND(iskeyheld, "s", (char* key), intret(iskeyheld(key) ? 1 : 0));
ICOMMAND(iscmdlineopen, "", (), intret(commandmillis >= 0 ? 1 : 0));

bool consoleinput(const char *str, int len)
{
    if(commandmillis < 0) return false;
    inputmillis = totalmillis;

    resetcomplete();
    int cmdlen = (int)strlen(commandbuf), cmdspace = int(sizeof(commandbuf)) - (cmdlen+1);
    len = min(len, cmdspace);
    if(commandpos<0)
    {
        memcpy(&commandbuf[cmdlen], str, len);
    }
    else
    {
        memmove(&commandbuf[commandpos+len], &commandbuf[commandpos], cmdlen - commandpos);
        memcpy(&commandbuf[commandpos], str, len);
        commandpos += len;
    }
    commandbuf[cmdlen + len] = '\0';

    return true;
}

// returns the contents of the clipboard
void getclipboard()
{
    if(!SDL_HasClipboardText())
    {
        result("");
        return;
    }
    char *cb = SDL_GetClipboardText();
    if(!cb)
    {
        result("");
        return;
    }
    string paste;
    size_t decoded = decodeutf8((uchar *)paste, sizeof(paste)-1, (const uchar *)cb, strlen(cb));
    paste[decoded] = '\0';
    result(paste);
    SDL_free(cb);
}
COMMAND(getclipboard, "");

void pasteconsole()
{
    if(!SDL_HasClipboardText()) return;
    char *cb = SDL_GetClipboardText();
    if(!cb) return;
    consoleinput(cb, strlen(cb));
    SDL_free(cb);
}

// returns the contents of the clipboard
ICOMMAND(getclipboard, "", (),
{
    if(!SDL_HasClipboardText())
    {
        result("");
    }
    else
    {
        char *cb = SDL_GetClipboardText();
        if(!cb)
        {
            result("");
        }
        else
        {
            result(cb);
            SDL_free(cb);
        }
    }
});

static char *skipword(char *s)
{
    while(int c = *s++) if(!iscubespace(c))
    {
        while(int c = *s++) if(iscubespace(c)) break;
        break;
    }
    return s-1;
}

static char *skipwordrev(char *s, int n = -1)
{
    char *e = s + strlen(s);
    if(n >= 0) e = min(e, &s[n]);
    while(--e >= s) if(!iscubespace(*e))
    {
        while(--e >= s && !iscubespace(*e));
        break;
    }
    return e+1;
}

bool consolekey(int code, bool isdown)
{
    if(commandmillis < 0) return false;
    inputmillis = totalmillis;

    #ifdef __APPLE__
        #define MOD_KEYS (KMOD_LGUI|KMOD_RGUI)
        #define SKIP_KEYS (KMOD_LALT|KMOD_RALT)
    #else
        #define MOD_KEYS (KMOD_LCTRL|KMOD_RCTRL)
        #define SKIP_KEYS (KMOD_LCTRL|KMOD_RCTRL)
    #endif

    if(isdown)
    {
        switch(code)
        {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                break;

            case SDLK_HOME:
                if(commandbuf[0]) commandpos = 0;
                break;

            case SDLK_END:
                commandpos = -1;
                break;

            case SDLK_DELETE:
            {
                int len = (int)strlen(commandbuf);
                if(commandpos<0) break;
                uint _codepoint;
                const int s = uni_getchar(&commandbuf[commandpos], _codepoint);
                int end = commandpos+s;
                if(SDL_GetModState()&SKIP_KEYS) end = skipword(&commandbuf[commandpos]) - commandbuf;
                memmove(&commandbuf[commandpos], &commandbuf[end], len + 1 - end);
                resetcomplete();
                if(commandpos>=len-1) commandpos = -1;
                break;
            }

            case SDLK_BACKSPACE:
            {
                int len = (int)strlen(commandbuf), i = commandpos>=0 ? commandpos : len;
                if(i<1) break;
                int start = i - uni_prevchar(commandbuf, i);
                if(SDL_GetModState()&SKIP_KEYS) start = skipwordrev(commandbuf, i) - commandbuf;
                memmove(&commandbuf[start], &commandbuf[i], len - i + 1);
                resetcomplete();
                if(commandpos>0) commandpos = start;
                else if(!commandpos && len<=1) commandpos = -1;
                break;
            }

            case SDLK_LEFT:
                if(SDL_GetModState()&SKIP_KEYS) commandpos = skipwordrev(commandbuf, commandpos) - commandbuf;
                else if(commandpos>0) commandpos -= uni_prevchar(commandbuf, commandpos);
                else if(commandpos<0) commandpos = (int)strlen(commandbuf) - uni_prevchar(commandbuf, strlen(commandbuf));
                break;

            case SDLK_RIGHT:
                if(commandpos>=0)
                {
                    if(SDL_GetModState()&SKIP_KEYS) commandpos = skipword(&commandbuf[commandpos]) - commandbuf;
                    else
                    {
                        uint _codepoint;
                        commandpos += uni_getchar(&commandbuf[commandpos], _codepoint);
                    }
                    if(commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                }
                break;

            case SDLK_UP:
                if(histpos > history.length()) histpos = history.length();
                if(histpos > 0)
                {
                    if(SDL_GetModState()&SKIP_KEYS) histpos = 0;
                    else --histpos;
                    history[histpos]->restore();
                }
                break;

            case SDLK_DOWN:
                if(histpos + 1 < history.length())
                {
                    if(SDL_GetModState()&SKIP_KEYS) histpos = history.length()-1;
                    else ++histpos;
                    history[histpos]->restore();
                }
                break;

            case SDLK_TAB:
                if(commandflags&CF_COMPLETE)
                {
                    complete(commandbuf, sizeof(commandbuf), commandflags&CF_EXECUTE ? "/" : NULL);
                    if(commandpos>=0 && commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                }
                break;

            case SDLK_v:
                if(SDL_GetModState()&MOD_KEYS) pasteconsole();
                break;
        }
    }
    else
    {
        if(code==SDLK_RETURN || code==SDLK_KP_ENTER)
        {
            hline *h = NULL;
            if(commandbuf[0])
            {
                if(history.empty() || history.last()->shouldsave())
                {
                    if(maxhistory && history.length() >= maxhistory)
                    {
                        loopi(history.length()-maxhistory+1) delete history[i];
                        history.remove(0, history.length()-maxhistory+1);
                    }
                    history.add(h = new hline)->save();
                }
                else h = history.last();
            }
            histpos = history.length();
            inputcommand(NULL);

            if(h) h->run();
        }
        else if(code==SDLK_ESCAPE)
        {
            histpos = history.length();
            inputcommand(NULL);

        }
    }

    return true;
}

void processtextinput(const char *str, int len)
{
    if(!UI::textinput(str, len))
        consoleinput(str, len);
}

void processkey(int code, bool isdown, int modstate)
{
    switch(code)
    {
        case SDLK_LGUI: case SDLK_RGUI:
            return;
    }
    keym *haskey = keyms.access(code);
    if(haskey && haskey->pressed) execbind(*haskey, isdown); // allow pressed keys to release
    else if(modstate&KMOD_GUI) return;
    else if(!UI::keypress(code, isdown)) // UI key intercept
    {
        if(!consolekey(code, isdown))
        {
            if(modstate&KMOD_GUI) return;
            if(haskey) execbind(*haskey, isdown);
        }
    }
}

void clear_console()
{
    keyms.clear();
}

void writebinds(stream *f)
{
    static const char * const cmds[3] = { "bind", "specbind", "editbind" };
    vector<keym *> binds;
    enumerate(keyms, keym, km, binds.add(&km));
    binds.sortname();
    loopj(3)
    {
        loopv(binds)
        {
            keym &km = *binds[i];
            if(*km.actions[j])
            {
                if(validateblock(km.actions[j])) f->printf("%s %s [%s]\n", cmds[j], escapestring(km.name), km.actions[j]);
                else f->printf("%s %s %s\n", cmds[j], escapestring(km.name), escapestring(km.actions[j]));
            }
        }
    }
}

// tab-completion of all idents and base maps

enum { FILES_DIR = 0, FILES_VAR, FILES_LIST };

struct fileskey
{
    int type;
    const char *dir, *ext;

    fileskey() {}
    fileskey(int type, const char *dir, const char *ext) : type(type), dir(dir), ext(ext) {}
};

static void cleanfilesdir(char *dir)
{
    int dirlen = (int)strlen(dir);
    while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\'))
        dir[--dirlen] = '\0';
}

struct filesval
{
    int type;
    char *dir, *ext;
    vector<char *> files;
    int millis;

    filesval(int type, const char *dir, const char *ext) : type(type), dir(newstring(dir)), ext(ext && ext[0] ? newstring(ext) : NULL), millis(-1) {}
    ~filesval() { DELETEA(dir); DELETEA(ext); files.deletearrays(); }

    void update()
    {
        if((type!=FILES_DIR && type!=FILES_VAR) || millis >= commandmillis) return;
        files.deletearrays();
        if(type==FILES_VAR)
        {
            string buf;
            buf[0] = '\0';
            if(ident *id = readident(dir)) switch(id->type)
            {
                case ID_SVAR: copystring(buf, *id->storage.s); break;
                case ID_ALIAS: copystring(buf, id->getstr()); break;
            }
            if(!buf[0]) copystring(buf, ".");
            cleanfilesdir(buf);
            listfiles(buf, ext, files);
        }
        else listfiles(dir, ext, files);
        files.sort();
        loopv(files) if(i && !strcmp(files[i], files[i-1])) delete[] files.remove(i--);
        millis = totalmillis;
    }
};

static inline bool htcmp(const fileskey &x, const fileskey &y)
{
    return x.type==y.type && !strcmp(x.dir, y.dir) && (x.ext == y.ext || (x.ext && y.ext && !strcmp(x.ext, y.ext)));
}

static inline uint hthash(const fileskey &k)
{
    return hthash(k.dir);
}

static hashtable<fileskey, filesval *> completefiles;
static hashtable<char *, filesval *> completions;

int completesize = 0;
char *lastcomplete = NULL;

void resetcomplete() { completesize = 0; }

void addcomplete(char *command, int type, char *dir, char *ext)
{
    if(identflags&IDF_OVERRIDDEN)
    {
        conoutf(CON_ERROR, "cannot override complete %s", command);
        return;
    }
    if(!dir[0])
    {
        filesval **hasfiles = completions.access(command);
        if(hasfiles) *hasfiles = NULL;
        return;
    }
    if(type==FILES_DIR) cleanfilesdir(dir);
    if(ext)
    {
        if(strchr(ext, '*')) ext[0] = '\0';
        if(!ext[0]) ext = NULL;
    }
    fileskey key(type, dir, ext);
    filesval **val = completefiles.access(key);
    if(!val)
    {
        filesval *f = new filesval(type, dir, ext);
        if(type==FILES_LIST) explodelist(dir, f->files);
        val = &completefiles[fileskey(type, f->dir, f->ext)];
        *val = f;
    }
    filesval **hasfiles = completions.access(command);
    if(hasfiles) *hasfiles = *val;
    else completions[newstring(command)] = *val;
}

void addfilecomplete(char *command, char *dir, char *ext)
{
    addcomplete(command, FILES_DIR, dir, ext);
}

void addvarcomplete(char *command, char *var, char *ext)
{
    addcomplete(command, FILES_VAR, var, ext);
}

void addlistcomplete(char *command, char *list)
{
    addcomplete(command, FILES_LIST, list, NULL);
}

COMMANDN(complete, addfilecomplete, "sss");
COMMANDN(varcomplete, addvarcomplete, "sss");
COMMANDN(listcomplete, addlistcomplete, "ss");

void complete(char *s, int maxlen, const char *cmdprefix)
{
    int cmdlen = 0;
    if(cmdprefix)
    {
        cmdlen = strlen(cmdprefix);
        if(strncmp(s, cmdprefix, cmdlen)) prependstring(s, cmdprefix, maxlen);
    }
    if(!s[cmdlen]) return;
    if(!completesize) { completesize = (int)strlen(&s[cmdlen]); DELETEA(lastcomplete); }

    filesval *f = NULL;
    if(completesize)
    {
        char *end = strchr(&s[cmdlen], ' ');
        if(end) f = completions.find(stringslice(&s[cmdlen], end), NULL);
    }

    const char *nextcomplete = NULL;
    if(f) // complete using filenames
    {
        int commandsize = strchr(&s[cmdlen], ' ')+1-s;
        f->update();
        loopv(f->files)
        {
            if(strncmp(f->files[i], &s[commandsize], completesize+cmdlen-commandsize)==0 &&
               (!lastcomplete || strcmp(f->files[i], lastcomplete) > 0) && (!nextcomplete || strcmp(f->files[i], nextcomplete) < 0))
                nextcomplete = f->files[i];
        }
        cmdprefix = s;
        cmdlen = commandsize;
    }
    else // complete using command names
    {
        enumerate(idents, ident, id,
            if(strncmp(id.name, &s[cmdlen], completesize)==0 &&
               (!lastcomplete || strcmp(id.name, lastcomplete) > 0) && (!nextcomplete || strcmp(id.name, nextcomplete) < 0))
                nextcomplete = id.name;
        );
    }
    DELETEA(lastcomplete);
    if(nextcomplete)
    {
        cmdlen = min(cmdlen, maxlen-1);
        if(cmdlen) memmove(s, cmdprefix, cmdlen);
        copystring(&s[cmdlen], nextcomplete, maxlen-cmdlen);
        lastcomplete = newstring(nextcomplete);
    }
}

void writecompletions(stream *f)
{
    vector<char *> cmds;
    enumeratekt(completions, char *, k, filesval *, v, { if(v) cmds.add(k); });
    cmds.sort();
    loopv(cmds)
    {
        char *k = cmds[i];
        filesval *v = completions[k];
        if(v->type==FILES_LIST)
        {
            if(validateblock(v->dir)) f->printf("listcomplete %s [%s]\n", escapeid(k), v->dir);
            else f->printf("listcomplete %s %s\n", escapeid(k), escapestring(v->dir));
        }
        else f->printf("%s %s %s %s\n", v->type==FILES_VAR ? "varcomplete" : "complete", escapeid(k), escapestring(v->dir), escapestring(v->ext ? v->ext : "*"));
    }
}

