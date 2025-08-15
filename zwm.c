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
// 1. Add some tilling windows

// Globals //

// Structures
typedef struct Client Client; 
struct Client {
  int x, y, h, w;
  int oldX, oldY, oldH, oldW;
  int focused, unfocused, isFullscreen;
  struct Client *next;
  Window win;
};

// Atoms
Atom netSupported, netWMState, netWMStateFullscreen;

// Variables
static Display *dp;
static Window rt;
static Screen *sc;
static XEvent ev;
static Client *clientList = NULL;
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

// Autostart programs and deamons
void autostart(void)
{
    int cmdArraySize = sizeof(autostartcmd) / sizeof(autostartcmd[0]);

    for (int i = 0;i < cmdArraySize; i++) 
    {
        spawn(autostartcmd[i]);
    };
}

// Handle zombies childs
void chldHandle(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Find a client by his window id
Client *srchClient(Window w)
{
    // Create a pointer to the current client
    Client *curr = clientList;
    
    // Search while current is null
    while (curr != NULL) 
    {
        // If current is equal to w then return current;
        if (curr->win == w) {
            return curr;         
        }
        // Else check next node
        curr = curr->next;
    }
    
    return NULL;
}

// Add a client to the list
void addClient(Window w)
{
    // Search if the client is been already created
    if (srchClient(w) != NULL) 
    {
        return;
    }

    // Allocate memory for the new client
    Client *new = malloc(sizeof(Client));

    // Handle memory allocation error
    if (new == NULL) 
    {
        return;
    }

    // Write the variables of the client
    XWindowAttributes wa;
    XGetWindowAttributes(dp, w, &wa);

    new->win = w;
    new->isFullscreen = 0;
    new->x = (XWidthOfScreen(sc) - wa.width) / 2;
    new->y = (XHeightOfScreen(sc) - wa.height) / 2;
    new->w = wa.width;
    new->h = wa.height;

    // Add it to the front of the list
    new->next = clientList;
    clientList = new;
}

void rmClient(Window w)
{
    // Previous client need to re link the list
    Client *curr = clientList;
    Client *prev = NULL;

    while (curr != NULL) 
    {
        if (curr->win == w) 
        {
            // If its the first on the list
            if (prev == NULL) 
            {
                clientList = curr->next;
            }
            else  // If its in the middle or end
            {
                prev->next = curr->next;
            }
            // Free the memory
            free(curr);
            return;
        }
        // Change
        prev = curr;
        curr = curr->next;
    }


}

// Handle events
void evHandler(void)
{
    switch (ev.type) 
    {   
        // Open the windows in the center of the screen
        case MapRequest: 
        {
            Window win = ev.xmaprequest.window;
            
            addClient(win);

            XWindowAttributes wa;
            XGetWindowAttributes(dp, win, &wa);
            
            int x = (XWidthOfScreen(sc) - wa.width) / 2;
            int y = (XHeightOfScreen(sc) - wa.height) / 2;

            XMoveResizeWindow(dp, win, x, y, wa.width, wa.height);
            XMapWindow(dp, win);
            XSetInputFocus(dp, win, RevertToParent, CurrentTime);
            break;
        }
        case DestroyNotify: 
        {
            Window win = ev.xdestroywindow.window;
            rmClient(win);
            break;
        }
        // Check for key press
        case KeyPress:
            // Quit the WM
            if (ev.xkey.keycode == stringToKeycode("q")) 
            {
                running = 0;
            }
            // Quit focused window
            if (ev.xkey.keycode == stringToKeycode("x"))
            {
                XKillClient(dp, ev.xkey.subwindow);
            }
            // Toggle fullscreen for a client
            if (ev.xkey.keycode == stringToKeycode("g"))
            {   
                Window win = ev.xkey.subwindow;
                
                // Ignore root window
                if (win == 0) 
                {
                    break;
                }

                Client *c = srchClient(win);
                
                // Do nothing if the client window do not match
                if (c == NULL) 
                {
                    break;
                }

                // Toggle fullscreen
                if (c->isFullscreen) 
                {
                    c->isFullscreen = 0;
                    XMoveResizeWindow(dp, c->win, c->oldX, c->oldY, 
                                      c->oldW, c->oldH);
                    XDeleteProperty(dp, c->win, netWMState);
                } 
                else 
                {
                    c->oldX = c->x;
                    c->oldY = c->y;
                    c->oldW = c->w;
                    c->oldH = c->h;

                    c->isFullscreen = 1;
                    XMoveResizeWindow(dp, c->win, 0, 0, 
                                      XWidthOfScreen(sc), XHeightOfScreen(sc));
                    XChangeProperty(dp, c->win, netWMState, XA_ATOM, 32, 
                                    PropModeReplace, 
                                    (unsigned char *)&netWMStateFullscreen, 1);
                }
                XFlush(dp);
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

// Handle atoms
void handleAtoms(void)
{
    // Init atoms
    netSupported = XInternAtom(dp, "_NET_SUPPORTED", False);
    netWMState = XInternAtom(dp, "_NET_WM_STATE", False);
    netWMStateFullscreen = XInternAtom(dp, "_NET_WM_STATE_FULLSCREEN", False);
    Atom netSupportedAtoms[] = {
        netWMState, 
        netWMStateFullscreen,
    };

    XChangeProperty(dp, rt, netSupported, XA_ATOM, 32, PropModeReplace, 
                    (unsigned char *)netSupportedAtoms, 
                    sizeof(netSupportedAtoms) / sizeof(Atom));
}

// Handle X11 errors
int xerrorHandler(Display *dp, XErrorEvent *ee) 
{
    fprintf(stderr,"X11 error:type %d,serial %ld,code %d,request %d,minor %d\n",
            ee->type, ee->serial, ee->error_code, ee->request_code, 
            ee->minor_code);
    return 0;
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

    // Call handler for atoms
    handleAtoms();

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
    
    // Sync calls
    XSync(dp, False);
    
    while (running)
    {
        XNextEvent(dp, &ev);
        // Check for events
        evHandler();
    }

    // Close the display
    XCloseDisplay(dp);
}
