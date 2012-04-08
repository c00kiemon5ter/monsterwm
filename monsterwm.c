/* see license for copyright and license */

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/Xproto.h>
#include <X11/Xatom.h>

#define LENGTH(x)       (sizeof(x)/sizeof(*x))
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))
#define BUTTONMASK      ButtonPressMask|ButtonReleaseMask
#define ISFFT(c)        (c->isfullscrn || c->isfloating || c->istransient)
#define USAGE           "usage: monsterwm [-h] [-v]"

enum { RESIZE, MOVE };
enum { TILE, MONOCLE, BSTACK, GRID, MODES };
enum { WM_PROTOCOLS, WM_DELETE_WINDOW, WM_COUNT };
enum { NET_SUPPORTED, NET_FULLSCREEN, NET_WM_STATE, NET_ACTIVE, NET_COUNT };

/* argument structure to be passed to function by config.h
 * com  - a command to run
 * i    - an integer to indicate different states
 */
typedef union {
    const char** com;
    const int i;
} Arg;

/* a key struct represents a combination of
 * mod      - a modifier mask
 * keysym   - and the key pressed
 * func     - the function to be triggered because of the above combo
 * arg      - the argument to the function
 */
typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*func)(const Arg *);
    const Arg arg;
} key;

/* a button struct represents a combination of
 * mask     - a modifier mask
 * button   - and the mouse button pressed
 * func     - the function to be triggered because of the above combo
 * arg      - the argument to the function
 */
typedef struct {
    unsigned int mask, button;
    void (*func)(const Arg *);
    const Arg arg;
} Button;

/* a client is a wrapper to a window that additionally
 * holds some properties for that window
 *
 * next        - the client after this one, or NULL if the current is the last client
 * isurgent    - set when the window received an urgent hint
 * istransient - set when the window is transient
 * isfullscrn  - set when the window is fullscreen
 * isfloating  - set when the window is floating
 * win         - the window this client is representing
 *
 * istransient is separate from isfloating as floating window can be reset
 * to their tiling positions, while the transients will always be floating
 */
typedef struct client {
    struct client *next;
    Bool isurgent, istransient, isfullscrn, isfloating;
    Window win;
} client;

/* properties of each desktop
 * mode         - the desktop's tiling layout mode
 * head         - the start of the client list
 * current      - the currently highlighted window
 * prevfocus    - the client that previously had focus
 */
typedef struct {
    int mode;
    client *head, *current, *prevfocus;
} desktop;

/* define behavior of certain applications
 * configured in config.h
 * class    - the class or name of the instance
 * desktop  - what desktop it should be spawned at
 * follow   - whether to change desktop focus to the specified desktop
 */
typedef struct {
    const char *class;
    const int desktop;
    const Bool follow, floating;
} AppRule;

/* function prototypes sorted alphabetically */
static client* addwindow(Window w);
static void buttonpress(XEvent *e);
static void change_desktop(const Arg *arg);
static void cleanup(void);
static void client_to_desktop(const Arg *arg);
static void clientmessage(XEvent *e);
static void configurerequest(XEvent *e);
static void deletewindow(Window w);
static void desktopinfo(void);
static void destroynotify(XEvent *e);
static void enternotify(XEvent *e);
static unsigned long getcolor(const char* color);
static void grabbuttons(client *c);
static void grabkeys(void);
static void grid(int h, int y);
static void keypress(XEvent *e);
static void killclient();
static void maprequest(XEvent *e);
static void monocle(int h, int y);
static void move_down();
static void move_up();
static void mousemotion(const Arg *arg);
static void next_win();
static client* prev_client();
static void prev_win();
static void propertynotify(XEvent *e);
static void quit();
static void removeclient(client *c);
static void run(void);
static void save_desktop(int i);
static void select_desktop(int i);
static void setfullscreen(client *c, Bool fullscrn);
static void setup(void);
static void sigchld();
static void spawn(const Arg *arg);
static void stack(int h, int y);
static void swap_master();
static void switch_mode(const Arg *arg);
static void tile(void);
static void update_current(client *c);
static void unmapnotify(XEvent *e);
static client* wintoclient(Window w);
static int xerror(Display *dis, XErrorEvent *ee);
static int xerrorstart();

#include "config.h"

static Bool running = True;
static int previous_desktop = 0, current_desktop = 0;
static int screen, wh, ww, mode = DEFAULT_MODE;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0, win_unfocus, win_focus;
static Display *dis;
static Window root;
static client *head, *prevfocus, *current;
static Atom wmatoms[WM_COUNT], netatoms[NET_COUNT];
static desktop desktops[DESKTOPS];

/* events array - on new event, call the appropriate handling function */
static void (*events[LASTEvent])(XEvent *e) = {
    [KeyPress]         = keypress,     [EnterNotify]    = enternotify,
    [MapRequest]       = maprequest,   [ClientMessage]  = clientmessage,
    [ButtonPress]      = buttonpress,  [DestroyNotify]  = destroynotify,
    [UnmapNotify]      = unmapnotify,  [PropertyNotify] = propertynotify,
    [ConfigureRequest] = configurerequest,
};

/* layout array - given the current layout mode, tile the windows
 * h (or hh) - avaible height that windows have to expand
 * y (or cy) - offset from top to place the windows (reserved by the panel) */
static void (*layout[MODES])(int h, int y) = {
    [TILE] = stack, [BSTACK] = stack, [GRID] = grid, [MONOCLE] = monocle,
};

/* create a new client and add the new window
 * window should notify of property change events */
client* addwindow(Window w) {
    client *c, *t = prev_client(head);
    if (!(c = (client *)calloc(1, sizeof(client)))) err(EXIT_FAILURE, "cannot allocate client");

    if (!head) head = c;
    else if (!ATTACH_ASIDE) { c->next = head; head = c; }
    else if (t) t->next = c; else head->next = c;

    XSelectInput(dis, (c->win = w), PropertyChangeMask|(FOLLOW_MOUSE?EnterWindowMask:0));
    return c;
}

/* on the press of a button check to see if there's a binded function to call */
void buttonpress(XEvent *e) {
    client *c = wintoclient(e->xbutton.window);
    if (!c) return;
    if (CLICK_TO_FOCUS && current != c && e->xbutton.button == Button1) update_current(c);

    for (unsigned int i=0; i<LENGTH(buttons); i++)
        if (buttons[i].func && buttons[i].button == e->xbutton.button &&
            CLEANMASK(buttons[i].mask) == CLEANMASK(e->xbutton.state)) {
            if (current != c) update_current(c);
            buttons[i].func(&(buttons[i].arg));
        }
}

/* focus another desktop
 *
 * to avoid flickering
 * first map the new windows
 * first the current window and then all other
 * then unmap the old windows
 * first all others then the current */
void change_desktop(const Arg *arg) {
    if (arg->i == current_desktop) return;
    previous_desktop = current_desktop;
    select_desktop(arg->i);
    if (current) XMapWindow(dis, current->win);
    for (client *c=head; c; c=c->next) XMapWindow(dis, c->win);
    select_desktop(previous_desktop);
    for (client *c=head; c; c=c->next) if (c != current) XUnmapWindow(dis, c->win);
    if (current) XUnmapWindow(dis, current->win);
    select_desktop(arg->i);
    tile(); update_current(current);
    desktopinfo();
}

/* remove all windows in all desktops by sending a delete message */
void cleanup(void) {
    Window root_return, parent_return, *children;
    unsigned int nchildren;

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    XQueryTree(dis, root, &root_return, &parent_return, &children, &nchildren);
    for (unsigned int i = 0; i<nchildren; i++) deletewindow(children[i]);
    if (children) XFree(children);
    XSync(dis, False);
    XSetInputFocus(dis, PointerRoot, RevertToPointerRoot, CurrentTime);
}

/* move a client to another desktop
 *
 * remove the current client from the current desktop's client list
 * and add it as last client of the new desktop's client list */
void client_to_desktop(const Arg *arg) {
    if (!current || arg->i == current_desktop) return;
    int cd = current_desktop;
    client *p = prev_client(current), *c = current;

    select_desktop(arg->i);
    client *l = prev_client(head);
    update_current(l ? (l->next = c):head ? (head->next = c):(head = c));

    select_desktop(cd);
    if (c == head || !p) head = c->next; else p->next = c->next;
    c->next = NULL;
    XUnmapWindow(dis, c->win);
    update_current(prevfocus);

    if (FOLLOW_WINDOW) change_desktop(arg); else tile();
    desktopinfo();
}

/* To change the state of a mapped window, a client MUST
 * send a _NET_WM_STATE client message to the root window
 * message_type must be _NET_WM_STATE
 *   data.l[0] is the action to be taken
 *   data.l[1] is the property to alter three actions:
 *   - remove/unset _NET_WM_STATE_REMOVE=0
 *   - add/set _NET_WM_STATE_ADD=1,
 *   - toggle _NET_WM_STATE_TOGGLE=2
 *
 * check if window requested fullscreen or activation */
void clientmessage(XEvent *e) {
    client *t = NULL, *c = wintoclient(e->xclient.window);
    if (c && e->xclient.message_type         == netatoms[NET_WM_STATE]
          && ((unsigned)e->xclient.data.l[1] == netatoms[NET_FULLSCREEN]
           || (unsigned)e->xclient.data.l[2] == netatoms[NET_FULLSCREEN]))
        setfullscreen(c, (e->xclient.data.l[0] == 1 || (e->xclient.data.l[0] == 2 && !c->isfullscrn)));
    else if (c && e->xclient.message_type == netatoms[NET_ACTIVE]) for (t=head; t && t!=c; t=t->next);
    if (t) update_current(c);
    tile();
}

/* a configure request means that the window requested changes in its geometry
 * state. if the window is fullscreen discard and fill the screen else set the
 * appropriate values as requested, and tile the window again so that it fills
 * the gaps that otherwise could have been created */
void configurerequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    client *c = wintoclient(ev->window);
    if (!c || !c->isfullscrn) {
        XConfigureWindow(dis, ev->window, ev->value_mask, &(XWindowChanges){ev->x,
            ev->y, ev->width, ev->height, ev->border_width, ev->above, ev->detail});
        XSync(dis, False);
    } else setfullscreen(c, True);
    tile();
}

/* close the window */
void deletewindow(Window w) {
    XEvent ev;
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = wmatoms[WM_PROTOCOLS];
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = wmatoms[WM_DELETE_WINDOW];
    ev.xclient.data.l[1] = CurrentTime;
    XSendEvent(dis, w, False, NoEventMask, &ev);
}

/* output info about the desktops on standard output stream
 *
 * the info is a list of ':' separated values for each desktop
 * desktop to desktop info is separated by ' ' single spaces
 * the info values are
 *   the desktop number/id
 *   the desktop's client count
 *   the desktop's tiling layout mode/id
 *   whether the desktop is the current focused (1) or not (0)
 *   whether any client in that desktop has received an urgent hint
 *
 * once the info is collected, immediately flush the stream */
void desktopinfo(void) {
    Bool urgent = False;
    int cd = current_desktop, n=0, d=0;
    for (client *c; d<DESKTOPS; d++) {
        for (select_desktop(d), c=head, n=0, urgent=False; c; c=c->next, ++n) if (c->isurgent) urgent = True;
        fprintf(stdout, "%d:%d:%d:%d:%d%c", d, n, mode, current_desktop == cd, urgent, d+1==DESKTOPS?'\n':' ');
    }
    fflush(stdout);
    if (cd != d-1) select_desktop(cd);
}

/* a destroy notification is received when a window is being closed
 * on receival, remove the appropriate client that held that window */
void destroynotify(XEvent *e) {
    client *c = wintoclient(e->xdestroywindow.window);
    if (c) removeclient(c);
    desktopinfo();
}

/* when the mouse enters a window's borders
 * the window, if notifying of such events (EnterWindowMask)
 * will notify the wm and will get focus */
void enternotify(XEvent *e) {
    if (!FOLLOW_MOUSE) return;
    client *c = wintoclient(e->xcrossing.window);
    if (c && e->xcrossing.mode   == NotifyNormal
          && e->xcrossing.detail != NotifyInferior) update_current(c);
}

/* get a pixel with the requested color
 * to fill some window area - borders */
unsigned long getcolor(const char* color) {
    XColor c; Colormap map = DefaultColormap(dis, screen);
    if (!XAllocNamedColor(dis, map, color, &c, &c)) err(EXIT_FAILURE, "cannot allocate color");
    return c.pixel;
}

/* set the given client to listen to button events (presses / releases) */
void grabbuttons(client *c) {
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    for (unsigned int b=0; b<LENGTH(buttons); b++)
        for (unsigned int m=0; m<LENGTH(modifiers); m++)
            XGrabButton(dis, buttons[b].button, buttons[b].mask|modifiers[m], c->win,
                        False, BUTTONMASK, GrabModeAsync, GrabModeAsync, None, None);
}

/* the wm should listen to key presses */
void grabkeys(void) {
    KeyCode code;
    XUngrabKey(dis, AnyKey, AnyModifier, root);

    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    for (unsigned int k=0; k<LENGTH(keys); k++)
        if ((code = XKeysymToKeycode(dis, keys[k].keysym)))
            for (unsigned int m=0; m<LENGTH(modifiers); m++)
                XGrabKey(dis, code, keys[k].mod|modifiers[m], root, True, GrabModeAsync, GrabModeAsync);
}

/* arrange windows in a grid */
void grid(int hh, int cy) {
    int n = 0, cols = 0, cn = 0, rn = 0, i = -1;
    for (client *c = head; c; c=c->next) if (!ISFFT(c)) ++n;
    for (cols=0; cols <= n/2; cols++) if (cols*cols >= n) break; /* emulate square root */
    if (n == 5) cols = 2;

    int rows = n/cols, ch = hh - BORDER_WIDTH, cw = (ww - BORDER_WIDTH)/(cols?cols:1);
    for (client *c=head; c; c=c->next) {
        if (ISFFT(c)) continue; else ++i;
        if (i/rows + 1 > cols - n%cols) rows = n/cols + 1;
        XMoveResizeWindow(dis, c->win, cn*cw, cy + rn*ch/rows, cw - BORDER_WIDTH, ch/rows - BORDER_WIDTH);
        if (++rn >= rows) { rn = 0; cn++; }
    }
}

/* on the press of a key check to see if there's a binded function to call */
void keypress(XEvent *e) {
    KeySym keysym = XkbKeycodeToKeysym(dis, e->xkey.keycode, 0, 0);
    for (unsigned int i=0; i<LENGTH(keys); i++)
        if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(e->xkey.state)
                   && keys[i].func) keys[i].func(&keys[i].arg);
}

/* explicitly kill a client - close the highlighted window
 * send a delete message and remove the client */
void killclient() {
    if (!current) return;
    Atom *prot; int n = -1;
    if (XGetWMProtocols(dis, current->win, &prot, &n)) while(!--n<0 && prot[n] != wmatoms[WM_DELETE_WINDOW]);
    if (n < 0) XKillClient(dis, current->win); else deletewindow(current->win);
    removeclient(current);
}

/* a map request is received when a window wants to display itself
 * if the window has override_redirect flag set then it should not be handled
 * by the wm. if the window already has a client then there is nothing to do.
 *
 * get the window class and name instance and try to match against an app rule.
 * create a client for the window, that client will always be current.
 * check for transient state, and fullscreen state and the appropriate values.
 * if the desktop in which the window was spawned is the current desktop then
 * display the window, else, if set, focus the new desktop. */
void maprequest(XEvent *e) {
    static XWindowAttributes wa; Window w;
    if (XGetWindowAttributes(dis, e->xmaprequest.window, &wa) && wa.override_redirect) return;
    if (wintoclient(e->xmaprequest.window)) return;

    Bool follow = False, floating = False;
    int cd = current_desktop, newdsk = current_desktop;
    XClassHint ch = {0, 0};
    if (XGetClassHint(dis, e->xmaprequest.window, &ch))
        for (unsigned int i=0; i<LENGTH(rules); i++)
            if (strstr(ch.res_class, rules[i].class) || strstr(ch.res_name, rules[i].class)) {
                follow = rules[i].follow;
                newdsk = (rules[i].desktop < 0) ? current_desktop:rules[i].desktop;
                floating = rules[i].floating;
                break;
            }
    if (ch.res_class) XFree(ch.res_class);
    if (ch.res_name) XFree(ch.res_name);

    if (cd != newdsk) select_desktop(newdsk);
    client *c = addwindow(e->xmaprequest.window);
    c->istransient = XGetTransientForHint(dis, c->win, &w);
    c->isfloating = floating || c->istransient;

    int di; unsigned long dl; unsigned char *state = NULL; Atom da;
    if (XGetWindowProperty(dis, c->win, netatoms[NET_WM_STATE], 0L, sizeof da,
              False, XA_ATOM, &da, &di, &dl, &dl, &state) == Success && state)
        setfullscreen(c, (*(Atom *)state == netatoms[NET_FULLSCREEN]));
    if (state) XFree(state);

    if (cd != newdsk) select_desktop(cd);
    if (cd == newdsk) { tile(); XMapWindow(dis, c->win); update_current(c); }
    else if (follow) { change_desktop(&(Arg){.i = newdsk}); update_current(c); }
    grabbuttons(c);

    desktopinfo();
}

/* grab the pointer and get it's current position
 * all pointer movement events will be reported until it's ungrabbed
 * until the mouse button has not been released,
 * grab the interesting events - button press/release and pointer motion
 * and on on pointer movement resize or move the window under the curson.
 * if the received event is a map request or a configure request call the
 * appropriate handler, and stop listening for other events.
 * Ungrab the poitner and event handling is passed back to run() function.
 * Once a window has been moved or resized, it's marked as floating. */
void mousemotion(const Arg *arg) {
    if (!current) return;
    static XWindowAttributes wa;
    XGetWindowAttributes(dis, current->win, &wa);

    if (XGrabPointer(dis, root, False, BUTTONMASK|PointerMotionMask, GrabModeAsync,
                     GrabModeAsync, None, None, CurrentTime) != GrabSuccess) return;
    int x, y, z, xw, yh; unsigned int v; Window w;
    XWarpPointer(dis, None, current->win, 0, 0, 0, 0, wa.width, wa.height);
    XQueryPointer(dis, root, &w, &w, &x, &y, &z, &z, &v);

    if (current->isfullscrn) setfullscreen(current, False);
    if (!current->isfloating) current->isfloating = True;
    tile(); update_current(current);

    XEvent ev;
    do {
        XMaskEvent(dis, BUTTONMASK|PointerMotionMask|SubstructureRedirectMask, &ev);
        switch (ev.type) {
            case ConfigureRequest: case MapRequest:
                events[ev.type](&ev);
                break;
            case MotionNotify:
                xw = (arg->i == MOVE ? wa.x:wa.width)  + ev.xmotion.x - x;
                yh = (arg->i == MOVE ? wa.y:wa.height) + ev.xmotion.y - y;
                if (arg->i == RESIZE) XResizeWindow(dis, current->win,
                   xw>MINWSZ ? xw:wa.width, yh>MINWSZ ? yh:wa.height);
                else if (arg->i == MOVE) XMoveWindow(dis, current->win, xw, yh);
                break;
        }
    } while(ev.type != ButtonRelease);
    XUngrabPointer(dis, CurrentTime);
}

/* each window should cover all the available screen space */
void monocle(int hh, int cy) {
    for (client *c=head; c; c=c->next) if (!ISFFT(c)) XMoveResizeWindow(dis, c->win, 0, cy, ww, hh);
}

/* move the current client, to current->next
 * and current->next to current client's position */
void move_down() {
    /* p is previous, c is current, n is next, if current is head n is last */
    client *p = NULL, *n = (current->next) ? current->next:head;
    if (!(p = prev_client(current))) return;
    /*
     * if c is head, swapping with n should update head to n
     * [c]->[n]->..  ==>  [n]->[c]->..
     *  ^head              ^head
     *
     * else there is a previous client and p->next should be what's after c
     * ..->[p]->[c]->[n]->..  ==>  ..->[p]->[n]->[c]->..
     */
    if (current == head) head = n; else p->next = current->next;
    /*
     * if c is the last client, c will be the current head
     * [n]->..->[p]->[c]->NULL  ==>  [c]->[n]->..->[p]->NULL
     *  ^head                         ^head
     * else c will take the place of n, so c-next will be n->next
     * ..->[p]->[c]->[n]->..  ==>  ..->[p]->[n]->[c]->..
     */
    current->next = (current->next) ? n->next:n;
    /*
     * if c was swapped with n then they now point to the same ->next. n->next should be c
     * ..->[p]->[c]->[n]->..  ==>  ..->[p]->[n]->..  ==>  ..->[p]->[n]->[c]->..
     *                                        [c]-^
     *
     * else c is the last client and n is head,
     * so c will be move to be head, no need to update n->next
     * [n]->..->[p]->[c]->NULL  ==>  [c]->[n]->..->[p]->NULL
     *  ^head                         ^head
     */
    if (current->next == n->next) n->next = current; else head = current;
    tile();
}

/* move the current client, to the previous from current and
 * the previous from  current to current client's position */
void move_up() {
    client *pp = NULL, *p;
    /* p is previous from current or last if current is head */
    if (!(p = prev_client(current))) return;
    /* pp is previous from p, or null if current is head and thus p is last */
    if (p->next) for (pp=head; pp && pp->next != p; pp=pp->next);
    /*
     * if p has a previous client then the next client should be current (current is c)
     * ..->[pp]->[p]->[c]->..  ==>  ..->[pp]->[c]->[p]->..
     *
     * if p doesn't have a previous client, then p might be head, so head must change to c
     * [p]->[c]->..  ==>  [c]->[p]->..
     *  ^head              ^head
     * if p is not head, then c is head (and p is last), so the new head is next of c
     * [c]->[n]->..->[p]->NULL  ==>  [n]->..->[p]->[c]->NULL
     *  ^head         ^last           ^head         ^last
     */
    if (pp) pp->next = current; else head = (current == head) ? current->next:current;
    /*
     * next of p should be next of c
     * ..->[pp]->[p]->[c]->[n]->..  ==>  ..->[pp]->[c]->[p]->[n]->..
     * except if c was head (now c->next is head), so next of p should be c
     * [c]->[n]->..->[p]->NULL  ==>  [n]->..->[p]->[c]->NULL
     *  ^head         ^last           ^head         ^last
     */
    p->next = (current->next == head) ? current:current->next;
    /*
     * next of c should be p
     * ..->[pp]->[p]->[c]->[n]->..  ==>  ..->[pp]->[c]->[p]->[n]->..
     * except if c was head (now c->next is head), so c is must be last
     * [c]->[n]->..->[p]->NULL  ==>  [n]->..->[p]->[c]->NULL
     *  ^head         ^last           ^head         ^last
     */
    current->next = (current->next == head) ? NULL:p;
    tile();
}

/* cyclic focus the next window
 * if the window is the last on stack, focus head */
void next_win() {
    if (!current || !head->next) return;
    update_current(current->next ? current->next:head);
}

/* get the previous client from the given
 * if no such client, return NULL */
client* prev_client(client *c) {
    if (!c || !head->next) return NULL;
    client *p; for (p=head; p->next && p->next != c; p=p->next);
    return p;
}

/* cyclic focus the previous window
 * if the window is the head, focus the last stack window */
void prev_win() {
    if (!current || !head->next) return;
    update_current(prev_client(prevfocus = current));
}

/* property notify is called when one of the window's properties
 * is changed, such as an urgent hint is received */
void propertynotify(XEvent *e) {
    client *c = wintoclient(e->xproperty.window);
    if (!c || e->xproperty.atom != XA_WM_HINTS) return;
    XWMHints *wmh = XGetWMHints(dis, c->win);
    c->isurgent = c != current && wmh && (wmh->flags & XUrgencyHint);
    XFree(wmh);
    desktopinfo();
}

/* to quit just stop receiving events
 * run() is stopped and control is back to main() */
void quit() {
    running = False;
}

/* remove the specified client
 *
 * note, the removing client can be on any desktop,
 * we must return back to the current focused desktop.
 * if c was the previously focused, prevfocus must be updated
 * else if c was the current one, current must be updated. */
void removeclient(client *c) {
    client **p = NULL;
    int nd = 0, cd = current_desktop;
    for (Bool found = False; nd<DESKTOPS && !found; nd++)
        for (select_desktop(nd), p = &head; *p && !(found = *p == c); p = &(*p)->next);
    *p = c->next;
    if (c == prevfocus) prevfocus = prev_client(current);
    if (c == current || !head->next) update_current(prevfocus);
    free(c); c = NULL;
    if (cd == nd -1) tile(); else select_desktop(cd);
}

/* main event loop - on receival of an event call the appropriate event handler */
void run(void) {
    XEvent ev;
    while(running && !XNextEvent(dis, &ev)) if (events[ev.type]) events[ev.type](&ev);
}

/* save specified desktop's properties */
void save_desktop(int i) {
    if (i < 0 || i >= DESKTOPS) return;
    desktops[i].mode        = mode;
    desktops[i].head        = head;
    desktops[i].current     = current;
    desktops[i].prevfocus   = prevfocus;
}

/* set the specified desktop's properties */
void select_desktop(int i) {
    if (i < 0 || i >= DESKTOPS) return;
    save_desktop(current_desktop);
    mode            = desktops[i].mode;
    head            = desktops[i].head;
    current         = desktops[i].current;
    prevfocus       = desktops[i].prevfocus;
    current_desktop = i;
}

/* set or unset fullscreen state of client */
void setfullscreen(client *c, Bool fullscrn) {
    if (fullscrn != c->isfullscrn) XChangeProperty(dis, c->win,
            netatoms[NET_WM_STATE], XA_ATOM, 32, PropModeReplace, (unsigned char*)
            ((c->isfullscrn = fullscrn) ? &netatoms[NET_FULLSCREEN]:0), fullscrn);
    if (fullscrn) XMoveResizeWindow(dis, c->win, 0, 0, ww, wh + PANEL_HEIGHT);
    XConfigureWindow(dis, c->win, CWBorderWidth, &(XWindowChanges){0,0,0,0,fullscrn?0:BORDER_WIDTH,0,0});
}

/* set initial values
 * root window - screen height/width - atoms - xerror handler
 * set masks for reporting events handled by the wm
 * and propagate the suported net atoms */
void setup(void) {
    sigchld();

    screen = DefaultScreen(dis);
    root = RootWindow(dis, screen);

    ww = XDisplayWidth(dis,  screen);
    wh = XDisplayHeight(dis, screen) - PANEL_HEIGHT;
    for (unsigned int i=0; i<DESKTOPS; i++) save_desktop(i);

    win_focus = getcolor(FOCUS);
    win_unfocus = getcolor(UNFOCUS);

    XModifierKeymap *modmap = XGetModifierMapping(dis);
    for (int k=0; k<8; k++) for (int j=0; j<modmap->max_keypermod; j++)
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
    XSelectInput(dis, DefaultRootWindow(dis), SubstructureRedirectMask|ButtonPressMask|
                                              SubstructureNotifyMask|PropertyChangeMask);
    XSync(dis, False);

    XSetErrorHandler(xerror);
    XSync(dis, False);
    XChangeProperty(dis, root, netatoms[NET_SUPPORTED], XA_ATOM, 32,
              PropModeReplace, (unsigned char *)netatoms, NET_COUNT);

    grabkeys();
}

void sigchld() {
    if (signal(SIGCHLD, sigchld) == SIG_ERR)
        err(EXIT_FAILURE, "cannot install SIGCHLD handler");
    while(0 < waitpid(-1, NULL, WNOHANG));
}

/* execute a command */
void spawn(const Arg *arg) {
    if (fork()) return;
    if (dis) close(ConnectionNumber(dis));
    setsid();
    execvp((char*)arg->com[0], (char**)arg->com);
    fprintf(stderr, "error: execvp %s", (char *)arg->com[0]);
    perror(" failed"); /* also prints the err msg */
    exit(EXIT_SUCCESS);
}

/* arrange windows in normal or bottom stack tile */
void stack(int hh, int cy) {
    client *c = NULL, *t = NULL; Bool b = mode == BSTACK;
    int n = 0, d = 0, z = b ? ww:hh, ma = (b ? hh:ww) * MASTER_SIZE;

    /* count stack windows and grab first non-floating, non-fullscreen window */
    for (t = head; t; t=t->next) if (!ISFFT(t)) { if (c) ++n; else c = t; }

    /* if there is only one window, it should cover the available screen space
     * if there is only one stack window (n == 1) then we don't care about the adjustments
     * if more than one stack windows (n > 1) on screen then adjustments may be needed
     *   - d is the num of pixels than remain when spliting
     *   the available width/height to the number of windows
     *   - z is the clients' height/width */
    if (!c) return; else if (!n) {
        XMoveResizeWindow(dis, c->win, 0, cy, ww - 2*BORDER_WIDTH, hh - 2*BORDER_WIDTH);
        return;
    } else if (n > 1) { d = z%n; z /= n; }

    /* tile the first non-floating, non-fullscreen window to cover the master area */
    if (b) XMoveResizeWindow(dis, c->win, 0, cy, ww - 2*BORDER_WIDTH, ma - BORDER_WIDTH);
    else   XMoveResizeWindow(dis, c->win, 0, cy, ma - BORDER_WIDTH, hh - 2*BORDER_WIDTH);

    int cx = b ? 0:ma, cw = (b ? hh:ww) - 2*BORDER_WIDTH - ma, ch = z - BORDER_WIDTH;

    for (cy += b ? ma:0, c=c->next; c; c=c->next) {
        if (ISFFT(c)) continue;
        for (t=c->next; t && ISFFT(t); t=t->next);
        if (!t) ch += d - BORDER_WIDTH; /* add remaining space to last window */
        if (b) { XMoveResizeWindow(dis, c->win, cx, cy, ch, cw); cx += z; }
        else   { XMoveResizeWindow(dis, c->win, cx, cy, cw, ch); cy += z; }
    }
}

/* swap master window with current or
 * if current is head swap with next
 * if current is not head, then head
 * is behind us, so move_up until we
 * are the head */
void swap_master() {
    if (!current || !head->next) return;
    if (current == head) move_down();
    else while (current != head) move_up();
    update_current(head);
}

/* switch the tiling mode and reset all floating windows */
void switch_mode(const Arg *arg) {
    if (mode == arg->i) for (client *c=head; c; c=c->next) c->isfloating = False;
    mode = arg->i;
    tile(); update_current(current);
    desktopinfo();
}

/* tile all windows of current desktop - call the handler tiling function */
void tile(void) {
    if (!head) return; /* nothing to arange */
    layout[head->next ? mode : MONOCLE](wh, TOP_PANEL ? PANEL_HEIGHT : 0);
}

/* windows that request to unmap should lose their
 * client, so no invisible windows exist on screen */
void unmapnotify(XEvent *e) {
    client *c = wintoclient(e->xunmap.window);
    if (c && e->xunmap.send_event) removeclient(c);
    desktopinfo();
}

/* highlight borders and set active window and input focus
 * if given current is NULL then delete the active window property
 *
 * stack order by client properties, top to bottom:
 *  - current when floating or transient
 *  - floating or trancient windows
 *  - current when tiled
 *  - current when fullscreen
 *  - fullscreen windows
 *  - tiled windows
 *
 * a window should have borders in any case, except if
 *  - the window is the only window on screen
 *  - the window is fullscreen
 *  - the mode is MONOCLE and the window is not floating or transient */
void update_current(client *c) {
    if (!head) {
        XDeleteProperty(dis, root, netatoms[NET_ACTIVE]);
        current = prevfocus = NULL;
        return;
    } else if (c == prevfocus) { prevfocus = prev_client(current = prevfocus ? prevfocus:head);
    } else if (c != current) { prevfocus = current; current = c; }

    /* num of n:all fl:fullscreen ft:floating/transient windows */
    int n = 0, fl = 0, ft = 0;
    for (c = head; c; c = c->next, ++n) if (ISFFT(c)) { fl++; if (!c->isfullscrn) ft++; }
    Window w[n];
    w[(current->isfloating||current->istransient) ? 0 : ft] = current->win;
    for (fl += !ISFFT(current) ? 1:0, c = head; c; c = c->next) {
        XSetWindowBorder(dis, c->win, c == current ? win_focus:win_unfocus);
        XSetWindowBorderWidth(dis, c->win, (!head->next || c->isfullscrn
                    || (mode == MONOCLE && !ISFFT(c))) ? 0:BORDER_WIDTH);
        if (c != current) w[c->isfullscrn ? --fl : ISFFT(c) ? --ft : --n] = c->win;
    }
    XRestackWindows(dis, w, LENGTH(w));

    XSetInputFocus(dis, current->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dis, root, netatoms[NET_ACTIVE], XA_WINDOW, 32,
                PropModeReplace, (unsigned char *)&current->win, 1);
    if (CLICK_TO_FOCUS) XUngrabButton(dis, Button1, None, current->win);

    XSync(dis, False);
}

/* find to which client the given window belongs to */
client* wintoclient(Window w) {
    client *c = NULL;
    int d = 0, cd = current_desktop;
    for (Bool found = False; d<DESKTOPS && !found; ++d)
        for (select_desktop(d), c=head; c && !(found = (w == c->win)); c=c->next);
    if (cd != d-1) select_desktop(cd);
    return c;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit through xerrorlib. */
int xerror(Display *dis, XErrorEvent *ee) {
    if (ee->error_code == BadWindow   || (ee->error_code == BadAccess && ee->request_code == X_GrabKey)
    || (ee->error_code == BadMatch    && (ee->request_code == X_SetInputFocus
                                      ||  ee->request_code == X_ConfigureWindow))
    || (ee->error_code == BadDrawable && (ee->request_code == X_PolyFillRectangle
    || ee->request_code == X_CopyArea ||  ee->request_code == X_PolySegment
                                      ||  ee->request_code == X_PolyText8))) return 0;
    fprintf(stderr, "error: xerror: request code: %d, error code: %d\n", ee->request_code, ee->error_code);
    return xerrorxlib(dis, ee);
}

int xerrorstart() {
    err(EXIT_FAILURE, "another window manager is already running");
}

int main(int argc, char *argv[]) {
    if (argc == 2 && argv[1][0] == '-') switch (argv[1][1]) {
        case 'v': errx(EXIT_SUCCESS, "%s - by c00kiemon5ter >:3 omnomnomnom", VERSION);
        case 'h': errx(EXIT_SUCCESS, "%s", USAGE);
        default: errx(EXIT_FAILURE, "%s", USAGE);
    } else if (argc != 1) errx(EXIT_FAILURE, "%s", USAGE);
    if (!(dis = XOpenDisplay(NULL))) errx(EXIT_FAILURE, "cannot open display");
    setup();
    desktopinfo(); /* zero out every desktop on (re)start */
    run();
    cleanup();
    XCloseDisplay(dis);
    return 0;
}
