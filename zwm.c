#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

// TODO
// 1. A LOT
// 2. Some functions that depend on the sel client do not work
// 3. Finish the making of a focus function
// 4. improve the code of DWM and make it simpler and clear
// 5. Add more events to manage
// 6. Add more EWMH atoms
// 7. Add dmenu (Install dmenu)

// Globals //

// Macros
#define LENGHTOF(x) (sizeof(x) / sizeof(x)[0])

// Mod mask Super && Shift
#define MODKEY Mod4Mask
#define SHIFT ShiftMask

// Structures
typedef union {
    int i;
    unsigned int ui;
    float f;
    const void *v;
} Arg;

typedef struct Client Client; 
struct Client {
    int x, y, h, w;
    int oldX, oldY, oldH, oldW;
    bool isFullscreen;
    Client *next;
    Window win;
};

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(const Arg *);
    const Arg arg;
} Key;

// Atoms
Atom netSupported, 
    netClientList,
    netActiveWindow,
    netWMName,
    netWMState, 
    netWMStateFullscreen;

// Function declaration
void printError(char errorName[]);
int xerrorHandler(Display *dp, XErrorEvent *ee);
void quitWM(const Arg *arg);
void killClient(const Arg *arg);
void grabKeys(void);
void spawn(const Arg *arg);
Client *searchClient(Window w);
Client *addClient(Window w);
void rmClient(Window w);
void setFullscreen(const Arg *arg);
void focusClient(Client *c);
void unfocusClient(Client *c);
void focus(Client *c);
void toggleFocusClient(Client *c, int moveBy);
void evHandler(void);
void handleAtoms(void);
void childHandle(int sig);
void setup(void);

// Variables
static Display *dp;
static Window rt;
static Screen *sc;
static XEvent ev;
static Client *head = NULL;
static Client *sel = NULL;
static int running = 1;

////// START USER CONFIG //////

// Programs
static const char *termcmd[] = {"ns", NULL};
static const char *browsercmd[] = {"librewolf", NULL};

// Keybinds
static const Key keys[] = { 
    /* Modifiers        Key        Function           Argument */
    {MODKEY|SHIFT,      XK_q,      quitWM,            {0}},
    {MODKEY,            XK_z,      killClient,        {0}},
    {MODKEY,            XK_f,      setFullscreen,     {0}},
    //{MODKEY,            XK_Tab,    toggleFocusClient, {.i = 1}},
    //{MODKEY|SHIFT,      XK_Tab,    toggleFocusClient, {.i = -1}},
    {MODKEY,            XK_Return, spawn,             {.v = termcmd}},
    {MODKEY,            XK_b,      spawn,             {.v = browsercmd}},
};

//////// END USER CONFIG ///////

// Prints error codes with specific format
void printError(char errorName[])
{
    fprintf(stderr, "zwm error: %s\n", errorName);
}

// Handle X11 errors
int xerrorHandler(Display *dp, XErrorEvent *ee) 
{
    fprintf(stderr,"ZWM X11 ERROR:\n"
            "TYPE: %d;\n"
            "SERIAL: %ld;\n"
            "CODE: %d;\n"
            "REQUEST_CODE: %d;\n"
            "MINOR: %d;\n",
            ee->type, ee->serial, ee->error_code, ee->request_code, 
            ee->minor_code);
    return 0;
}

// Quit the wm
void quitWM(const Arg *arg)
{
    running = 0;
}

// Kill a client
void killClient(const Arg *arg)
{
    if (!sel) // ADD A CHECK FOR ROOT WINDOW
        return;
    XKillClient(dp, sel->win);
}

// Grab keys function
void grabKeys(void)
{
    KeyCode code;
    XUngrabKey(dp, AnyKey, AnyModifier, rt);
    
    for (unsigned int i = 0; i < LENGHTOF(keys); i++)
        if ((code = XKeysymToKeycode(dp, keys[i].keysym))) {
            XGrabKey(dp, code, keys[i].mod, rt, True, 
                     GrabModeAsync, GrabModeAsync);
        }
}

// Launch programs
void spawn(const Arg *arg)
{
    if (fork() == 0) {
        execvp(((char **)arg->v)[0], (char **)arg->v);

        printError("Problem in spawn function");
        exit(1);
    }
}

// Search for a client by his window id
Client *searchClient(Window w)
{
    if (head != NULL) {
        Client *c = head;
        Client *x = head;
        
        do {
            if (c->win == w)
                return c;
            c = c->next;
        } while (c != x);
    }
    return NULL;
}

// Add a client to the list
Client *addClient(Window w)
{
    if (searchClient(w) != NULL) {
        printError("The client already exist");
        return NULL;
    }

    Client *new = malloc(sizeof(Client));
    
    if (new == NULL) {
        printError("Failed to allocate memory for a client");
        return NULL;
    } 

    XWindowAttributes wa;
    XGetWindowAttributes(dp, w, &wa);

    new->win = w;
    new->isFullscreen = false;
    new->x = (XWidthOfScreen(sc) - wa.width) / 2;
    new->y = (XHeightOfScreen(sc) - wa.height) / 2;
    new->w = wa.width;
    new->h = wa.height;
    
    if (head != NULL) {
        new->next = head->next;
        head->next = new;
        head = new;
    } else {
        new->next = new;
        head = new;
    }

    return new;
}

// Search for a client and removes it
void rmClient(Window w)
{
    // FIX THIS
    Client *c = searchClient(w);
    Client *x = head;

    if (c != NULL) {
        if (c->next != c) {
            while (x->next != c)
                x = x->next;
            x->next = c->next;
            if (c == head)
                head = x;
        } else {
            free(c);
            head = NULL;
            return;
        }
        free(c);
        return;
    }
    return;
}

// Fullscreen toggle function
void setFullscreen(const Arg *arg)
{
    Client *c = sel;
    
    if (c == NULL) 
        return;

    if (c->isFullscreen) {
        c->isFullscreen = false;
        XMoveResizeWindow(dp, c->win, c->oldX, c->oldY, c->oldW, c->oldH);
        XDeleteProperty(dp, c->win, netWMState);
    } else if (!c->isFullscreen) {
        c->oldX = c->x;
        c->oldY = c->y;
        c->oldW = c->w;
        c->oldH = c->h;

        c->isFullscreen = true;
        XMoveResizeWindow(dp, c->win, 0, 0,  
                          XWidthOfScreen(sc), XHeightOfScreen(sc));
        XChangeProperty(dp, c->win, netWMState, XA_ATOM, 32, PropModeReplace, 
                        (unsigned char *)&netWMStateFullscreen, 1);
    }
    XFlush(dp);
}


// Focus a client
void focusClient(Client *c)
{
    if (!c)
        return;

    XSetInputFocus(dp, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dp, rt, netActiveWindow, XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *) &(c->win), 1);
}

// Remove focus of a client
void unfocusClient(Client *c)
{
    if (!c)
        return;

    XSetInputFocus(dp, rt, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dp, rt, netActiveWindow);
}

// Focus management
void focus(Client *c)
{
    Client *x = head;

    if (!c)
        for (c = sel; c != x; c = c->next)
    
    if (sel && sel != c)
        unfocusClient(sel);

    if (c)
        focusClient(c);
    else {
        XSetInputFocus(dp, rt, RevertToPointerRoot, CurrentTime);
        XDeleteProperty(dp, rt, netActiveWindow);
    }

    sel = c;
}

// Handle how focus work on a client window
void toggleFocusClient(Client *c, int moveBy)
{
    Client *x = head;

    if (moveBy > 0) {
        for (c = c->next; c != x; c = c->next);
        if (!c)
            for (c = head; c != x; c = c->next);
    } else {
        for (x = head; x != c->next; x = x->next);
        c = x;
        if (!c)
            for (; x; x = x->next);
        c = x;
    }

    if (c) {
        focus(c);
    }
}

// Handle events
void evHandler(void)
{
    switch (ev.type) {   
        case MapRequest: {
            if (sel != NULL)
                unfocusClient(sel);

            sel = addClient(ev.xmaprequest.window);

            XMoveResizeWindow(dp, sel->win, sel->x, sel->y,
                              sel->w, sel->h);
            XMapWindow(dp, sel->win);
            focusClient(sel);
            break;
        }
        case DestroyNotify: {
            rmClient(ev.xdestroywindow.window);
            break;
        }
        case KeyPress: {
            KeySym keysym;
            
            keysym = XKeycodeToKeysym(dp, (KeyCode)ev.xkey.keycode, 0);
            for (unsigned int i = 0; i < LENGHTOF(keys); i++)
                if (keysym == keys[i].keysym && keys[i].func)
                    keys[i].func(&(keys[i].arg));
            break;
        }
        default:
            break;
    }
}

// Handle atoms
void handleAtoms(void)
{
    netSupported = XInternAtom(dp, "_NET_SUPPORTED", False);
    netActiveWindow = XInternAtom(dp, "_NET_ACTIVE_WINDOW", False);
    netClientList = XInternAtom(dp, "_NET_CLIENT_LIST", False);
    netWMState = XInternAtom(dp, "_NET_WM_STATE", False);
    netWMStateFullscreen = XInternAtom(dp, "_NET_WM_STATE_FULLSCREEN", False);
    Atom netSupportedAtoms[] = {
        netClientList,
        netActiveWindow,
        netWMState, 
        netWMStateFullscreen,
    };

    XChangeProperty(dp, rt, netSupported, XA_ATOM, 32, PropModeReplace, 
                    (unsigned char *)netSupportedAtoms, 
                    sizeof(netSupportedAtoms) / sizeof(Atom));
}

// Handle zombies childs
void childHandle(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Check and balances
void setup(void)
{   
    XSetErrorHandler(xerrorHandler);
    
    sc = XScreenOfDisplay(dp, DefaultScreen(dp));
    rt = DefaultRootWindow(dp);

    handleAtoms();

    XUngrabKey(dp, AnyKey, AnyModifier, rt);
    
    XSelectInput(dp, rt, SubstructureRedirectMask | SubstructureNotifyMask);
  
    grabKeys();
    
    system("~/Dev/Dots/BIN/autostart");

    signal(SIGCHLD, childHandle);
    
    XFlush(dp);
}

int main(void)
{
    if (!(dp = XOpenDisplay(NULL))) {
        printError("Cannot open display");
        exit(1);
    }
   
    setup();
    
    XSync(dp, False);
    
    while (running) {
        XNextEvent(dp, &ev);
        evHandler();
    }

    XCloseDisplay(dp);
}
