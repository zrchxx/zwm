#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

// TODO
// 1. Check why fullscreen dont work
// 2. Check if kill process work
// 3. Check the thing on chldHandle
// 4. Add some tilling windows
// 5. Add more support for events
// 6. Make more robust the code and less prone to fail
// 7. More debug

// Globals //

// Window struct
typedef struct Client Client;
struct Client {
    int x, y, w, h;
    int isFullscreen;
    struct Client *next;
    Window win;
};

// Atoms
Atom supported, wmState, wmStateFullscreen;

// Variables
static Display *dp;
static Window rt;
static Screen *sc;
static XEvent ev;
static Client *clients = NULL;
static int running = 1;

#include "config.h"

// String "a" to keycode
KeyCode stringToKeycode(const char *key)
{
    return XKeysymToKeycode(dp, XStringToKeysym(key));
}

// Get keys and put a modifier mask
void keysToModMask(void)
{
    int arraySize = sizeof(modKeys) / sizeof(modKeys[0]);

    for (int i = 0;i < arraySize; i++) 
    {
        XGrabKey(dp, stringToKeycode(modKeys[i]), MODKEY, rt, 1, 1, 1);
    };
}

// Launch programs
void spawn(const char *cmd[])
{
    if (fork() == 0)
    {
        execvp(cmd[0], (char *const *)cmd);

        perror("Error: fail to execute, binary not found.");
        exit(1);
    }
}
// Handle zombies childs
void chldHandle(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Handle X11 errors
int xerrorHandler(Display *dp, XErrorEvent *ee) 
{
    fprintf(stderr,"X11 error:type %d,serial %ld,code %d,request %d,minor %d\n",
            ee->type, ee->serial, ee->error_code, ee->request_code, 
            ee->minor_code);
    return 0;
}

// Autostart programs and deamons
void autostart(void)
{
    int cmdArraySize = sizeof(autostartcmd) / sizeof(autostartcmd[0]);

    for (int i = 0;i < cmdArraySize; i++) 
    {
        spawn(autostartcmd[i]);
    };
}

// Search for the correct client using a window id
Client* getClient(Window w)
{
    Client *c;

    for (c = clients; c ;c = c->next) 
    {
        if (c->win == w) 
        {
            return c;
        }
    }
    return NULL;
}

// Set the focused window to fullscreen
void setFullscreen(Client *c, int state)
{
    switch (state) {
        case 0:
            XMoveResizeWindow(dp, c->win, c->x, c->y, c->w, c->h);
            XDeleteProperty(dp, c->win, wmState);
            c->isFullscreen = 0;
            break;
        case 1: {
            XWindowAttributes attrs;
            XGetWindowAttributes(dp, c->win, &attrs);
            
            int screenW = DisplayWidth(dp, DefaultScreen(dp));
            int screenH = DisplayHeight(dp, DefaultScreen(dp));

            c->x = attrs.x;
            c->y = attrs.y;
            c->w = attrs.width;
            c->h = attrs.height;

            XMoveResizeWindow(dp, c->win, 0, 0, screenW, screenH);
            XRaiseWindow(dp, c->win);
            
            c->isFullscreen = 1;

            XChangeProperty(dp, c->win, wmState, XA_ATOM, 32, PropModeAppend, 
                            (unsigned char *)&wmStateFullscreen, 1);
            XSetInputFocus(dp, c->win, RevertToParent, CurrentTime);

            break;
        }    
        default:
            break;
    }
}

// Handle client messages events
void handleClientMessages(Client *c, XClientMessageEvent evClient)
{
    if (evClient.message_type == wmState ) 
    {
        c->win = evClient.window;
        long action = evClient.data.l[0];
        Atom change = (Atom)evClient.data.l[1];
        Atom optChange = (Atom)evClient.data.l[2];
       
        if (change == wmStateFullscreen || optChange == wmStateFullscreen) 
        {
            int check = c->isFullscreen;

            if (action == 1) 
            {
                if (!check)
                    setFullscreen(c, 1);
            } else if (action == 0) 
            {
                if(check)
                    setFullscreen(c, 0);
            } else if (action == 2) 
            {
                if (check)
                {
                    setFullscreen(c, 0);
                } else 
                {
                    setFullscreen(c, 1);
                }
            }

        }
    }
}

// Init atoms
void initAtoms(void)
{
    supported = XInternAtom(dp, "_NET_SUPPORTED", False);
    wmState = XInternAtom(dp, "_NET_WM_STATE", False);
    wmStateFullscreen = XInternAtom(dp, "_NET_WM_STATE_FULLSCREEN", False);
}

// Check and balances
void setup(void)
{   
    // Start threads
    XInitThreads();
   
    // Call error handler
    XSetErrorHandler(xerrorHandler);
    
    // Init variables
    sc = XScreenOfDisplay(dp, DefaultScreen(dp));
    rt = DefaultRootWindow(dp);

    // Init atoms
    initAtoms();
    Atom supportedAtoms[] = {
      wmState, 
      wmStateFullscreen,
    };

    XChangeProperty(dp, rt, supported, XA_ATOM, 32, PropModeReplace, 
                    (unsigned char *)supportedAtoms, 
                    sizeof(supportedAtoms) / sizeof(Atom));

    // Remove any keybinds
    XUngrabKey(dp, AnyKey, AnyModifier, rt);
    
    // Select the input of the display
    XSelectInput(dp, rt, SubstructureRedirectMask | SubstructureNotifyMask);
  
    // Put a mask to the keys
    keysToModMask();
    
    // Start deamons
    autostart();

    // Check for zombies
    signal(SIGCHLD, chldHandle);
    
    // Pass all the events
    XFlush(dp);
}

// Main loop
void run(void)
{
    // Sync calls
    XSync(dp, False);
    
    while (running)
    {
        XNextEvent(dp, &ev);
        // Check for events
        switch (ev.type) 
        {   
            // Open the windows in the center of the screen
            case MapRequest: {
                XWindowAttributes wa;
                XGetWindowAttributes(dp, ev.xmaprequest.window, &wa);
                
                Client *c = malloc(sizeof(Client));
                
                if (c == NULL) 
                {
                    perror("Failed to allocate memory for a new client.");
                    return;
                }

                c->win = ev.xmaprequest.window;
                c->isFullscreen = 0;

                c->next = clients;
                clients = c;

                int x = (XWidthOfScreen(sc) - wa.width) / 2;
                int y = (XHeightOfScreen(sc) - wa.height) / 2;

                XMoveResizeWindow(dp, ev.xmaprequest.window, x, y, 
                                  wa.width, wa.height);
                
                XMapWindow(dp, ev.xmaprequest.window);
                XSetInputFocus(dp, ev.xmaprequest.window, RevertToNone, 
                               CurrentTime);
                break;
            }
            // Handle client messages
            case ClientMessage: {
                Client *c = getClient(ev.xclient.window);
                if (c != NULL) {
                    handleClientMessages(c, ev.xclient);
                } else 
                {
                    printf("No window.\n");
                }
                break;
            }
            // Check for key press
            case KeyPress:
                // Quit the WM
                if (ev.xkey.keycode == stringToKeycode("q")) 
                {
                    running = 0;
                }
                if (ev.xkey.keycode == stringToKeycode("g")) 
                {
                    Window focused;
                    XGetInputFocus(dp, &focused, RevertToNone);
                    
                    if (focused != None) 
                    {
                        Client *c = getClient(focused);
                        if (c != NULL) {
                          int state = c->isFullscreen ? 0 : 1;
                          setFullscreen(c, state);
                        }
                    } else 
                    {
                        printf("No window focused.\n");
                    }
                }
                // Quit focused window
                if (ev.xkey.keycode == stringToKeycode("x"))
                {
                    Client *c = getClient(ev.xclient.window);
                    if (c != NULL) {
                      XKillClient(dp, ev.xkey.subwindow);
                    } else 
                    {
                        printf("No window in kill function.\n");
                    }
                    free(c);
                }
                // Open a terminal
                if (ev.xkey.keycode == stringToKeycode("k")) 
                {
                    spawn(termcmd);
                }
                // Open a browser
                if (ev.xkey.keycode == stringToKeycode("v")) 
                {
                    spawn(browsercmd);
                }
                break;
            default:
                break;
        }
    }
}

int main(void)
{
    // Check if theres a display
    if (!(dp = XOpenDisplay(NULL))) 
    {
        fprintf(stderr, "Error: cannot open display \n");
        exit(1);
    }
   
    // Check and load stuff to memory
    setup();
    
    // Main loop
    run();
    
    // Clean up allocated memory

    // Close the display
    XCloseDisplay(dp);
}
