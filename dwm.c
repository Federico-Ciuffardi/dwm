/* See LICENSE file for copyright and license details.
 *
 * dynamic window manager is designed like any other X client as well. It is
 * driven through handling X events. In contrast to other X clients, a window
 * manager selects for SubstructureRedirectMask on the root window, to receive
 * events about window (dis-)appearance. Only one X connection at a time is
 * allowed to select for this event mask.
 *
 * The event handlers of dwm are organized in an array which is accessed
 * whenever a new event has been fetched. This allows event dispatching
 * in O(1) time.
 *
 * Each child of the root window is called a client, except windows which have
 * set the override_redirect flag. Clients are organized in a linked client
 * list on each monitor, the focus history is remembered through a stack list
 * on each monitor. Each client contains a bit array to indicate the tags of a
 * client.
 *
 * Keys and tagging rules are organized as arrays and defined in config.h.
 *
 * To understand everything else, start reading main().
 */
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif /* XINERAMA */
#include <X11/Xft/Xft.h>
#include <X11/Xlib-xcb.h>
#include <xcb/res.h>

#include "drw.h"
#include "util.h"

/* macros */
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
#define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
    * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
#define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]) || C->issticky)
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define WIDTH(X)                ((X)->w + 2 * (X)->bw)
#define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
#define TAGMASK                 ((1 << LENGTH(tags)) - 1)
#define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

#define SYSTEM_TRAY_REQUEST_DOCK    0

/* XEMBED messages */
#define XEMBED_EMBEDDED_NOTIFY      0
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_FOCUS_IN             4
#define XEMBED_MODALITY_ON         10

#define XEMBED_MAPPED              (1 << 0)
#define XEMBED_WINDOW_ACTIVATE      1
#define XEMBED_WINDOW_DEACTIVATE    2

#define VERSION_MAJOR               0
#define VERSION_MINOR               0
#define XEMBED_EMBEDDED_VERSION (VERSION_MAJOR << 16) | VERSION_MINOR

/* enums */
enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
enum { SchemeNorm, SchemeSel, SchemeNormTab, SchemeSelTab, SchemeSelAlt}; /* color schemes */
enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
  NetSystemTray, NetSystemTrayOP, NetSystemTrayOrientation, NetSystemTrayOrientationHorz,
  NetWMFullscreen, NetActiveWindow, NetWMWindowType,
  NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
enum { Manager, Xembed, XembedInfo, XLast }; /* Xembed atoms */
enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
enum { ClkTagBar, ClkTabBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
  ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  unsigned int click;
  unsigned int mask;
  unsigned int button;
  void (*func)(const Arg *arg);
  const Arg arg;
} Button;

typedef struct Monitor Monitor;
typedef struct Client Client;
struct Client {
  char name[256];
  int sp_id;
  float mina, maxa;
  int x, y, w, h;
  int floatx, floaty, floatw, floath;
  int ofloatzonex, ofloatzoney; 
  int floatzonex, floatzoney; 
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh;
  int bw, oldbw;
  unsigned int tags;
  int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen, issticky, isterminal, noswallow;
  pid_t pid;
  Client *next;
  Client *snext;
  Client *swallowing;
  Monitor *mon;
  Window win;
};

typedef struct {
  unsigned int mod;
  KeySym keysym;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
} Layout;

typedef struct Pertag Pertag;

#define MAXTABS 50

struct Monitor {
  char ltsymbol[16];
  float mfact;
  int nmaster;
  int num;
  int by;               /* bar geometry */
  int ty;               /* tab bar geometry */
  int mx, my, mw, mh;   /* screen size */
  int wx, wy, ww, wh;   /* window area  */
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  int showbar;
  int showtab;
  int topbar;
  int toptab;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;
  Window barwin;
  Window tabwin;
  int ntabs;
  const Layout *lt[2];
  Pertag *pertag;
  int tab_width;
};

typedef struct {
  const char *class;
  const char *instance;
  const char *title;
  unsigned int tags;
  int isfloating;
  int floatx, floaty, floatw, floath;
  int floatzonex, floatzoney;
  int isterminal;
  int noswallow;
  int sp_id;
  int monitor;
} Rule;

typedef struct Systray   Systray;
struct Systray {
  Window win;
  Client *icons;
};

/* function declarations */
static void applyrules(Client *c);
static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
static void arrange(Monitor *m);
static void arrangemon(Monitor *m);
static void attach(Client *c);
static void attachstack(Client *c);
static void buttonpress(XEvent *e);
static void checkotherwm(void);
static void cleanup(void);
static void cleanupmon(Monitor *mon);
static void clientmessage(XEvent *e);
static void configure(Client *c);
static void configurenotify(XEvent *e);
static void configurerequest(XEvent *e);
static void copyvalidchars(char *text, char *rawtext);
static Monitor *createmon(void);
static void destroynotify(XEvent *e);
static void detach(Client *c);
static void detachstack(Client *c);
static Monitor *dirtomon(int dir);
static void drawbar(Monitor *m);
static void drawbars(void);
static void drawtab(Monitor *m);
static void drawtabs(void);
static void enqueue(Client *c);
static void enqueuestack(Client *c);
static void enternotify(XEvent *e);
static void expose(XEvent *e);
static void focus(Client *c);
static void focusin(XEvent *e);
static void focusmon(const Arg *arg);
static void focusstack(const Arg *arg);
static void swappos(const Arg *arg);
static void togglefocustype(const Arg *arg);
static int getdwmblockspid();
static void focuswin(const Arg* arg);
static void killwin(const Arg* arg);
static Atom getatomprop(Client *c, Atom prop);
static int getrootptr(int *x, int *y);
static long getstate(Window w);
static unsigned int getsystraywidth();
static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
static void grabbuttons(Client *c, int focused);
static void grabkeys(void);
static void incnmaster(const Arg *arg);
static void keypress(XEvent *e);
static void killclient(const Arg *arg);
static void manage(Window w, XWindowAttributes *wa);
static void mappingnotify(XEvent *e);
static void maprequest(XEvent *e);
static void monocle(Monitor *m);
static void motionnotify(XEvent *e);
static void movemouse(const Arg *arg);
static Client *nexttiled(Client *c);
static void pop(Client *);
static Client *prevtiled(Client *c);
static void propertynotify(XEvent *e);
static void pushdown(const Arg *arg);
static void pushup(const Arg *arg);
static void quit(const Arg *arg);
static Monitor *recttomon(int x, int y, int w, int h);
static void removesystrayicon(Client *i);
static void reorganizetags(const Arg *arg);
static void resize(Client *c, int x, int y, int w, int h, int interact);
static void resizebarwin(Monitor *m);
static void resizeclient(Client *c, int x, int y, int w, int h);
static void resizemouse(const Arg *arg);
static void resizerequest(XEvent *e);
static void restack(Monitor *m);
static void rotatestack(const Arg *arg);
static void rotate(const Arg *arg);
static void run(void);
static void scan(void);
static int sendevent(Window w, Atom proto, int m, long d0, long d1, long d2, long d3, long d4);
static void sendtomon(Client *c, Monitor *m, int master, int preserve);
static void sendmon(Client *c, Monitor *m);
static void setclientstate(Client *c, long state);
static void setfocus(Client *c);
static void setfullscreen(Client *c, int fullscreen);
static void cyclelayout(const Arg *arg);
static void setlayout(const Arg *arg);
static void setmfact(const Arg *arg);
static void setup(void);
static void seturgent(Client *c, int urg);
static void showhide(Client *c);
static void sigchld(int unused);
static void sigdwmblocks(const Arg *arg);
static void spawn(const Arg *arg);
static void togglesp(const Arg *arg);
static Monitor *systraytomon(Monitor *m);
static void tag(const Arg *arg);
static void tagmon(const Arg *arg);
static void tile(Monitor *);
static void togglebar(const Arg *arg);
static void togglefloatingcore(int);
static void togglefloating(const Arg *arg);
static void togglefullfullscreen(const Arg *arg);
static void togglefullscreen(const Arg *arg);
static void togglesticky(const Arg *arg);
static void toggletag(const Arg *arg);
static void toggleview(const Arg *arg);
static void lastfreetag(const Arg *arg);
static void freeview(const Arg *arg);
static void freetag(const Arg *arg);
static void incview(const Arg *arg);
static void lastfreeview();
static void unfocus(Client *c, int setfocus);
static void unmanage(Client *c, int destroyed);
static void unmapnotify(XEvent *e);
static void updatebarpos(Monitor *m);
static void updatebars(void);
static void updateclientlist(void);
static int updategeom(void);
static void updatenumlockmask(void);
static void updatesizehints(Client *c);
static void updatestatus(void);
static void updatesystray(void);
static void updatesystrayicongeom(Client *i, int w, int h);
static void updatesystrayiconstate(Client *i, XPropertyEvent *ev);
static void updatetitle(Client *c);
static void updatewindowtype(Client *c);
static void updatewmhints(Client *c);
static void view(const Arg *arg);
static void warp(const Client *c);
static Client *wintoclient(Window w);
static Monitor *wintomon(Window w);
static Client *wintosystrayicon(Window w);
static int xerror(Display *dpy, XErrorEvent *ee);
static int xerrordummy(Display *dpy, XErrorEvent *ee);
static int xerrorstart(Display *dpy, XErrorEvent *ee);
static void zoom(const Arg *arg);
static void horizontalfocus(const Arg *arg);
static void autostart_exec(void);
static void grid(Monitor *m);
static void movevertical(const Arg* arg);
static void movehorizontal(const Arg* arg);
static void resizevertical(const Arg* arg);
static void resizehorizontal(const Arg* arg);
static void hardresizehorizontal(const Arg* arg);

static pid_t getparentprocess(pid_t p);
static int isdescprocess(pid_t p, pid_t c);
static Client *swallowingclient(Window w);
static Client *termforwin(const Client *c);
static pid_t winpid(Window w);


/* variables */
unsigned int cols,rows;
static int warping = 0;
static Systray *systray =  NULL;
static const char broken[] = "broken";
static char stext[256];
static char rawstext[256];
static int dwmblockssig;
pid_t dwmblockspid = 0;
static int screen;
static int sw, sh;           /* X display screen geometry width, height */
static int bh, blw = 0;      /* bar geometry */
static int th = 0;           /* tab bar geometry */
static int lrpad;            /* sum of left and right padding for text */
static int (*xerrorxlib)(Display *, XErrorEvent *);
static unsigned int numlockmask = 0;
static void (*handler[LASTEvent]) (XEvent *) = {
  [ButtonPress] = buttonpress,
  [ClientMessage] = clientmessage,
  [ConfigureRequest] = configurerequest,
  [ConfigureNotify] = configurenotify,
  [DestroyNotify] = destroynotify,
  [EnterNotify] = enternotify,
  [Expose] = expose,
  [FocusIn] = focusin,
  [KeyPress] = keypress,
  [MappingNotify] = mappingnotify,
  [MapRequest] = maprequest,
  [MotionNotify] = motionnotify,
  [PropertyNotify] = propertynotify,
  [ResizeRequest] = resizerequest,
  [UnmapNotify] = unmapnotify
};
static Atom wmatom[WMLast], netatom[NetLast], xatom[XLast];
static int running = 1;
static Cur *cursor[CurLast];
static Clr **scheme;
static Display *dpy;
static Drw *drw;
static Monitor *mons, *selmon;
static Window root, wmcheckwin;

static xcb_connection_t *xcon;

/* configuration, allows nested code to access above variables */
#include "config.h"

struct Pertag {
  unsigned int curtag, prevtag; /* current and previous tag */
  int nmasters[LENGTH(tags) + 1]; /* number of windows in master area */
  float mfacts[LENGTH(tags) + 1]; /* mfacts per tag */
  unsigned int sellts[LENGTH(tags) + 1]; /* selected layouts */
  const Layout *ltidxs[LENGTH(tags) + 1][2]; /* matrix of tags and layouts indexes  */
  int showbars[LENGTH(tags) + 1]; /* display bar for the current tag */
};

/* compile-time check if all tags fit into an unsigned int bit array. */
struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

/* dwm will keep pid's of processes from autostart array and kill them at quit */
static pid_t *autostart_pids;
static size_t autostart_len;

/* execute command from autostart array */
static void
autostart_exec() {
  const char *const *p;
  size_t i = 0;

  /* count entries */
  for (p = autostart; *p; autostart_len++, p++)
    while (*++p);

  autostart_pids = malloc(autostart_len * sizeof(pid_t));
  for (p = autostart; *p; i++, p++) {
    if ((autostart_pids[i] = fork()) == 0) {
      setsid();
      execvp(*p, (char *const *)p);
      fprintf(stderr, "dwm: execvp %s\n", *p);
      perror(" failed");
      _exit(EXIT_FAILURE);
    }
    /* skip arguments */
    while (*++p);
  }
}

/* function implementations */

void
setsticky(Client *c, int sticky){
  c->issticky = sticky;
  if(sticky)
    c->tags = 0;
  else
    c->tags = selmon->tagset[selmon->seltags];
}

  void
applyrules(Client *c)
{
  const char *class, *instance;
  unsigned int i;
  const Rule *r;
  Monitor *m;
  XClassHint ch = { NULL, NULL };

  /* rule matching */
  c->isfloating = 0;
  c->ofloatzonex = c->floatzonex = c->ofloatzoney = c->floatzoney = -1;
  if(!c->issticky)
    c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
  else
    c->tags = 0;
  XGetClassHint(dpy, c->win, &ch);
  class    = ch.res_class ? ch.res_class : broken;
  instance = ch.res_name  ? ch.res_name  : broken;

  for (i = 0; i < LENGTH(rules); i++) {
    r = &rules[i];
    if ((!r->title || strstr(c->name, r->title))
        && (!r->class || strstr(class, r->class))
        && (!r->instance || strstr(instance, r->instance)))
    {
      c->sp_id = r->sp_id;
      c->isterminal = r->isterminal;
      c->isfloating = r->isfloating;
      c->noswallow  = r->noswallow;
      c->floatx = r->floatx;
      c->floaty = r->floaty;
      c->floatw = r->floatw;
      c->floath = r->floath;
      c->floatzonex = c->ofloatzonex = r->floatzonex;
      c->floatzoney = c->ofloatzoney = r->floatzoney;
      if(r->tags == ~0 || c->tags == TAGMASK )
        setsticky(c, 1);
      else if(r->tags)
        c->tags = r->tags & TAGMASK;

      for (m = mons; m && m->num != r->monitor; m = m->next);
      if (m)
        c->mon = m;
    }
  }
  if (ch.res_class)
    XFree(ch.res_class);
  if (ch.res_name)
    XFree(ch.res_name);
}

  int
applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
{
  int baseismin;
  Monitor *m = c->mon;

  /* set minimum possible */
  *w = MAX(1, *w);
  *h = MAX(1, *h);
  if (interact) {
    if (*x > sw)
      *x = sw - WIDTH(c);
    if (*y > sh)
      *y = sh - HEIGHT(c);
    if (*x + *w + 2 * c->bw < 0)
      *x = 0;
    if (*y + *h + 2 * c->bw < 0)
      *y = 0;
  } else {
    if (*x >= m->wx + m->ww)
      *x = m->wx + m->ww - WIDTH(c);
    if (*y >= m->wy + m->wh)
      *y = m->wy + m->wh - HEIGHT(c);
    if (*x + *w + 2 * c->bw <= m->wx)
      *x = m->wx;
    if (*y + *h + 2 * c->bw <= m->wy)
      *y = m->wy;
  }
  if (*h < bh)
    *h = bh;
  if (*w < bh)
    *w = bh;
  if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
    /* see last two sentences in ICCCM 4.1.2.3 */
    baseismin = c->basew == c->minw && c->baseh == c->minh;
    if (!baseismin) { /* temporarily remove base dimensions */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for aspect limits */
    if (c->mina > 0 && c->maxa > 0) {
      if (c->maxa < (float)*w / *h)
        *w = *h * c->maxa + 0.5;
      else if (c->mina < (float)*h / *w)
        *h = *w * c->mina + 0.5;
    }
    if (baseismin) { /* increment calculation requires this */
      *w -= c->basew;
      *h -= c->baseh;
    }
    /* adjust for increment value */
    if (c->incw)
      *w -= *w % c->incw;
    if (c->inch)
      *h -= *h % c->inch;
    /* restore base dimensions */
    *w = MAX(*w + c->basew, c->minw);
    *h = MAX(*h + c->baseh, c->minh);
    if (c->maxw)
      *w = MIN(*w, c->maxw);
    if (c->maxh)
      *h = MIN(*h, c->maxh);
  }
  return *x != c->x || *y != c->y || *w != c->w || *h != c->h;
}

  void
arrange(Monitor *m)
{
  if (m)
    showhide(m->stack);
  else for (m = mons; m; m = m->next)
    showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else for (m = mons; m; m = m->next)
    arrangemon(m);
}

void
arrangemon(Monitor *m) {
  updatebarpos(m);
  XMoveResizeWindow(dpy, m->tabwin, m->wx, m->ty, m->ww, th);
  strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, sizeof m->ltsymbol);
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

  void
attach(Client *c)
{
  c->next = c->mon->clients;
  c->mon->clients = c;
}

  void
attachstack(Client *c)
{
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

  void
swallow(Client *p, Client *c)
{
  if (c->noswallow || c->isterminal)
    return;

  detach(c);
  detachstack(c);

  setclientstate(c, WithdrawnState);
  XUnmapWindow(dpy, p->win);

  p->swallowing = c;
  c->mon = p->mon;

  Window w = p->win;
  p->win = c->win;
  c->win = w;
  updatetitle(p);
  arrange(p->mon);
  XMoveResizeWindow(dpy, p->win, p->x, p->y, p->w, p->h);
  configure(p);
  updateclientlist();
}

  void
unswallow(Client *c)
{
  c->win = c->swallowing->win;

  free(c->swallowing);
  c->swallowing = NULL;

  updatetitle(c);
  arrange(c->mon);
  XMapWindow(dpy, c->win);
  XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
  configure(c);
  setclientstate(c, NormalState);
  focus(c);
}

  void
buttonpress(XEvent *e)
{
  int stw = 0;
 	unsigned int i, x, click, occ = 0;
  Arg arg = {0};
  Client *c;
  Monitor *m;
  XButtonPressedEvent *ev = &e->xbutton;

  click = ClkRootWin;
  /* focus monitor if necessary */
  if ((m = wintomon(ev->window)) && m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }

  if(showsystray && m == systraytomon(m))
    stw = getsystraywidth();

  if (ev->window == selmon->barwin) {
    i = x = 0;
 		for (c = m->clients; c && hidevacant; c = c->next)
 			occ |= c->tags == 255 ? 0 : c->tags;
 		do {
 			/* do not reserve space for vacant tags */
 			if (hidevacant && !(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
 				continue;
 			x += TEXTW(tags[i]);
 		} while (ev->x >= x && ++i < LENGTH(tags));
    if (i < LENGTH(tags)) {
      click = ClkTagBar;
      arg.ui = 1 << i;
    } else if (ev->x < x + blw)
      click = ClkLtSymbol;
    else if (ev->x > (x = selmon->ww - TEXTW(stext) - stw + lrpad)){
      click = ClkStatusText;

      char *text = rawstext;
      int i = -1;
      char ch;
      dwmblockssig = 0;
      while (text[++i]) {
        if ((unsigned char)text[i] < ' ') {
          ch = text[i];
          text[i] = '\0';
          x += TEXTW(text) - lrpad - 2;
          text[i] = ch;
          text += i+1;
          i = -1;
          if (x >= ev->x) break;
          dwmblockssig = ch;
        }
      }
    } else
      click = ClkWinTitle;
  }
  if(ev->window == selmon->tabwin) {
    i = 0; x = 0;
    for(c = selmon->clients; c; c = c->next){
      if(!ISVISIBLE(c)) continue;
      x += selmon->tab_width;
      if (ev->x > x)
        ++i;
      else
        break;
      if(i >= m->ntabs) break;
    }
    if(c) {
      click = ClkTabBar;
      arg.ui = i;
    }
  }
  else if((c = wintoclient(ev->window))) {
    focus(c);
    restack(selmon);
    XAllowEvents(dpy, ReplayPointer, CurrentTime);
    click = ClkClientWin;
  }
  for (i = 0; i < LENGTH(buttons); i++)
    if (click == buttons[i].click && buttons[i].func && buttons[i].button == ev->button
        && CLEANMASK(buttons[i].mask) == CLEANMASK(ev->state)){
      buttons[i].func(((click == ClkTagBar || click == ClkTabBar) && buttons[i].arg.i == 0) ? &arg : &buttons[i].arg);
    }
}

  void
checkotherwm(void)
{
  xerrorxlib = XSetErrorHandler(xerrorstart);
  /* this causes an error if some other window manager is running */
  XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(xerror);
  XSync(dpy, False);
}

  void
cleanup(void)
{
  Arg a = {.ui = ~0};
  Layout foo = { "", NULL };
  Monitor *m;
  size_t i;

  view(&a);
  selmon->lt[selmon->sellt] = &foo;
  for (m = mons; m; m = m->next)
    while (m->stack)
      unmanage(m->stack, 0);
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  while (mons)
    cleanupmon(mons);
  if (showsystray) {
    XUnmapWindow(dpy, systray->win);
    XDestroyWindow(dpy, systray->win);
    free(systray);
  }
  for (i = 0; i < CurLast; i++)
    drw_cur_free(drw, cursor[i]);
  for (i = 0; i < LENGTH(colors); i++)
    free(scheme[i]);
  XDestroyWindow(dpy, wmcheckwin);
  drw_free(drw);
  XSync(dpy, False);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
}

  void
cleanupmon(Monitor *mon)
{
  Monitor *m;

  if (mon == mons)
    mons = mons->next;
  else {
    for (m = mons; m && m->next != mon; m = m->next);
    m->next = mon->next;
  }
  XUnmapWindow(dpy, mon->barwin);
  XDestroyWindow(dpy, mon->barwin);
  XUnmapWindow(dpy, mon->tabwin);
  XDestroyWindow(dpy, mon->tabwin);
  free(mon);
}

  void
clientmessage(XEvent *e)
{
  XWindowAttributes wa;
  XSetWindowAttributes swa;
  XClientMessageEvent *cme = &e->xclient;
  Client *c = wintoclient(cme->window);

  if (showsystray && cme->window == systray->win && cme->message_type == netatom[NetSystemTrayOP]) {
    /* add systray icons */
    if (cme->data.l[1] == SYSTEM_TRAY_REQUEST_DOCK) {
      if (!(c = (Client *)calloc(1, sizeof(Client))))
        die("fatal: could not malloc() %u bytes\n", sizeof(Client));
      if (!(c->win = cme->data.l[2])) {
        free(c);
        return;
      }
      c->mon = selmon;
      c->next = systray->icons;
      systray->icons = c;
      if (!XGetWindowAttributes(dpy, c->win, &wa)) {
        /* use sane defaults */
        wa.width = bh;
        wa.height = bh;
        wa.border_width = 0;
      }
      c->x = c->oldx = c->y = c->oldy = 0;
      c->w = c->oldw = wa.width;
      c->h = c->oldh = wa.height;
      c->oldbw = wa.border_width;
      c->bw = 0;
      c->isfloating = True;
      /* reuse tags field as mapped status */
      c->tags = 1;
      updatesizehints(c);
      updatesystrayicongeom(c, wa.width, wa.height);
      XAddToSaveSet(dpy, c->win);
      XSelectInput(dpy, c->win, StructureNotifyMask | PropertyChangeMask | ResizeRedirectMask);
      XReparentWindow(dpy, c->win, systray->win, 0, 0);
      /* use parents background color */
      swa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
      XChangeWindowAttributes(dpy, c->win, CWBackPixel, &swa);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_EMBEDDED_NOTIFY, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      /* FIXME not sure if I have to send these events, too */
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_FOCUS_IN, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      sendevent(c->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_MODALITY_ON, 0 , systray->win, XEMBED_EMBEDDED_VERSION);
      XSync(dpy, False);
      resizebarwin(selmon);
      updatesystray();
      setclientstate(c, NormalState);
    }
    return;
  }
  if (!c)
    return;
  if (cme->message_type == netatom[NetWMState]) {
    if (cme->data.l[1] == netatom[NetWMFullscreen]
        || cme->data.l[2] == netatom[NetWMFullscreen])
      setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
            || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
  } else if (cme->message_type == netatom[NetActiveWindow]) {
    uint i = 0;
    if(!c->issticky){
      for (i = 0; i < LENGTH(tags) && !((1 << i) & c->tags); i++);
      if (i >= LENGTH(tags)) return;
    }
    if (c->mon != selmon)
      unfocus(selmon->sel, 0);
    else if (c->mon->sel && c->mon->sel->isfullscreen)
      return;
    selmon = c->mon;
    if(!c->issticky){
      const Arg a = {.ui = 1 << i};
      view(&a);
    }
    focus(c);
    restack(selmon);
  }
}

  void
configure(Client *c)
{
  XConfigureEvent ce;

  ce.type = ConfigureNotify;
  ce.display = dpy;
  ce.event = c->win;
  ce.window = c->win;
  ce.x = c->x;
  ce.y = c->y;
  ce.width = c->w;
  ce.height = c->h;
  ce.border_width = c->bw;
  ce.above = None;
  ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

  void
configurenotify(XEvent *e)
{
  Monitor *m;
  Client *c;
  XConfigureEvent *ev = &e->xconfigure;
  int dirty;

  /* TODO: updategeom handling sucks, needs to be simplified */
  if (ev->window == root) {
    dirty = (sw != ev->width || sh != ev->height);
    sw = ev->width;
    sh = ev->height;
    if (updategeom() || dirty) {
      drw_resize(drw, sw, bh);
      updatebars();
      for (m = mons; m; m = m->next) {
        for (c = m->clients; c; c = c->next)
          if (c->isfullscreen)
            resizeclient(c, m->mx, m->my, m->mw, m->mh);
        resizebarwin(m);
      }
      focus(NULL);
      arrange(NULL);
    }
  }
}

  void
configurerequest(XEvent *e)
{
  Client *c;
  Monitor *m;
  XConfigureRequestEvent *ev = &e->xconfigurerequest;
  XWindowChanges wc;

  if ((c = wintoclient(ev->window))) {
    if (ev->value_mask & CWBorderWidth)
      c->bw = ev->border_width;
    else if (c->isfloating || !selmon->lt[selmon->sellt]->arrange) {
      m = c->mon;
      if (ev->value_mask & CWX) {
        c->oldx = c->x;
        c->x = m->mx + ev->x;
      }
      if (ev->value_mask & CWY) {
        c->oldy = c->y;
        c->y = m->my + ev->y;
      }
      if (ev->value_mask & CWWidth) {
        c->oldw = c->w;
        c->w = ev->width;
      }
      if (ev->value_mask & CWHeight) {
        c->oldh = c->h;
        c->h = ev->height;
      }
      if ((c->x + c->w) > m->mx + m->mw && c->isfloating)
        c->x = m->mx + (m->mw / 2 - WIDTH(c) / 2); /* center in x direction */
      if ((c->y + c->h) > m->my + m->mh && c->isfloating)
        c->y = m->my + (m->mh / 2 - HEIGHT(c) / 2); /* center in y direction */
      if ((ev->value_mask & (CWX|CWY)) && !(ev->value_mask & (CWWidth|CWHeight)))
        configure(c);
      if (ISVISIBLE(c))
        XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
    } else
      configure(c);
  } else {
    wc.x = ev->x;
    wc.y = ev->y;
    wc.width = ev->width;
    wc.height = ev->height;
    wc.border_width = ev->border_width;
    wc.sibling = ev->above;
    wc.stack_mode = ev->detail;
    XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
  }
  XSync(dpy, False);
}

  void
copyvalidchars(char *text, char *rawtext)
{
  int i = -1, j = 0;

  while(rawtext[++i]) {
    if ((unsigned char)rawtext[i] >= ' ') {
      text[j++] = rawtext[i];
    }
  }
  text[j] = '\0';
}

  Monitor *
createmon(void)
{
  Monitor *m;
  unsigned int i;

  m = ecalloc(1, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->showtab = showtab;
  m->topbar = topbar;
  m->toptab = toptab;
  m->ntabs = 0;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  strncpy(m->ltsymbol, layouts[0].symbol, sizeof m->ltsymbol);
  m->pertag = ecalloc(1, sizeof(Pertag));
  m->pertag->curtag = m->pertag->prevtag = 1;

  for (i = 0; i <= LENGTH(tags); i++) {
    m->pertag->nmasters[i] = m->nmaster;
    m->pertag->mfacts[i] = m->mfact;

    m->pertag->ltidxs[i][0] = m->lt[0];
    m->pertag->ltidxs[i][1] = m->lt[1];
    m->pertag->sellts[i] = m->sellt;

    m->pertag->showbars[i] = m->showbar;
  }

  return m;
}

  void
destroynotify(XEvent *e)
{
  Client *c;
  XDestroyWindowEvent *ev = &e->xdestroywindow;

  if ((c = wintoclient(ev->window)))
    unmanage(c, 1);
  else if ((c = swallowingclient(ev->window)))
    unmanage(c->swallowing, 1);
  else if ((c = wintosystrayicon(ev->window))) {
    removesystrayicon(c);
    resizebarwin(selmon);
    updatesystray();
  }
}

  void
detach(Client *c)
{
  Client **tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next);
  *tc = c->next;
}

  void
detachstack(Client *c)
{
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext);
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext);
    c->mon->sel = t;
  }
}

  Monitor *
dirtomon(int dir)
{
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next);
  else
    for (m = mons; m->next != selmon; m = m->next);
  return m;
}

void
cyclelayout(const Arg *arg) {
	Layout *l;
	for(l = (Layout *)layouts; l != selmon->lt[selmon->sellt]; l++);
	if(arg->i > 0) {
		if(l->symbol && (l + 1)->symbol)
			setlayout(&((Arg) { .v = (l + 1) }));
		else
			setlayout(&((Arg) { .v = layouts }));
	} else {
		if(l != layouts && (l - 1)->symbol)
			setlayout(&((Arg) { .v = (l - 1) }));
		else
			setlayout(&((Arg) { .v = &layouts[LENGTH(layouts) - 2] }));
	}
}



  void
drawbar(Monitor *m)
{
  int x, w, sw = 0, stw = 0;
  int boxs = drw->fonts->h / 9;
  int boxw = drw->fonts->h / 6 + 2;
  unsigned int i, occ = 0, urg = 0;
  Client *c;

  if(showsystray && m == systraytomon(m))
    stw = getsystraywidth();

  /* draw status first so it can be overdrawn by tags later */
  if (m == selmon || 1) { /* status is only drawn on selected monitor */
    drw_setscheme(drw, scheme[SchemeNorm]);
 		sw = TEXTW(stext) - lrpad / 2;
 		drw_text(drw, m->ww - sw - stw, 0, sw, bh, lrpad / 2, stext, 0);
  }

  resizebarwin(m);
  for (c = m->clients; c; c = c->next) {
 		occ |= c->tags == 255 ? 0 : c->tags;
    if (c->isurgent)
      urg |= c->tags;
  }
  x = 0;
  for (i = 0; i < LENGTH(tags); i++) {
 		/* do not draw vacant tags */
 		if (hidevacant && !(occ & 1 << i || m->tagset[m->seltags] & 1 << i))
      continue;
 
    w = TEXTW(tags[i]);
    drw_setscheme(drw, scheme[m->tagset[m->seltags] & 1 << i ? (m == selmon ? SchemeSel : SchemeSelAlt) : SchemeNorm]);
    drw_text(drw, x, 0, w, bh, lrpad / 2, tags[i], urg & 1 << i);
		if (uline && (ulineall || m->tagset[m->seltags] & 1 << i)) /* if there are conflicts, just move these lines directly underneath both 'drw_setscheme' and 'drw_text' :) */
			drw_rect(drw, x + ulinepad, bh - ulinestroke - ulinevoffset, w - (ulinepad * 2), ulinestroke, 1, 0);
    if (hidevacant < 2 && occ & 1 << i)
      drw_rect(drw, x + boxs, boxs, boxw, boxw,
          m == selmon && selmon->sel && selmon->sel->tags & 1 << i,
          urg & 1 << i);
    x += w;
  }
  w = blw = TEXTW(m->ltsymbol);
  drw_setscheme(drw, scheme[SchemeNorm]);
  x = drw_text(drw, x, 0, w, bh, lrpad / 2, m->ltsymbol, 0);

  if ((w = m->ww - sw - stw - x) > bh) {
    if (m->sel) {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_text(drw, x, 0, w, bh, lrpad / 2, m->sel->name, 0);
      if (m->sel->isfloating)
        drw_rect(drw, x + boxs, boxs, boxw, boxw, m->sel->isfixed, 0);
    } else {
      drw_setscheme(drw, scheme[SchemeNorm]);
      drw_rect(drw, x, 0, w, bh, 1, 1);
    }
  }
  drw_map(drw, m->barwin, 0, 0, m->ww - stw, bh);
}

  void
drawbars(void)
{
  Monitor *m;

  for (m = mons; m; m = m->next)
    drawbar(m);
}

void
drawtabs(void) {
  Monitor *m;

  for(m = mons; m; m = m->next)
    drawtab(m);
}

static int
cmpint(const void *p1, const void *p2) {
  /* The actual arguments to this function are "pointers to
     pointers to char", but strcmp(3) arguments are "pointers
     to char", hence the following cast plus dereference */
  return *((int*) p1) > * (int*) p2;
}


void
drawtab(Monitor *m) {
  Client *c;
  int i;

  /* Calculates number of labels and their width */
  for(m->ntabs = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next)){
    m->ntabs++;
    if(m->ntabs >= MAXTABS) break;
  }

  if(!m->ntabs) return;
  m->tab_width = m->ww/m->ntabs;

  int x = 0;
  for(i=0, c = nexttiled(m->clients); c && i < m->ntabs; c = nexttiled(c->next), i++){
    drw_setscheme(drw, scheme[(c == m->sel) ? SchemeSelTab : SchemeNormTab]);
    char tab_name[sizeof(c->name) + 1];
    sprintf(tab_name, " %s", c->name);
    drw_text(drw, x, 0, m->tab_width, th, 0, tab_name, 0);
    x += m->tab_width;
  }

  /* cleans interspace between window names and current viewed tag label */
  drw_setscheme(drw, scheme[SchemeNormTab]);
  drw_text(drw, x, 0, m->ww - x, th, 0, "", 0);

  drw_map(drw, m->tabwin, 0, 0, m->ww, th);
}

void
rotate(const Arg *arg){
  Client *c = selmon->sel;
	if (c){
    if (c->isfloating){
      swappos(arg);
    }else{
      rotatestack(arg);
    }
  }
}

void
enqueue(Client *c)
{
	Client *l;
	for (l = c->mon->clients; l && l->next; l = l->next);
	if (l) {
		l->next = c;
		c->next = NULL;
	}
}

void
enqueuestack(Client *c)
{
	Client *l;
	for (l = c->mon->stack; l && l->snext; l = l->snext);
	if (l) {
		l->snext = c;
		c->snext = NULL;
	}
}

void
rotatestack(const Arg *arg)
{
	Client *c = NULL, *f;

	if (!selmon->sel)
		return;
	f = selmon->sel;
	if (arg->i > 0) {
		for (c = nexttiled(selmon->clients); c && nexttiled(c->next); c = nexttiled(c->next));
		if (c){
			detach(c);
			attach(c);
			detachstack(c);
			attachstack(c);
		}
	} else {
		if ((c = nexttiled(selmon->clients))){
			detach(c);
			enqueue(c);
			detachstack(c);
			enqueuestack(c);
		}
	}
	if ((c = nexttiled(selmon->clients))){
		unfocus(f, 1);
		arrange(selmon);
		focus(c);
		/* restack(selmon); */
	}
}

  void
enternotify(XEvent *e)
{
  if (warping){
    warping = 0;
    return;
  }	
  Client *c;
  Monitor *m;
  XCrossingEvent *ev = &e->xcrossing;

  if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
    return;
  c = wintoclient(ev->window);
  m = c ? c->mon : wintomon(ev->window);
  if (m != selmon) {
    unfocus(selmon->sel, 1);
    selmon = m;
  } else if (!c || c == selmon->sel)
    return;
  focus(c);
}

  void
expose(XEvent *e)
{
  Monitor *m;
  XExposeEvent *ev = &e->xexpose;

  if (ev->count == 0 && (m = wintomon(ev->window))) {
    drawbar(m);
    drawtab(m);
    if (m == selmon)
      updatesystray();
  }
}

  void
focus(Client *c)
{
  Client *f = selmon->stack;
  Client *v = NULL;
  if (f && !f->issticky){
    for (; f && (!ISVISIBLE(f) || f->issticky); f = f->snext)
      if(ISVISIBLE(f))
        v = f;
    if (!f)
      f = v;
  }
  if (!c || !ISVISIBLE(c) || ( f && f->isfullscreen ) )
    c = f;
  if (selmon->sel && selmon->sel != c)
    unfocus(selmon->sel, 0);
  if (c) {
    if (c->mon != selmon)
      selmon = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    grabbuttons(c, 1);
    XSetWindowBorder(dpy, c->win, scheme[SchemeSel][ColBorder].pixel);
    setfocus(c);
  } else {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
  selmon->sel = c;
  drawbars();
  drawtabs();
}

/* there are some broken focus acquiring clients needing extra handling */
  void
focusin(XEvent *e)
{
  XFocusChangeEvent *ev = &e->xfocus;

  if (selmon->sel && ev->window != selmon->sel->win)
    setfocus(selmon->sel);
}

  Client*
focustiled(Client* c, int n)
{
  if(n > 0)
    for (;c && n ; c = nexttiled(c->next), n--); 
  else {
    Client *i1, *i2;
    i1 = i2 = nexttiled(selmon->clients);
    for (; i2 && i2 != c ; i2 = nexttiled(i2->next)){
      if ( n < 0)
        n++;
      else
        i1 = nexttiled(i1->next);
    }
    if (n<0) 
      return NULL;
    else 
      c = i1;
  }
  return c;
}

  void
focusmon(const Arg *arg)
{
  Monitor *m;
  Client *c;

  if (!mons->next || (m = dirtomon(arg->i)) == selmon)
    return;

  unfocus(selmon->sel, 0);
  selmon = m;

  if ((c = nexttiled(selmon->clients))){
    if (selmon->lt[selmon->sellt] == &layouts[0] && !selmon->sel->isfloating){
      if (arg->ui == -1)
        c = nexttiled(c->next);
    }else if ( selmon->lt[selmon->sellt] == &layouts[2] ){
      if (arg->ui == -1)
        c = focustiled(c,rows*(cols -1));
    }else{
      c = NULL;
    }
  }   

  focus(c);
  warp(selmon->sel);
}

int
validtarget(Client *c, int floating){
  /* return ISVISIBLE(c); (old behavior) */
  return ISVISIBLE(c) && c->isfloating == floating;
}

Client*
focusstackcore(int dir){
  Client *c = NULL, *i;

  if (!selmon->sel || selmon->sel->isfullscreen)
    return NULL;
  if (dir > 0) {
    for (c = selmon->sel->next; c && !validtarget(c,selmon->sel->isfloating); c = c->next);
    if (!c)
      for (c = selmon->clients; c && !validtarget(c,selmon->sel->isfloating); c = c->next);
  } else {
    for (i = selmon->clients; i != selmon->sel; i = i->next)
      if (validtarget(i,selmon->sel->isfloating))
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (validtarget(i,selmon->sel->isfloating))
          c = i;
  }

  return c;
}

  void
focusstack(const Arg *arg)
{
  Client *c = focusstackcore(arg->i);
  if (c) {
    focus(c);
    restack(selmon);
  }
}

  void
togglefocustype(const Arg *arg)
{
  Client *c = NULL;

  if (!selmon->sel || selmon->sel->isfullscreen)
    return;
  for (c = selmon->sel->snext; c && !validtarget(c,!selmon->sel->isfloating); c = c->snext);

  if (!c)
    for (c = selmon->clients; c && !validtarget(c,!selmon->sel->isfloating); c = c->next);

  if (c) {
    focus(c);
    restack(selmon);
  }
}



void
focuswin(const Arg* arg){
  int iwin = arg->i;
  Client* c = NULL;
  for(c = selmon->clients; c && (iwin || !ISVISIBLE(c) || c->isfloating) ; c = c->next){
    if(ISVISIBLE(c) && !c->isfloating) --iwin;
  };
  if(c) {
    focus(c);
    restack(selmon);
  }
}

void
killwin(const Arg* arg){
  int iwin = arg->i;
  Client* c = NULL;
  for(c = selmon->clients; c && (iwin || !ISVISIBLE(c) || c->isfloating) ; c = c->next){
    if(ISVISIBLE(c) && !c->isfloating) --iwin;
  };
  if(c) {
    if (!sendevent(c->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0)) {
      XGrabServer(dpy);
      XSetErrorHandler(xerrordummy);
      XSetCloseDownMode(dpy, DestroyAll);
      XKillClient(dpy, c->win);
      XSync(dpy, False);
      XSetErrorHandler(xerror);
      XUngrabServer(dpy);
    }
    restack(selmon);
  }
}


  void
swappos(const Arg *a)
{
  if (!selmon->sel || selmon->sel->isfullscreen || !selmon->sel->isfloating)
    return;
  togglefloatingcore(a != NULL);
  unfocus(selmon->sel, 0);
  selmon->sel = focusstackcore(a->i);
  togglefloatingcore(a != NULL);
  focus(selmon->sel);
  arrange(selmon);
}


  Atom
getatomprop(Client *c, Atom prop)
{
  int di;
  unsigned long dl;
  unsigned char *p = NULL;
  Atom da, atom = None;
  /* FIXME getatomprop should return the number of items and a pointer to
   * the stored data instead of this workaround */
  Atom req = XA_ATOM;
  if (prop == xatom[XembedInfo])
    req = xatom[XembedInfo];

  if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, req,
        &da, &di, &dl, &dl, &p) == Success && p) {
    atom = *(Atom *)p;
    if (da == xatom[XembedInfo] && dl == 2)
      atom = ((Atom *)p)[1];
    XFree(p);
  }
  return atom;
}

  int
getdwmblockspid()
{
  char buf[16];
  FILE *fp = popen("pidof -s dwmblocks", "r");
  fgets(buf, sizeof(buf), fp);
  pid_t pid = strtoul(buf, NULL, 10);
  pclose(fp);
  dwmblockspid = pid;
  return pid != 0 ? 0 : -1;
}

  int
getrootptr(int *x, int *y)
{
  int di;
  unsigned int dui;
  Window dummy;

  return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

  long
getstate(Window w)
{
  int format;
  long result = -1;
  unsigned char *p = NULL;
  unsigned long n, extra;
  Atom real;

  if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
        &real, &format, &n, &extra, (unsigned char **)&p) != Success)
    return -1;
  if (n != 0)
    result = *p;
  XFree(p);
  return result;
}

  unsigned int
getsystraywidth()
{
  unsigned int w = 0;
  Client *i;
  if(showsystray)
    for(i = systray->icons; i; w += i->w + systrayspacing, i = i->next) ;
  return w ? w + systrayspacing : 1;
}

  int
gettextprop(Window w, Atom atom, char *text, unsigned int size)
{
  char **list = NULL;
  int n;
  XTextProperty name;

  if (!text || size == 0)
    return 0;
  text[0] = '\0';
  if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
    return 0;
  if (name.encoding == XA_STRING)
    strncpy(text, (char *)name.value, size - 1);
  else {
    if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
      strncpy(text, *list, size - 1);
      XFreeStringList(list);
    }
  }
  text[size - 1] = '\0';
  XFree(name.value);
  return 1;
}

  void
grabbuttons(Client *c, int focused)
{
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    if (!focused)
      XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
          BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
    for (i = 0; i < LENGTH(buttons); i++)
      if (buttons[i].click == ClkClientWin)
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabButton(dpy, buttons[i].button,
              buttons[i].mask | modifiers[j],
              c->win, False, BUTTONMASK,
              GrabModeAsync, GrabModeSync, None, None);
  }
}

  void
grabkeys(void)
{
  updatenumlockmask();
  {
    unsigned int i, j;
    unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
    KeyCode code;

    XUngrabKey(dpy, AnyKey, AnyModifier, root);
    for (i = 0; i < LENGTH(keys); i++)
      if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
        for (j = 0; j < LENGTH(modifiers); j++)
          XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
              True, GrabModeAsync, GrabModeAsync);
  }
}

  void
incnmaster(const Arg *arg)
{
  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag] = MAX(selmon->nmaster + arg->i, 0);
  arrange(selmon);
}

#ifdef XINERAMA
  static int
isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
{
  while (n--)
    if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
        && unique[n].width == info->width && unique[n].height == info->height)
      return 0;
  return 1;
}
#endif /* XINERAMA */

  void
keypress(XEvent *e)
{
  unsigned int i;
  KeySym keysym;
  XKeyEvent *ev;

  ev = &e->xkey;
  keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
  for (i = 0; i < LENGTH(keys); i++)
    if (keysym == keys[i].keysym
        && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
        && keys[i].func)
      keys[i].func(&(keys[i].arg));
}

void hide(Client* c){
  c->tags=0;
  c->issticky = 0;
}

  void
killclient(const Arg *arg)
{
  if (!selmon->sel)
    return;
  if (selmon->sel->sp_id){
    hide(selmon->sel);
    arrange(selmon);
    focus(NULL);
    return;
  }
  if (!sendevent(selmon->sel->win, wmatom[WMDelete], NoEventMask, wmatom[WMDelete], CurrentTime, 0 , 0, 0)) {
    XGrabServer(dpy);
    XSetErrorHandler(xerrordummy);
    XSetCloseDownMode(dpy, DestroyAll);
    XKillClient(dpy, selmon->sel->win);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
}

  void
manage(Window w, XWindowAttributes *wa)
{
  Client *c, *t = NULL, *term = NULL;
  Window trans = None;
  XWindowChanges wc;

  c = ecalloc(1, sizeof(Client));
  c->win = w;
  c->pid = winpid(w);
  /* geometry */
  c->x = c->oldx = wa->x;
  c->y = c->oldy = wa->y;
  c->w = c->oldw = wa->width;
  c->h = c->oldh = wa->height;
  c->oldbw = wa->border_width;

  updatetitle(c);
  c->floatx = c->floaty = c->floatw = c->floath = -1; /* default float* to center */
  if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
    c->mon = t->mon;
    if(!c->issticky)
      c->tags = t->tags;
  } else {
    c->mon = selmon;
    applyrules(c);
    term = termforwin(c);
  }

  if (c->x + WIDTH(c) > c->mon->mx + c->mon->mw)
    c->x = c->mon->mx + c->mon->mw - WIDTH(c);
  if (c->y + HEIGHT(c) > c->mon->my + c->mon->mh)
    c->y = c->mon->my + c->mon->mh - HEIGHT(c);
  c->x = MAX(c->x, c->mon->mx);
  /* only fix client y-offset, if the client center might cover the bar */
  c->y = MAX(c->y, ((c->mon->by == c->mon->my) && (c->x + (c->w / 2) >= c->mon->wx)
        && (c->x + (c->w / 2) < c->mon->wx + c->mon->ww)) ? bh : c->mon->my);
  c->bw = borderpx;

  wc.border_width = c->bw;
  XConfigureWindow(dpy, w, CWBorderWidth, &wc);
  XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
  configure(c); /* propagates border_width, if size doesn't change */
  updatewindowtype(c);
  updatesizehints(c);
  if (c->floatw > 0) /* set the position/size of the floating window according to the rules */
    c->w = (c->mon->mw * c->floatw) / 100.0 - c->bw*2;
  if (c->floath > 0)
    c->h = (c->mon->mh * c->floath) / 100.0 - c->bw*2;
  if (c->floatx >= 0)
    c->x = c->mon->mx + (c->mon->mw * c->floatx) / 100.0;
  else if(c->floatx != -2)
    c->x = c->mon->mx + (c->mon->mw - WIDTH(c)) / 2;
  else
    c->x -= 2*c->bw; /* adjustment for firefox picture in picture out of bound otherwise */
  if (c->floaty >= 0)
    c->y = c->mon->my + (c->mon->mh * c->floaty) / 100.0;
  else if(c->floaty != -2)
    c->y = c->mon->my + (c->mon->mh - HEIGHT(c)) / 2;
  else
    c->y -= 2*c->bw; /* adjustment for firefox picture in picture out of bound otherwise */
  updatewmhints(c);
  XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
  grabbuttons(c, 0);
  if (!c->isfloating)
    c->isfloating = c->oldstate = trans != None || c->isfixed;
  if (c->isfloating)
    XRaiseWindow(dpy, c->win);
  attach(c);
  attachstack(c);
  XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
      (unsigned char *) &(c->win), 1);
  XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
  setclientstate(c, NormalState);
  if(!selmon->sel || !selmon->sel->isfullscreen){
    if (c->mon == selmon)
      unfocus(selmon->sel, 0);
    c->mon->sel = c;
  }
  arrange(c->mon);
  XMapWindow(dpy, c->win);
  if (term)
    swallow(term, c);
  if(!selmon->sel || !selmon->sel->isfullscreen)
    focus(NULL);
}

  void
mappingnotify(XEvent *e)
{
  XMappingEvent *ev = &e->xmapping;

  XRefreshKeyboardMapping(ev);
  if (ev->request == MappingKeyboard)
    grabkeys();
}

  void
maprequest(XEvent *e)
{
  static XWindowAttributes wa;
  XMapRequestEvent *ev = &e->xmaprequest;
  Client *i;
  if ((i = wintosystrayicon(ev->window))) {
    sendevent(i->win, netatom[Xembed], StructureNotifyMask, CurrentTime, XEMBED_WINDOW_ACTIVATE, 0, systray->win, XEMBED_EMBEDDED_VERSION);
    resizebarwin(selmon);
    updatesystray();
  }

  if (!XGetWindowAttributes(dpy, ev->window, &wa))
    return;
  if (wa.override_redirect)
    return;
  if (!wintoclient(ev->window))
    manage(ev->window, &wa);
}

  void
monocle(Monitor *m)
{
  Client *c;

  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

  void
motionnotify(XEvent *e)
{
  if (warping){
    warping = 0;
    return;
  }
  static Monitor *mon = NULL;
  Monitor *m;
  XMotionEvent *ev = &e->xmotion;

  if (ev->window != root)
    return;
  if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
    unfocus(selmon->sel, 1);
    selmon = m;
    focus(NULL);
  }
  mon = m;
}

  void
movemouse(const Arg *arg)
{
  int x, y, ocx, ocy, nx, ny,wasfloating , moved = 0;
  Client *c;
  Monitor *m;
  XEvent ev;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
    togglefullscreen(NULL);
  wasfloating = c->isfloating;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
        None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
    return;
  if (!getrootptr(&x, &y))
    return;
  do {
    XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
    switch(ev.type) {
      case ConfigureRequest:
      case Expose:
      case MapRequest:
        handler[ev.type](&ev);
        break;
      case MotionNotify:
        if ((ev.xmotion.time - lasttime) <= (1000 / 60))
          continue;
        lasttime = ev.xmotion.time;

        nx = ocx + (ev.xmotion.x - x);
        ny = ocy + (ev.xmotion.y - y);
        if (abs(selmon->wx - nx) < snap)
          nx = selmon->wx;
        else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
          nx = selmon->wx + selmon->ww - WIDTH(c);
        if (abs(selmon->wy - ny) < snap)
          ny = selmon->wy;
        else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
          ny = selmon->wy + selmon->wh - HEIGHT(c);
        if (selmon->lt[selmon->sellt]->arrange
            && (abs(nx - c->x) > snap || abs(ny - c->y) > snap)){
          moved = 1;	
          if (!c->isfloating)
            togglefloating(NULL);
        } if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
        resize(c, nx, ny, c->w, c->h, 1);
        break;
    }
  } while (ev.type != ButtonRelease);
  XUngrabPointer(dpy, CurrentTime);
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  } else if( arg-> i)
    pop(c);
  if( arg->i && (wasfloating != moved))
    togglefloating(NULL);

}

  Client *
nexttiled(Client *c)
{
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
  return c;
}

  void
pop(Client *c)
{
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}

Client *
prevtiled(Client *c) {
  Client *p, *r;

  for(p = selmon->clients, r = NULL; p && p != c; p = p->next)
    if(!p->isfloating && ISVISIBLE(p))
      r = p;
  return r;
}

  void
propertynotify(XEvent *e)
{
  Client *c;
  Window trans;
  XPropertyEvent *ev = &e->xproperty;

  if ((c = wintosystrayicon(ev->window))) {
    if (ev->atom == XA_WM_NORMAL_HINTS) {
      updatesizehints(c);
      updatesystrayicongeom(c, c->w, c->h);
    }
    else
      updatesystrayiconstate(c, ev);
    resizebarwin(selmon);
    updatesystray();
  }
  if ((ev->window == root) && (ev->atom == XA_WM_NAME))
    updatestatus();
  else if (ev->state == PropertyDelete)
    return; /* ignore */
  else if ((c = wintoclient(ev->window))) {
    switch(ev->atom) {
      default: break;
      case XA_WM_TRANSIENT_FOR:
               if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
                   (c->isfloating = (wintoclient(trans)) != NULL))
                 arrange(c->mon);
               break;
      case XA_WM_NORMAL_HINTS:
               updatesizehints(c);
               break;
      case XA_WM_HINTS:
               updatewmhints(c);
               drawbars();
               drawtabs();
               break;
    }
    if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
      updatetitle(c);
      if (c == c->mon->sel)
        drawbar(c->mon);
      drawtab(c->mon);
    }
    if (ev->atom == netatom[NetWMWindowType])
      updatewindowtype(c);
  }
}

void
pushdown(const Arg *arg) {
  Client *sel = selmon->sel, *c;

  if(!sel || sel->isfloating || sel->isfullscreen)
    return;
  if((c = nexttiled(sel->next))) {
    detach(sel);
    sel->next = c->next;
    c->next = sel;
    focus(sel);
    arrange(selmon);
  }else{
    zoom(NULL);
  }
}

void
pushup(const Arg *arg) {
  Client *sel = selmon->sel, *c;

  if(!sel || sel->isfloating || sel->isfullscreen)
    return;
  if((c = prevtiled(sel))) {
    detach(sel);
    sel->next = c;
    if(selmon->clients != sel->next){
      for(c = selmon->clients; c->next != sel->next; c = c->next);
      c->next = sel;
    }else
      selmon->clients = sel;
  }else{
    if(!(c = nexttiled(sel->next)))
      return;
    detach(sel);
    sel->next = NULL;
    for(;c->next;c=c->next);
    c->next = sel;
  }
  focus(sel);
  arrange(selmon);
}

  void
quit(const Arg *arg)
{
  size_t i;

  /* kill child processes */
  for (i = 0; i < autostart_len; i++) {
    if (0 < autostart_pids[i]) {
      kill(autostart_pids[i], SIGTERM);
      waitpid(autostart_pids[i], NULL, 0);
    }
  }

  running = 0;
}

  Monitor *
recttomon(int x, int y, int w, int h)
{
  Monitor *m, *r = selmon;
  int a, area = 0;

  for (m = mons; m; m = m->next)
    if ((a = INTERSECT(x, y, w, h, m)) > area) {
      area = a;
      r = m;
    }
  return r;
}

  void
removesystrayicon(Client *i)
{
  Client **ii;

  if (!showsystray || !i)
    return;
  for (ii = &systray->icons; *ii && *ii != i; ii = &(*ii)->next);
  if (ii)
    *ii = i->next;
  free(i);
}

void
reorganizetags(const Arg *arg) {
  Client *c;
  unsigned int occ, funocc, focc;

  occ = 0;
  for (c = selmon->clients; c; c = c->next)
    if (!c->issticky)
      occ |= (1 << (ffs(c->tags)-1));

  if ( !arg  || arg->i > 0){

    for (funocc = 0; funocc < LENGTH(tags) && ((1 << funocc) & occ); ++funocc);

    for (focc = funocc + 1; focc < LENGTH(tags) && !((1 << focc) & occ); ++focc);

    if ( funocc >= LENGTH(tags) || focc >= LENGTH(tags) )
      return;

  }else{

    for (funocc = LENGTH(tags) - 1; funocc >= 0 && ((1 << funocc) & occ); --funocc);

    for (focc = funocc - 1; focc >= 0 && !((1 << focc) & occ); --focc);

    if ( funocc < 0 || focc < 0 || funocc <= focc ) 
      return;
  }

  for (c = selmon->clients; c; c = c->next)
    if ( !c->issticky && (c->tags & (1 << focc)) )
      c->tags = 1 << funocc;

  if (selmon->tagset[selmon->seltags] & (1 << focc))
    selmon->tagset[selmon->seltags] = (1 << funocc);

  arrange(selmon);
}

/* void */
/* reorganizetags(const Arg *arg) { */
/* 	Client *c; */
/* 	unsigned int occ, unocc, i; */
/* 	unsigned int tagdest[LENGTH(tags)]; */

/* 	occ = 0; */
/* 	for (c = selmon->clients; c; c = c->next) */
/*     if (!c->issticky) */
/*       occ |= (1 << (ffs(c->tags)-1)); */
/* 	unocc = 0; */
/* 	for (i = 0; i < LENGTH(tags); ++i) { */
/* 		while (unocc < i && (occ & (1 << unocc))) */
/* 			unocc++; */
/* 		if (occ & (1 << i)) { */
/* 			tagdest[i] = unocc; */
/* 			occ &= ~(1 << i); */
/* 			occ |= 1 << unocc; */
/* 		} */
/* 	} */

/* 	for (c = selmon->clients; c; c = c->next) */
/*     if (!c->issticky) */
/*       c->tags = 1 << tagdest[ffs(c->tags)-1]; */
/* 	if (selmon->sel) */
/* 		selmon->tagset[selmon->seltags] = selmon->sel->tags; */
/* 	arrange(selmon); */
/* } */

  void
resize(Client *c, int x, int y, int w, int h, int interact)
{
  if (applysizehints(c, &x, &y, &w, &h, interact))
    resizeclient(c, x, y, w, h);
}

void
resizebarwin(Monitor *m) {
  unsigned int w = m->ww;
  if (showsystray && m == systraytomon(m))
    w -= getsystraywidth();
  XMoveResizeWindow(dpy, m->barwin, m->wx, m->by, w, bh);
}

  void
resizeclient(Client *c, int x, int y, int w, int h)
{
  XWindowChanges wc;

  c->oldx = c->x; c->x = wc.x = x;
  c->oldy = c->y; c->y = wc.y = y;
  c->oldw = c->w; c->w = wc.width = w;
  c->oldh = c->h; c->h = wc.height = h;
  wc.border_width = c->bw;
  XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
  configure(c);
  XSync(dpy, False);
}
int mod(int x, int mod){
  x = x % mod;
  return x >= 0 ? x : x + mod;
}

void
changefloatzone(int cx,int cy){
  Client *c = selmon->sel;
  if (!c) return;
  if (c->floatzonex < 0) c->floatzonex = 1;
  if (c->floatzoney < 0) c->floatzoney = 1;
  if ( c->floatzonex + cx < 0 || 2 < c->floatzonex + cx ){
    Monitor* m = dirtomon(c->floatzonex + cx < 0 ? -1 : 1);
    sendmon(c, m);
    unfocus(selmon->sel, 0);
    selmon = c->mon;
    focus(NULL);
  }
  c->floatzonex = mod(c->floatzonex + cx, 3);
  c->floatzoney = mod(c->floatzoney + cy, 3);
  int x = selmon->wx + floatzones[c->floatzoney][c->floatzonex][0] * selmon->ww / 100;
  int y = selmon->wy + floatzones[c->floatzoney][c->floatzonex][1] * selmon->wh / 100;
  int w = (floatzones[c->floatzoney][c->floatzonex][2] * selmon->ww / 100) - 2*c->bw;
  int h = (floatzones[c->floatzoney][c->floatzonex][3] * selmon->wh / 100) - 2*c->bw;
  resize(c, x, y, w, h,0);
  warp(c);
}

  void
changefloatzonex(const Arg *arg)
{
  changefloatzone(arg->i,0);
}

  void
changefloatzoney(const Arg *arg)
{
  changefloatzone(0,arg->i);
}

  void
resizemouse(const Arg *arg)
{
  int ocx, ocy, nw, nh;
  int ocx2, ocy2, nx, ny;
  Client *c;
  Monitor *m;
  XEvent ev;
  int horizcorner, vertcorner;
  int di;
  unsigned int dui;
  Window dummy;
  Time lasttime = 0;

  if (!(c = selmon->sel))
    return;
  if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
    return;
  restack(selmon);
  ocx = c->x;
  ocy = c->y;
  ocx2 = c->x + c->w;
  ocy2 = c->y + c->h;
  if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
        None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
    return;
  if (!XQueryPointer (dpy, c->win, &dummy, &dummy, &di, &di, &nx, &ny, &dui))
    return;
  horizcorner = nx < c->w / 2;
  vertcorner  = ny < c->h / 2;
  XWarpPointer (dpy, None, c->win, 0, 0, 0, 0,
      horizcorner ? (-c->bw) : (c->w + c->bw -1),
      vertcorner  ? (-c->bw) : (c->h + c->bw -1));
  do {
    XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
    switch(ev.type) {
      case ConfigureRequest:
      case Expose:
      case MapRequest:
        handler[ev.type](&ev);
        break;
      case MotionNotify:
        if ((ev.xmotion.time - lasttime) <= (1000 / 60))
          continue;
        lasttime = ev.xmotion.time;

        nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
        nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
        nx = horizcorner ? ev.xmotion.x : c->x;
        ny = vertcorner ? ev.xmotion.y : c->y;
        nw = MAX(horizcorner ? (ocx2 - nx) : (ev.xmotion.x - ocx - 2 * c->bw + 1), 1);
        nh = MAX(vertcorner ? (ocy2 - ny) : (ev.xmotion.y - ocy - 2 * c->bw + 1), 1);

        if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
            && c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
        {
          if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
              && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
            togglefloating(NULL);
        }
        if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
          resize(c, nx, ny, nw, nh, 1);
        break;
    }
  } while (ev.type != ButtonRelease);
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0,
      horizcorner ? (-c->bw) : (c->w + c->bw - 1),
      vertcorner ? (-c->bw) : (c->h + c->bw - 1));
  XUngrabPointer(dpy, CurrentTime);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
  if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
    sendmon(c, m);
    selmon = m;
    focus(NULL);
  }
}

  void
resizerequest(XEvent *e)
{
  XResizeRequestEvent *ev = &e->xresizerequest;
  Client *i;

  if ((i = wintosystrayicon(ev->window))) {
    updatesystrayicongeom(i, ev->width, ev->height);
    resizebarwin(selmon);
    updatesystray();
  }
}

  void
restack(Monitor *m)
{
  Client *c;
  XEvent ev;
  XWindowChanges wc;

  drawbar(m);
  drawtab(m);
  if (!m->sel)
    return;
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
    XRaiseWindow(dpy, m->sel->win);
  if (m->lt[m->sellt]->arrange) {
    wc.stack_mode = Below;
    wc.sibling = m->barwin;
    for (c = m->stack; c; c = c->snext)
      if (!c->isfloating && ISVISIBLE(c)) {
        XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
        wc.sibling = c->win;
      }
  }
  XSync(dpy, False);
  while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
  if (m == selmon && ((m->tagset[m->seltags] & m->sel->tags) || m->sel->issticky )  && selmon->lt[selmon->sellt] != &layouts[1])
    warp(m->sel);
}

  void
run(void)
{
  XEvent ev;
  /* main event loop */
  XSync(dpy, False);
  while (running && !XNextEvent(dpy, &ev))
    if (handler[ev.type])
      handler[ev.type](&ev); /* call handler */
}

  void
scan(void)
{
  unsigned int i, num;
  Window d1, d2, *wins = NULL;
  XWindowAttributes wa;

  if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
    for (i = 0; i < num; i++) {
      if (!XGetWindowAttributes(dpy, wins[i], &wa)
          || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
        continue;
      if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
        manage(wins[i], &wa);
    }
    for (i = 0; i < num; i++) { /* now the transients */
      if (!XGetWindowAttributes(dpy, wins[i], &wa))
        continue;
      if (XGetTransientForHint(dpy, wins[i], &d1)
          && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
        manage(wins[i], &wa);
    }
    if (wins)
      XFree(wins);
  }
}

  void
sendtomon(Client *c, Monitor *m, int master, int preserve)
{
  if (c->mon == m)
    return;
  if (preserve && c->isfloating){
    c->x += m->mx - c->mon->mx;
    c->y += m->my - c->mon->my;
  }
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon = m;
  if(!c->issticky)
    c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  Client* cmaster = nexttiled(m->clients);
  if ( !master && cmaster ){
    c->next = cmaster->next;
    cmaster->next = c;
  }else
    attach(c);
  attachstack(c);
  arrange(NULL);
}

  void
sendmon(Client *c, Monitor *m)
{
  sendtomon(c, m, 1, 0);
}

  void
setclientstate(Client *c, long state)
{
  long data[] = { state, None };

  XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
      PropModeReplace, (unsigned char *)data, 2);
}

  int
sendevent(Window w, Atom proto, int mask, long d0, long d1, long d2, long d3, long d4)
{
  int n;
  Atom *protocols, mt;
  int exists = 0;
  XEvent ev;

  if (proto == wmatom[WMTakeFocus] || proto == wmatom[WMDelete]) {
    mt = wmatom[WMProtocols];
    if (XGetWMProtocols(dpy, w, &protocols, &n)) {
      while (!exists && n--)
        exists = protocols[n] == proto;
      XFree(protocols);
    }
  }
  else {
    exists = True;
    mt = proto;
  }
  if (exists) {
    ev.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = mt;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = d0;
    ev.xclient.data.l[1] = d1;
    ev.xclient.data.l[2] = d2;
    ev.xclient.data.l[3] = d3;
    ev.xclient.data.l[4] = d4;
    XSendEvent(dpy, w, False, mask, &ev);
  }
  return exists;
}

  void
setfocus(Client *c)
{
  if (!c->neverfocus) {
    XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    XChangeProperty(dpy, root, netatom[NetActiveWindow],
        XA_WINDOW, 32, PropModeReplace,
        (unsigned char *) &(c->win), 1);
  }
  sendevent(c->win, wmatom[WMTakeFocus], NoEventMask, wmatom[WMTakeFocus], CurrentTime, 0, 0, 0);
}

  void
setfullscreen(Client *c, int fullscreen)
{
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
        PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  } else if (!fullscreen && c->isfullscreen){
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
        PropModeReplace, (unsigned char*)0, 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}

  void
setfullfullscreen(Client *c, int fullscreen)
{
  if (fullscreen && !c->isfullscreen) {
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
        PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
    c->isfullscreen = 1;
    c->oldstate = c->isfloating;
    c->oldbw = c->bw;
    c->bw = 0;
    c->isfloating = 1;
    resizeclient(c, 0, c->mon->my, c->mon->mw*2, c->mon->mh);
    XRaiseWindow(dpy, c->win);
  } else if (!fullscreen && c->isfullscreen){
    XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
        PropModeReplace, (unsigned char*)0, 0);
    c->isfullscreen = 0;
    c->isfloating = c->oldstate;
    c->bw = c->oldbw;
    c->x = c->oldx;
    c->y = c->oldy;
    c->w = c->oldw;
    c->h = c->oldh;
    resizeclient(c, c->x, c->y, c->w, c->h);
    arrange(c->mon);
  }
}


  void
setlayout(const Arg *arg)
{
  selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag] ^= 1;
  if (arg && arg->v && arg->v != selmon->lt[selmon->sellt ^1])
    selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt] = (Layout *)arg->v;
  strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
  if (selmon->sel)
    arrange(selmon);
  else
    drawbar(selmon);
}

/* arg > 1.0 will set mfact absolutely */
  void
setmfact(const Arg *arg)
{
  float f;

  if (!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if (f < 0.1 || f > 0.9)
    return;
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag] = f;
  arrange(selmon);
}

  void
setup(void)
{
  int i;
  XSetWindowAttributes wa;
  Atom utf8string;

  /* clean up any zombies immediately */
  sigchld(0);

  /* init screen */
  screen = DefaultScreen(dpy);
  sw = DisplayWidth(dpy, screen);
  sh = DisplayHeight(dpy, screen);
  root = RootWindow(dpy, screen);
  drw = drw_create(dpy, screen, root, sw, sh);
  if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
    die("no fonts could be loaded.");
 	lrpad = drw->fonts->h + horizpadbar;
 	bh = drw->fonts->h + vertpadbar;
  th = bh;
  updategeom();
  /* init atoms */
  utf8string = XInternAtom(dpy, "UTF8_STRING", False);
  wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
  wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
  wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
  netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
  netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
  netatom[NetSystemTray] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_S0", False);
  netatom[NetSystemTrayOP] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
  netatom[NetSystemTrayOrientation] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION", False);
  netatom[NetSystemTrayOrientationHorz] = XInternAtom(dpy, "_NET_SYSTEM_TRAY_ORIENTATION_HORZ", False);
  netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
  netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
  netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
  netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
  netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
  netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
  xatom[Manager] = XInternAtom(dpy, "MANAGER", False);
  xatom[Xembed] = XInternAtom(dpy, "_XEMBED", False);
  xatom[XembedInfo] = XInternAtom(dpy, "_XEMBED_INFO", False);
  /* init cursors */
  cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
  cursor[CurResize] = drw_cur_create(drw, XC_sizing);
  cursor[CurMove] = drw_cur_create(drw, XC_fleur);
  /* init appearance */
  scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
  for (i = 0; i < LENGTH(colors); i++)
    scheme[i] = drw_scm_create(drw, colors[i], 3);
  /* init system tray */
  updatesystray();
  /* init bars */
  updatebars();
  updatestatus();
  /* supporting window for NetWMCheck */
  wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
      PropModeReplace, (unsigned char *) &wmcheckwin, 1);
  XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
      PropModeReplace, (unsigned char *) "dwm", 3);
  XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
      PropModeReplace, (unsigned char *) &wmcheckwin, 1);
  /* EWMH support per view */
  XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
      PropModeReplace, (unsigned char *) netatom, NetLast);
  XDeleteProperty(dpy, root, netatom[NetClientList]);
  /* select events */
  wa.cursor = cursor[CurNormal]->cursor;
  wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
    |ButtonPressMask|PointerMotionMask|EnterWindowMask
    |LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
  XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
  XSelectInput(dpy, root, wa.event_mask);
  grabkeys();
  focus(NULL);
}


  void
seturgent(Client *c, int urg)
{
  XWMHints *wmh;

  c->isurgent = urg;
  if (!(wmh = XGetWMHints(dpy, c->win)))
    return;
  wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
  XSetWMHints(dpy, c->win, wmh);
  XFree(wmh);
}

  void
showhide(Client *c)
{
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    /* show clients top down */
    XMoveWindow(dpy, c->win, c->x, c->y);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    /* hide clients bottom up */
    showhide(c->snext);
    XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
  }
}

  void
sigchld(int unused)
{
  pid_t pid;

  if (signal(SIGCHLD, sigchld) == SIG_ERR)
    die("can't install SIGCHLD handler:");
  while (0 < (pid = waitpid(-1, NULL, WNOHANG))) {
    pid_t *p, *lim;

    if (!(p = autostart_pids))
      continue;
    lim = &p[autostart_len];

    for (; p < lim; p++) {
      if (*p == pid) {
        *p = -1;
        break;
      }
    }

  }
}

  void
sigdwmblocks(const Arg *arg)
{
  union sigval sv;
  sv.sival_int = (dwmblockssig << 8) | arg->i;
  if (!dwmblockspid)
    if (getdwmblockspid() == -1)
      return;

  if (sigqueue(dwmblockspid, SIGUSR1, sv) == -1) {
    if (errno == ESRCH) {
      if (!getdwmblockspid())
        sigqueue(dwmblockspid, SIGUSR1, sv);
    }
  }
}

  void
spawn(const Arg *arg)
{
  if (fork() == 0) {
    if (dpy)
      close(ConnectionNumber(dpy));
    setsid();
    execvp(((char **)arg->v)[0], (char **)arg->v);
    fprintf(stderr, "dwm: execvp %s", ((char **)arg->v)[0]);
    perror(" failed");
    exit(EXIT_SUCCESS);
  }
}

  void
tag(const Arg *arg)
{
  if (selmon->sel && arg->ui & TAGMASK) {
    if(!selmon->sel->issticky)
      selmon->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selmon);
  }
}

  void
tagmon(const Arg *arg)
{
  Client* c = selmon->sel;
  if (!c || !mons->next)
    return;
  Monitor* m = dirtomon(arg->i);
  sendtomon(c, m, arg->ui, 1);
  unfocus(selmon->sel, 0);
  selmon = c->mon;
  focus(NULL);

}

  void
tile(Monitor *m)
{
  unsigned int i, n, h, mw, my, ty;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
  if (n == 0)
    return;

  if (n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if (i < m->nmaster) {
      h = (m->wh - my) / (MIN(n, m->nmaster) - i);
      resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), 0);
      my += HEIGHT(c);
    } else {
      h = (m->wh - ty) / (n - i);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), 0);
      ty += HEIGHT(c);
    }
}

  void
togglebar(const Arg *arg)
{
  selmon->showbar = selmon->pertag->showbars[selmon->pertag->curtag] = !selmon->showbar;
  updatebarpos(selmon);
  resizebarwin(selmon);
  if (showsystray && selmon == systraytomon(NULL) ) {
    XWindowChanges wc;
    if (!selmon->showbar)
      wc.y = -bh;
    else {
      wc.y = 0;
      if (!selmon->topbar)
        wc.y = selmon->mh - bh;
    }
    XConfigureWindow(dpy, systray->win, CWY, &wc);
  }
  arrange(selmon);
}

void
togglefloatingcore(int setzone){ // setzone acts as bool
  if (!selmon->sel)
    return;
  if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
    togglefullscreen(NULL);
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating){
    int fzx = selmon->sel->floatzonex = selmon->sel->ofloatzonex;
    int fzy = selmon->sel->floatzoney = selmon->sel->ofloatzoney;
    if ( setzone ){
      if( fzx >= 0 && fzy >= 0 ) {
        selmon->sel->x = selmon->wx + floatzones[fzy][fzx][0] * selmon->ww / 100;
        selmon->sel->y = selmon->wy + floatzones[fzy][fzx][1] * selmon->wh / 100;
        selmon->sel->w = (floatzones[fzy][fzx][2] * selmon->ww / 100) - 2*selmon->sel->bw;
        selmon->sel->h = (floatzones[fzy][fzx][3] * selmon->wh / 100) - 2*selmon->sel->bw;
      }else{
        selmon->sel->x = selmon->mx + (floatingdims[0]*selmon->mw)/100.0;
        selmon->sel->y = selmon->my + (floatingdims[1]*selmon->mh)/100.0;
        selmon->sel->w = (floatingdims[2]*selmon->mw)/100.0;
        selmon->sel->h = (floatingdims[3]*selmon->mh)/100.0;
      }
      XMoveResizeWindow(dpy, selmon->sel->win, selmon->sel->x + 2 * sw,
          selmon->sel->y, selmon->sel->w, selmon->sel->h);
    }
    resize(selmon->sel, selmon->sel->x, selmon->sel->y,
        selmon->sel->w, selmon->sel->h, 0);
  }
}

  void
togglefloating(const Arg *arg)
{
  togglefloatingcore(arg != NULL);
  arrange(selmon);
}

  void
togglefullscreen(const Arg *arg)
{
  if (selmon->sel){
    setfullscreen(selmon->sel, !selmon->sel->isfullscreen);
    arrange(selmon);
  }
}
  void
togglefullfullscreen(const Arg *arg)
{
  setfullfullscreen(selmon->sel, !selmon->sel->isfullscreen);
  arrange(selmon);
}

  void
togglesticky(const Arg *arg)
{
  if (!selmon->sel)
    return;
  setsticky(selmon->sel,!selmon->sel->issticky);
  arrange(selmon);
}

  void
togglesp(const Arg *arg)
{
  Client *c = NULL;
  Monitor* m =NULL;
  for (m = mons; m && !c; m = m->next)
    for (c = m->clients; c && c->sp_id != arg->i; c = c->next);
  if (c) {
    if(c != selmon->sel){
      if (!c->issticky && !c->tags){
        c->issticky = 1;
        if(c->mon != selmon)
          sendtomon(c, selmon, 1, 1);
      }
      unfocus(selmon->sel, 1);
      selmon = c->mon;
      if(!c->issticky){
        int i;
        for (i = 0; i < LENGTH(tags) && !((1 << i) & c->tags); i++);
        if (i >= LENGTH(tags)) return;
        const Arg a = {.ui = 1 << i};
        view(&a);
      }
      focus(c);
      arrange(selmon);
    } else {
      hide(c);
      focus(c->snext);
      arrange(selmon);
    }
  }else{
    Arg a;
    a.v = (const char*[]){ "/bin/sh", "-c", scratchpads_cmd[arg->i-1], NULL };
    spawn(&a);
  }
}

  void
toggletag(const Arg *arg)
{
  unsigned int newtags;

  if (!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    if(!selmon->sel->issticky)
      selmon->sel->tags = newtags;
    focus(NULL);
    arrange(selmon);
  }
}

  void
toggleview(const Arg *arg)
{
  unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);
  int i;

  if (newtagset) {
    selmon->tagset[selmon->seltags] = newtagset;

    if (newtagset == ~0) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      selmon->pertag->curtag = 0;
    }

    /* test if the user did not select the same tag */
    if (!(newtagset & 1 << (selmon->pertag->curtag - 1))) {
      selmon->pertag->prevtag = selmon->pertag->curtag;
      for (i = 0; !(newtagset & 1 << i); i++) ;
      selmon->pertag->curtag = i + 1;
    }

    /* apply settings for this view */
    selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
    selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
    selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
    selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
    selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

    if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
      togglebar(NULL);

    focus(NULL);
    arrange(selmon);
  }
}

// mode = 0 incview
// mode = 1 freeview
unsigned int
moveviewcore(int mode, int prev, int ai, unsigned int newtagset){
  unsigned int rotatetagset,shifttagset, occ, prevtagset;

  occ = 0;
  for (Client *c = selmon->clients; c; c = c->next)
    if (!c->issticky)
      occ |= (1 << (ffs(c->tags)-1));

  if (! newtagset)
    newtagset = selmon->tagset[selmon->seltags];

  if (ai > 0){
    for (int i = 0; i < LENGTH(tags); i++){
      prevtagset = newtagset;
      shifttagset  = newtagset  << ai;
      rotatetagset = shifttagset & (TAGMASK << LENGTH(tags));
      newtagset = (shifttagset | (rotatetagset >> LENGTH(tags))) & TAGMASK;
      if (!mode != !(newtagset & occ))
        break;
    }
  }else{
    for (int i = 0; i < LENGTH(tags); i++){
      prevtagset = newtagset;
      shifttagset  = newtagset >> (- ai);
      rotatetagset = newtagset<<(LENGTH(tags) + ai);
      newtagset = (rotatetagset | shifttagset) & TAGMASK;
      if (mode != (newtagset & occ))
        break;
    }
  }

  if ( prev )
    newtagset = prevtagset;

  return newtagset;
}

void freetag(const Arg *arg){
  const Arg a = {.ui =  moveviewcore(1,0,arg->i,0)};
  tag(&a);
}

void lastfreetag(const Arg *arg){
  uint newtagset = moveviewcore(0,0,arg->i,0);
  const Arg a = {.ui =  moveviewcore(1,1,arg->i,newtagset)};
  tag(&a);
}

void freeview(const Arg *arg){
  const Arg a = {.ui =  moveviewcore(1,0,arg->i,0)};
  view(&a);
}

void incview(const Arg *arg){
  const Arg a = {.ui =  moveviewcore(0,0,arg->i,0)};
  view(&a);
}

void lastfreeview(const Arg *arg){
  uint newtagset = moveviewcore(0,0,arg->i,0);
  const Arg a = {.ui =  moveviewcore(1,1,arg->i,newtagset)};
  view(&a);
}

void lastview(){
  const Arg a = {.ui =  selmon->tagset[selmon->seltags^1]};
  view(&a);
}

  void
unfocus(Client *c, int setfocus)
{
  if (!c)
    return;
  grabbuttons(c, 0);
  XSetWindowBorder(dpy, c->win, scheme[SchemeNorm][ColBorder].pixel);
  if (setfocus) {
    XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
    XDeleteProperty(dpy, root, netatom[NetActiveWindow]);
  }
}

  void
unmanage(Client *c, int destroyed)
{
  Monitor *m = c->mon;
  XWindowChanges wc;

  if (c->swallowing) {
    unswallow(c);
    return;
  }

  Client *s = swallowingclient(c->win);
  if (s) {
    free(s->swallowing);
    s->swallowing = NULL;
    arrange(m);
    focus(NULL);
    return;
  }

  detach(c);
  detachstack(c);
  if (!destroyed) {
    wc.border_width = c->oldbw;
    XGrabServer(dpy); /* avoid race conditions */
    XSetErrorHandler(xerrordummy);
    XConfigureWindow(dpy, c->win, CWBorderWidth, &wc); /* restore border */
    XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
    setclientstate(c, WithdrawnState);
    XSync(dpy, False);
    XSetErrorHandler(xerror);
    XUngrabServer(dpy);
  }
  free(c);

  if (!s) {
    arrange(m);
    focus(NULL);
    updateclientlist();
  }
}

  void
unmapnotify(XEvent *e)
{
  Client *c;
  XUnmapEvent *ev = &e->xunmap;

  if ((c = wintoclient(ev->window))) {
    if (ev->send_event)
      setclientstate(c, WithdrawnState);
    else
      unmanage(c, 0);
  }
  else if ((c = wintosystrayicon(ev->window))) {
    /* KLUDGE! sometimes icons occasionally unmap their windows, but do
     * _not_ destroy them. We map those windows back */
    XMapRaised(dpy, c->win);
    updatesystray();
  }
}

  void
updatebars(void)
{
  unsigned int w;
  Monitor *m;
  XSetWindowAttributes wa = {
    .override_redirect = True,
    .background_pixmap = ParentRelative,
    .event_mask = ButtonPressMask|ExposureMask
  };
  XClassHint ch = {"dwm", "dwm"};
  for (m = mons; m; m = m->next) {
    if (m->barwin)
      continue;
    w = m->ww;
    if (showsystray && m == systraytomon(m))
      w -= getsystraywidth();
    m->barwin = XCreateWindow(dpy, root, m->wx, m->by, w, bh, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
    XDefineCursor(dpy, m->barwin, cursor[CurNormal]->cursor);
    if (showsystray && m == systraytomon(m))
      XMapRaised(dpy, systray->win);
    XMapRaised(dpy, m->barwin);
    m->tabwin = XCreateWindow(dpy, root, m->wx, m->ty, m->ww, th, 0, DefaultDepth(dpy, screen),
        CopyFromParent, DefaultVisual(dpy, screen),
        CWOverrideRedirect|CWBackPixmap|CWEventMask, &wa);
    XDefineCursor(dpy, m->tabwin, cursor[CurNormal]->cursor);
    XMapRaised(dpy, m->tabwin);
    XSetClassHint(dpy, m->barwin, &ch);
  }
}

  void
updatebarpos(Monitor *m)
{
  Client *c;
  int nvis = 0;

  m->wy = m->my;
  m->wh = m->mh;
  if (m->showbar) {
    m->wh -= bh;
    m->by = m->topbar ? m->wy : m->wy + m->wh;
    if ( m->topbar )
      m->wy += bh;
  } else {
    m->by = -bh;
  }

  for(c = m->clients; c; c = c->next) {
    if(ISVISIBLE(c) && !c->isfloating) ++nvis;
  }

  if(m->showtab == showtab_always
      || ((m->showtab == showtab_auto) && (nvis > 1) && (m->lt[m->sellt]->arrange == monocle))) {
    m->wh -= th;
    m->ty = m->toptab ? m->wy : m->wy + m->wh;
    if ( m->toptab )
      m->wy += th;
  } else {
    m->ty = -th;
  }
}

  void
updateclientlist()
{
  Client *c;
  Monitor *m;

  XDeleteProperty(dpy, root, netatom[NetClientList]);
  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      XChangeProperty(dpy, root, netatom[NetClientList],
          XA_WINDOW, 32, PropModeAppend,
          (unsigned char *) &(c->win), 1);
}

  int
updategeom(void)
{
  int dirty = 0;

#ifdef XINERAMA
  if (XineramaIsActive(dpy)) {
    int i, j, n, nn;
    Client *c;
    Monitor *m;
    XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
    XineramaScreenInfo *unique = NULL;

    for (n = 0, m = mons; m; m = m->next, n++);
    /* only consider unique geometries as separate screens */
    unique = ecalloc(nn, sizeof(XineramaScreenInfo));
    for (i = 0, j = 0; i < nn; i++)
      if (isuniquegeom(unique, j, &info[i]))
        memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
    XFree(info);
    nn = j;
    if (n <= nn) { /* new monitors available */
      for (i = 0; i < (nn - n); i++) {
        for (m = mons; m && m->next; m = m->next);
        if (m)
          m->next = createmon();
        else
          mons = createmon();
      }
      for (i = 0, m = mons; i < nn && m; m = m->next, i++)
        if (i >= n
            || unique[i].x_org != m->mx || unique[i].y_org != m->my
            || unique[i].width != m->mw || unique[i].height != m->mh)
        {
          dirty = 1;
          m->num = i;
          m->mx = m->wx = unique[i].x_org;
          m->my = m->wy = unique[i].y_org;
          m->mw = m->ww = unique[i].width;
          m->mh = m->wh = unique[i].height;
          updatebarpos(m);
        }
    } else { /* less monitors available nn < n */
      for (i = nn; i < n; i++) {
        for (m = mons; m && m->next; m = m->next);
        while ((c = m->clients)) {
          dirty = 1;
          m->clients = c->next;
          detachstack(c);
          c->mon = mons;
          attach(c);
          attachstack(c);
        }
        if (m == selmon)
          selmon = mons;
        cleanupmon(m);
      }
    }
    free(unique);
  } else
#endif /* XINERAMA */
  { /* default monitor setup */
    if (!mons)
      mons = createmon();
    if (mons->mw != sw || mons->mh != sh) {
      dirty = 1;
      mons->mw = mons->ww = sw;
      mons->mh = mons->wh = sh;
      updatebarpos(mons);
    }
  }
  if (dirty) {
    selmon = mons;
    selmon = wintomon(root);
  }
  return dirty;
}

  void
updatenumlockmask(void)
{
  unsigned int i, j;
  XModifierKeymap *modmap;

  numlockmask = 0;
  modmap = XGetModifierMapping(dpy);
  for (i = 0; i < 8; i++)
    for (j = 0; j < modmap->max_keypermod; j++)
      if (modmap->modifiermap[i * modmap->max_keypermod + j]
          == XKeysymToKeycode(dpy, XK_Num_Lock))
        numlockmask = (1 << i);
  XFreeModifiermap(modmap);
}

  void
updatesizehints(Client *c)
{
  long msize;
  XSizeHints size;

  if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
    /* size is uninitialized, ensure that size.flags aren't used */
    size.flags = PSize;
  if (size.flags & PBaseSize) {
    c->basew = size.base_width;
    c->baseh = size.base_height;
  } else if (size.flags & PMinSize) {
    c->basew = size.min_width;
    c->baseh = size.min_height;
  } else
    c->basew = c->baseh = 0;
  if (size.flags & PResizeInc) {
    c->incw = size.width_inc;
    c->inch = size.height_inc;
  } else
    c->incw = c->inch = 0;
  if (size.flags & PMaxSize) {
    c->maxw = size.max_width;
    c->maxh = size.max_height;
  } else
    c->maxw = c->maxh = 0;
  if (size.flags & PMinSize) {
    c->minw = size.min_width;
    c->minh = size.min_height;
  } else if (size.flags & PBaseSize) {
    c->minw = size.base_width;
    c->minh = size.base_height;
  } else
    c->minw = c->minh = 0;
  if (size.flags & PAspect) {
    c->mina = (float)size.min_aspect.y / size.min_aspect.x;
    c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
  } else
    c->maxa = c->mina = 0.0;
  c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
}

  void
updatestatus(void)
{
  Monitor* m;
  if (!gettextprop(root, XA_WM_NAME, rawstext, sizeof(rawstext)))
    strcpy(stext, "dwm-"VERSION);
  else
    copyvalidchars(stext, rawstext);
  drawbar(selmon);
  for(m = mons; m; m = m->next)
    drawbar(m);
  updatesystray();
}

  void
updatesystrayicongeom(Client *i, int w, int h)
{
  if (i) {
    i->h = bh;
    if (w == h)
      i->w = bh;
    else if (h == bh)
      i->w = w;
    else
      i->w = (int) ((float)bh * ((float)w / (float)h));
    applysizehints(i, &(i->x), &(i->y), &(i->w), &(i->h), False);
    /* force icons into the systray dimensions if they don't want to */
    if (i->h > bh) {
      if (i->w == i->h)
        i->w = bh;
      else
        i->w = (int) ((float)bh * ((float)i->w / (float)i->h));
      i->h = bh;
    }
  }
}

  void
updatesystrayiconstate(Client *i, XPropertyEvent *ev)
{
  long flags;
  int code = 0;

  if (!showsystray || !i || ev->atom != xatom[XembedInfo] ||
      !(flags = getatomprop(i, xatom[XembedInfo])))
    return;

  if (flags & XEMBED_MAPPED && !i->tags) {
    i->tags = 1;
    code = XEMBED_WINDOW_ACTIVATE;
    XMapRaised(dpy, i->win);
    setclientstate(i, NormalState);
  }
  else if (!(flags & XEMBED_MAPPED) && i->tags) {
    i->tags = 0;
    code = XEMBED_WINDOW_DEACTIVATE;
    XUnmapWindow(dpy, i->win);
    setclientstate(i, WithdrawnState);
  }
  else
    return;
  sendevent(i->win, xatom[Xembed], StructureNotifyMask, CurrentTime, code, 0,
      systray->win, XEMBED_EMBEDDED_VERSION);
}

  void
updatesystray(void)
{
  XSetWindowAttributes wa;
  XWindowChanges wc;
  Client *i;
  Monitor *m = systraytomon(NULL);
  unsigned int x = m->mx + m->mw;
  unsigned int w = 1;

  if (!showsystray)
    return;
  if (!systray) {
    /* init systray */
    if (!(systray = (Systray *)calloc(1, sizeof(Systray))))
      die("fatal: could not malloc() %u bytes\n", sizeof(Systray));
    systray->win = XCreateSimpleWindow(dpy, root, x, m->by, w, bh, 0, 0, scheme[SchemeSel][ColBg].pixel);
    wa.event_mask        = ButtonPressMask | ExposureMask;
    wa.override_redirect = True;
    wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
    XSelectInput(dpy, systray->win, SubstructureNotifyMask);
    XChangeProperty(dpy, systray->win, netatom[NetSystemTrayOrientation], XA_CARDINAL, 32,
        PropModeReplace, (unsigned char *)&netatom[NetSystemTrayOrientationHorz], 1);
    XChangeWindowAttributes(dpy, systray->win, CWEventMask|CWOverrideRedirect|CWBackPixel, &wa);
    XMapRaised(dpy, systray->win);
    XSetSelectionOwner(dpy, netatom[NetSystemTray], systray->win, CurrentTime);
    if (XGetSelectionOwner(dpy, netatom[NetSystemTray]) == systray->win) {
      sendevent(root, xatom[Manager], StructureNotifyMask, CurrentTime, netatom[NetSystemTray], systray->win, 0, 0);
      XSync(dpy, False);
    }
    else {
      fprintf(stderr, "dwm: unable to obtain system tray.\n");
      free(systray);
      systray = NULL;
      return;
    }
  }
  for (w = 0, i = systray->icons; i; i = i->next) {
    /* make sure the background color stays the same */
    wa.background_pixel  = scheme[SchemeNorm][ColBg].pixel;
    XChangeWindowAttributes(dpy, i->win, CWBackPixel, &wa);
    XMapRaised(dpy, i->win);
    w += systrayspacing;
    i->x = w;
    XMoveResizeWindow(dpy, i->win, i->x, 0, i->w, i->h);
    w += i->w;
    if (i->mon != m)
      i->mon = m;
  }
  w = w ? w + systrayspacing : 1;
  x -= w;
  XMoveResizeWindow(dpy, systray->win, x, m->by, w, bh);
  wc.x = x; wc.y = m->by; wc.width = w; wc.height = bh;
  wc.stack_mode = Above; wc.sibling = m->barwin;
  XConfigureWindow(dpy, systray->win, CWX|CWY|CWWidth|CWHeight|CWSibling|CWStackMode, &wc);
  XMapWindow(dpy, systray->win);
  XMapSubwindows(dpy, systray->win);
  /* redraw background */
  XSetForeground(dpy, drw->gc, scheme[SchemeNorm][ColBg].pixel);
  XFillRectangle(dpy, systray->win, drw->gc, 0, 0, w, bh);
  XSync(dpy, False);
}

  void
updatetitle(Client *c)
{
  if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
    gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
  if (c->name[0] == '\0') /* hack to mark broken clients */
    strcpy(c->name, broken);
}

  void
updatewindowtype(Client *c)
{
  Atom state = getatomprop(c, netatom[NetWMState]);
  Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

  if (state == netatom[NetWMFullscreen])
    setfullscreen(c, 1);
  if (wtype == netatom[NetWMWindowTypeDialog])
    c->isfloating = 1;
}

  void
updatewmhints(Client *c)
{
  XWMHints *wmh;

  if ((wmh = XGetWMHints(dpy, c->win))) {
    if (c == selmon->sel && wmh->flags & XUrgencyHint) {
      wmh->flags &= ~XUrgencyHint;
      XSetWMHints(dpy, c->win, wmh);
    } else
      c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
    if (wmh->flags & InputHint)
      c->neverfocus = !wmh->input;
    else
      c->neverfocus = 0;
    XFree(wmh);
  }
}

  void
view(const Arg *arg)
{
  int i;
  unsigned int tmptag;

  if (arg->ui != ~0 && (arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  selmon->seltags ^= 1; /* toggle sel tagset */
  if(arg->ui == ~0 && selmon->tagset[selmon->seltags^1] == TAGMASK){
    if(selmon->sel && !selmon->sel->issticky){
      selmon->tagset[selmon->seltags] = selmon->sel->tags;
      for (i = 0; !(selmon->sel->tags & 1 << i); i++);
      selmon->pertag->curtag = i + 1;
    }
  }else if(arg->ui & TAGMASK){
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
    selmon->pertag->prevtag = selmon->pertag->curtag;

    if (arg->ui == ~0)
      selmon->pertag->curtag = 0;
    else {
      for (i = 0; !(arg->ui & 1 << i); i++);
      selmon->pertag->curtag = i + 1;
    }
  } else {
    tmptag = selmon->pertag->prevtag;
    selmon->pertag->prevtag = selmon->pertag->curtag;
    selmon->pertag->curtag = tmptag;
  }

  selmon->nmaster = selmon->pertag->nmasters[selmon->pertag->curtag];
  selmon->mfact = selmon->pertag->mfacts[selmon->pertag->curtag];
  selmon->sellt = selmon->pertag->sellts[selmon->pertag->curtag];
  selmon->lt[selmon->sellt] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt];
  selmon->lt[selmon->sellt^1] = selmon->pertag->ltidxs[selmon->pertag->curtag][selmon->sellt^1];

  if (selmon->showbar != selmon->pertag->showbars[selmon->pertag->curtag])
    togglebar(NULL);

  if((selmon->tagset[selmon->seltags] == (~0 & TAGMASK))  && (selmon->lt[selmon->sellt] != &layouts[2]) ){
    Arg a;
    a.v = &layouts[2];
    setlayout(&a);
  }

  focus(NULL);
  arrange(selmon);
}

  pid_t
winpid(Window w)
{
  pid_t result = 0;

  xcb_res_client_id_spec_t spec = {0};
  spec.client = w;
  spec.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID;

  xcb_generic_error_t *e = NULL;
  xcb_res_query_client_ids_cookie_t c = xcb_res_query_client_ids(xcon, 1, &spec);
  xcb_res_query_client_ids_reply_t *r = xcb_res_query_client_ids_reply(xcon, c, &e);

  if (!r)
    return (pid_t)0;

  xcb_res_client_id_value_iterator_t i = xcb_res_query_client_ids_ids_iterator(r);
  for (; i.rem; xcb_res_client_id_value_next(&i)) {
    spec = i.data->spec;
    if (spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID) {
      uint32_t *t = xcb_res_client_id_value_value(i.data);
      result = *t;
      break;
    }
  }

  free(r);

  if (result == (pid_t)-1)
    result = 0;
  return result;
}

  pid_t
getparentprocess(pid_t p)
{
  unsigned int v = 0;

#ifdef __linux__
  FILE *f;
  char buf[256];
  snprintf(buf, sizeof(buf) - 1, "/proc/%u/stat", (unsigned)p);

  if (!(f = fopen(buf, "r")))
    return 0;

  fscanf(f, "%*u %*s %*c %u", &v);
  fclose(f);
#endif /* __linux__ */

  return (pid_t)v;
}

  int
isdescprocess(pid_t p, pid_t c)
{
  while (p != c && c != 0)
    c = getparentprocess(c);

  return (int)c;
}

  Client *
termforwin(const Client *w)
{
  Client *c;
  Monitor *m;

  if (!w->pid || w->isterminal)
    return NULL;

  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      if (c->isterminal && !c->swallowing && c->pid && isdescprocess(c->pid, w->pid))
        return c;
    }
  }

  return NULL;
}

  Client *
swallowingclient(Window w)
{
  Client *c;
  Monitor *m;

  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next) {
      if (c->swallowing && c->swallowing->win == w)
        return c;
    }
  }

  return NULL;
}
  void
warp(const Client *c)
{
  int x, y;

  if (!c) {
    warping = 1;
    XWarpPointer(dpy, None, root, 0, 0, 0, 0, selmon->wx + selmon->ww/2, selmon->wy + selmon->wh/2);
    return;
  }

  if (!getrootptr(&x, &y) ||
      (x > c->x - c->bw &&
       y > c->y - c->bw &&
       x < c->x + c->w + c->bw*2 &&
       y < c->y + c->h + c->bw*2) ||
      (y > c->mon->by && y < c->mon->by + bh) ||
      (c->mon->topbar && !y))
    return;
  warping = 1;
  XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w / 2, c->h / 2);
}

  Client *
wintoclient(Window w)
{
  Client *c;
  Monitor *m;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->win == w)
        return c;
  return NULL;
}

Client *
wintosystrayicon(Window w) {
  Client *i = NULL;

  if (!showsystray || !w)
    return i;
  for (i = systray->icons; i && i->win != w; i = i->next) ;
  return i;
}

  Monitor *
wintomon(Window w)
{
  int x, y;
  Client *c;
  Monitor *m;

  if (w == root && getrootptr(&x, &y))
    return recttomon(x, y, 1, 1);
  for (m = mons; m; m = m->next)
    if (w == m->barwin || w == m->tabwin)
      return m;
  if ((c = wintoclient(w)))
    return c->mon;
  return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
  int
xerror(Display *dpy, XErrorEvent *ee)
{
  if (ee->error_code == BadWindow
      || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
      || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
      || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
      || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
      || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
      || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
      || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
      || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
    return 0;
  fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
      ee->request_code, ee->error_code);
  return xerrorxlib(dpy, ee); /* may call exit */
}

  int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
  return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
  int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
  die("dwm: another window manager is already running");
  return -1;
}

Monitor *
systraytomon(Monitor *m) {
  Monitor *t;
  int i, n;
  if(!systraypinning) {
    if(!m)
      return selmon;
    return m == selmon ? m : NULL;
  }
  for(n = 1, t = mons; t && t->next; n++, t = t->next) ;
  for(i = 1, t = mons; t && t->next && i < systraypinning; i++, t = t->next) ;
  if(systraypinningfailfirst && n < systraypinning)
    return mons;
  return t;
}

  void
zoom(const Arg *arg)
{
  Client *cm = nexttiled(selmon->clients);
  Client *c = selmon->sel;
  if (arg->i == 0 && (selmon->lt[selmon->sellt] != &layouts[TALL] || c->isfloating || c == cm)){
    rotate(&((Arg){.i=-1}));
    return;
  }

  if (arg && (!cm || !nexttiled(cm->next)
        || !selmon->lt[selmon->sellt]->arrange
        || selmon->sel->isfloating || selmon->sel->isfullscreen
        || selmon->lt[selmon->sellt] != &layouts[0] 
        || ((arg->i<=0) == (nexttiled(selmon->clients) == c )))){
    const Arg a = {.i = arg->i, .ui = (arg->i==1)};
    tagmon(&a);
    focus(c);
    warp(c);
    restack(selmon);
    return;
  }
  c = selmon->sel;
  Client *o = c;

  if (c == nexttiled(selmon->clients))
    if (!c || !(c = nexttiled(c->next)))
      return;
  pop(c);
  focus(o);
  warp(o);
}

  void
horizontalfocus(const Arg *arg)
{
  Client *c = nexttiled(selmon->clients) ;

  if (c && nexttiled(c->next)	&& !selmon->sel->isfloating && !selmon->sel->isfullscreen){
    if ( selmon->lt[selmon->sellt] == &layouts[0] ){
      if ( (c == selmon->sel) == (arg->i > 0) ){
        if (arg->i > 0)
          c = nexttiled(c->next);
        focus(c);
        warp(c);
        return;
      }
    }else if ( selmon->lt[selmon->sellt] == &layouts[2] ){
      c = focustiled(selmon->sel,arg->i*rows);
      if (c){
        focus(c);
        warp(c);
        return;
      }
    }
  }
  const Arg a = {.i = arg->i, .ui = arg->i};
  focusmon(&a);
}

/*layouts*/
void
grid(Monitor *m) {
  unsigned int i, n, cx, cy, cw, ch, aw, ah;
  Client *c;

  for(n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next))
    n++;

  /* grid dimensions */
  for(cols = 0; cols <= n/2; cols++)
    if(cols*cols >= n)
      break;
  rows = (cols && (cols - 1) * cols >= n) ? cols - 1 : cols;

  /* window geoms (cell height/width) */
  ch = m->wh / (rows ? rows : 1);
  cw = m->ww / (cols ? cols : 1);
  for(i = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next)) {
    cx = m->wx + (i / rows) * cw;
    cy = m->wy + (i % rows) * ch;
    /* adjust height/width of last row/column's windows */
    ah = ((i + 1) % rows == 0) ? m->wh - ch * rows : 0;
    aw = (i >= rows * (cols - 1)) ? m->ww - cw * cols : 0;
    resize(c, cx, cy, cw - 2 * c->bw + aw, ch - 2 * c->bw + ah, False);
    i++;
  }
}

void
resizehorizontal(const Arg* arg){
  if(selmon->sel){
    if(selmon->sel->isfloating)
      changefloatzonex(arg);
    else{
      Arg a;
      if(arg->i<0)
        a.f = -0.05;
      else
        a.f =  0.05;
      setmfact(&a);
    }
  }
}

void
hardresizehorizontal(const Arg* arg){
  if(selmon->sel){
    if(selmon->sel->isfloating)
      changefloatzonex(arg);
    else{
      Arg a;
      if(arg->i<0)
        a.f =  1.5;
      else if(arg->i>0){
        a.f =  1.85;
      }else{
        if(selmon->mfact>0.675)
          a.f = 1.5;
        else
          a.f = 1.85;
      }
      setmfact(&a);
    }
  }
}

void
resizevertical(const Arg* arg){
  if(selmon->sel)
    if(selmon->sel->isfloating)
      changefloatzoney(arg);
}
void
movehorizontal(const Arg* arg){
  if(selmon->sel)
    zoom(arg);
}

void
movevertical(const Arg* arg){
  if(selmon->sel){
    if(!selmon->sel->isfloating){
      if(arg->i<0)
        pushup(NULL);
      else
        pushdown(NULL);
    }
  }
}

  int
main(int argc, char *argv[])
{
  if (argc == 2 && !strcmp("-v", argv[1]))
    die("dwm-"VERSION);
  else if (argc != 1)
    die("usage: dwm [-v]");
  if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
    fputs("warning: no locale support\n", stderr);
  if (!(dpy = XOpenDisplay(NULL)))
    die("dwm: cannot open display");
  if (!(xcon = XGetXCBConnection(dpy)))
    die("dwm: cannot get xcb connection\n");
  checkotherwm();
  autostart_exec();
  setup();
#ifdef __OpenBSD__
  if (pledge("stdio rpath proc exec", NULL) == -1)
    die("pledge");
#endif /* __OpenBSD__ */
  scan();
  run();
  cleanup();
  XCloseDisplay(dpy);
  return EXIT_SUCCESS;
}
