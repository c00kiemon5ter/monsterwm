/* see LICENSE for copyright and license */

#ifndef CONFIG_H
#define CONFIG_H

/* "name" desktops */
enum { CURRENT=-1, WEB, DEV, FOO, NIL };
/* "name" monitors */
enum { LARGE, SMALL };

/** modifiers **/
#define MOD1            Mod1Mask    /* ALT key */
#define MOD4            Mod4Mask    /* Super/Windows key */
#define CONTROL         ControlMask /* Control key */
#define SHIFT           ShiftMask   /* Shift key */

/** generic settings **/
#define MASTER_SIZE     0.52
#define SHOW_PANEL      True      /* show panel by default on exec */
#define TOP_PANEL       True      /* False means panel is on bottom */
#define PANEL_HEIGHT    18        /* 0 for no space for panel, thus no panel */
#define DEFAULT_MODE    TILE      /* initial layout/mode: TILE MONOCLE BSTACK GRID FLOAT */
#define ATTACH_ASIDE    True      /* False means new window is master */
#define FOLLOW_WINDOW   False     /* follow the window when moved to a different desktop */
#define FOLLOW_MONITOR  True      /* follow the window when moved to a different monitor */
#define FOLLOW_MOUSE    False     /* focus the window the mouse just entered */
#define CLICK_TO_FOCUS  True      /* focus an unfocused window when clicked  */
#define FOCUS_BUTTON    Button3   /* mouse button to be used along with CLICK_TO_FOCUS */
#define BORDER_WIDTH    2         /* window border width */
#define FOCUS           "#ff950e" /* focused window border color   */
#define UNFOCUS         "#666666" /* unfocused window border color */
#define INFOCUS         "#1177AA" /* focused window border color on unfocused monitor */
#define MINWSZ          50        /* minimum window size in pixels */
#define DESKTOPS        4         /* number of desktops - edit DESKTOPCHANGE keys to suit */
#define DEFAULT_DESKTOP WEB       /* the desktop to focus initially */
#define DEFAULT_MONITOR LARGE     /* the monitor to focus initially */

struct ml {
    int m; /* monitor that the desktop in on  */
    int d; /* desktop which properties follow */
    struct {
        int mode;  /* layout mode for desktop d of monitor m    */
        int masz;  /* incread or decrease master area in px     */
        Bool sbar; /* whether or not to show panel on desktop d */
    } dl;
};

/**
 * define initial values for each monitor and dekstop properties
 *
 * in the example below:
 * - the first desktop (0) on the first monitor (0) will have
 *   tile layout, with its master area increased by 50px and
 *   the panel will be visible.
 * - the third desktop (2) on the second monitor (1) will have
 *   grid layout, with no changes to its master area and
 *   the panel will be hidden.
 */
static const struct ml init[] = { \
    /* monitor  desktop   mode    masz  sbar   */
    {   SMALL,    WEB,  { BSTACK, 400,  False } },
    {   SMALL,    DEV,  { GRID,    0,   False } },
    {   SMALL,    FOO,  { TILE,    0,   False } },
    {   SMALL,    NIL,  { FLOAT,   0,   False } },
    {   LARGE,    WEB,  { TILE,    0,   True  } },
    {   LARGE,    DEV,  { GRID,    0,   False } },
};

/**
 * open applications to specified monitor and desktop
 * with the specified properties.
 * if monitor is negative, then current is assumed
 * if desktop is negative, then current is assumed
 */
static const AppRule rules[] = { \
    /*  class       monitor  desktop  follow  float */
    { "MPlayer",    SMALL,   CURRENT, True,   True  },
    { "Gimp",       LARGE,   FOO,     True,   True  },
    { "Deluge",     SMALL,   FOO,     False,  False },
    { "IRC-",       LARGE,   WEB,     False,  False },
    { "scratchpad", CURRENT, CURRENT, False,  True  },
};

/* helper for spawning shell commands */
#define SHCMD(cmd) {.com = (const char*[]){"/bin/sh", "-c", cmd, NULL}}

/**
 * custom commands
 * must always end with ', NULL };'
 */
static const char *dropterm[] = { "/bin/sh", "-c", "scratchpad.sh", NULL };
static const char *mainterm[] = { "urxvtdc",   NULL };
static const char *termcmd[]  = { "xterm",     NULL };
static const char *menucmd[]  = { "dmenu_run", NULL };
static const char *torrent[]  = { "deluge",    NULL };
static const char *surfcmd[]  = { "chromium", "--enable-seccomp-sandbox", "--memory-model=low",
                                  "--purge-memory-button", "--disk-cache-dir=/tmp/chromium", NULL };

/* audio volume */
static const char *volupcmd[]     = { "volctrl", "+2",     NULL };
static const char *voldncmd[]     = { "volctrl", "-2",     NULL };
static const char *voltogglecmd[] = { "volctrl", "toggle", NULL };
/* audio playback [mpd/mpc] */
static const char *mstopcmd[]   = { "mpc", "stop",   NULL };
static const char *mnextcmd[]   = { "mpc", "next",   NULL };
static const char *mprevcmd[]   = { "mpc", "prev",   NULL };
static const char *mtogglecmd[] = { "mpc", "toggle", NULL };

#define MONITORCHANGE(K,N) \
    {  MOD4,             K,              change_monitor, {.i = N}}, \
    {  MOD4|ShiftMask,   K,              client_to_monitor, {.i = N}},

#define DESKTOPCHANGE(K,N) \
    {  MOD1,             K,              change_desktop, {.i = N}}, \
    {  MOD1|ShiftMask,   K,              client_to_desktop, {.i = N}},

/**
 * keyboard shortcuts
 */
static Key keys[] = {
    /* modifier          key            function           argument */
    {  MOD1,             XK_b,          togglepanel,       {NULL}},
    {  MOD1,             XK_BackSpace,  focusurgent,       {NULL}},
    {  MOD1|SHIFT,       XK_c,          killclient,        {NULL}},
    {  MOD1,             XK_j,          next_win,          {NULL}},
    {  MOD1,             XK_k,          prev_win,          {NULL}},
    {  MOD1,             XK_h,          resize_master,     {.i = -10}}, /* decrease size in px */
    {  MOD1,             XK_l,          resize_master,     {.i = +10}}, /* increase size in px */
    {  MOD1,             XK_o,          resize_stack,      {.i = -10}}, /* shrink   size in px */
    {  MOD1,             XK_p,          resize_stack,      {.i = +10}}, /* grow     size in px */
    {  MOD1|CONTROL,     XK_h,          rotate,            {.i = -1}},
    {  MOD1|CONTROL,     XK_l,          rotate,            {.i = +1}},
    {  MOD1|SHIFT,       XK_h,          rotate_filled,     {.i = -1}},
    {  MOD1|SHIFT,       XK_l,          rotate_filled,     {.i = +1}},
    {  MOD1,             XK_Tab,        last_desktop,      {NULL}},
    {  MOD1,             XK_Return,     swap_master,       {NULL}},
    {  MOD1|SHIFT,       XK_j,          move_down,         {NULL}},
    {  MOD1|SHIFT,       XK_k,          move_up,           {NULL}},
    {  MOD1|SHIFT,       XK_t,          switch_mode,       {.i = TILE}},
    {  MOD1|SHIFT,       XK_m,          switch_mode,       {.i = MONOCLE}},
    {  MOD1|SHIFT,       XK_b,          switch_mode,       {.i = BSTACK}},
    {  MOD1|SHIFT,       XK_g,          switch_mode,       {.i = GRID}},
    {  MOD1|SHIFT,       XK_f,          switch_mode,       {.i = FLOAT}},
    {  MOD1|CONTROL,     XK_r,          quit,              {.i = 0}}, /* quit with exit value 0 */
    {  MOD1|CONTROL,     XK_q,          quit,              {.i = 1}}, /* quit with exit value 1 */
    {  MOD1|SHIFT,       XK_Return,     spawn,             {.com = termcmd}},
    {  MOD4,             XK_v,          spawn,             {.com = menucmd}},
    {  MOD4,             XK_t,          spawn,             {.com = torrent}},
    {  MOD4,             XK_w,          spawn,             {.com = surfcmd}},
    {  MOD4,             XK_grave,      spawn,             {.com = mainterm}},
    {  MOD1,             XK_grave,      spawn,             {.com = dropterm}},
    {  MOD4,             XK_equal,      spawn,             {.com = volupcmd}},
    {  MOD4,             XK_KP_Add,     spawn,             {.com = volupcmd}},
    {  MOD4,             XK_minus,      spawn,             {.com = voldncmd}},
    {  MOD4,             XK_KP_Subtract,spawn,             {.com = voldncmd}},
    {  MOD4,             XK_m,          spawn,             {.com = voltogglecmd}},
    {  MOD4,             XK_s,          spawn,             {.com = mstopcmd}},
    {  MOD4,             XK_period,     spawn,             {.com = mnextcmd}},
    {  MOD4,             XK_comma,      spawn,             {.com = mprevcmd}},
    {  MOD4,             XK_p,          spawn,             {.com = mtogglecmd}},
    {  MOD4,             XK_j,          moveresize,        {.v = (int []){   0,  25,   0,   0 }}}, /* move down  */
    {  MOD4,             XK_k,          moveresize,        {.v = (int []){   0, -25,   0,   0 }}}, /* move up    */
    {  MOD4,             XK_l,          moveresize,        {.v = (int []){  25,   0,   0,   0 }}}, /* move right */
    {  MOD4,             XK_h,          moveresize,        {.v = (int []){ -25,   0,   0,   0 }}}, /* move left  */
    {  MOD4|SHIFT,       XK_j,          moveresize,        {.v = (int []){   0,   0,   0,  25 }}}, /* height grow   */
    {  MOD4|SHIFT,       XK_k,          moveresize,        {.v = (int []){   0,   0,   0, -25 }}}, /* height shrink */
    {  MOD4|SHIFT,       XK_l,          moveresize,        {.v = (int []){   0,   0,  25,   0 }}}, /* width grow    */
    {  MOD4|SHIFT,       XK_h,          moveresize,        {.v = (int []){   0,   0, -25,   0 }}}, /* width shrink  */
       DESKTOPCHANGE(    XK_F1,                             WEB)
       DESKTOPCHANGE(    XK_F2,                             DEV)
       DESKTOPCHANGE(    XK_F3,                             FOO)
       DESKTOPCHANGE(    XK_F4,                             NIL)
       MONITORCHANGE(    XK_F1,                             SMALL)
       MONITORCHANGE(    XK_F2,                             LARGE)
};

/**
 * mouse shortcuts
 */
static Button buttons[] = {
    {  MOD1,    Button1,     mousemotion,   {.i = MOVE}},
    {  MOD1,    Button3,     mousemotion,   {.i = RESIZE}},
    {  MOD4,    Button3,     spawn,         {.com = menucmd}},
};
#endif

/* vim: set expandtab ts=4 sts=4 sw=4 : */
