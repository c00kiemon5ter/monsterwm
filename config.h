/* Based on:
 * - catwm at https://github.com/pyknite/catwm
 * - dminiwm at https://github.com/moetunes/dminiwm
 * - dwm at http://dwm.suckless.org/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONFIG_H
#define CONFIG_H

#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))

/** buttons **/
#define MOD1            Mod1Mask    /* ALT key */
#define MOD4            Mod4Mask    /* Super/Windows key */
#define CONTROL         ControlMask /* Cotrol key */
#define SHIFT           ShiftMask   /* Shift key */

/** generic settings **/
#define MASTER_SIZE     0.52
#define SHOW_PANEL      True      /* show panel by default on exec */
#define TOP_PANEL       True      /* False mean panel is on bottom */
#define PANEL_HEIGHT    18        /* 0 for no space for panel, thus no panel */
#define DEFAULT_MODE    TILE      /* TILE MONOCLE BSTACK GRID */
#define ATTACH_ASIDE    True      /* False means new window is master */
#define FOLLOW_MOUSE    False     /* Focus the window the mouse just entered */
#define FOLLOW_WINDOW   True      /* Follow the window when moved to a different desktop */
#define CLICK_TO_FOCUS  False     /* Focus an unfocused window when clicked */
#define BORDER_WIDTH    2         /* window border width */
#define FOCUS           "#ff950e" /* focused window border color   */
#define UNFOCUS         "#444444" /* unfocused window border color */
#define DESKTOPS        4         /* number of desktops - edit DESKTOPCHANGE keys to suit */
#define DEFAULT_DESKTOP 0         /* the desktop to focus on exec */

/** open applications to specified desktop **/
static const AppRule rules[] = { \
    /*  class      desktop    follow */
    { "MPlayer",      3,       True  },
};

/* helper for spawning shell commands */
#define SHCMD(cmd) {.com = (const char*[]){"/bin/bash", "-c", cmd, NULL}}

/** commands **/
static const char *termcmd[]     = { "urxvt",    NULL };
static const char *dmenucmd[]    = { "dmn",      NULL };
static const char *urxvtcmd[]    = { "urxvtdc",  NULL };
static const char *chromiumcmd[] = { "chromium", NULL };
/* audio volume */
static const char *volupcmd[]     = { "volctrl", "+2",     NULL };
static const char *voldowncmd[]   = { "volctrl", "-2",     NULL };
static const char *voltogglecmd[] = { "volctrl", "toggle", NULL };
/* audio playback [mpd/mpc] */
static const char *mplaycmd[]   = { "mpc", "play",   NULL };
static const char *mstopcmd[]   = { "mpc", "stop",   NULL };
static const char *mnextcmd[]   = { "mpc", "next",   NULL };
static const char *mprevcmd[]   = { "mpc", "prev",   NULL };
static const char *mtogglecmd[] = { "mpc", "toggle", NULL };

#define DESKTOPCHANGE(K,N) \
    {  MOD1,             K,              change_desktop, {.i = N}}, \
    {  MOD1|ShiftMask,   K,              client_to_desktop, {.i = N}},

/** Shortcuts **/
static key keys[] = {
    /* modifier          key            function           argument */
    {  MOD1,             XK_b,          togglepanel,       {NULL}},
    {  MOD1|SHIFT,       XK_c,          killclient,        {NULL}},
    {  MOD1,             XK_j,          next_win,          {NULL}},
    {  MOD1,             XK_k,          prev_win,          {NULL}},
    {  MOD1,             XK_h,          resize_master,     {.i = -10}}, /* decrease */
    {  MOD1,             XK_l,          resize_master,     {.i = +10}}, /* increase */
    {  MOD1,             XK_o,          resize_stack,      {.i = -10}}, /* shrink */
    {  MOD1,             XK_p,          resize_stack,      {.i = +10}}, /* grow   */
    {  MOD1|SHIFT,       XK_Left,       rotate_desktop,    {.i = -1}},  /* prev */
    {  MOD1|SHIFT,       XK_Right,      rotate_desktop,    {.i = +1}},  /* next */
    {  MOD1,             XK_Tab,        last_desktop,      {NULL}},
    {  MOD1,             XK_Return,     swap_master,       {NULL}},
    {  MOD1|SHIFT,       XK_j,          move_down,         {NULL}},
    {  MOD1|SHIFT,       XK_k,          move_up,           {NULL}},
    {  MOD1|SHIFT,       XK_t,          switch_mode,       {.i = TILE}},
    {  MOD1|SHIFT,       XK_m,          switch_mode,       {.i = MONOCLE}},
    {  MOD1|SHIFT,       XK_b,          switch_mode,       {.i = BSTACK}},
    {  MOD1|SHIFT,       XK_g,          switch_mode,       {.i = GRID}},
    {  MOD1|CONTROL,     XK_r,          quit,              {.i = 0}}, /* restart */
    {  MOD1|CONTROL,     XK_q,          quit,              {.i = 1}}, /* quit    */
    {  MOD1|SHIFT,       XK_Return,     spawn,             {.com = termcmd}},
    {  MOD4,             XK_v,          spawn,             {.com = dmenucmd}},
    {  MOD4,             XK_grave,      spawn,             {.com = urxvtcmd}},
    {  MOD4,             XK_equal,	    spawn,             {.com = volupcmd}},
    {  MOD4,             XK_KP_Add,	    spawn,             {.com = volupcmd}},
    {  MOD4,             XK_minus,      spawn,             {.com = voldowncmd}},
    {  MOD4,             XK_KP_Subtract,spawn,             {.com = voldowncmd}},
    {  MOD4,             XK_m,	        spawn,             {.com = voltogglecmd}},
    {  MOD4,             XK_w,          spawn,             {.com = chromiumcmd}},
    {  MOD4,             XK_c,	        spawn,             {.com = mplaycmd}},
    {  MOD4,             XK_s,	        spawn,             {.com = mstopcmd}},
    {  MOD4,             XK_j,	        spawn,             {.com = mnextcmd}},
    {  MOD4,             XK_k,	        spawn,             {.com = mprevcmd}},
    {  MOD4,             XK_p,	        spawn,             {.com = mtogglecmd}},
       DESKTOPCHANGE(    XK_F1,                             0)
       DESKTOPCHANGE(    XK_F2,                             1)
       DESKTOPCHANGE(    XK_F3,                             2)
       DESKTOPCHANGE(    XK_F4,                             3)
};

#endif

