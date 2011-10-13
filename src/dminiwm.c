/* dminiwm.c [ 0.1.2 ]
*
*  I started this from catwm 31/12/10 
*  Bad window error checking and numlock checking used from
*  2wm at http://hg.suckless.org/2wm/
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <X11/Xlib.h>
#include <X11/keysym.h>
//#include <X11/XF86keysym.h>
#include <X11/Xproto.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>

#define TABLENGTH(X)    (sizeof(X)/sizeof(*X))

typedef union {
    const char** com;
    const int i;
} Arg;

// Structs
typedef struct {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg arg);
    const Arg arg;
} key;

typedef struct client client;
struct client{
    // Prev and next client
    client *next;
    client *prev;

    // The window
    Window win;
};

typedef struct desktop desktop;
struct desktop{
    int master_size;
    int mode;
    int growth;
    client *head;
    client *current;
};

// Functions
static void add_window(Window w);
static void change_desktop(const Arg arg);
static void client_to_desktop(const Arg arg);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void decrease();
static void destroynotify(XEvent *e);
static void enternotify(XEvent *e);
static void logger(const char* e);
static unsigned long getcolor(const char* color);
static void grabkeys();
static void grow_window();
static void increase();
static void keypress(XEvent *e);
static void kill_client();
static void maprequest(XEvent *e);
static void move_down();
static void move_up();
static void next_desktop();
static void next_win();
static void prev_desktop();
static void prev_win();
static void quit();
static void remove_window(Window w);
static void save_desktop(int i);
static void select_desktop(int i);
static void send_kill_signal(Window w);
static void setup();
static void shrink_window();
static void sigchld(int unused);
static void spawn(const Arg arg);
static void start();
static void swap_master();
static void tile();
static void switch_fullscreen();
static void switch_grid();
static void switch_horizontal();
static void switch_vertical();
static void update_current();

// Include configuration file (need struct key)
#include "config.h"

// Variable
static Display *dis;
static int bool_quit;
static int current_desktop;
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
unsigned int numlockmask;		/* dynamic key lock mask */
static Window root;
static client *head;
static client *current;

// Events array
static void (*events[LASTEvent])(XEvent *e) = {
    [KeyPress] = keypress,
    [MapRequest] = maprequest,
    [EnterNotify] = enternotify,
    [DestroyNotify] = destroynotify,
    [ConfigureNotify] = configurenotify,
    [ConfigureRequest] = configurerequest
};

// Desktop array
static desktop desktops[6];

/* ***************************** Window Management ******************************* */
void add_window(Window w) {
    client *c,*t;

    if(!(c = (client *)calloc(1,sizeof(client)))) {
        logger("\033[0;31mError calloc!");
        exit(1);
    }

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
    // for folow mouse
    if(FOLLOW_MOUSE == 0)
        XSelectInput(dis, c->win, EnterWindowMask);
}

void remove_window(Window w) {
    client *c;

    // CHANGE THIS UGLY CODE
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

void kill_client() {
    if(current != NULL) {
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
}

void next_win() {
    client *c;

    if(current != NULL && head != NULL) {
        if(current->next == NULL)
            c = head;
        else
            c = current->next;

        current = c;
        if(mode == 1)
            tile();
        update_current();
    }
}

void prev_win() {
    client *c;

    if(current != NULL && head != NULL) {
        if(current->prev == NULL)
            for(c=head;c->next;c=c->next);
        else
            c = current->prev;

        current = c;
        if(mode == 1)
            tile();
        update_current();
    }
}

void move_down() {
    Window tmp;
    if(current == NULL || current->next == NULL || current->win == head->win || current->prev == NULL)
        return;

    tmp = current->win;
    current->win = current->next->win;
    current->next->win = tmp;
    //keep the moved window activated
    next_win();
    save_desktop(current_desktop);
    tile();
}

void move_up() {
    Window tmp;
    if(current == NULL || current->prev == head || current->win == head->win) {
        return;
    }
    tmp = current->win;
    current->win = current->prev->win;
    current->prev->win = tmp;
    prev_win();
    save_desktop(current_desktop);
    tile();
}

void swap_master() {
    Window tmp;

    if(head != NULL && current != NULL && mode != 1) {
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

void decrease() {
        master_size -= 10;
        tile();
}

void increase() {
        master_size += 10;
        tile();
}

/* **************************** Desktop Management ************************************* */
void change_desktop(const Arg arg) {
    client *c;

    if(arg.i == current_desktop)
        return;

    // Save current "properties"
    save_desktop(current_desktop);

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

void next_desktop() {
    int tmp = current_desktop;
    if(tmp == TABLENGTH(desktops)-1)
        tmp = 0;
    else
        tmp++;

    Arg a = {.i = tmp};
    change_desktop(a);
}

void prev_desktop() {
    int tmp = current_desktop;
    if(tmp == 0)
        tmp = TABLENGTH(desktops)-1;
    else
        tmp--;

    Arg a = {.i = tmp};
    change_desktop(a);
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

void tile() {
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
            case 0: /* Vertical */
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
            case 1: /* Fullscreen */
                for(c=head;c;c=c->next) {
                    XMoveResizeWindow(dis,c->win,0,y,sw+2*BORDER_WIDTH,sh+2*BORDER_WIDTH);
                }
                break;
            case 2: /* Horizontal */
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
            case 3: { // Grid
                int xpos = 0;
                int wdt = 0;
                int ht = 0;
                int nwin = 0;

                for(c=head;c;c=c->next) {
                    ++nwin;
                    if((nwin == 1) || (nwin == 3) || (nwin == 5) || (nwin == 7))
                        x += 1;
                }
                for(c=head;c;c=c->next) {
                    ++n;
                    if(x == 4) {
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
                        if((n == nwin) && (n == 7))
                            wdt = sw - BORDER_WIDTH;
                        if((n == nwin) && (n == 8))
                            wdt = 2*sw/3 - BORDER_WIDTH;
                    } else 
                    if(x == 3) {
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
                        if((n == nwin) && (n == 5))
                            wdt = 2*sw/3 - BORDER_WIDTH;

                    } else {
                        if(n > 2)
                            ht = (sh/x) - 2*BORDER_WIDTH;
                        else
                            ht = (sh/x)-BORDER_WIDTH;
                        if((n == 1) || (n == 3)) {
                            xpos = 0;
                            wdt = master_size - BORDER_WIDTH;
                        }
                        if((n == 2) || (n == 4)) {
                            xpos = master_size+BORDER_WIDTH;
                            wdt = (sw - master_size) - 2*BORDER_WIDTH;
                        }
                        if(n == 3)
                            y += (sh/x)+BORDER_WIDTH;
                        if((n == nwin) && (n == 3))
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

void update_current() {
    client *c;

    for(c=head;c;c=c->next) {
        if((head->next == NULL) || (mode == 1))
            XSetWindowBorderWidth(dis,c->win,0);
        else
            XSetWindowBorderWidth(dis,c->win,BORDER_WIDTH);

        if(current == c) {
            // "Enable" current window
            XSetWindowBorder(dis,c->win,win_focus);
            XSetInputFocus(dis,c->win,RevertToParent,CurrentTime);
            XRaiseWindow(dis,c->win);
        }
        else
            XSetWindowBorder(dis,c->win,win_unfocus);
    }
    XSync(dis, False);
}

void switch_vertical() {
    if(mode != 0) {
        mode = 0;
        master_size = sw * MASTER_SIZE;
	tile();
        update_current();
    }
}

void switch_fullscreen() {
    if(mode != 1) {
        mode = 1;
        tile();
        update_current();
    }
}

void switch_horizontal() {
    if(mode != 2) {
        mode = 2;
        master_size = sh * MASTER_SIZE;
        tile();
        update_current();
    }
}

void switch_grid() {
    if(mode != 3) {
        mode = 3;
        master_size = sw * MASTER_SIZE;
        tile();
        update_current();
    }
}

void grow_window() {
    growth += 10;
    tile();
}

void shrink_window() {
    growth -= 10;
    tile();
}

/* ********************** Keyboard Management ********************** */
void grabkeys() {
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

void configurenotify(XEvent *e) {
    // Do nothing for the moment
}

/* ********************** Signal Management ************************** */
void configurerequest(XEvent *e) {
    // Paste from DWM, thx again \o/
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
    Window dialog_window;
    
    // For dialog windows
    XGetTransientForHint(dis, ev->window, &dialog_window);
        if(dialog_window != 0) {
            add_window(ev->window);
            XMapWindow(dis, ev->window);
            XSetInputFocus(dis,ev->window,RevertToParent,CurrentTime);
            XRaiseWindow(dis,ev->window);
            return;
        }

    // For fullscreen mplayer (and maybe some other program)
    client *c;
    
    for(c=head;c;c=c->next)
        if(ev->window == c->win) {
            XMapWindow(dis,ev->window);
            XMoveResizeWindow(dis,c->win,-BORDER_WIDTH,-BORDER_WIDTH,sw+BORDER_WIDTH,sh+BORDER_WIDTH);
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
    XColor c;
    Colormap map = DefaultColormap(dis,screen);

    if(!XAllocNamedColor(dis,map,color,&c,&c)) {
        logger("\033[0;31mError parsing color!");
        exit(1);
    }
    return c.pixel;
}

void quit() {
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
        logger(" \033[0;33mThanks for using!");
        XCloseDisplay(dis);
        logger("\033[0;31mforced shutdown");
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
    logger("\033[0;34mYou Quit : Thanks for using!");
}

void logger(const char* e) {
    fprintf(stdout,"\n\033[0;34m:: dminiwm : %s \033[0;m\n", e);
}
 
void setup() {
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

    // Default stack
    mode = DEFAULT_MODE;
    growth = 0;

    // For exiting
    bool_quit = 0;

    // List of client
    head = NULL;
    current = NULL;

    // Master size
    if(mode == 2)
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
    // To catch maprequest and destroynotify (if other wm running)
    XSelectInput(dis,root,SubstructureNotifyMask|SubstructureRedirectMask);
    XSetErrorHandler(xerror);
    logger("\033[0;32mWe're up and running!");
}

void sigchld(int unused) {
    // Again, thx to dwm ;)
	if(signal(SIGCHLD, sigchld) == SIG_ERR) {
		logger("\033[0;31mCan't install SIGCHLD handler");
		exit(1);
        }
	while(0 < waitpid(-1, NULL, WNOHANG));
}

void spawn(const Arg arg) {
    if(fork() == 0) {
        if(fork() == 0) {
            if(dis)
                close(ConnectionNumber(dis));

            setsid();
            execvp((char*)arg.com[0],(char**)arg.com);
        }
        exit(0);
    }
}

/* There's no way to check accesses to destroyed windows, thus those cases are ignored (especially on UnmapNotify's).  Other types of errors call Xlibs default error handler, which may call exit.  */
int xerror(Display *dis, XErrorEvent *ee) {
    if(ee->error_code == BadWindow
	|| (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	|| (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	|| (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	|| (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	|| (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	|| (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
    logger("\033[0;31mBad Window Error!");
    return xerrorxlib(dis, ee); /* may call exit */
}

void start() {
    XEvent ev;

    // Main loop, just dispatch events (thx to dwm ;)
    while(!bool_quit && !XNextEvent(dis,&ev)) {
        if(events[ev.type])
            events[ev.type](&ev);
    }
}


int main(int argc, char **argv) {
    // Open display   
    if(!(dis = XOpenDisplay(NULL))) {
        logger("\033[0;31mCannot open display!");
        exit(1);
    }

    // Setup env
    setup();

    // Start wm
    start();

    // Close display
    XCloseDisplay(dis);

    return 0;
}
