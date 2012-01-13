/* see license for copyright and license */

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

#define LENGTH(x)       (sizeof(x)/sizeof(*x))
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))
#define BUTTONMASK      ButtonPressMask|ButtonReleaseMask

enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_COUNT };
enum { TILE, MONOCLE, BSTACK, GRID, };
enum { NET_SUPPORTED, NET_FULLSCREEN, NET_WM_STATE, NET_ACTIVE, NET_COUNT };

/* structs */
typedef union {
    const char** com;
    const int i;
} Arg;

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(const Arg *);
    const Arg arg;
} key;

typedef struct {
    unsigned int mask;
    unsigned int button;
    void (*func)(const Arg *);
    const Arg arg;
} Button;

typedef struct client {
    struct client *next;
    Bool isurgent, istransient, isfullscreen;
    Window win;
} client;

typedef struct {
    int master_size;
    int mode;
    int growth;
    client *head;
    client *current;
    client *prevfocus;
    Bool showpanel;
} desktop;

typedef struct {
    const char *class;
    const int desktop;
    const Bool follow;
} AppRule;

/* Functions */
static void addwindow(Window w);
static void buttonpress(XEvent *e);
static void change_desktop(const Arg *arg);
static void cleanup(void);
static void client_to_desktop(const Arg *arg);
static void clientmessage(XEvent *e);
static void configurerequest(XEvent *e);
static void desktopinfo(void);
static void destroynotify(XEvent *e);
static void die(const char* errstr, ...);
static void enternotify(XEvent *e);
static void focusurgent();
static unsigned long getcolor(const char* color);
static void grabbuttons(client *c);
static void grabkeys(void);
static void keypress(XEvent *e);
static void killclient();
static void last_desktop();
static void maprequest(XEvent *e);
static void move_down();
static void move_up();
static void mousemove(const Arg *arg);
static void mouseresize(const Arg *arg);
static void next_win();
static void prev_win();
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static void removeclient(client *c);
static void resize_master(const Arg *arg);
static void resize_stack(const Arg *arg);
static void rotate_desktop(const Arg *arg);
static void run(void);
static void save_desktop(int i);
static void select_desktop(int i);
static void sendevent(Window w, int atom);
static void setfullscreen(client *c, Bool fullscreen);
static void setup(void);
static void sigchld();
static void spawn(const Arg *arg);
static void swap_master();
static void switch_mode(const Arg *arg);
static void tile(void);
static void togglepanel();
static void update_current(client *c);
static void unmapnotify(XEvent *e);
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
static int wh; /* window area height - screen height minus the border size and panel height */
static int ww; /* window area width - screen width minus the border size */
static int screen;
static int xerror(Display *dis, XErrorEvent *ee);
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int win_focus;
static unsigned int win_unfocus;
static unsigned int numlockmask = 0; /* dynamic key lock mask */
static Display *dis;
static Window root;
static client *head, *prevfocus, *current;
static Atom wmatoms[WM_COUNT], netatoms[NET_COUNT];
static desktop desktops[DESKTOPS];

/* events array */
static void (*events[LASTEvent])(XEvent *e) = {
    [ButtonPress] = buttonpress,
    [ClientMessage] = clientmessage,
    [ConfigureRequest] = configurerequest,
    [DestroyNotify] = destroynotify,
    [EnterNotify] = enternotify,
    [KeyPress] = keypress,
    [MapRequest] = maprequest,
    [PropertyNotify] = propertynotify,
    [UnmapNotify] = unmapnotify,
};

void addwindow(Window w) {
    client *c, *t;
    if (!(c = (client *)calloc(1, sizeof(client))))
        die("error: could not calloc() %u bytes\n", sizeof(client));

    if (!head) head = c;
    else if (ATTACH_ASIDE) {
        for(t=head; t->next; t=t->next); /* get the last client */
        t->next = c;
    } else {
        c->next = (t = head);
        head = c;
    }

    prevfocus = current;
    XSelectInput(dis, ((current = c)->win = w), PropertyChangeMask|(FOLLOW_MOUSE?EnterWindowMask:0));
}

void buttonpress(XEvent *e) {
    client *c = wintoclient(e->xbutton.window);
    if (!c) return;
    if (CLICK_TO_FOCUS && current != c && e->xbutton.button == Button1) update_current(c);

    for (unsigned int i=0; i<LENGTH(buttons); i++)
        if (buttons[i].func && buttons[i].button == e->xbutton.button &&
            CLEANMASK(buttons[i].mask) == CLEANMASK(e->xbutton.state)) {
            update_current(c);
            buttons[i].func(&(buttons[i].arg));
        }
}

void change_desktop(const Arg *arg) {
    if (arg->i == current_desktop) return;
    previous_desktop = current_desktop;
    select_desktop(arg->i);
    tile();
    if (mode == MONOCLE && current) XMapWindow(dis, current->win);
    else for (client *c=head; c; c=c->next) XMapWindow(dis, c->win);
    update_current(NULL);
    select_desktop(previous_desktop);
    for (client *c=head; c; c=c->next) XUnmapWindow(dis, c->win);
    select_desktop(arg->i);
    desktopinfo();
}

void cleanup(void) {
    Window root_return;
    Window parent_return;
    Window *children;
    unsigned int nchildren;

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    XQueryTree(dis, root, &root_return, &parent_return, &children, &nchildren);
    for (unsigned int i = 0; i<nchildren; i++) sendevent(children[i], WM_DELETE_WINDOW);
    if (children) XFree(children);
    XSync(dis, False);
    XSetInputFocus(dis, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void client_to_desktop(const Arg *arg) {
    if (arg->i == current_desktop || !current) return;
    Window w = current->win;
    int cd = current_desktop;

    XUnmapWindow(dis, current->win);
    if (current->isfullscreen) setfullscreen(current, False);
    removeclient(current);

    select_desktop(arg->i);
    addwindow(w);

    select_desktop(cd);
    tile();
    update_current(NULL);
    if (FOLLOW_WINDOW) change_desktop(arg);
    desktopinfo();
}

/* check if window requested fullscreen wm_state
 * To change the state of a mapped window, a client MUST send a _NET_WM_STATE client message to the root window
 * message_type must be _NET_WM_STATE, data.l[0] is the action to be taken, data.l[1] is the property to alter
 * three actions: remove/unset _NET_WM_STATE_REMOVE=0, add/set _NET_WM_STATE_ADD=1, toggle _NET_WM_STATE_TOGGLE=2
 */
void clientmessage(XEvent *e) {
    client *c = wintoclient(e->xclient.window);
    if (c && e->xclient.message_type == netatoms[NET_WM_STATE] && ((unsigned)e->xclient.data.l[1]
        == netatoms[NET_FULLSCREEN] || (unsigned)e->xclient.data.l[2] == netatoms[NET_FULLSCREEN]))
        setfullscreen(c, (e->xclient.data.l[0] == 1 || (e->xclient.data.l[0] == 2 && !c->isfullscreen)));
    else if (c && e->xclient.message_type == netatoms[NET_ACTIVE]) current = c;
    tile();
    update_current(NULL);
}

void configurerequest(XEvent *e) {
    client *c = wintoclient(e->xconfigurerequest.window);
    if (c && c->isfullscreen)
        XMoveResizeWindow(dis, c->win, 0, 0, ww + BORDER_WIDTH, wh + BORDER_WIDTH + PANEL_HEIGHT);
    else {
        XWindowChanges wc;
        wc.x = e->xconfigurerequest.x;
        wc.y = e->xconfigurerequest.y + (showpanel && TOP_PANEL) ? PANEL_HEIGHT : 0;
        wc.width  = (e->xconfigurerequest.width  < ww - BORDER_WIDTH) ? e->xconfigurerequest.width  : ww + BORDER_WIDTH;
        wc.height = (e->xconfigurerequest.height < wh - BORDER_WIDTH) ? e->xconfigurerequest.height : wh + BORDER_WIDTH;
        wc.border_width = e->xconfigurerequest.border_width;
        wc.sibling    = e->xconfigurerequest.above;
        wc.stack_mode = e->xconfigurerequest.detail;
        XConfigureWindow(dis, e->xconfigurerequest.window, e->xconfigurerequest.value_mask, &wc);
        XSync(dis, False);
    }
    tile();
}

void desktopinfo(void) {
    Bool urgent = False;
    int cd = current_desktop, n=0, d=0;
    for (client *c; d<DESKTOPS; d++) {
        for (select_desktop(d), c=head, n=0, urgent=False; c; c=c->next, ++n) if (c->isurgent) urgent = True;
        fprintf(stdout, "%d:%d:%d:%d:%d%c", d, n, mode, current_desktop == cd, urgent, d+1==DESKTOPS?'\n':' ');
    }
    fflush(stdout);
    select_desktop(cd);
}

void destroynotify(XEvent *e) {
    client *c = wintoclient(e->xdestroywindow.window);
    if (c) removeclient(c);
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
    if (!FOLLOW_MOUSE) return;
    client *c = wintoclient(e->xcrossing.window);
    if (!c) return;
    if (e->xcrossing.mode == NotifyNormal && e->xcrossing.detail != NotifyInferior) update_current(c);
}

void focusurgent() {
    for (client *c=head; c; c=c->next) if (c->isurgent) update_current(c);
}

unsigned long getcolor(const char* color) {
    Colormap map = DefaultColormap(dis, screen);
    XColor c;

    if (!XAllocNamedColor(dis, map, color, &c, &c))
        die("error: cannot allocate color '%s'\n", c);
    return c.pixel;
}

void grabbuttons(client *c) {
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    for (unsigned int b=0; b<LENGTH(buttons); b++)
        for (unsigned int m=0; m<LENGTH(modifiers); m++)
            XGrabButton(dis, buttons[b].button, buttons[b].mask|modifiers[m], c->win,
                        False, BUTTONMASK, GrabModeAsync, GrabModeAsync, None, None);
}

void grabkeys(void) {
    KeyCode code;
    XUngrabKey(dis, AnyKey, AnyModifier, root);

    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    for (unsigned int k=0; k<LENGTH(keys); k++)
        if ((code = XKeysymToKeycode(dis, keys[k].keysym)))
            for (unsigned int m=0; m<LENGTH(modifiers); m++)
                XGrabKey(dis, code, keys[k].mod|modifiers[m], root, True, GrabModeAsync, GrabModeAsync);
}

void keypress(XEvent *e) {
    KeySym keysym;
    keysym = XKeycodeToKeysym(dis, (KeyCode)e->xkey.keycode, 0);
    for (unsigned int i=0; i<LENGTH(keys); i++)
        if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(e->xkey.state) && keys[i].func)
                keys[i].func(&keys[i].arg);
}

void killclient() {
    if (!current) return;
    sendevent(current->win, WM_DELETE_WINDOW);
    removeclient(current);
}

void last_desktop() {
    change_desktop(&(Arg){.i = previous_desktop});
}

void maprequest(XEvent *e) {
    static XWindowAttributes wa;
    if (XGetWindowAttributes(dis, e->xmaprequest.window, &wa) && wa.override_redirect) return;
    if (wintoclient(e->xmaprequest.window)) return;

    Bool follow = False;
    int cd = current_desktop, newdsk = current_desktop;
    XClassHint ch = {0, 0};
    if (XGetClassHint(dis, e->xmaprequest.window, &ch))
        for (unsigned int i=0; i<LENGTH(rules); i++)
            if (!strcmp(ch.res_class, rules[i].class) || !strcmp(ch.res_name, rules[i].class)) {
                follow = rules[i].follow;
                newdsk = rules[i].desktop;
                break;
            }
    if (ch.res_class) XFree(ch.res_class);
    if (ch.res_name) XFree(ch.res_name);

    select_desktop(newdsk);
    addwindow(e->xmaprequest.window);

    Window w;
    current->istransient = XGetTransientForHint(dis, e->xmaprequest.window, &w);

    int di; unsigned long dl; unsigned char *state = NULL; Atom da;
    if (XGetWindowProperty(dis, current->win, netatoms[NET_WM_STATE], 0L, sizeof da,
                    False, XA_ATOM, &da, &di, &dl, &dl, &state) == Success && state)
        setfullscreen(current, (*(Atom *)state == netatoms[NET_FULLSCREEN]));
    if (state) XFree(state);

    select_desktop(cd);
    if (cd == newdsk) {
        tile();
        XMapWindow(dis, e->xmaprequest.window);
        update_current(NULL);
        grabbuttons(current);
    } else if (follow) change_desktop(&(Arg){.i = newdsk});
    desktopinfo();
}

void mousemove(const Arg *arg) {
    if (!current || !arg) return;
    static XWindowAttributes wa;
    XGetWindowAttributes(dis, current->win, &wa);

    if (XGrabPointer(dis, root, False, BUTTONMASK|PointerMotionMask, GrabModeAsync,
                     GrabModeAsync, None, None, CurrentTime) != GrabSuccess) return;
    int x, y, z; unsigned int v; Window w;
    XQueryPointer(dis, root, &w, &w, &x, &y, &z, &z, &v);

    XEvent ev;
    do {
        XMaskEvent(dis, BUTTONMASK|PointerMotionMask|SubstructureRedirectMask, &ev);
        switch (ev.type) {
            case ConfigureRequest:
            case MapRequest:
                events[ev.type](&ev);
                break;
            case MotionNotify:
                XMoveWindow(dis, current->win, wa.x + ev.xmotion.x - x, wa.y + ev.xmotion.y - y);
                break;
        }
    } while(ev.type != ButtonRelease);
    XUngrabPointer(dis, CurrentTime);
}

void mouseresize(const Arg *arg) {
    if (!current || !arg) return;
    static XWindowAttributes wa;
    XGetWindowAttributes(dis, current->win, &wa);

    if (XGrabPointer(dis, root, False, BUTTONMASK|PointerMotionMask, GrabModeAsync,
                     GrabModeAsync, None, None, CurrentTime) != GrabSuccess) return;
    int x, y, z; unsigned int v; Window w;
    XQueryPointer(dis, root, &w, &w, &x, &y, &z, &z, &v);

    XEvent ev;
    do {
        XMaskEvent(dis, BUTTONMASK|PointerMotionMask|SubstructureRedirectMask, &ev);
        switch (ev.type) {
            case ConfigureRequest:
            case MapRequest:
                events[ev.type](&ev);
                break;
            case MotionNotify:
                XResizeWindow(dis, current->win, wa.width + ev.xmotion.x - x, wa.height + ev.xmotion.y - y);
                break;
        }
    } while(ev.type != ButtonRelease);
    XUngrabPointer(dis, CurrentTime);
}

/* move the current client, to current->next
 * and current->next to current client's position
 */
void move_down() {
    if (!current || !head->next) return;
    for (client *t=head; t; t=t->next) if (t->isfullscreen) return;

    /* p is previous, n is next, if current is head n is last, c is current */
    client *p = NULL, *n = (current->next) ? current->next : head;
    for (p=head; p && p->next != current; p=p->next);
    /* if there's a previous client then p->next should be what's after c
     * ..->[p]->[c]->[n]->..  ==>  ..->[p]->[n]->[c]->..
     */
    if (p) p->next = current->next;
    /* else if no p client, then c is head, swapping with n should update head
     * [c]->[n]->..  ==>  [n]->[c]->..
     *  ^head              ^head
     */
    else head = n;
    /* if c is the last client, c will be the current head
     * [n]->..->[p]->[c]->NULL  ==>  [c]->[n]->..->[p]->NULL
     *  ^head                         ^head
     * else c will take the place of n, so c-next will be n->next
     * ..->[p]->[c]->[n]->..  ==>  ..->[p]->[n]->[c]->..
     */
    current->next = (current->next) ? n->next : n;
    /* if c was swapped with n then they now point to the same ->next. n->next should be c
     * ..->[p]->[c]->[n]->..  ==>  ..->[p]->[n]->..  ==>  ..->[p]->[n]->[c]->..
     *                                        [c]-^
     */
    if (current->next == n->next) n->next = current;
    /* else c is the last client and n is head,
     * so c will be move to be head, no need to update n->next
     * [n]->..->[p]->[c]->NULL  ==>  [c]->[n]->..->[p]->NULL
     *  ^head                         ^head
     */
    else head = current;

    tile();
    update_current(NULL);
}

/* move the current client, to the previous from current
 * and the previous from  current to current client's position
 */
void move_up() {
    if (!current || !head->next) return;
    for (client *t=head; t; t=t->next) if (t->isfullscreen) return;

    client *pp = NULL, *p;
    /* p is previous from current or last if current is head */
    for (p=head; p->next && p->next != current; p=p->next);
    /* pp is previous from p, or null if current is head and thus p is last */
    if (p->next) for (pp=head; pp; pp=pp->next) if (pp->next == p) break;
    /* if p has a previous client then the next client should be current (current is c)
     * ..->[pp]->[p]->[c]->..  ==>  ..->[pp]->[c]->[p]->..
     */
    if (pp) pp->next = current;
    /* if p doesn't have a previous client, then p might be head, so head must change to c
     * [p]->[c]->..  ==>  [c]->[p]->..
     *  ^head              ^head
     * if p is not head, then c is head (and p is last), so the new head is next of c
     * [c]->[n]->..->[p]->NULL  ==>  [n]->..->[p]->[c]->NULL
     *  ^head         ^last           ^head         ^last
     */
    else head = (current == head) ? current->next : current;
    /* next of p should be next of c
     * ..->[pp]->[p]->[c]->[n]->..  ==>  ..->[pp]->[c]->[p]->[n]->..
     * except if c was head (now c->next is head), so next of p should be c
     * [c]->[n]->..->[p]->NULL  ==>  [n]->..->[p]->[c]->NULL
     *  ^head         ^last           ^head         ^last
     */
    p->next = (current->next == head) ? current : current->next;
    /* next of c should be p
     * ..->[pp]->[p]->[c]->[n]->..  ==>  ..->[pp]->[c]->[p]->[n]->..
     * except if c was head (now c->next is head), so c is must be last
     * [c]->[n]->..->[p]->NULL  ==>  [n]->..->[p]->[c]->NULL
     *  ^head         ^last           ^head         ^last
     */
    current->next = (current->next == head) ? NULL : p;

    tile();
    update_current(NULL);
}

void next_win() {
    if (!current || !head->next) return;
    update_current((prevfocus = current)->next ? current->next : head);
    if (mode == MONOCLE) XMapWindow(dis, current->win);
}

void prev_win() {
    if (!current || !head->next) return;
    if (head == (prevfocus = current)) while (current->next) current=current->next;
    else for (client *t=head; t; t=t->next) if (t->next == current) { current = t; break; }
    if (mode == MONOCLE) XMapWindow(dis, current->win);
    update_current(NULL);
}

void propertynotify(XEvent *e) {
    client *c;
    if ((c = wintoclient(e->xproperty.window)))
        if (e->xproperty.atom == XA_WM_HINTS) {
            XWMHints *wmh = XGetWMHints(dis, e->xproperty.window);
            c->isurgent = wmh && (wmh->flags & XUrgencyHint);
            XFree(wmh);
            desktopinfo();
        }
}

void quit(const Arg *arg) {
    retval = arg->i;
    running = False;
}

void removeclient(client *c) {
    client **p = NULL;
    int nd = 0, cd = current_desktop;
    for (Bool found = False; nd<DESKTOPS && !found; nd++)
        for (select_desktop(nd), p = &head; *p && !(found = *p == c); p = &(*p)->next);
    *p = c->next;
    Bool transient = c->istransient;
    free(c);
    current = (prevfocus && prevfocus != current) ? prevfocus : (*p) ? (prevfocus = *p) : (prevfocus = head);
    select_desktop(cd);
    if (!transient) tile();
    if (mode == MONOCLE && cd == --nd && current) XMapWindow(dis, current->win);
    update_current(NULL);
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
    while(running && !XNextEvent(dis, &ev)) if (events[ev.type]) events[ev.type](&ev);
}

void save_desktop(int i) {
    if (i >= DESKTOPS) return;
    desktops[i].master_size = master_size;
    desktops[i].mode = mode;
    desktops[i].growth = growth;
    desktops[i].head = head;
    desktops[i].current = current;
    desktops[i].showpanel = showpanel;
    desktops[i].prevfocus = prevfocus;
}

void select_desktop(int i) {
    if (i >= DESKTOPS || i == current_desktop) return;
    save_desktop(current_desktop);
    master_size = desktops[i].master_size;
    mode = desktops[i].mode;
    growth = desktops[i].growth;
    head = desktops[i].head;
    current = desktops[i].current;
    showpanel = desktops[i].showpanel;
    prevfocus = desktops[i].prevfocus;
    current_desktop = i;
}

void sendevent(Window w, int atom) {
    if (atom >= WM_COUNT) return;
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = wmatoms[WM_PROTOCOLS];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = wmatoms[atom];
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dis, w, False, NoEventMask, &ev);
}

void setfullscreen(client *c, Bool fullscreen) {
    XChangeProperty(dis, c->win, netatoms[NET_WM_STATE], XA_ATOM, 32, PropModeReplace, (unsigned char*)
                   ((c->isfullscreen = fullscreen) ? &netatoms[NET_FULLSCREEN] : 0), fullscreen);
    if (c->isfullscreen) XMoveResizeWindow(dis, c->win, 0, 0, ww + BORDER_WIDTH, wh + BORDER_WIDTH + PANEL_HEIGHT);
}

void setup(void) {
    sigchld();

    screen = DefaultScreen(dis);
    root = RootWindow(dis, screen);

    ww = XDisplayWidth(dis,  screen) - BORDER_WIDTH;
    wh = XDisplayHeight(dis, screen) - (SHOW_PANEL ? PANEL_HEIGHT : 0) - BORDER_WIDTH;
    master_size = ((mode == BSTACK) ? wh : ww) * MASTER_SIZE;
    for (unsigned int i=0; i<DESKTOPS; i++) save_desktop(i);
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
    wmatoms[WM_PROTOCOLS]     = XInternAtom(dis, "WM_PROTOCOLS",     False);
    wmatoms[WM_DELETE_WINDOW] = XInternAtom(dis, "WM_DELETE_WINDOW", False);
    netatoms[NET_SUPPORTED]   = XInternAtom(dis, "_NET_SUPPORTED",   False);
    netatoms[NET_WM_STATE]    = XInternAtom(dis, "_NET_WM_STATE",    False);
    netatoms[NET_ACTIVE]      = XInternAtom(dis, "_NET_ACTIVE_WINDOW",       False);
    netatoms[NET_FULLSCREEN]  = XInternAtom(dis, "_NET_WM_STATE_FULLSCREEN", False);

    /* check if another window manager is running */
    xerrorxlib = XSetErrorHandler(xerrorstart);
    XSelectInput(dis, DefaultRootWindow(dis), SubstructureRedirectMask|SubstructureNotifyMask|PropertyChangeMask|ButtonPressMask);
    XSync(dis, False);

    XSetErrorHandler(xerror);
    XSync(dis, False);
    XChangeProperty(dis, root, netatoms[NET_SUPPORTED], XA_ATOM, 32, PropModeReplace, (unsigned char *)netatoms, NET_COUNT);

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
    for (client *t=head; t; t=t->next) if (t->isfullscreen) return;
    /* if current is head swap with next window */
    if (current == head) move_down();
    /* if not head, then head is always behind us, so move_up until is head */
    else while (current != head) move_up();
    update_current(head);
    tile();
}

void switch_mode(const Arg *arg) {
    if (mode == MONOCLE) for (client *c=head; c; c=c->next) XMapWindow(dis, c->win);
    mode = arg->i;
    master_size = (mode == BSTACK ? wh : ww) * MASTER_SIZE;
    tile();
    update_current(NULL);
    desktopinfo();
}

void tile(void) {
    if (!head) return; /* nothing to arange */

    client *c;
    /* n:number of windows, d:difference, h:available height, z:client height */
    int n = 0, d = 0, h = wh + (showpanel ? 0 : PANEL_HEIGHT), z = mode == BSTACK ? ww : h;
    /* client's x,y coordinates, width and height */
    int cx = 0, cy = (TOP_PANEL && showpanel ? PANEL_HEIGHT : 0), cw = 0, ch = 0;

    /* count stack windows -- do not consider fullscreen or transient clients */
    for (n=0, c=head->next; c; c=c->next) if (!c->istransient && !c->isfullscreen) ++n;

    if (!head->next || (head->next->istransient && !head->next->next) || mode == MONOCLE) {
        for (c=head; c; c=c->next) if (!c->isfullscreen && !c->istransient)
            XMoveResizeWindow(dis, c->win, cx, cy, ww + BORDER_WIDTH, h + BORDER_WIDTH);
    } else if (mode == TILE || mode == BSTACK) {
        d = (z - growth)%n + growth;       /* n should be greater than one */
        z = (z - growth)/n;         /* adjust to match screen height/width */
        if (!head->isfullscreen && !head->istransient)
            (mode == BSTACK) ? XMoveResizeWindow(dis, head->win, cx, cy, ww - BORDER_WIDTH, master_size - BORDER_WIDTH)
                             : XMoveResizeWindow(dis, head->win, cx, cy, master_size - BORDER_WIDTH,  h - BORDER_WIDTH);
        for (c=head->next; c && (c->isfullscreen || c->istransient); c=c->next);
        if (c) (mode == BSTACK) ? XMoveResizeWindow(dis, c->win, cx, (cy += master_size),
                                (cw = z - BORDER_WIDTH) + d, (ch = h - master_size - BORDER_WIDTH))
                                : XMoveResizeWindow(dis, c->win, (cx += master_size), cy,
                                (cw = ww - master_size - BORDER_WIDTH), (ch = z - BORDER_WIDTH) + d);
        if (c) for (mode==BSTACK?(cx+=z+d):(cy+=z+d), c=c->next; c; c=c->next)
            if (!c->isfullscreen && !c->istransient) {
                XMoveResizeWindow(dis, c->win, cx, cy, cw, ch);
                mode==BSTACK?(cx+=z):(cy+=z);
            }
    } else if (mode == GRID) {
        ++n;                              /* include head on window count */
        int cols, rows, cn=0, rn=0, i=0;  /* columns, rows, and current column and row number */
        for (cols=0; cols <= n/2; cols++) if (cols*cols >= n) break;   /* emulate square root */
        if (n == 5) cols = 2;
        rows = n/cols;
        cw = cols ? ww/cols : ww;
        for (i=0, c=head; c; c=c->next, i++) {
            if (i/rows + 1 > cols - n%cols) rows = n/cols + 1;
            ch = h/rows;
            cx = cn*cw;
            cy = (TOP_PANEL && showpanel ? PANEL_HEIGHT : 0) + rn*ch;
            if (!c->isfullscreen && !c->istransient) XMoveResizeWindow(dis, c->win, cx, cy, cw - BORDER_WIDTH, ch - BORDER_WIDTH);
            if (++rn >= rows) { rn = 0; cn++; }
        }
    } else fprintf(stderr, "error: no such layout mode: %d\n", mode);
    free(c);
}

void togglepanel() {
    showpanel = !showpanel;
    tile();
}

void unmapnotify(XEvent *e) {
    client *c = wintoclient(e->xunmap.window);
    if (c && e->xunmap.send_event) removeclient(c);
    desktopinfo();
}

void update_current(client *c) {
    if (!current && !c) {
        XDeleteProperty(dis, root, netatoms[NET_ACTIVE]);
        return;
    } else if(c) current = c;

    int border_width = (!head->next || (head->next->istransient &&
                        !head->next->next) || mode == MONOCLE) ? 0 : BORDER_WIDTH;

    for (client *c=head; c; c=c->next) {
        XSetWindowBorderWidth(dis, c->win, (c->isfullscreen ? 0 : border_width));
        XSetWindowBorder(dis, c->win, (current == c ? win_focus : win_unfocus));
        if (CLICK_TO_FOCUS) XGrabButton(dis, AnyButton, AnyModifier, c->win, True,
                            BUTTONMASK, GrabModeAsync, GrabModeAsync, None, None);
    }

    XChangeProperty(dis, root, netatoms[NET_ACTIVE], XA_WINDOW, 32, PropModeReplace, (unsigned char *)&current->win, 1);
    XSetInputFocus(dis, current->win, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(dis, current->win);

    if (CLICK_TO_FOCUS) XUngrabButton(dis, AnyButton, AnyModifier, current->win);
    XSync(dis, False);
}

client* wintoclient(Window w) {
    client *c = NULL;
    int d = 0, cd = current_desktop;
    for (Bool found = False; d<DESKTOPS && !found; ++d)
        for (select_desktop(d), c=head; c && !(found = (w == c->win)); c=c->next);
    select_desktop(cd);
    return c;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit through xerrorlib.  */
int xerror(Display *dis, XErrorEvent *ee) {
    if (ee->error_code == BadWindow
            || (ee->error_code == BadMatch    && (ee->request_code == X_SetInputFocus || ee->request_code == X_ConfigureWindow))
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
    } else if (argc != 1) die("usage: %s [-v]\n", WMNAME);
    if (!(dis = XOpenDisplay(NULL))) die("error: cannot open display\n");
    setup();
    desktopinfo(); /* zero out every desktop on (re)start */
    run();
    cleanup();
    XCloseDisplay(dis);
    return retval;
}
