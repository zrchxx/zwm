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
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>

// TODO
// 1. A LOT
// 2. Some functions that depend on the sel client do not work
// 3. Finish the making of a focus function
// 4. improve the code of DWM and make it simpler and clear
// 5. Add more events to manage
// 6. Add more EWMH atoms
// 7. Add dmenu (Install dmenu)
// 8. Add a way to move a window and center it

// Globals //

// Macros
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x)[0])

// Mod mask Super && Shift
#define MODKEY Mod4Mask
#define SHIFT ShiftMask

// Structures
typedef union 
{
    int i;
    unsigned int ui;
    float f;
    const void *v;
} Arg;

typedef struct Client Client; 
struct Client 
{
    int x, y, h, w;
    int oldX, oldY, oldH, oldW;
    bool isFullscreen;
    Client *next;
    Window win;
};

typedef struct 
{
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
void printError(char errorName[], bool checkExit);
int xerrorHandler(Display *dp, XErrorEvent *ee);
void quitWM(const Arg *arg);
void killClient(const Arg *arg);
void grabKeys(void);
void spawn(const Arg *arg);
Client *searchClient(Window w);
Client *addClient(Window w);
void rmClient(Client *c);
void setFullscreen(const Arg *arg);
void focus(Client *c);
void toggleFocus(const Arg *arg);
void evHandler(void);
void handleAtoms(void);
void setup(void);

// Variables
static Display *dp;
static Window rt;
static int sc;
static XEvent ev;
static Client *head = NULL;
static Client *sel = NULL;
static int running = 1;

//////// USER CONFIG ///////

// Programs
static const char *termcmd[] = {"ns", NULL};
static const char *browsercmd[] = {"librewolf", NULL};
static const char *autostart[] = {"/home/zrchx/Dev/Dots/BIN/autostart", NULL};

// Keybinds
static const Key keys[] = { 
    /* Modifiers        Key        Function           Argument */
    {MODKEY|SHIFT,      XK_q,      quitWM,            {0}},
    {MODKEY,            XK_z,      killClient,        {0}},
    {MODKEY,            XK_f,      setFullscreen,     {0}},
    {MODKEY,            XK_Tab,    toggleFocus,       {.i = 1}},
    {MODKEY|SHIFT,      XK_Tab,    toggleFocus,       {.i = -1}},
    {MODKEY,            XK_Return, spawn,             {.v = termcmd}},
    {MODKEY,            XK_b,      spawn,             {.v = browsercmd}},
};

//////// USER CONFIG ///////

// Prints error codes with specific format
void printError(char errorName[], bool checkExit)
{
    fprintf(stderr, "zwm error: %s\n", errorName);
    if (checkExit)
        exit(1);
}

// Handle X11 errors
int xerrorHandler(Display *dp, XErrorEvent *ee) 
{
    (void)dp;
    fprintf(stderr,"zwm fatal error:\n TYPE: %d;\n SERIAL: %ld;\n CODE: %d;\n REQUEST_CODE: %d;\n"
                    "MINOR: %d;\n",
            ee->type, ee->serial, ee->error_code, ee->request_code, ee->minor_code);
    return 0;
}

// Quit the wm
void quitWM(const Arg *arg)
{
    running = 0;
}

// Grab keys function
void grabKeys(void)
{
    KeyCode code;
    XUngrabKey(dp, AnyKey, AnyModifier, rt);
    
    for (unsigned int i = 0; i < ARRAYSIZE(keys); i++)
        if ((code = XKeysymToKeycode(dp, keys[i].keysym))) 
            XGrabKey(dp, code, keys[i].mod, rt, True, GrabModeAsync, GrabModeAsync);
}

// Launch programs
void spawn(const Arg *arg)
{
    struct sigaction sa;
    if (fork() == 0)
    {
        if (dp)
            close(ConnectionNumber(dp));

        setsid();
        
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = SIG_DFL;
        sigaction(SIGCHLD, &sa, NULL);

        execvp(((char **)arg->v)[0], (char **)arg->v);
        printError("Problem in spawn function", true);
    }
}

// Search for a client by his window id
Client *searchClient(Window w)
{
    if (head != NULL) 
    {
        Client *c = head;
        Client *x = head;
        
        do 
        {
            if (c->win == w)
                return c;
            c = c->next;
        } 
        while (c != x);
    }
    return NULL;
}

// Add a client to the list
Client *addClient(Window w)
{
    if (searchClient(w) != NULL) 
    {
        printError("The client already exist", false);
        return NULL;
    }

    Client *new = malloc(sizeof(Client));
    
    if (new == NULL) 
    {
        printError("Failed to allocate memory for a client", false);
        return NULL;
    } 

    XWindowAttributes wa;
    XGetWindowAttributes(dp, w, &wa);

    new->win = w;
    new->isFullscreen = false;
    new->x = (DisplayWidth(dp, sc) - wa.width) / 2;
    new->y = (DisplayHeight(dp, sc) - wa.height) / 2;
    new->w = wa.width;
    new->h = wa.height;
    
    if (head != NULL) 
    {
        new->next = head->next;
        head->next = new;
        head = new;
    } 
    else 
    {
        new->next = new;
        head = new;
    }

    return new;
}

// Search for a client and removes it
void rmClient(Client *c)
{
    // FIX THIS // Kinda fixed?
    Client *x = head;

    if (c != NULL) 
    {
        if (c->next != c) 
        {
            while (x->next != c)
                x = x->next;
            x->next = c->next;
            if (c == sel)
                sel = x;
        } 
        else 
        {
            free(c);
            head = NULL;
            return;
        }
        free(c);
        return;
    }
    return;
}

// Kill a client
void killClient(const Arg *arg)
{
    if (!sel)
        return;
    XKillClient(dp, sel->win);
}

// Fullscreen toggle function
void setFullscreen(const Arg *arg)
{
    Client *c = sel;
    
    if (c == NULL) 
        return;

    if (c->isFullscreen) 
    {
        c->isFullscreen = false;
        XMoveResizeWindow(dp, c->win, c->oldX, c->oldY, c->oldW, c->oldH);
        XDeleteProperty(dp, c->win, netWMState);
    } 
    else if (!c->isFullscreen) 
    {
        c->oldX = c->x;
        c->oldY = c->y;
        c->oldW = c->w;
        c->oldH = c->h;

        c->isFullscreen = true;
        XMoveResizeWindow(dp, c->win, 0, 0, 
                          DisplayWidth(dp, sc), DisplayHeight(dp, sc));
        XChangeProperty(dp, c->win, netWMState, XA_ATOM, 32, PropModeReplace, 
                        (unsigned char *)&netWMStateFullscreen, 1);
    }
    XFlush(dp);
}

void unfocus(Client *c)
{
    // Remove focus from sel window to root window
    XSetInputFocus(dp, rt, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dp, rt, netActiveWindow);
}

// Focus management
void focus(Client *c)
{
    // Set focus to c window
    XRaiseWindow(dp, c->win);
    XSetInputFocus(dp, c->win, RevertToParent, CurrentTime);
    XChangeProperty(dp, rt, netActiveWindow, XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *) &(c->win), 1);
    sel = c;
    XFlush(dp);
}

// Handle how focus work on a client window
void toggleFocus(const Arg *arg)
{
    if (!sel)
        return;

    Client *c = NULL;

    if (arg->i == 1)
    {
        c = sel->next;
        focus(c);
    }
    else if (arg->i == -1)
    {
        for (c = head; c->next != sel; c = c->next);
        focus(c);
    }
}

// Handle events
void evHandler(void)
{
    switch (ev.type) 
    {   
        case MapRequest: 
        {
            sel = addClient(ev.xmaprequest.window);

            XMoveResizeWindow(dp, sel->win, sel->x, sel->y, sel->w, sel->h);
            XMapWindow(dp, sel->win);
            focus(sel);
            break;
        }
        case DestroyNotify: 
        {
            Client *c;
            if ((c = searchClient(ev.xdestroywindow.window)))
                rmClient(c);
            break;
        }
        case KeyPress: 
        {
            KeySym keysym = XkbKeycodeToKeysym(dp, (KeyCode)ev.xkey.keycode, 0, 0);

            for (unsigned int i = 0; i < ARRAYSIZE(keys); i++)
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

// Check and balances
void setup(void)
{
    const Arg autostartArg = {.v = autostart};
    spawn(&autostartArg);

    struct sigaction sa;
    
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa, NULL);

    while (waitpid(-1, NULL, WNOHANG) > 0);

    XSetErrorHandler(xerrorHandler);
    
    sc = DefaultScreen(dp);
    rt = RootWindow(dp, sc);

    handleAtoms();

    XSelectInput(dp, rt, SubstructureRedirectMask | SubstructureNotifyMask);
  
    grabKeys();
    
    XFlush(dp);
}

int main(void)
{
    if (!(dp = XOpenDisplay(NULL)))
        printError("Cannot open display", true);
   
    setup();
    
    XSync(dp, False);
    
    while (running) {
        XNextEvent(dp, &ev);
        evHandler();
    }

    XCloseDisplay(dp);
}
