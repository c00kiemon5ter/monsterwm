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

#define TABLENGTH(X)    (sizeof(X)/sizeof(*X))

/* structs */
typedef union {
    const char** com;
    const int i;
} Arg;

typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg arg);
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
} desktop;

typedef struct {
    const char *class;
    int preferredd;
    int followwin;
} Convenience;

typedef enum {
    ATOM_NET_WM_WINDOW_TYPE,
    ATOM_NET_WM_WINDOW_TYPE_UTILITY,
    ATOM_NET_WM_WINDOW_TYPE_DOCK,
    ATOM_NET_WM_WINDOW_TYPE_SPLASH,
    ATOM_NET_WM_WINDOW_TYPE_DIALOG,
    ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION,
    ATOM_COUNT
} AtomType;

typedef struct {
    Atom *atom;
    const char *name;
} AtomNode;

Atom atoms[ATOM_COUNT];

static const AtomNode atomList[] = {
    { &atoms[ATOM_NET_WM_WINDOW_TYPE],              "_NET_WM_WINDOW_TYPE"             },
    { &atoms[ATOM_NET_WM_WINDOW_TYPE_UTILITY],      "_NET_WM_WINDOW_TYPE_UTILITY"     },
    { &atoms[ATOM_NET_WM_WINDOW_TYPE_DOCK],         "_NET_WM_WINDOW_TYPE_DOCK"        },
    { &atoms[ATOM_NET_WM_WINDOW_TYPE_SPLASH],       "_NET_WM_WINDOW_TYPE_SPLASH"      },
    { &atoms[ATOM_NET_WM_WINDOW_TYPE_DIALOG],       "_NET_WM_WINDOW_TYPE_DIALOG"      },
    { &atoms[ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION], "_NET_WM_WINDOW_TYPE_NOTIFICATION"},
};

/* layout modes */
enum mode {
    TILE,
    MONOCYCLE,
    BSTACK,
    GRID,
};

/* Functions */
static void add_window(Window w);
static void buttonpressed(XEvent *e);
static void change_desktop(const Arg arg);
static void client_to_desktop(const Arg arg);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void destroynotify(XEvent *e);
static void enternotify(XEvent *e);
static void die(const char* errstr, ...);
static int xerrorstart(Display *dis, XErrorEvent *ee);
static void checkotherwm(void);
static unsigned long getcolor(const char* color);
static void grabkeys(void);
static void keypress(XEvent *e);
static void kill_client(const Arg arg);
static void maprequest(XEvent *e);
static void move_down(const Arg arg);
static void move_up(const Arg arg);
static void rotate_desktop(const Arg arg);
static void next_win(const Arg arg);
static void prev_win(const Arg arg);
static void quit(const Arg arg);
static void remove_window(Window w);
static void resize_master(const Arg arg);
static void resize_stack(const Arg arg);
static void save_desktop(int i);
static void select_desktop(int i);
static void send_kill_signal(Window w);
static void setup(void);
static void sigchld(int unused);
static void spawn(const Arg arg);
static void run(void);
static void swap_master(const Arg arg);
static void tile(void);
static void last_desktop(const Arg arg);
static void switch_mode(const Arg arg);
static void update_current(void);

#include "config.h"

/* variables */
static Display *dis;
static int bool_quit;
static int current_desktop;
static int previous_desktop;
static int growth;
static int master_size;
static int mode;
static int sh;
static int sw;
static int screen;
static int xerror(Display *dis, XErrorEvent *ee);
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int win_focus;
static unsigned int win_unfocus;
static unsigned int numlockmask; /* dynamic key lock mask */
static Window root;
static client *head;
static client *current;

/* events array */
static void (*events[LASTEvent])(XEvent *e) = {
    [KeyPress] = keypress,
    [MapRequest] = maprequest,
    [EnterNotify] = enternotify,
    [ButtonPress] = buttonpressed,
    [DestroyNotify] = destroynotify,
    [ConfigureNotify] = configurenotify,
    [ConfigureRequest] = configurerequest
};

/* desktops array */
static desktop desktops[DESKTOPS];

/* ***************************** Window Management ******************************* */
void add_window(Window w) {
    client *c,*t;

    if(!(c = (client *)calloc(1,sizeof(client))))
        die("error: could not calloc() %u bytes\n", sizeof(client));

    if(head == NULL) {
        c->next = NULL;
        c->prev = NULL;
        c->win = w;
        head = c;
    }
    else {
        if(ATTACH_ASIDE == 0) {
            for(t=head;t->next;t=t->next);

            c->next = NULL;
            c->prev = t;
            c->win = w;

            t->next = c;
        }
        else {
            for(t=head;t->prev;t=t->prev);

            c->prev = NULL;
            c->next = t;
            c->win = w;

            t->prev = c;

            head = c;
        }
    }

    current = c;
    save_desktop(current_desktop);

    if(FOLLOW_MOUSE == 0)
        XSelectInput(dis, c->win, EnterWindowMask);
}

void remove_window(Window w) {
    client *c;

    /* FIXME: change this ugly code */
    for(c=head;c;c=c->next) {

        if(c->win == w) {
            if(c->prev == NULL && c->next == NULL) {
                free(head);
                head = NULL;
                current = NULL;
                save_desktop(current_desktop);
                return;
            }

            if(c->prev == NULL) {
                head = c->next;
                c->next->prev = NULL;
                current = c->next;
            }
            else if(c->next == NULL) {
                c->prev->next = NULL;
                current = c->prev;
            }
            else {
                c->prev->next = c->next;
                c->next->prev = c->prev;
                current = c->prev;
            }

            free(c);
            save_desktop(current_desktop);
            tile();
            update_current();
            return;
        }
    }
}

void kill_client(const Arg arg) {
    if(current == NULL) return;
    //send delete signal to window
    XEvent ke;
    ke.type = ClientMessage;
    ke.xclient.window = current->win;
    ke.xclient.message_type = XInternAtom(dis, "WM_PROTOCOLS", True);
    ke.xclient.format = 32;
    ke.xclient.data.l[0] = XInternAtom(dis, "WM_DELETE_WINDOW", True);
    ke.xclient.data.l[1] = CurrentTime;
    XSendEvent(dis, current->win, False, NoEventMask, &ke);
    send_kill_signal(current->win);
    remove_window(current->win);
}

void next_win(const Arg arg) {
    client *c;

    if(current != NULL && head != NULL) {
        if(current->next == NULL)
            c = head;
        else
            c = current->next;

        current = c;
        if(mode == MONOCYCLE)
            tile();
        update_current();
    }
}

void prev_win(const Arg arg) {
    client *c;

    if(current != NULL && head != NULL) {
        if(current->prev == NULL)
            for(c=head;c->next;c=c->next);
        else
            c = current->prev;

        current = c;
        if(mode == MONOCYCLE)
            tile();
        update_current();
    }
}

void move_down(const Arg arg) {
    Window tmp;
    if(current == NULL || current->next == NULL || current->win == head->win || current->prev == NULL)
        return;

    tmp = current->win;
    current->win = current->next->win;
    current->next->win = tmp;
    next_win((Arg){NULL});
    save_desktop(current_desktop);
    tile();
}

void move_up(const Arg arg) {
    Window tmp;
    if(current == NULL || current->prev == head || current->win == head->win) {
        return;
    }
    tmp = current->win;
    current->win = current->prev->win;
    current->prev->win = tmp;
    prev_win((Arg){NULL});
    save_desktop(current_desktop);
    tile();
}

void swap_master(const Arg arg) {
    Window tmp;

    if(head->next != NULL && current != NULL && mode != MONOCYCLE) {
        if(current == head) {
            tmp = head->next->win;
            head->next->win = head->win;
            head->win = tmp;
        } else {
            tmp = head->win;
            head->win = current->win;
            current->win = tmp;
            current = head;
        }
        save_desktop(current_desktop);
        tile();
        update_current();
    }
}

/* **************************** Desktop Management ************************************* */
void change_desktop(const Arg arg) {
    client *c;

    if(arg.i == current_desktop)
        return;

    // Save current "properties"
    save_desktop(current_desktop);
    previous_desktop = current_desktop;

    // Unmap all window
    if(head != NULL)
        for(c=head;c;c=c->next)
            XUnmapWindow(dis,c->win);

    // Take "properties" from the new desktop
    select_desktop(arg.i);

    // Map all windows
    if(head != NULL)
        for(c=head;c;c=c->next)
            XMapWindow(dis,c->win);

    tile();
    update_current();
}

void last_desktop(const Arg arg) {
    change_desktop((Arg){.i = previous_desktop});
}

void rotate_desktop(const Arg arg) {
    change_desktop((Arg){.i = (current_desktop + DESKTOPS + arg.i) % DESKTOPS});
}

void client_to_desktop(const Arg arg) {
    client *tmp = current;
    int tmp2 = current_desktop;

    if(arg.i == current_desktop || current == NULL)
        return;

    // Add client to desktop
    select_desktop(arg.i);
    add_window(tmp->win);
    save_desktop(arg.i);

    // Remove client from current desktop
    select_desktop(tmp2);
    XUnmapWindow(dis,tmp->win);
    remove_window(tmp->win);
    save_desktop(tmp2);
    tile();
    update_current();
    if(FOLLOW_WINDOW == 0)
        change_desktop(arg);
}

void save_desktop(int i) {
    desktops[i].master_size = master_size;
    desktops[i].mode = mode;
    desktops[i].growth = growth;
    desktops[i].head = head;
    desktops[i].current = current;
}

void select_desktop(int i) {
    master_size = desktops[i].master_size;
    mode = desktops[i].mode;
    growth = desktops[i].growth;
    head = desktops[i].head;
    current = desktops[i].current;
    current_desktop = i;
}

void tile(void) {
    client *c;
    int n = 0;
    int x = 0;
    int y = 0;

    // For a top panel
    if(TOP_PANEL == 0)
        y = PANEL_HEIGHT;

    // If only one window
    if(head != NULL && head->next == NULL) {
        XMoveResizeWindow(dis,head->win,0,y,sw+2*BORDER_WIDTH,sh+2*BORDER_WIDTH);
    }
    else if(head != NULL) {
        switch(mode) {
            case TILE:
                // Master window
                XMoveResizeWindow(dis,head->win,0,y,master_size - BORDER_WIDTH,sh - BORDER_WIDTH);
                // Stack
                for(c=head->next;c;c=c->next) ++n;
                if(n == 1) growth = 0;
                XMoveResizeWindow(dis,head->next->win,master_size + BORDER_WIDTH,y,sw-master_size-(2*BORDER_WIDTH),(sh/n)+growth - BORDER_WIDTH);
                y += (sh/n)+growth;
                for(c=head->next->next;c;c=c->next) {
                    XMoveResizeWindow(dis,c->win,master_size + BORDER_WIDTH,y,sw-master_size-(2*BORDER_WIDTH),(sh/n)-(growth/(n-1)) - BORDER_WIDTH);
                    y += (sh/n)-(growth/(n-1));
                }
                break;
            case MONOCYCLE:
                for(c=head;c;c=c->next) {
                    XMoveResizeWindow(dis,c->win,0,y,sw+2*BORDER_WIDTH,sh+2*BORDER_WIDTH);
                }
                break;
            case BSTACK:
                // Master window
                XMoveResizeWindow(dis,head->win,0,y,sw-BORDER_WIDTH,master_size - BORDER_WIDTH);
                // Stack
                for(c=head->next;c;c=c->next) ++n;
                if(n == 1) growth = 0;
                XMoveResizeWindow(dis,head->next->win,0,y+master_size + BORDER_WIDTH,(sw/n)+growth-BORDER_WIDTH,sh-master_size-(2*BORDER_WIDTH));
                x = (sw/n)+growth;
                for(c=head->next->next;c;c=c->next) {
                    XMoveResizeWindow(dis,c->win,x,y+master_size + BORDER_WIDTH,(sw/n)-(growth/(n-1)) - BORDER_WIDTH,sh-master_size-(2*BORDER_WIDTH));
                    x += (sw/n)-(growth/(n-1));
                }
                break;
            case GRID:
                {
                    int xpos = 0;
                    int wdt = 0;
                    int ht = 0;

                    for(c=head;c;c=c->next) ++x;

                    for(c=head;c;c=c->next) {
                        ++n;
                        if(x >= 7) {
                            wdt = (sw/3) - BORDER_WIDTH;
                            ht  = (sh/3) - BORDER_WIDTH;
                            if((n == 1) || (n == 4) || (n == 7))
                                xpos = 0;
                            if((n == 2) || (n == 5) || (n == 8))
                                xpos = (sw/3) + BORDER_WIDTH;
                            if((n == 3) || (n == 6) || (n == 9))
                                xpos = (2*(sw/3)) + BORDER_WIDTH;
                            if((n == 4) || (n == 7))
                                y += (sh/3) + BORDER_WIDTH;
                            if((n == x) && (n == 7))
                                wdt = sw - BORDER_WIDTH;
                            if((n == x) && (n == 8))
                                wdt = 2*sw/3 - BORDER_WIDTH;
                        } else
                            if(x >= 5) {
                                wdt = (sw/3) - BORDER_WIDTH;
                                ht  = (sh/2) - BORDER_WIDTH;
                                if((n == 1) || (n == 4))
                                    xpos = 0;
                                if((n == 2) || (n == 5))
                                    xpos = (sw/3) + BORDER_WIDTH;
                                if((n == 3) || (n == 6))
                                    xpos = (2*(sw/3)) + BORDER_WIDTH;
                                if(n == 4)
                                    y += (sh/2); // + BORDER_WIDTH;
                                if((n == x) && (n == 5))
                                    wdt = 2*sw/3 - BORDER_WIDTH;
                            } else {
                                if(x > 2) {
                                    if((n == 1) || (n == 2))
                                        ht = (sh/2) + growth - BORDER_WIDTH;
                                    if(n >= 3)
                                        ht = (sh/2) - growth - 2*BORDER_WIDTH;
                                }
                                else
                                    ht = sh - BORDER_WIDTH;
                                if((n == 1) || (n == 3)) {
                                    xpos = 0;
                                    wdt = master_size - BORDER_WIDTH;
                                }
                                if((n == 2) || (n == 4)) {
                                    xpos = master_size+BORDER_WIDTH;
                                    wdt = (sw - master_size) - 2*BORDER_WIDTH;
                                }
                                if(n == 3)
                                    y += (sh/2) + growth + BORDER_WIDTH;
                                if((n == x) && (n == 3))
                                    wdt = sw - BORDER_WIDTH;
                            }
                        XMoveResizeWindow(dis,c->win,xpos,y,wdt,ht);
                    }
                }
                break;
            default:
                break;
        }
    }
}

void update_current(void) {
    client *c;

    for(c=head;c;c=c->next) {
        if((head->next == NULL) || (mode == MONOCYCLE))
            XSetWindowBorderWidth(dis,c->win,0);
        else
            XSetWindowBorderWidth(dis,c->win,BORDER_WIDTH);

        if(current == c) {
            // "Enable" current window
            XSetWindowBorder(dis,c->win,win_focus);
            XSetInputFocus(dis,c->win,RevertToParent,CurrentTime);
            XRaiseWindow(dis,c->win);
            if(CLICK_TO_FOCUS == 0)
                XUngrabButton(dis, AnyButton, AnyModifier, c->win);
        }
        else {
            XSetWindowBorder(dis,c->win,win_unfocus);
            if(CLICK_TO_FOCUS == 0)
                XGrabButton(dis, AnyButton, AnyModifier, c->win, True, ButtonPressMask|ButtonReleaseMask, GrabModeAsync, GrabModeAsync, None, None);
        }
    }
    XSync(dis, False);
}

void switch_mode(const Arg arg) {
    if(mode == arg.i) return;
    mode = arg.i;
    if(mode == TILE || mode == GRID)
        master_size = sw * MASTER_SIZE;
    else if(mode == BSTACK)
        master_size = sh * MASTER_SIZE;
    tile();
    update_current();
}

void resize_master(const Arg arg) {
    master_size += arg.i;
    tile();
}

void resize_stack(const Arg arg) {
    growth += arg.i;
    tile();
}

/* ********************** Keyboard Management ********************** */
void grabkeys(void) {
    int i;
    KeyCode code;

    XUngrabKey(dis, AnyKey, AnyModifier, root);
    // For each shortcuts
    for(i=0;i<TABLENGTH(keys);++i) {
        code = XKeysymToKeycode(dis,keys[i].keysym);
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
    for(i = 0; i < len; i++) {
        if(keysym == keys[i].keysym && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)) {
            if(keys[i].function)
                keys[i].function(keys[i].arg);
        }
    }
}

/* FIXME: what's this for ? */
void configurenotify(XEvent *e) {
    // Do nothing for the moment
}

/* ********************** Signal Management ************************** */
void configurerequest(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    XWindowChanges wc;

    wc.x = ev->x;
    wc.y = ev->y;
    if(ev->width < sw-BORDER_WIDTH)
        wc.width = ev->width;
    else
        wc.width = sw-BORDER_WIDTH;
    if(ev->height < sh-BORDER_WIDTH)
        wc.height = ev->height;
    else
        wc.height = sh-BORDER_WIDTH;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dis, ev->window, ev->value_mask, &wc);
    XSync(dis, False);
}

void maprequest(XEvent *e) {
    XMapRequestEvent *ev = &e->xmaprequest;

    // For fullscreen mplayer (and maybe some other program)
    client *c;

    for(c=head;c;c=c->next)
        if(ev->window == c->win) {
            XMapWindow(dis,ev->window);
            XMoveResizeWindow(dis,c->win,-BORDER_WIDTH,-BORDER_WIDTH,sw+BORDER_WIDTH,sh+BORDER_WIDTH);
            return;
        }

    Window trans = None;
    if(XGetTransientForHint(dis, ev->window, &trans) && trans != None) {
        add_window(ev->window);
        XMapWindow(dis, ev->window);
        XSetInputFocus(dis,ev->window,RevertToParent,CurrentTime);
        XRaiseWindow(dis,ev->window);
        return;
    }

    unsigned long count, j, extra;
    Atom realType;
    int realFormat;
    unsigned char *temp;
    Atom *type;

    if(XGetWindowProperty(dis, ev->window, atoms[ATOM_NET_WM_WINDOW_TYPE], 0, 32, False, XA_ATOM, &realType, &realFormat, &count, &extra, &temp) == Success) {
        if(count > 0) {
            type = (unsigned long*)temp;
            for(j=0; j<count; j++) {
                if((type[j] == atoms[ATOM_NET_WM_WINDOW_TYPE_UTILITY]) ||
                        (type[j] == atoms[ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION]) ||
                        (type[j] == atoms[ATOM_NET_WM_WINDOW_TYPE_SPLASH]) ||
                        (type[j] == atoms[ATOM_NET_WM_WINDOW_TYPE_DIALOG]) ||
                        (type[j] == atoms[ATOM_NET_WM_WINDOW_TYPE_DOCK])) {
                    add_window(ev->window);
                    XMapWindow(dis, ev->window);
                    XSetInputFocus(dis,ev->window,RevertToParent,CurrentTime);
                    XRaiseWindow(dis,ev->window);
                    return;
                }
            }
        }
        if(temp)
            XFree(temp);
    }

    XClassHint ch = {0};
    static unsigned int len = sizeof convenience / sizeof convenience[0];
    int i = 0;
    int tmp = current_desktop;
    if(XGetClassHint(dis, ev->window, &ch))
        for(i=0;i<len;i++)
            if(strcmp(ch.res_class, convenience[i].class) == 0) {
                save_desktop(tmp);
                select_desktop(convenience[i].preferredd-1);
                add_window(ev->window);
                if(tmp == convenience[i].preferredd-1) {
                    XMapWindow(dis, ev->window);
                    tile();
                    update_current();
                } else {
                    select_desktop(tmp);
                }
                if(convenience[i].followwin != 0) {
                    Arg a = {.i = convenience[i].preferredd-1};
                    change_desktop(a);
                }
                if(ch.res_class)
                    XFree(ch.res_class);
                if(ch.res_name)
                    XFree(ch.res_name);
                return;
            }

    add_window(ev->window);
    XMapWindow(dis,ev->window);
    tile();
    update_current();
}

void destroynotify(XEvent *e) {
    int i = 0;
    int j = 0;
    int tmp = current_desktop;
    client *c;
    XDestroyWindowEvent *ev = &e->xdestroywindow;

    save_desktop(tmp);
    for(j=0;j<TABLENGTH(desktops);++j) {
        select_desktop(j);
        for(c=head;c;c=c->next)
            if(ev->window == c->win)
                i++;

        if(i != 0) {
            remove_window(ev->window);
            select_desktop(tmp);
            return;
        }

        i = 0;
    }
    select_desktop(tmp);
}

void enternotify(XEvent *e) {
    client *c;
    XCrossingEvent *ev = &e->xcrossing;

    if(FOLLOW_MOUSE == 0) {
        if((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
            return;
        for(c=head;c;c=c->next)
            if(ev->window == c->win) {
                current = c;
                update_current();
                return;
            }
    }
}

void buttonpressed(XEvent *e) {
    client *c;
    XButtonPressedEvent *ev = &e->xbutton;

    // change focus with LMB
    if(CLICK_TO_FOCUS == 0 && ev->window != current->win && ev->button == Button1)
        for(c=head;c;c=c->next)
            if(ev->window == c->win) {
                current = c;
                update_current();
                return;
            }
}

void send_kill_signal(Window w) {
    XEvent ke;
    ke.type = ClientMessage;
    ke.xclient.window = w;
    ke.xclient.message_type = XInternAtom(dis, "WM_PROTOCOLS", True);
    ke.xclient.format = 32;
    ke.xclient.data.l[0] = XInternAtom(dis, "WM_DELETE_WINDOW", True);
    ke.xclient.data.l[1] = CurrentTime;
    XSendEvent(dis, w, False, NoEventMask, &ke);
}

unsigned long getcolor(const char* color) {
    Colormap map = DefaultColormap(dis,screen);
    XColor c;

    if(!XAllocNamedColor(dis,map,color,&c,&c))
        die("error: cannot allocate color '%s'\n", c);
    return c.pixel;
}

/* FIXME: fix segfaults */
void quit(const Arg arg) {
    Window root_return, parent;
    Window *children;
    int i;
    unsigned int nchildren;
    XEvent ev;

    /*
     * if a client refuses to terminate itself,
     * we kill every window remaining the brutal way.
     * Since we're stuck in the while(nchildren > 0) { ... } loop
     * we can't exit through the main method.
     * This all happens if MOD+q is pushed a second time.
     */
    if(bool_quit == 1) {
        XUngrabKey(dis, AnyKey, AnyModifier, root);
        XDestroySubwindows(dis, root);
        XCloseDisplay(dis);
        die("error: forced shutdown\n");
    }

    bool_quit = 1;
    XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);
    for(i = 0; i < nchildren; i++) {
        send_kill_signal(children[i]);
    }
    //keep alive until all windows are killed
    while(nchildren > 0) {
        XQueryTree(dis, root, &root_return, &parent, &children, &nchildren);
        XNextEvent(dis,&ev);
        if(events[ev.type])
            events[ev.type](&ev);
    }

    XUngrabKey(dis,AnyKey,AnyModifier,root);
}

void setup(void) {
    // Install a signal
    sigchld(0);

    // Screen and root window
    screen = DefaultScreen(dis);
    root = RootWindow(dis,screen);

    // Screen width and height
    sw = XDisplayWidth(dis,screen) - BORDER_WIDTH;
    sh = (XDisplayHeight(dis,screen) - PANEL_HEIGHT) - BORDER_WIDTH;

    // Colors
    win_focus = getcolor(FOCUS);
    win_unfocus = getcolor(UNFOCUS);

    // numlock workaround
    int j, k;
    XModifierKeymap *modmap;
    numlockmask = 0;
    modmap = XGetModifierMapping(dis);
    for (k = 0; k < 8; k++) {
        for (j = 0; j < modmap->max_keypermod; j++) {
            if(modmap->modifiermap[k * modmap->max_keypermod + j] == XKeysymToKeycode(dis, XK_Num_Lock))
                numlockmask = (1 << k);
        }
    }
    XFreeModifiermap(modmap);

    // Shortcuts
    grabkeys();

    /* FIXME: move defaults to declerations */
    // Default stack
    mode = DEFAULT_MODE;
    growth = 0;

    // For exiting
    bool_quit = 0;

    // List of client
    head = NULL;
    current = NULL;

    // Master size
    if(mode == BSTACK)
        master_size = sh*MASTER_SIZE;
    else
        master_size = sw*MASTER_SIZE;

    // Set up all desktop
    int i;
    for(i=0;i<TABLENGTH(desktops);++i) {
        desktops[i].master_size = master_size;
        desktops[i].mode = mode;
        desktops[i].growth = growth;
        desktops[i].head = head;
        desktops[i].current = current;
    }

    // Select first dekstop by default
    const Arg arg = {.i = 0};
    current_desktop = arg.i;
    change_desktop(arg);
    // Set up atoms for dialog/notification windows
    int x;
    for(x = 0; x < ATOM_COUNT; x++)
        *atomList[x].atom = XInternAtom(dis, atomList[x].name, False);
    // To catch maprequest and destroynotify (if other wm running)
    XSelectInput(dis, root, SubstructureNotifyMask|SubstructureRedirectMask);
}

int xerrorstart(Display *dis, XErrorEvent *ee) {
    die("error: another window manager is already running\n");
    return -1;
}

void checkotherwm(void) {
    xerrorxlib = XSetErrorHandler(xerrorstart);
    /* this causes an error if some other window manager is running */
    XSelectInput(dis, DefaultRootWindow(dis), SubstructureRedirectMask);
    XSync(dis, False);
    XSetErrorHandler(xerror);
    XSync(dis, False);
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit.  */
int xerror(Display *dis, XErrorEvent *ee) {
    if(ee->error_code == BadWindow
            || (ee->error_code == BadMatch && (ee->request_code == X_SetInputFocus || ee->request_code ==  X_ConfigureWindow))
            || (ee->error_code == BadDrawable && (ee->request_code == X_PolyText8 || ee->request_code == X_PolyFillRectangle
                 || ee->request_code == X_PolySegment || ee->request_code == X_CopyArea))
            || (ee->error_code == BadAccess && ee->request_code == X_GrabKey))
        return 0;
    fprintf(stderr, "error: xerror: request code: %d, error code: %d\n", ee->request_code, ee->error_code);
    return xerrorxlib(dis, ee); /* may call exit */
}

void sigchld(int unused) {
    if(signal(SIGCHLD, sigchld) == SIG_ERR)
        die("error: can't install SIGCHLD handler\n");
    while(0 < waitpid(-1, NULL, WNOHANG));
}

void spawn(const Arg arg) {
    if(fork() == 0) {
        if(dis)
            close(ConnectionNumber(dis));
        setsid();
        execvp((char*)arg.com[0], (char**)arg.com);
        fprintf(stderr, "error: execvp %s", (char *)arg.com[0]);
        perror(" failed"); /* also prints the err msg */
        exit(EXIT_SUCCESS);
    }
}

void run(void) {
    XEvent ev;
    while(!bool_quit && !XNextEvent(dis,&ev))
        if(events[ev.type])
            events[ev.type](&ev);
}

void die(const char *errstr, ...) {
    va_list ap;
    va_start(ap, errstr);
    vfprintf(stderr, errstr, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if(argc == 2 && strcmp("-v", argv[1]) == 0) {
        fprintf(stdout, "dminiwm-%s\n", VERSION);
        return EXIT_SUCCESS;
    } else if(argc != 1)
        die("usage: dminiwm [-v]\n");
    if(!(dis = XOpenDisplay(NULL)))
        die("error: cannot open display\n");
    checkotherwm();
    setup();
    run();
    XCloseDisplay(dis);
    return EXIT_SUCCESS;
}

