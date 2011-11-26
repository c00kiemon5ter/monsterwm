 /* config.h for dminiwm.c [ 0.1.7 ]
 *
 *  Started from catwm 31/12/10
 *  Bad window error checking and numlock checking used from
 *  2wm at http://hg.suckless.org/2wm/
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef CONFIG_H
#define CONFIG_H

#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))

/** generic settings **/
#define MOD1            Mod1Mask /* ALT key */
#define MOD4            Mod4Mask /* Super/Windows key */
#define MASTER_SIZE     0.52
#define TOP_PANEL       0  /* Place panel at the 1=bottom or 0=top */
#define PANEL_HEIGHT	18 /* 0 for no space for a panel */
#define BORDER_WIDTH    2
#define ATTACH_ASIDE    0  /* 0=TRUE, 1=New window is master */
#define DEFAULT_MODE    0  /* 0=Vertical, 1=Fullscreen 2=Horizontal 3=grid */
#define FOLLOW_MOUSE    1  /* 1=Don't 0=Focus the window the mouse just entered */
#define FOLLOW_WINDOW   1  /* 1=Don't 0=Follow the window when moved to a different desktop */
#define DESKTOPS        4  /* Must edit DESKTOPCHANGE keys to suit */

/** Colors **/
#define FOCUS           "#ff950e"
#define UNFOCUS         "#444444"

/** Applications to a specific desktop **/
static const Convenience convenience[] = { \
    /*  class      desktop follow */
    { "MPlayer",      3,      1 },
};

/** commands **/
const char *termcmd[]     = { "urxvt",    NULL };
const char *dmenucmd[]    = { "dmn",      NULL };
const char *urxvtcmd[]    = { "urxvtdc",  NULL };
const char *chromiumcmd[] = { "chromium", NULL };
/* audio volume */
static const char *volupcmd[]     = { "volctrl", "+2",     NULL };
static const char *voldowncmd[]   = { "volctrl", "-2",     NULL };
static const char *voltogglecmd[] = { "volctrl", "toggle", NULL };
/* audio playback [mpd/mpc] */
const char *mplaycmd[]   = { "mpc", "play",   NULL };
const char *mstopcmd[]   = { "mpc", "stop",   NULL };
const char *mnextcmd[]   = { "mpc", "next",   NULL };
const char *mprevcmd[]   = { "mpc", "prev",   NULL };
const char *mtogglecmd[] = { "mpc", "toggle", NULL };
/* operation */
/*
 * const char* rebootcmd[]   = { "sudo", "reboot", NULL};
 * const char* shutdowncmd[] = { "sudo", "shutdown", "-h", "now", NULL};
 */

/* Avoid multiple paste */
#define DESKTOPCHANGE(K,N) \
    {  MOD1,             K,              change_desktop, {.i = N}}, \
    {  MOD1|ShiftMask,   K,              client_to_desktop, {.i = N}},

/** Shortcuts **/
static key keys[] = {
/*     modifier          key            function           argument */
    {  MOD1,             XK_h,          resize_master,     {.i = -10}}, /* decrease */
    {  MOD1,             XK_l,          resize_master,     {.i = +10}}, /* increase */
    {  MOD1|ShiftMask,   XK_c,          kill_client,       {NULL} },
    {  MOD1,             XK_j,          next_win,          {NULL} },
    {  MOD1,             XK_k,          prev_win,          {NULL} },
    {  MOD1,             XK_o,          resize_stack,      {.i = -10}}, /* shrink */
    {  MOD1,             XK_p,          resize_stack,      {.i = +10}}, /* grow   */
    {  MOD1,             XK_Return,     swap_master,       {NULL} },
    {  MOD1|ShiftMask,   XK_j,          move_down,         {NULL} },
    {  MOD1|ShiftMask,   XK_k,          move_up,           {NULL} },
    {  MOD1|ShiftMask,   XK_v,          switch_vertical,   {NULL} },
    {  MOD1|ShiftMask,   XK_h,          switch_horizontal, {NULL} },
    {  MOD1|ShiftMask,   XK_g,          switch_grid,       {NULL} },
    {  MOD1|ShiftMask,   XK_m,          switch_fullscreen, {NULL} },
    {  MOD1|ShiftMask,   XK_Right,      next_desktop,      {NULL} },
    {  MOD1|ShiftMask,   XK_Left,       prev_desktop,      {NULL} },
    /*
     * {  MOD1|ControlMask, XK_r,          spawn,             {.com = rebootcmd}   },
     * {  MOD1|ControlMask, XK_s,          spawn,             {.com = shutdowncmd} },
     */
    {  MOD1|ShiftMask,   XK_q,          quit,              {NULL} },
    {  MOD1|ShiftMask,   XK_Return,     spawn,             {.com = termcmd}  },
    {  MOD4,             XK_v,          spawn,             {.com = dmenucmd} },
    {  MOD4,             XK_grave,      spawn,             {.com = urxvtcmd} },
    {  MOD4,             XK_equal,	    spawn,             {.com = volupcmd}   },
    {  MOD4,             XK_KP_Add,	    spawn,             {.com = volupcmd}   },
    {  MOD4,             XK_minus,      spawn,             {.com = voldowncmd} },
    {  MOD4,             XK_KP_Subtract,spawn,             {.com = voldowncmd} },
    {  MOD4,             XK_m,	        spawn,             {.com = voltogglecmd} },
    {  MOD4,             XK_w,          spawn,             {.com = chromiumcmd} },
    {  MOD4,             XK_c,	        spawn,             {.com = mplaycmd}   },
    {  MOD4,             XK_s,	        spawn,             {.com = mstopcmd}   },
    {  MOD4,             XK_j,	        spawn,             {.com = mnextcmd}   },
    {  MOD4,             XK_k,	        spawn,             {.com = mprevcmd}   },
    {  MOD4,             XK_p,	        spawn,             {.com = mtogglecmd} },
       DESKTOPCHANGE(    XK_F1,                             0)
       DESKTOPCHANGE(    XK_F2,                             1)
       DESKTOPCHANGE(    XK_F3,                             2)
       DESKTOPCHANGE(    XK_F4,                             3)
};

#endif

