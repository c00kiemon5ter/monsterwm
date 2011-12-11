/* Based on:
 * - catwm at https://github.com/pyknite/catwm
 * - 2wm at http://hg.suckless.org/2wm/
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

/** generic settings **/
#define MOD1            Mod1Mask  /* ALT key */
#define MOD4            Mod4Mask  /* Super/Windows key */
#define MASTER_SIZE     0.52
#define TOP_PANEL       True      /* False mean panel is on bottom */
#define PANEL_HEIGHT    18        /* 0 for no space for panel, thus no panel */
#define DEFAULT_MODE    TILE      /* TILE MONOCYCLE BSTACK GRID */
#define ATTACH_ASIDE    True      /* False means new window is master */
#define FOLLOW_MOUSE    False     /* Focus the window the mouse just entered */
#define FOLLOW_WINDOW   False     /* Follow the window when moved to a different desktop */
#define CLICK_TO_FOCUS  False     /* Focus an unfocused window when clicked */
#define BORDER_WIDTH    2         /* window border width */
#define FOCUS           "#ff950e" /* focused window border color   */
#define UNFOCUS         "#444444" /* unfocused window border color */
#define DESKTOPS        4         /* number of desktops - edit DESKTOPCHANGE keys to suit */

/** open applications to specified desktop **/
static const AppRule rules[] = { \
    /*  class      desktop    follow */  /* desktop index starts from 0 */
    { "MPlayer",      3,       True  },  /* if there are 4 desktops, 3 is the  */
    { "Chromium",     0,       False },  /* last desktop, 0 is always the fist */
};

/** commands **/
const char *termcmd[]  = { "xterm", NULL };
const char *dmenucmd[] = { "dmenu", NULL };

#define DESKTOPCHANGE(K,N) \
    {  MOD1,             K,              change_desktop, {.i = N}}, \
    {  MOD1|ShiftMask,   K,              client_to_desktop, {.i = N}},

/** Shortcuts **/
static key keys[] = {
    /* modifier          key            function           argument */
    {  MOD1|ShiftMask,   XK_c,          killclient,        {NULL}},
    {  MOD1,             XK_j,          next_win,          {NULL}},
    {  MOD1,             XK_k,          prev_win,          {NULL}},
    {  MOD1,             XK_h,          resize_master,     {.i = -10}}, /* decrease */
    {  MOD1,             XK_l,          resize_master,     {.i = +10}}, /* increase */
    {  MOD1,             XK_o,          resize_stack,      {.i = -10}}, /* shrink */
    {  MOD1,             XK_p,          resize_stack,      {.i = +10}}, /* grow   */
    {  MOD1|ShiftMask,   XK_Left,       rotate_desktop,    {.i = -1}},  /* prev */
    {  MOD1|ShiftMask,   XK_Right,      rotate_desktop,    {.i = +1}},  /* next */
    {  MOD1,             XK_Tab,        last_desktop,      {NULL}},
    {  MOD1,             XK_Return,     swap_master,       {NULL}},
    {  MOD1|ShiftMask,   XK_j,          move_down,         {NULL}},
    {  MOD1|ShiftMask,   XK_k,          move_up,           {NULL}},
    {  MOD1|ShiftMask,   XK_t,          switch_mode,       {.i = TILE}},
    {  MOD1|ShiftMask,   XK_m,          switch_mode,       {.i = MONOCYCLE}},
    {  MOD1|ShiftMask,   XK_b,          switch_mode,       {.i = BSTACK}},
    {  MOD1|ShiftMask,   XK_g,          switch_mode,       {.i = GRID}},
    {  MOD1|ShiftMask,   XK_r,          quit,              {.i = 0}}, /* quit with exit value 0 */
    {  MOD1|ShiftMask,   XK_q,          quit,              {.i = 1}}, /* quit with exit value 1 */
    {  MOD1|ShiftMask,   XK_Return,     spawn,             {.com = termcmd}},
    {  MOD4,             XK_v,          spawn,             {.com = dmenucmd}},
       DESKTOPCHANGE(    XK_F1,                             0)
       DESKTOPCHANGE(    XK_F2,                             1)
       DESKTOPCHANGE(    XK_F3,                             2)
       DESKTOPCHANGE(    XK_F4,                             3)
};

#endif
