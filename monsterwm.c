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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>

#define LENGTH(x) (sizeof(x)/sizeof(*x))

enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_COUNT };
enum { TILE, MONOCLE, BSTACK, GRID, };

/* structs */
typedef union {
    const char** com;
    const int i;
} Arg;

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg *);
    const Arg arg;
} key;

typedef struct client {
    struct client *next;
    struct client *prev;
    Window win;
} client;

typedef struct {
    int master_size;
    int mode;
    int growth;
    client *head;
    client *current;
    Bool showpanel;
} desktop;

typedef struct {
    const char *class;
    const int desktop;
    const Bool follow;
} AppRule;

/* Functions */
static void add_window(Window w);
static void buttonpressed(XEvent *e);
static void change_desktop(const Arg *arg);
static void cleanup(void);
static void client_to_desktop(const Arg *arg);
static void configurerequest(XEvent *e);
static void deletewindow(Window w);
static void desktopinfo(void);
static void destroynotify(XEvent *e);
static void die(const char* errstr, ...);
static void enternotify(XEvent *e);
static unsigned long getcolor(const char* color);
static void grabkeys(void);
static void keypress(XEvent *e);
static void killclient();
static void last_desktop();
static void maprequest(XEvent *e);
static void move_down();
static void move_up();
static void next_win();
static void prev_win();
static void quit(const Arg *arg);
static void removeclient(client *c);
static void resize_master(const Arg *arg);
static void resize_stack(const Arg *arg);
static void rotate_desktop(const Arg *arg);
static void run(void);
static void save_desktop(int i);
static void select_desktop(int i);
static void setup(void);
static void sigchld();
static void spawn(const Arg *arg);
static void swap_master();
static void switch_mode(const Arg *arg);
static void tile(void);
static void togglepanel();
static void update_current(void);
static client* wintoclient(Window w);
static int xerrorstart();

#include "config.h"

/* variables */
static Bool running = True;
static Bool showpanel = SHOW_PANEL;
static int retval = 0;
static int current_desktop = 0;
static int previous_desktop = 0;
static int growth = 0;
static int mode = DEFAULT_MODE;
static int master_size;
static int wh; /* window area heght - screen height minus the border size and panel height */
static int ww; /* window area width - screen width minus the border size */
static int screen;
static int xerror(Display *dis, XErrorEvent *ee);
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int win_focus;
static unsigned int win_unfocus;
static unsigned int numlockmask = 0; /* dynamic key lock mask */
static Display *dis;
static Window root;
static client *head = NULL;
static client *current = NULL;
static Atom atoms[WM_COUNT];
static desktop desktops[DESKTOPS];

/* events array */
static void (*events[LASTEvent])(XEvent *e) = {
    [ButtonPress] = buttonpressed,
    [ConfigureRequest] = configurerequest,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [KeyPress] = keypress,
    [MapRequest] = maprequest,
};

void add_window(Window w) {
    client *c;
    if (!(c = (client *)calloc(1, sizeof(client))))
        die("error: could not calloc() %u bytes\n", sizeof(client));
    if (!head) {
        head = c;
        head->prev = head;
    } else if (ATTACH_ASIDE) {
        head->prev->next = c;
        c->prev = head->prev;
        head->prev = c;
    } else {
        c->prev = head->prev;
        head->prev = c;
        c->next = head;
        head = c;
    }
    c->win = w;
    current = c;
    save_desktop(current_desktop);
    if (FOLLOW_MOUSE) XSelectInput(dis, c->win, EnterWindowMask);
}

void buttonpressed(XEvent *e) {
    client *c;
    XButtonPressedEvent *ev = &e->xbutton;
    if (CLICK_TO_FOCUS && ev->window != current->win && ev->button == Button1)
        for (c=head; c; c=c->next)
            if (ev->window == c->win) {
                current = c;
                update_current();
                return;
            }
}

void change_desktop(const Arg *arg) {
    if (arg->i == current_desktop) return;
    previous_desktop = current_desktop;
    /* save current desktop settings and unmap windows */
    save_desktop(current_desktop);
    for (client *c=head; c; c=c->next)
        XUnmapWindow(dis, c->win);
    /* read new desktop properties, tile and map new windows */
    select_desktop(arg->i);
    tile();
    if (mode == MONOCLE && current) XMapWindow(dis, current->win);
    else for (client *c=head; c; c=c->next) XMapWindow(dis, c->win);
    update_current();
    desktopinfo();
}

void cleanup(void) {
    Window root_return;
    Window parent_return;
    Window *children;
    unsigned int nchildren;

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    XQueryTree(dis, root, &root_return, &parent_return, &children, &nchildren);
    for (unsigned int i = 0; i<nchildren; i++) deletewindow(children[i]);
    if (children) XFree(children);
    XSync(dis, False);
    XSetInputFocus(dis, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void client_to_desktop(const Arg *arg) {
    if (arg->i == current_desktop || !current) return;
    client *c = current;
    int cd = current_desktop;

    select_desktop(arg->i);
    add_window(c->win);
    save_desktop(arg->i);

    select_desktop(cd);
    XUnmapWindow(dis, c->win);
    removeclient(c);
    save_desktop(cd);

    tile();
    update_current();
    if (FOLLOW_WINDOW) change_desktop(arg);
}

void configurerequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;
    wc.x = ev->x;
    wc.y = ev->y + (showpanel && TOP_PANEL) ? PANEL_HEIGHT : 0;
    wc.width  = (ev->width  < ww - BORDER_WIDTH) ? ev->width  : ww + BORDER_WIDTH;
    wc.height = (ev->height < wh - BORDER_WIDTH) ? ev->height : wh + BORDER_WIDTH;
    wc.border_width = ev->border_width;
    wc.sibling    = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dis, ev->window, ev->value_mask, &wc);
    XSync(dis, False);
    tile();
}

void deletewindow(Window w) {
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = atoms[WM_PROTOCOLS];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = atoms[WM_DELETE_WINDOW];
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dis, w, False, NoEventMask, &ev);
}

void desktopinfo(void) {
    int cd = current_desktop;
    save_desktop(cd);
    for (int n=0, d=0; d<DESKTOPS; d++, n=0) {
        select_desktop(d);
        for (client *c=head; c; c=c->next, ++n);
        printf("%d:%d:%d:%d%c", d, n, mode, current_desktop==cd?1:0, d+1==DESKTOPS?'\n':' ');
    }
    fflush(stdout);
    select_desktop(cd);
}

void destroynotify(XEvent *e) {
    XDestroyWindowEvent *ev = &e->xdestroywindow;
    client *c;
    if ((c = wintoclient(ev->window)))
        removeclient(c);
    desktopinfo();
}

void die(const char *errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

void enternotify(XEvent *e) {
    client *c;
    XCrossingEvent *ev = &e->xcrossing;

    if (FOLLOW_MOUSE) {
        if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root) return;
        for (c=head; c; c=c->next)
            if (ev->window == c->win) {
                current = c;
                update_current();
                return;
            }
    }
}

unsigned long getcolor(const char* color) {
    Colormap map = DefaultColormap(dis, screen);
    XColor c;

    if (!XAllocNamedColor(dis, map, color, &c, &c))
        die("error: cannot allocate color '%s'\n", c);
    return c.pixel;
}

void grabkeys(void) {
    KeyCode code;

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    for (unsigned int i=0; i<LENGTH(keys); i++) {
        code = XKeysymToKeycode(dis, keys[i].keysym);
        XGrabKey(dis, code, keys[i].mod, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | LockMask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | numlockmask, root, True, GrabModeAsync, GrabModeAsync);
        XGrabKey(dis, code, keys[i].mod | numlockmask | LockMask, root, True, GrabModeAsync, GrabModeAsync);
    }
}

void keypress(XEvent *e) {
    static unsigned int len = sizeof keys / sizeof keys[0];
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev = &e->xkey;

    keysym = XKeycodeToKeysym(dis, (KeyCode)ev->keycode, 0);
    for (i = 0; i < len; i++)
        if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state) && keys[i].function)
                keys[i].function(&keys[i].arg);
}

void killclient() {
    if (!current) return;
    deletewindow(current->win);
    removeclient(current);
}

void last_desktop() {
    change_desktop(&(Arg){.i = previous_desktop});
}

void maprequest(XEvent *e) {
    XMapRequestEvent *ev = &e->xmaprequest;
    static XWindowAttributes wa;
    if (XGetWindowAttributes(dis, ev->window, &wa) && wa.override_redirect) return;
    if (wintoclient(ev->window)) return;

    Window trans;
    Bool follow = False, winistrans = XGetTransientForHint(dis, ev->window, &trans) && trans;
    int cd = current_desktop, nd = current_desktop;

    XClassHint ch = {0, 0};
    if (!winistrans && XGetClassHint(dis, ev->window, &ch))
        for (unsigned int i=0; i<LENGTH(rules); i++) {
            if (strcmp(ch.res_class, rules[i].class)
            && strcmp(ch.res_name, rules[i].class)) continue;
            nd = rules[i].desktop;
            follow = rules[i].follow;
            if (ch.res_class) XFree(ch.res_class);
            if (ch.res_name)  XFree(ch.res_name);
            break;
        }

    save_desktop(cd);
    select_desktop(nd);
    add_window(ev->window);
    select_desktop(cd);
    if (cd == nd) {
        if (!winistrans) tile();
        XMapWindow(dis, ev->window);
        update_current();
    }
    if (follow) change_desktop(&(Arg){.i = nd});
    desktopinfo();
}

void move_down() {
    if (!current || head->prev == head) return;
    client *next = (current->next) ? current->next : head;
    Window tmpwin = current->win;
    current->win = next->win;
    next->win = tmpwin;
    current = next;
    save_desktop(current_desktop);
    tile();
    update_current();
}

void move_up() {
    if (!current || head->prev == head) return;
    Window tmpwin = current->win;
    current->win = current->prev->win;
    current->prev->win = tmpwin;
    current = current->prev;
    save_desktop(current_desktop);
    tile();
    update_current();
}

void next_win() {
    if (!current || head->prev == head) return;
    if (mode == MONOCLE) XUnmapWindow(dis, current->win);
    current = (current->next) ? current->next : head;
    if (mode == MONOCLE) XMapWindow(dis, current->win);
    update_current();
}

void prev_win() {
    if (!current || head->prev == head) return;
    if (mode == MONOCLE) XUnmapWindow(dis, current->win);
    current = current->prev;
    if (mode == MONOCLE) XMapWindow(dis, current->win);
    update_current();
}

void quit(const Arg *arg) {
    retval = arg->i;
    running = False;
}

void removeclient(client *c) {
    if (c == head) {
        if (head->next) {            /* more windows on screen */
            head->next->prev = head->prev;
            head = head->next;
        } else {                    /* head is only window on screen */
            free(head);
            head = head->prev = NULL;
        }
        current = head;
    } else {
        if (c->next) {               /* w is somewhere in the middle */
            c->next->prev = c->prev;
            c->prev->next = c->next;
        } else {                    /* w is last window on screen */
            head->prev = c->prev;
            c->prev->next = NULL;
        }
        current = c->prev;
    }
    save_desktop(current_desktop);
    tile();
    if (mode == MONOCLE && current) XMapWindow(dis, current->win);
    update_current();
}

void resize_master(const Arg *arg) {
    int msz = master_size + arg->i;
    if ((mode == BSTACK ? wh : ww) - msz <= MINWSZ || msz <= MINWSZ) return;
    master_size = msz;
    tile();
}

void resize_stack(const Arg *arg) {
    growth += arg->i;
    tile();
}

void rotate_desktop(const Arg *arg) {
    change_desktop(&(Arg){.i = (current_desktop + DESKTOPS + arg->i) % DESKTOPS});
}

void run(void) {
    XEvent ev;
    while(running && !XNextEvent(dis, &ev))
        if (events[ev.type])
            events[ev.type](&ev);
}

void save_desktop(int i) {
    desktops[i].master_size = master_size;
    desktops[i].mode = mode;
    desktops[i].growth = growth;
    desktops[i].head = head;
    desktops[i].current = current;
    desktops[i].showpanel = showpanel;
}

void select_desktop(int i) {
    if (current_desktop == i) return;
    master_size = desktops[i].master_size;
    mode = desktops[i].mode;
    growth = desktops[i].growth;
    head = desktops[i].head;
    current = desktops[i].current;
    showpanel = desktops[i].showpanel;
    current_desktop = i;
}

void setup(void) {
    sigchld();

    screen = DefaultScreen(dis);
    root = RootWindow(dis, screen);

    ww = XDisplayWidth(dis,  screen) - BORDER_WIDTH;
    wh = XDisplayHeight(dis, screen) - (SHOW_PANEL ? PANEL_HEIGHT : 0) - BORDER_WIDTH;
    master_size = ((mode == BSTACK) ? wh : ww) * MASTER_SIZE;
    for (int i=0; i<DESKTOPS; i++)
        save_desktop(i);
    change_desktop(&(Arg){.i = DEFAULT_DESKTOP});

    win_focus = getcolor(FOCUS);
    win_unfocus = getcolor(UNFOCUS);

    XModifierKeymap *modmap = XGetModifierMapping(dis);
    for (int k=0; k<8; k++)
        for (int j=0; j<modmap->max_keypermod; j++)
            if (modmap->modifiermap[modmap->max_keypermod*k + j] == XKeysymToKeycode(dis, XK_Num_Lock))
                numlockmask = (1 << k);
    XFreeModifiermap(modmap);

    /* set up atoms for dialog/notification windows */
    atoms[WM_PROTOCOLS]     = XInternAtom(dis, "WM_PROTOCOLS",     False);
    atoms[WM_DELETE_WINDOW] = XInternAtom(dis, "WM_DELETE_WINDOW", False);

    /* check if another window manager is running */
    xerrorxlib = XSetErrorHandler(xerrorstart);
    XSelectInput(dis, DefaultRootWindow(dis), SubstructureNotifyMask|SubstructureRedirectMask);
    XSync(dis, False);
    XSetErrorHandler(xerror);
    XSync(dis, False);

    grabkeys();
}

void sigchld() {
    if (signal(SIGCHLD, sigchld) == SIG_ERR)
        die("error: can't install SIGCHLD handler\n");
    while(0 < waitpid(-1, NULL, WNOHANG));
}

void spawn(const Arg *arg) {
    if (fork() == 0) {
        if (dis) close(ConnectionNumber(dis));
        setsid();
        execvp((char*)arg->com[0], (char**)arg->com);
        fprintf(stderr, "error: execvp %s", (char *)arg->com[0]);
        perror(" failed"); /* also prints the err msg */
        exit(EXIT_SUCCESS);
    }
}

void swap_master() {
    if (!current || !head->next || mode == MONOCLE) return;
    Window tmpwin = head->win;
    current = (current == head) ? head->next : current;
    head->win = current->win;
    current->win = tmpwin;
    current = head;
    save_desktop(current_desktop);
    tile();
    update_current();
}

void switch_mode(const Arg *arg) {
    if (mode == arg->i) return;
    if (mode == MONOCLE) for (client *c=head; c; c=c->next) XMapWindow(dis, c->win);
    mode = arg->i;
    master_size = (mode == BSTACK ? wh : ww) * MASTER_SIZE;
    tile();
    update_current();
    desktopinfo();
}

void tile(void) {
    if (!head) return; /* nothing to arange */
    client *c;
    int panel_height = (showpanel) ? PANEL_HEIGHT : 0;
    int n = 0, z = mode == BSTACK ? ww : wh, d = 0, cx = 0, cy = TOP_PANEL ? panel_height : 0, cw = 0, ch = 0;
    for (n=0, c=head->next; c; c=c->next, ++n);      /* count windows on stack */
    if ((mode == TILE || mode == BSTACK) && n > 1) { /* adjust to match screen height/width */
        d = (z - growth)%n + growth;
        z = (z - growth)/n;
    }
    if (!head->next || mode == MONOCLE) {
        for (c=head; c; c=c->next) XMoveResizeWindow(dis, c->win, cx, cy, ww + 2*BORDER_WIDTH, wh + 2*BORDER_WIDTH);
    } else if (mode == TILE) {
        XMoveResizeWindow(dis, head->win, cx, cy, master_size - BORDER_WIDTH, wh - BORDER_WIDTH);
        XMoveResizeWindow(dis, head->next->win, (cx = master_size + BORDER_WIDTH), cy,
                         (cw = ww - master_size - 2*BORDER_WIDTH), (ch = z - BORDER_WIDTH) + d);
        for (cy+=z+d, c=head->next->next; c; c=c->next, cy+=z) XMoveResizeWindow(dis, c->win, cx, cy, cw, ch);
    } else if (mode == BSTACK) {
        XMoveResizeWindow(dis, head->win, cx, cy, ww - BORDER_WIDTH, master_size - BORDER_WIDTH);
        XMoveResizeWindow(dis, head->next->win, cx, (cy += master_size + BORDER_WIDTH),
                         (cw = z - BORDER_WIDTH) + d, (ch = wh - master_size - 2*BORDER_WIDTH));
        for (cx+=z+d, c=head->next->next; c; c=c->next, cx+=z) XMoveResizeWindow(dis, c->win, cx, cy, cw, ch);
    } else if (mode == GRID) {
        ++n;                              /* include head on window count */
        int cols, rows, cn=0, rn=0, i=0;  /* columns, rows, and current column and row number */
        for (cols=0; cols <= n/2; cols++) if (cols*cols >= n) break;     /* emulate square root */
        if (n == 5) cols = 2;
        rows = n/cols;
        cw = cols ? ww/cols : ww;
        for (i=0, c=head; c; c=c->next, i++) {
            if (i/rows + 1 > cols - n%cols)
                rows = n/cols + 1;
            ch = wh/rows;
            cx = 0 + cn*cw;
            cy = (TOP_PANEL ? panel_height : 0) + rn*ch;
            XMoveResizeWindow(dis, c->win, cx, cy, cw - 2*BORDER_WIDTH, ch - 2*BORDER_WIDTH);
            rn++;
            if (rn >= rows) {
                rn = 0;
                cn++;
            }
        }
    } else fprintf(stderr, "--> error: no such layout mode: %d\n", mode);
    free(c);
}

void togglepanel() {
    wh += (showpanel = !showpanel) ? -PANEL_HEIGHT : +PANEL_HEIGHT;
    save_desktop(current_desktop);
    tile();
}

void update_current(void) {
    if (!current) return;
    int border_width = (!head->next || mode == MONOCLE) ? 0 : BORDER_WIDTH;

    for (client *c=head; c; c=c->next) {
        XSetWindowBorderWidth(dis, c->win, border_width);
        XSetWindowBorder(dis, c->win, win_unfocus);
        if (CLICK_TO_FOCUS) XGrabButton(dis, AnyButton, AnyModifier, c->win, True,
            ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
    }
    XSetWindowBorder(dis, current->win, win_focus);
    XSetInputFocus(dis, current->win, RevertToParent, CurrentTime);
    XRaiseWindow(dis, current->win);
    if (CLICK_TO_FOCUS) XUngrabButton(dis, AnyButton, AnyModifier, current->win);
    XSync(dis, False);
}

client* wintoclient(Window w) {
    client *c = NULL;
    Bool found = False;
    int cd = current_desktop;
    save_desktop(cd);
    for (int d=0; d<DESKTOPS && !found; d++) {
        select_desktop(d);
        for (c=head; c; c=c->next) if ((found = (w == c->win))) break;
    }
    select_desktop(cd);
    return c;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit through xerrorlib.  */
int xerror(Display *dis, XErrorEvent *ee) {
    if (ee->error_code == BadWindow
            || (ee->error_code == BadMatch    && (ee->request_code == X_SetInputFocus || ee->request_code ==  X_ConfigureWindow))
            || (ee->error_code == BadDrawable && (ee->request_code == X_PolyText8     || ee->request_code == X_PolyFillRectangle
                                               || ee->request_code == X_PolySegment   || ee->request_code == X_CopyArea))
            || (ee->error_code == BadAccess   &&  ee->request_code == X_GrabKey))
        return 0;
    fprintf(stderr, "error: xerror: request code: %d, error code: %d\n", ee->request_code, ee->error_code);
    return xerrorxlib(dis, ee);
}

int xerrorstart() {
    die("error: another window manager is already running\n");
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp("-v", argv[1]) == 0) {
        fprintf(stdout, "%s-%s\n", WMNAME, VERSION);
        return EXIT_SUCCESS;
    } else if (argc != 1)
        die("usage: %s [-v]\n", WMNAME);
    if (!(dis = XOpenDisplay(NULL)))
        die("error: cannot open display\n");
    setup();
    desktopinfo(); /* zero out every desktop on (re)start */
    run();
    cleanup();
    XCloseDisplay(dis);
    return retval;
}
