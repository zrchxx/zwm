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
// 1. Fullscreen work but dont restore the window
// 2. Add some tilling windows

// Globals //

// Atoms
Atom netSupported, netWMState, netWMStateFullscreen;

// Variables
static Display *dp;
static Window rt;
static Screen *sc;
static XEvent ev;
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

// Set the focused window to fullscreen
void setFullscreen(int x, int y, int w, int h, int fullscreen)
{
    if (fullscreen == 1) 
    {   
        XChangeProperty(dp, ev.xkey.subwindow, netWMState, XA_ATOM, 32, 
                        PropModeReplace, 
                        (unsigned char *)&netWMStateFullscreen, 1);
        XMoveResizeWindow(dp, ev.xkey.subwindow, 0, 0, 
                          XWidthOfScreen(sc), XHeightOfScreen(sc));
        XRaiseWindow(dp, ev.xkey.subwindow);
   
    } else if (fullscreen == 0)
    {
        XDeleteProperty(dp, ev.xkey.subwindow, netWMState);
        XMoveResizeWindow(dp, ev.xkey.subwindow, x, y, w, h);
    }
    XFlush(dp);
}

// Handle events
void evHandler(void)
{
    switch (ev.type) 
    {   
        // Open the windows in the center of the screen
        case MapRequest: 
        {
            XWindowAttributes wa;
            XGetWindowAttributes(dp, ev.xmaprequest.window, &wa);
            
            int x = (XWidthOfScreen(sc) - wa.width) / 2;
            int y = (XHeightOfScreen(sc) - wa.height) / 2;

            XMoveResizeWindow(dp, ev.xmaprequest.window, x, y, 
                              wa.width, wa.height);
            
            XMapWindow(dp, ev.xmaprequest.window);
            XSetInputFocus(dp, ev.xmaprequest.window, RevertToParent, 
                           CurrentTime);
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
                int isFullscreen = 0;
                XWindowAttributes wa;
                XGetWindowAttributes(dp, ev.xkey.subwindow, &wa);
                
                if (wa.width != XWidthOfScreen(sc) && 
                    wa.height != XHeightOfScreen(sc)) 
                {
                    isFullscreen = 1;
                } else if (wa.width == XWidthOfScreen(sc)&& 
                           wa.height == XHeightOfScreen(sc)) 
                {
                    isFullscreen = 0;
                }
                
                setFullscreen((XWidthOfScreen(sc) - wa.width) / 2, 
                              (XHeightOfScreen(sc) - wa.height) / 2, 
                              wa.width, wa.height, isFullscreen);
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

// Main loop
void run(void)
{
    // Sync calls
    XSync(dp, False);
    
    while (running)
    {
        XNextEvent(dp, &ev);
        // Check for events
        evHandler();
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

    // Close the display
    XCloseDisplay(dp);
}
