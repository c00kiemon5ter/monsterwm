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

enum { PREV = -1, NEXT = 1, RESIZE, MOVE };
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
    unsigned int mask;
    unsigned int button;
    void (*func)(const Arg *);
    const Arg arg;
} Button;

/* a client is a wrapper to a window that additionally
 * holds some properties for that window
 *
 * next        - the client after this one, or NULL if the current is the only or last client
 * isurgent    - the window received an urgent hint
 * istransient - the window is transient
 * isfullscrn  - the window is fullscreen
 * isfloating  - the window is floating
 * win         - the window
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
 * master_size  - the size of the master window
 * mode         - the desktop's tiling layout mode
 * growth       - growth factor of the first stack window
 * head         - the start of the client list
 * current      - the currently highlighted window
 * prevfocus    - the client that previously had focus
 * showpanel    - the visibility status of the panel
 */
typedef struct {
    int master_size, mode, growth;
    client *head, *current, *prevfocus;
    Bool showpanel;
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
    const Bool follow;
    const Bool floating;
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
static void die(const char* errstr, ...);
static void enternotify(XEvent *e);
static void focusurgent();
static unsigned long getcolor(const char* color);
static void grabbuttons(client *c);
static void grabkeys(void);
static void grid(int h, int y);
static void keypress(XEvent *e);
static void killclient();
static void last_desktop();
static void maprequest(XEvent *e);
static void monocle(int h, int y);
static void move_down();
static void move_up();
static void mousemotion(const Arg *arg);
static void next_win();
static void prev_win();
static void propertynotify(XEvent *e);
static void quit(const Arg *arg);
static void removeclient(client *c);
static void resize_master(const Arg *arg);
static void resize_stack(const Arg *arg);
static void rotate(const Arg *arg);
static void rotate_filled(const Arg *arg);
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
static void togglepanel();
static void update_current(client *c);
static void unmapnotify(XEvent *e);
static client* wintoclient(Window w);
static int xerror(Display *dis, XErrorEvent *ee);
static int xerrorstart();

#include "config.h"

static Bool running = True;
static Bool showpanel = SHOW_PANEL;
static int retval = 0;
static int previous_desktop = 0, current_desktop = 0;
static int mode = DEFAULT_MODE;
static int master_size, growth = 0;
static int wh, ww; /* window area width/height - screen height minus the panel height */
static int screen;
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int win_unfocus, win_focus;
static unsigned int numlockmask = 0; /* dynamic key lock mask */
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
 * h (or hh) is the avaible height that windows have to expand
 * y (or cy) is the num of pixels from top to place the windows (y coordinate)
 */
static void (*layout[MODES])(int h, int y) = {
    [TILE] = stack, [BSTACK]  = stack,
    [GRID] = grid,  [MONOCLE] = monocle,
};

/* create a new client and add the new window
 * window should notify of property change events
 */
client* addwindow(Window w) {
    client *c, *t;
    if (!(c = (client *)calloc(1, sizeof(client))))
        die("error: could not calloc() %u bytes\n", sizeof(client));

    if (!head) head = c;
    else if (ATTACH_ASIDE) {
        for(t=head; t->next; t=t->next); /* get the last client */
        t->next = c;
    } else {
        c->next = head;
        head = c;
    }

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
            update_current(c);
            buttons[i].func(&(buttons[i].arg));
        }
}

/* focus another desktop
 * to avoid flickering
 * first map the new windows
 * if the layout mode is fullscreen map only one window
 * then unmap previous windows
 */
void change_desktop(const Arg *arg) {
    if (arg->i == current_desktop) return;
    previous_desktop = current_desktop;
    select_desktop(arg->i);
    tile();
    if (mode == MONOCLE && current) XMapWindow(dis, current->win);
    else for (client *c=head; c; c=c->next) XMapWindow(dis, c->win);
    update_current(current);
    select_desktop(previous_desktop);
    for (client *c=head; c; c=c->next) XUnmapWindow(dis, c->win);
    select_desktop(arg->i);
    desktopinfo();
}

/* remove all windows in all desktops by sending a delete message */
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

/* move a client to another desktop
 * store the client's window
 * remove the client
 * add the window to the new desktop
 * if defined change focus to the new desktop
 *
 * keep in mind that current pointer might
 * change with each select_desktop() invocation
 */
void client_to_desktop(const Arg *arg) {
    if (arg->i == current_desktop || !current) return;
    int cd = current_desktop;
    client *c = current;

    /* add the window to the new desktop keeping the client's properties */
    select_desktop(arg->i);
    current = addwindow(c->win);
    current->isfloating  = c->isfloating;
    current->isfullscrn  = c->isfullscrn;
    current->istransient = c->istransient;

    /* remove the window and client from the current desktop */
    select_desktop(cd);
    XUnmapWindow(dis, c->win);
    removeclient(c);
    tile();
    update_current(current);

    if (FOLLOW_WINDOW) change_desktop(arg);
    desktopinfo();
}

/* check if window requested fullscreen or activation
 * To change the state of a mapped window, a client MUST
 * send a _NET_WM_STATE client message to the root window
 * message_type must be _NET_WM_STATE
 *   data.l[0] is the action to be taken
 *   data.l[1] is the property to alter three actions:
 *     remove/unset _NET_WM_STATE_REMOVE=0
 *     add/set _NET_WM_STATE_ADD=1,
 *     toggle _NET_WM_STATE_TOGGLE=2
 */
void clientmessage(XEvent *e) {
    client *c = wintoclient(e->xclient.window);
    if (c && e->xclient.message_type         == netatoms[NET_WM_STATE]
          && ((unsigned)e->xclient.data.l[1] == netatoms[NET_FULLSCREEN]
           || (unsigned)e->xclient.data.l[2] == netatoms[NET_FULLSCREEN]))
        setfullscreen(c, (e->xclient.data.l[0] == 1 || (e->xclient.data.l[0] == 2 && !c->isfullscrn)));
    else if (c && e->xclient.message_type == netatoms[NET_ACTIVE]) current = c;
    tile();
    update_current(current);
}

/* a configure request means that the window requested changes in its geometry
 * state. if the window is fullscreen discard and fill the screen, else set the
 * appropriate values as requested, and tile the window again so that it fills
 * the gaps that otherwise could have been created
 */
void configurerequest(XEvent *e) {
    client *c = wintoclient(e->xconfigurerequest.window);
    if (c && c->isfullscrn) setfullscreen(c, True);
    else { /* keeping the window happy costs 10 lines ..fuuuu! shrink them! */
        XWindowChanges wc;                     wc.border_width = e->xconfigurerequest.border_width;
        wc.x       = e->xconfigurerequest.x;     wc.width      = e->xconfigurerequest.width;
        wc.y       = e->xconfigurerequest.y;     wc.height     = e->xconfigurerequest.height;
        wc.sibling = e->xconfigurerequest.above; wc.stack_mode = e->xconfigurerequest.detail;
        XConfigureWindow(dis, e->xconfigurerequest.window, e->xconfigurerequest.value_mask, &wc);
        XSync(dis, False);
    }
    tile();
    if (c && c == current) update_current(c);
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
 * once the info is collected, immediately flush the stream
 */
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

/* a destroy notification is received when a window is being closed
 * on receival, remove the appropriate client that held that window
 */
void destroynotify(XEvent *e) {
    client *c = wintoclient(e->xdestroywindow.window);
    if (c) removeclient(c);
    desktopinfo();
}

/* print a message on standard error stream
 * and exit with failure exit code
 */
void die(const char *errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

/* when the mouse enters a window's borders
 * the window, if notifying of such events (EnterWindowMask)
 * will notify the wm and will get focus
 */
void enternotify(XEvent *e) {
    if (!FOLLOW_MOUSE) return;
    client *c = wintoclient(e->xcrossing.window);
    if (c && e->xcrossing.mode   == NotifyNormal
          && e->xcrossing.detail != NotifyInferior) update_current(c);
}

/* find and focus the client which received
 * the urgent hint in the current desktop
 */
void focusurgent() {
    for (client *c=head; c; c=c->next) if (c->isurgent) update_current(c);
}

/* get a pixel with the requested color
 * to fill some window area - borders
 */
unsigned long getcolor(const char* color) {
    Colormap map = DefaultColormap(dis, screen);
    XColor c;

    if (!XAllocNamedColor(dis, map, color, &c, &c))
        die("error: cannot allocate color '%s'\n", c);
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
    int n = 0, cols = 0;
    for (client *c = head; c; c=c->next) if (!c->istransient && !c->isfullscrn && !c->isfloating) ++n;
    for (cols=0; cols <= n/2; cols++) if (cols*cols >= n) break; /* emulate square root */
    if (n == 5) cols = 2;

    int rows = n/cols, cn = 0, rn = 0, i = 0, ch = hh - BORDER_WIDTH, cw = (ww - BORDER_WIDTH)/(cols?cols:1);
    for (client *c=head; c; c=c->next, i++) {
        if (c->isfullscrn || c->istransient || c->isfloating) { i--; continue; }
        if (i/rows + 1 > cols - n%cols) rows = n/cols + 1;
        XMoveResizeWindow(dis, c->win, cn*cw, cy + rn*ch/rows, cw - BORDER_WIDTH, ch/rows - BORDER_WIDTH);
        if (++rn >= rows) { rn = 0; cn++; }
    }
}

/* on the press of a key check to see if there's a binded function to call */
void keypress(XEvent *e) {
    KeySym keysym;
    keysym = XKeycodeToKeysym(dis, (KeyCode)e->xkey.keycode, 0);

    for (unsigned int i=0; i<LENGTH(keys); i++)
        if (keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(e->xkey.state) && keys[i].func)
            keys[i].func(&keys[i].arg);
}

/* explicitly kill a client - close the highlighted window
 * send a delete message and remove the client
 */
void killclient() {
    if (!current) return;
    Atom *protocols; int n = 0;
    if (XGetWMProtocols(dis, current->win, &protocols, &n)) while(n-- && protocols[n] != wmatoms[WM_DELETE_WINDOW]);
    if (n) deletewindow(current->win);
    else XKillClient(dis, current->win);
    removeclient(current);
}

/* focus the previously focused desktop */
void last_desktop() {
    change_desktop(&(Arg){.i = previous_desktop});
}

/* a map request is received when a window wants to display itself
 * if the window has override_redirect flag set then it should not be handled
 * by the wm. if the window already has a client then there is nothing to do.
 *
 * get the window class and name instance and try to match against an app rule.
 * create a client for the window, that client will always be current.
 * check for transient state, and fullscreen state and the appropriate values.
 * if the desktop in which the window was spawned is the current desktop then
 * display the window, else, if set, focus the new desktop.
 */
void maprequest(XEvent *e) {
    static XWindowAttributes wa;
    if (XGetWindowAttributes(dis, e->xmaprequest.window, &wa) && wa.override_redirect) return;
    if (wintoclient(e->xmaprequest.window)) return;

    Bool follow = False, floating = False;
    int cd = current_desktop, newdsk = current_desktop;
    XClassHint ch = {0, 0};
    if (XGetClassHint(dis, e->xmaprequest.window, &ch))
        for (unsigned int i=0; i<LENGTH(rules); i++)
            if (!strcmp(ch.res_class, rules[i].class) || !strcmp(ch.res_name, rules[i].class)) {
                follow = rules[i].follow;
                newdsk = (rules[i].desktop < 0) ? current_desktop : rules[i].desktop;
                floating = rules[i].floating;
                break;
            }
    if (ch.res_class) XFree(ch.res_class);
    if (ch.res_name) XFree(ch.res_name);

    select_desktop(newdsk);
    prevfocus = current;
    current = addwindow(e->xmaprequest.window);

    Window w;
    current->istransient = XGetTransientForHint(dis, current->win, &w);
    current->isfloating = floating || current->istransient;

    int di; unsigned long dl; unsigned char *state = NULL; Atom da;
    if (XGetWindowProperty(dis, current->win, netatoms[NET_WM_STATE], 0L, sizeof da,
                    False, XA_ATOM, &da, &di, &dl, &dl, &state) == Success && state)
        setfullscreen(current, (*(Atom *)state == netatoms[NET_FULLSCREEN]));
    if (state) XFree(state);

    select_desktop(cd);
    if (cd == newdsk) {
        tile();
        XMapWindow(dis, current->win);
        update_current(current);
        grabbuttons(current);
    } else if (follow) change_desktop(&(Arg){.i = newdsk});
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
 * Once a window has been moved or resized, it's marked as floating.
 */
void mousemotion(const Arg *arg) {
    if (!current) return;
    static XWindowAttributes wa;
    XGetWindowAttributes(dis, current->win, &wa);

    if (XGrabPointer(dis, root, False, BUTTONMASK|PointerMotionMask, GrabModeAsync,
                     GrabModeAsync, None, None, CurrentTime) != GrabSuccess) return;
    int x, y, z, xw, yh; unsigned int v; Window w;
    XWarpPointer(dis, None, current->win, 0, 0, 0, 0, wa.width, wa.height);
    XQueryPointer(dis, root, &w, &w, &x, &y, &z, &z, &v);

    XEvent ev;
    do {
        XMaskEvent(dis, BUTTONMASK|PointerMotionMask|SubstructureRedirectMask, &ev);
        switch (ev.type) {
            case ConfigureRequest: case MapRequest:
                events[ev.type](&ev);
                break;
            case MotionNotify:
                xw = (arg->i == MOVE ? wa.x : wa.width)  + ev.xmotion.x - x;
                yh = (arg->i == MOVE ? wa.y : wa.height) + ev.xmotion.y - y;
                if (arg->i == RESIZE) XResizeWindow(dis, current->win,
                        xw>MINWSZ?xw:wa.width, yh>MINWSZ?yh:wa.height);
                else if (arg->i == MOVE) XMoveWindow(dis, current->win, xw, yh);
                break;
        }
        current->isfloating = True;
    } while(ev.type != ButtonRelease);
    XUngrabPointer(dis, CurrentTime);
    update_current(current);
    tile();
}

/* each window should cover all the available screen space */
void monocle(int hh, int cy) {
    for (client *c=head; c; c=c->next) if (!c->isfullscrn && !c->isfloating && !c->istransient)
        XMoveResizeWindow(dis, c->win, 0, cy, ww, hh);
}

/* move the current client, to current->next
 * and current->next to current client's position
 */
void move_down() {
    if (!current || !head->next) return;

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
    update_current(current);
}

/* move the current client, to the previous from current
 * and the previous from  current to current client's position
 */
void move_up() {
    if (!current || !head->next) return;

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
    update_current(current);
}

/* cyclic focus the next window
 * if the window is the last on stack, focus head
 */
void next_win() {
    if (!current || !head->next) return;
    current = (prevfocus = current)->next ? current->next : head;
    if (mode == MONOCLE) XMapWindow(dis, current->win);
    update_current(current);
}

/* cyclic focus the previous window
 * if the window is the head, focus the last stack window
 */
void prev_win() {
    if (!current || !head->next) return;
    if (head == (prevfocus = current)) while (current->next) current=current->next;
    else for (client *t=head; t; t=t->next) if (t->next == current) { current = t; break; }
    if (mode == MONOCLE) XMapWindow(dis, current->win);
    update_current(current);
}

/* property notify is called when one of the window's properties
 * is changed, such as an urgent hint is received
 */
void propertynotify(XEvent *e) {
    client *c = wintoclient(e->xproperty.window);
    if (!c || e->xproperty.atom != XA_WM_HINTS) return;
    XWMHints *wmh = XGetWMHints(dis, c->win);
    c->isurgent = wmh && (wmh->flags & XUrgencyHint);
    XFree(wmh);
    desktopinfo();
}

/* to quit just stop receiving events
 * run() is stopped and control is back to main()
 */
void quit(const Arg *arg) {
    retval = arg->i;
    running = False;
}

/* remove the specified client
 * the previous client must point to the next client of the given
 * the removing client can be on any desktop, so we must return
 * back the current focused desktop
 *
 * keep in mind that the current set and the current update may
 * differ. current pointer changes in every select_desktop()
 * invocation.
 */
void removeclient(client *c) {
    client **p = NULL;
    int nd = 0, cd = current_desktop;
    for (Bool found = False; nd<DESKTOPS && !found; nd++)
        for (select_desktop(nd), p = &head; *p && !(found = *p == c); p = &(*p)->next);
    *p = c->next;
    current = (prevfocus && prevfocus != c) ? prevfocus : (*p) ? (prevfocus = *p) : (prevfocus = head);
    select_desktop(cd);
    tile();
    if (mode == MONOCLE && cd == --nd && current) XMapWindow(dis, current->win);
    update_current(current);
    free(c);
}

/* resize the master window - check for boundary size limits
 * the size of a window can't be less than MINWSZ
 */
void resize_master(const Arg *arg) {
    int msz = master_size + arg->i;
    if ((mode == BSTACK ? wh : ww) - msz <= MINWSZ || msz <= MINWSZ) return;
    master_size = msz;
    tile();
}

/* resize the first stack window - no boundary checks */
void resize_stack(const Arg *arg) {
    growth += arg->i;
    tile();
}

/* jump and focus the next or previous desktop */
void rotate(const Arg *arg) {
    change_desktop(&(Arg){.i = (DESKTOPS + current_desktop + arg->i) % DESKTOPS});
}

/* jump and focus the next or previous desktop that has clients */
void rotate_filled(const Arg *arg) {
    int n = arg->i;
    while (n < DESKTOPS && !desktops[(DESKTOPS + current_desktop + n) % DESKTOPS].head) (n += arg->i);
    change_desktop(&(Arg){.i = (DESKTOPS + current_desktop + n) % DESKTOPS});
}

/* main event loop - on receival of an event call the appropriate event handler */
void run(void) {
    XEvent ev;
    while(running && !XNextEvent(dis, &ev)) if (events[ev.type]) events[ev.type](&ev);
}

/* save specified desktop's properties */
void save_desktop(int i) {
    if (i < 0 || i >= DESKTOPS) return;
    desktops[i].master_size = master_size;
    desktops[i].mode        = mode;
    desktops[i].growth      = growth;
    desktops[i].head        = head;
    desktops[i].current     = current;
    desktops[i].showpanel   = showpanel;
    desktops[i].prevfocus   = prevfocus;
}

/* set the specified desktop's properties */
void select_desktop(int i) {
    if (i < 0 || i >= DESKTOPS) return;
    save_desktop(current_desktop);
    master_size     = desktops[i].master_size;
    mode            = desktops[i].mode;
    growth          = desktops[i].growth;
    head            = desktops[i].head;
    current         = desktops[i].current;
    showpanel       = desktops[i].showpanel;
    prevfocus       = desktops[i].prevfocus;
    current_desktop = i;
}

/* set or unset fullscreen state of client */
void setfullscreen(client *c, Bool fullscrn) {
    if (fullscrn != c->isfullscrn) XChangeProperty(dis, c->win,
            netatoms[NET_WM_STATE], XA_ATOM, 32, PropModeReplace, (unsigned char*)
            ((c->isfullscrn = fullscrn) ? &netatoms[NET_FULLSCREEN] : 0), fullscrn);
    if (c->isfullscrn) XMoveResizeWindow(dis, c->win, 0, 0, ww, wh + PANEL_HEIGHT);
}

/* set initial values
 * root window - screen height/width - atoms - xerror handler
 * set masks for reporting events handled by the wm
 * and propagate the suported net atoms
 */
void setup(void) {
    sigchld();

    screen = DefaultScreen(dis);
    root = RootWindow(dis, screen);

    ww = XDisplayWidth(dis,  screen);
    wh = XDisplayHeight(dis, screen) - (SHOW_PANEL ? PANEL_HEIGHT : 0);
    master_size = ((mode == BSTACK) ? wh : ww) * MASTER_SIZE;
    for (unsigned int i=0; i<DESKTOPS; i++) save_desktop(i);

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
    XSelectInput(dis, DefaultRootWindow(dis), SubstructureRedirectMask|ButtonPressMask|
                                              SubstructureNotifyMask|PropertyChangeMask);
    XSync(dis, False);

    XSetErrorHandler(xerror);
    XSync(dis, False);
    XChangeProperty(dis, root, netatoms[NET_SUPPORTED], XA_ATOM, 32,
              PropModeReplace, (unsigned char *)netatoms, NET_COUNT);

    grabkeys();
    change_desktop(&(Arg){.i = DEFAULT_DESKTOP});
}

void sigchld() {
    if (signal(SIGCHLD, sigchld) == SIG_ERR)
        die("error: can't install SIGCHLD handler\n");
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
    client *c;
    int n = 0, d = 0, z = (mode == BSTACK ? ww : hh), cx = 0, cw = 0, ch = 0;

    /* count stack windows - start from head->next */
    for (n=0, c=head->next; c; c=c->next) if (!c->isfullscrn && !c->isfloating && !c->istransient) ++n;

    /* grab the first non-floating, non-fullscreen window and place it on master
     * if it's a stack window, remove it from the stack count (--n)
     */
    for (c=head; c && (c->isfullscrn || c->isfloating || c->istransient); c=c->next, --n);

    /* if there is only one window, it should cover the available screen space
     * if there is only one stack window (n == 1) then we don't care about growth
     * if more than one stack windows (n > 1) on screen then adjustments may be needed
     *   - d is the num of pixels than remain on the bottom of the screen plus the growth
     *   - z is the clients' height/width
     *
     *      ----------  -.    --------------------.
     *      |   |----| --|--> growth               `}--> first client will get (z+d) height/width
     *      |   |    |   |                          |
     *      |   |----|   }--> screen height - hh  --'
     *      |   |    | }-|--> client height - z       :: 2 stack clients on tile mode ..looks like a spaceship
     *      ----------  -'                            :: piece of aart by c00kiemon5ter o.O om nom nom nom nom
     *
     *     what we do is, remove the growth from the screen height  : (z - growth)
     *     and then divide that space with the windows on the stack : (z - growth)/n
     *     so all windows have equal height/width (z)
     *     growth is left out and will later be added to the first's client height/width
     *     before that, there will be cases when the num of windows is not perfectly
     *     divided with then available screen height/width (ie 100px scr. height, and 3 windows)
     *     so we get that remaining space and merge growth to it (d): (z - growth) % n + growth
     *     finally we know each client's height, and how many pixels should be added to
     *     the first stack window so that it satisfies growth, and doesn't create gaps
     *     on the bottom of the screen.
     */
    if (c && n < 1) {
        XMoveResizeWindow(dis, c->win, cx, cy, ww - 2*BORDER_WIDTH, hh - 2*BORDER_WIDTH);
        return;
    } else if (c && n > 1) { d = (z - growth) % n + growth; z = (z - growth) / n; }

    /* tile the first non-floating, non-fullscreen window to cover the master area */
    if (c) (mode == BSTACK) ? XMoveResizeWindow(dis, c->win, cx, cy, ww - 2*BORDER_WIDTH, master_size - BORDER_WIDTH)
                            : XMoveResizeWindow(dis, c->win, cx, cy, master_size - BORDER_WIDTH, hh - 2*BORDER_WIDTH);

    /* tile the next non-floating, non-fullscreen stack window with growth/d */
    if (c) for (c=c->next; c && (c->isfullscrn || c->isfloating || c->istransient); c=c->next);
    if (c) (mode == BSTACK) ? XMoveResizeWindow(dis, c->win, cx, (cy += master_size),
                              (cw =  z - BORDER_WIDTH)  - BORDER_WIDTH + d,
                              (ch = hh - BORDER_WIDTH*2 - master_size))
                            : XMoveResizeWindow(dis, c->win, (cx += master_size), cy,
                              (cw = ww - BORDER_WIDTH*2 - master_size),
                              (ch =  z - BORDER_WIDTH)  - BORDER_WIDTH + d);

    /* tile the rest of the non-floating, non-fullscreen stack windows */
    if (c) for (mode==BSTACK?(cx+=cw+d):(cy+=ch+d), c=c->next; c; c=c->next)
        if (!c->isfullscrn && !c->isfloating && !c->istransient) {
            XMoveResizeWindow(dis, c->win, cx, cy, cw, ch);
            (mode == BSTACK) ? (cx+=z) : (cy+=z);
        }
}

/* swap master window with current or
 * if current is head swap with next
 * if current is not head, then head
 * is behind us, so move_up until we
 * are the head
 */
void swap_master() {
    if (!current || !head->next || mode == MONOCLE) return;
    if (current == head) move_down();
    else while (current != head) move_up();
    update_current(head);
    tile();
}

/* switch the tiling mode and reset all floating windows */
void switch_mode(const Arg *arg) {
    if (mode == arg->i) for (client *c=head; c; c=c->next) c->isfloating = False;
    if (mode == MONOCLE) for (client *c=head; c; c=c->next) XMapWindow(dis, c->win);
    mode = arg->i;
    master_size = (mode == BSTACK ? wh : ww) * MASTER_SIZE;
    tile();
    update_current(current);
    desktopinfo();
}

/* tile all windows of current desktop - call the handler tiling function */
void tile(void) {
    if (!head) return; /* nothing to arange */
    layout[head->next ? mode : MONOCLE](wh + (showpanel ? 0 : PANEL_HEIGHT),
                                (TOP_PANEL && showpanel ? PANEL_HEIGHT : 0));
}

/* toggle visibility state of the panel */
void togglepanel() {
    showpanel = !showpanel;
    tile();
}

/* windows that request to unmap should lose their
 * client, so no invisible windows exist on screen
 */
void unmapnotify(XEvent *e) {
    client *c = wintoclient(e->xunmap.window);
    if (c && e->xunmap.send_event) removeclient(c);
    desktopinfo();
}

/* update client - set highlighted borders and active window
 * if no client is given update current
 * if current is NULL then delete the active window property
 *
 * a window should have borders in any case except if
 *  - the window is not floating or transient
 *  - the window is fullscreen
 *  - the window is the only window on screen
 *  - the mode is MONOCLE and non of the above applies
 */
void update_current(client *c) {
    if (!c) {
        XDeleteProperty(dis, root, netatoms[NET_ACTIVE]);
        return;
    } else current = c;

    for (c=head; c; c=c->next) {
        XSetWindowBorderWidth(dis, c->win, (!head->next || c->isfullscrn || (mode == MONOCLE
                                && (!c->isfloating && !c->istransient))) ? 0 : BORDER_WIDTH);
        XSetWindowBorder(dis, c->win, (current == c ? win_focus : win_unfocus));
        if (CLICK_TO_FOCUS) XGrabButton(dis, Button1, None, c->win, True,
                ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);
    }

    XChangeProperty(dis, root, netatoms[NET_ACTIVE], XA_WINDOW, 32,
                PropModeReplace, (unsigned char *)&current->win, 1);
    XSetInputFocus(dis, current->win, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(dis, current->win);
    if (CLICK_TO_FOCUS) XUngrabButton(dis, Button1, None, current->win);
    XSync(dis, False);
}

/* find to which client the given window belongs to */
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
