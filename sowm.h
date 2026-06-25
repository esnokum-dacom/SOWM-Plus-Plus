#pragma once
#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdlib.h>

#define MAX_MONITORS 8
#define TITLEBAR_HEIGHT 24

#define win (client *t = 0, *c = list; c && t != list->prev; t = c, c = c->next)

#define canvas_to_screen(val, pan, zoom) (int)(((val) - (pan)) * (zoom))
#define screen_to_canvas(val, pan, zoom) ((val) / (zoom) + (pan))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CLAMP(v, lo, hi) (MAX((lo), MIN((v), (hi))))

#define win_size(W, gx, gy, gw, gh) \
  XGetGeometry(d, W, &(Window){0}, gx, gy, gw, gh, \
               &(unsigned int){0}, &(unsigned int){0})

#define mod_clean(mask) \
  (mask & ~(numlock | LockMask) & \
   (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | \
    Mod5Mask))

typedef struct {
  const char **com;
  const int    i;
  const Window w;
} Arg;

struct key {
  unsigned int  mod;
  KeySym        keysym;
  void        (*function)(const Arg arg);
  const Arg     arg;
};

typedef struct client {
  struct client *next, *prev;
  Window titlebar;
  Window w;
  int monitor;
  int f;
  int wx, wy;
  int mx, my;
  int x, y;
  int width, height;
  int oldx, oldy, oldwidth, oldheight;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh;
  float mina, maxa;      
  float cx, cy;
  unsigned int ww, wh;
} client;

typedef struct {
  float pan_x[MAX_MONITORS];
  float pan_y[MAX_MONITORS];
  float zoom[MAX_MONITORS];
} canvas_state;

typedef struct {
    int x, y;
    int tx, ty;
    int active;
} MinimapState;

char *copystr(const char *s);
void button_press(XEvent *e);
void button_release(XEvent *e);
void configure_request(XEvent *e);
void input_grab(Window root);
void key_press(XEvent *e);
void notify_property(XEvent *e);
void notify_unmap(XEvent *e);
void map_request(XEvent *e);
void mapping_notify(XEvent *e);
void notify_destroy(XEvent *e);
void notify_enter(XEvent *e);
void notify_motion(XEvent *e);
void run(const Arg arg);
void win_add(Window w);
void win_center(const Arg arg);
void win_del(Window w);
void win_fs(const Arg arg);
void win_focus(client *c);
void win_kill(const Arg arg);
void win_prev(const Arg arg);
void win_next(const Arg arg);
void win_round_corners(Window w, int rad);
void move_nextmon(const Arg arg);
void ws_focusnext(const Arg arg);

void canvas_pan(int mon, float dx, float dy);
void canvas_pan_key(const Arg arg);
void canvas_reset(const Arg arg);
void canvas_apply_all(void);
void canvas_focus(client *c);

static void hud_create(void);
void hud_update(void);
void minimap_create(void);
static void minimap_init(Display *dpy);
static void minimap_draw_one(Window panel, int mon, int mon_w, int mon_h, int mon_x, int mon_y);
static long now_ms(void);
void minimap_update(void);
static void always_ot();
void toggle_minimap(const Arg arg);
void minimap_tick(void);
Window titlebar_create(client *c);
void titlebar_draw(client *c);
void titlebar_del(client *c);
client *client_from_titlebar(Window w);
int is_titlebar(Window w);
void client_move(client *c, int x, int y);
void updatesizehints(client *c);
void resizeclient(client *c, int w, int h);
void configure(client *c);
void client_resize(client *c, unsigned int w, unsigned int h);
int applysizehints(client *c, int *w, int *h);

int  mon_at_ptr(void);
int  mon_at_win(Window w);

static int xerror() { return 0; }
