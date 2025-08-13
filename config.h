#include <stddef.h>

// Mod key Super
#define MODKEY Mod4Mask

// Keybinds
static const char *modKeys[] = { "q", "x", "k", "v", "g" };

// Programs
static const char *termcmd[] = {"ns", NULL};
static const char *browsercmd[] = {"librewolf", NULL};

// Autostart
static char const *autostartcmd[][4] = {
    {"picom", "--config", "/home/zrchx/Dev/Dots/picom/picom.conf", NULL},
    {"/home/zrchx/Dev/Dots/BIN/simplegb", "000000", NULL},
    {"xrdb", "merge", "/home/zrchx/Dev/Dots/.Xresources", NULL}
    //"[ ! -s /home/zrchx/Dev/Dots/mpd/pid ] && mpd &"
};
