#include <stddef.h>

// Mod key Super
#define MODKEY Mod4Mask

// Keybinds
static const char *modKeys[] = { 
    // WM keys
    "q", "x", "g",
    // Programs keys
    "k", "v", 
};

// Programs
static const char *termcmd[] = {"ns", NULL};
static const char *browsercmd[] = {"librewolf", NULL};

// Autostart
static char const *autostartcmd[][4] = {
    {"picom", "--config", "/home/zrchx/Dev/Dots/picom/picom.conf", NULL},
    {"/home/zrchx/Dev/Dots/BIN/simplegb", "ebdbd2", NULL},
    {"xrdb", "merge", "/home/zrchx/Dev/Dots/.Xresources", NULL}
    //"[ ! -s /home/zrchx/Dev/Dots/mpd/pid ] && mpd &"
};
