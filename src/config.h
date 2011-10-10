 /* config.h for dminiwm.c [ 0.1.0 ]
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

/* Mod (Mod1 == alt) (Mod4 == Super/windows) */
#define MOD1            Mod1Mask
#define MOD4		Mod4Mask
#define MASTER_SIZE     0.6
#define PANEL_HEIGHT	18 /* 0 for no space for a panel */
#define BORDER_WIDTH    2
#define ATTACH_ASIDE    1  /* 0=TRUE, 1=New window is master */
#define DEFAULT_MODE    0  /* 0=Vertical, 1=Fullscreen 2=Horizontal 3=grid*/
#define FOLLOW_MOUSE    0  /* 1=Don't 0=Focus the window the mouse just entered */
#define FOLLOW_WINDOW   0  /* 1=Don't 0=Follow the window when moved to a different desktop */
#define TOP_PANEL       1  /* 1=Don't 0=Have the panel at the top instead of the bottom */

// Colors
#define FOCUS           "#664422" // dkorange
#define UNFOCUS         "#004050" // blueish

const char* dmenucmd[]      = {"dmenu_run","-i","-nb","#666622","-nf","white",NULL};
const char* urxvtcmd[]      = {"urxvtc",NULL};
const char* terminalcmd[]   = {"Terminal",NULL};
const char* thunarcmd[]     = {"thunar",NULL};
const char* firefoxcmd[]    = {"firefox",NULL};
const char* conkerorcmd[]   = {"conkeror",NULL};
const char* mailcmd[]       = {"thunderbird",NULL };
const char* voldowncmd[]    = {"/home/pnewm/.bin/voldown",NULL};
const char* volupcmd[]      = {"/home/pnewm/.bin/volup",NULL};
const char* vols_what[]     = {"/home/pnewm/.bin/volumes_what",NULL};
// for reboot and shutdown
const char* rebootcmd[]     = {"sudo","reboot",NULL};
const char* shutdowncmd[]   = {"sudo","shutdown","-h","now",NULL};

// Avoid multiple paste
#define DESKTOPCHANGE(K,N) \
    {  MOD1,             K,              change_desktop, {.i = N}}, \
    {  MOD1|ShiftMask,   K,              client_to_desktop, {.i = N}},

// Shortcuts
static key keys[] = {
    // MOD               KEY             FUNCTION            ARGS
    {  MOD1,             XK_h,          increase,          {NULL}},
    {  MOD1,             XK_l,          decrease,          {NULL}},
    {  MOD1,             XK_c,          kill_client,       {NULL}},
    {  MOD1,             XK_j,          next_win,          {NULL}},
    {  MOD1,             XK_k,          prev_win,          {NULL}},
    {  MOD1,             XK_v,          spawn,             {.com = dmenucmd}},
    {  MOD1,             XK_p,          grow_window,       {NULL}},
    {  MOD1,             XK_o,          shrink_window,     {NULL}},
    {  MOD1,             XK_Return,     spawn,             {.com = urxvtcmd}},
    {  MOD1,             XK_Up,	        spawn,             {.com = volupcmd}},
    {  MOD1,             XK_Down,       spawn,             {.com = voldowncmd}},
// alt + shift + shortcut
    {  MOD1|ShiftMask,   XK_j,          move_up,           {NULL}},
    {  MOD1|ShiftMask,   XK_k,          move_down,         {NULL}},
    {  MOD1|ShiftMask,   XK_Return,     swap_master,       {NULL}},
    {  MOD1|ShiftMask,   XK_g,          switch_grid,       {NULL}},
    {  MOD1|ShiftMask,   XK_h,          switch_horizontal, {NULL}},
    {  MOD1|ShiftMask,   XK_m,          switch_fullscreen, {NULL}},
    {  MOD1|ShiftMask,   XK_v,          switch_vertical,   {NULL}},
// Control + alt + shortcut
    {  MOD1|ControlMask, XK_q,          quit,              {NULL}},
    {  MOD1|ControlMask, XK_r,          spawn,             {.com = rebootcmd}},
    {  MOD1|ControlMask, XK_s,          spawn,             {.com = shutdowncmd}},
// Window key + shortcut
    {  MOD4,             XK_Right,      next_desktop,      {NULL}},
    {  MOD4,             XK_Left,       prev_desktop,      {NULL}},
    {  MOD4,             XK_e,		spawn,             {.com = mailcmd}},
    {  MOD4,             XK_f,		spawn,             {.com = firefoxcmd}},
    {  MOD4,             XK_w,		spawn,             {.com = conkerorcmd}},
    {  MOD4,             XK_h,		spawn,             {.com = thunarcmd}},
    {  MOD4,             XK_t,          spawn,             {.com = terminalcmd}},
    {  MOD4,             XK_v,          spawn,             {.com = vols_what}},
       DESKTOPCHANGE(   XK_1,                              0)
       DESKTOPCHANGE(   XK_2,                              1)
       DESKTOPCHANGE(   XK_3,                              2)
       DESKTOPCHANGE(   XK_4,                              3)
       DESKTOPCHANGE(   XK_5,                              4)
       DESKTOPCHANGE(   XK_6,                              5)
};

#endif

