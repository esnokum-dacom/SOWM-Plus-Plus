#include <X11/X.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/shape.h>
#include <X11/keysym.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sowm.h"
#include "config.def.h"

static client *list = NULL;
static client *cur  = NULL;

static XftFont  *mm_font = NULL;
static XftDraw  *mm_draw = NULL;
static XftColor  mm_color;
static int mm_inited = 0;

static canvas_state canvas = {
    .pan_x = {0}, .pan_y = {0},
    .zoom  = {1,1,1,1,1,1,1,1}
};

static int sw, sh;
static int wx, wy;
static unsigned int ww, wh;

static int   pan_active   = 0;
static int   pan_start_x  = 0;
static int   pan_start_y  = 0;
static float pan_origin_x = 0;
static float pan_origin_y = 0;
static int   pan_mon      = 0;

static int numlock = 0;
char *client_get_title(Window w);

static Display     *d;
static XButtonEvent mouse;
static Window       root;
static Window       hud_win = 0;
static Window	    minimap_win[MAX_MONITORS] = {0};
static GC	    minimap_gc                = 0;
int		    minimap		      = 1;
static int	    minimap_px[MAX_MONITORS]  = {0};
static int	    minimap_py[MAX_MONITORS]  = {0};

static void (*events[LASTEvent])(XEvent *e);

#include "config.h"

static void (*events[LASTEvent])(XEvent *e) = {
    [ButtonPress]      = button_press,
    [PropertyNotify]   = notify_property,
    [ButtonRelease]    = button_release,
    [ConfigureRequest] = configure_request,
    [KeyPress]         = key_press,
    [MapRequest]       = map_request,
    [MappingNotify]    = mapping_notify,
    [DestroyNotify]    = notify_destroy,
    [UnmapNotify]      = notify_unmap,
    [EnterNotify]      = notify_enter,
    [MotionNotify]     = notify_motion,
};

char *copystr(const char *s)
{
    size_t len = strlen(s) + 1;

    char *p = malloc(len);

    if (p) memcpy(p, s, len);

    return p;
}

int mon_at_ptr(void) {
    int px, py, di;
    unsigned int du;
    Window dw;
    XQueryPointer(d, root, &dw, &dw, &px, &py, &di, &di, &du);

    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(d, &n);
    if (!info) return 0;

    int mon = 0;
    for (int i = 0; i < n && i < MAX_MONITORS; i++) {
        if (px >= info[i].x_org && px < info[i].x_org + info[i].width &&
            py >= info[i].y_org && py < info[i].y_org + info[i].height) {
            mon = i; break;
        }
    }
    XFree(info);
    return mon;
}

int mon_at_win(Window w) {
    int wx2, wy2;
    unsigned int ww2, wh2;
    win_size(w, &wx2, &wy2, &ww2, &wh2);

    int cx2 = wx2 + (int)ww2 / 2;
    int cy2 = wy2 + (int)wh2 / 2;

    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(d, &n);
    if (!info) return 0;

    int mon = 0;
    for (int i = 0; i < n && i < MAX_MONITORS; i++) {
        if (cx2 >= info[i].x_org && cx2 < info[i].x_org + info[i].width &&
            cy2 >= info[i].y_org && cy2 < info[i].y_org + info[i].height) {
            mon = i; break;
        }
    }
    XFree(info);
    return mon;
}

void minimap_create(void) {
    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(d, &n);

    if (!info) return;

    int x0 = 1<<30, y0 = 1<<30, x1 = 0, y1 = 0;

    for (int i = 0; i < n; i++) {
	x0 = MIN(x0, info[i].x_org);
	y0 = MIN(y0, info[i].y_org);
	x1 = MAX(x1, info[i].x_org + info[i].width);
	y1 = MAX(y1, info[i].y_org + info[i].height);
    }

    float layout_w = (float)(x1 - x0);
    float layout_h = (float)(y1 - y0);
    float box_w = 220, box_h = 220;
    float scale = MIN(box_w / layout_w, box_h / layout_h);

    int gap = 5;
    int origin_x = 10, origin_y = 10; 

    minimap_gc = XCreateGC(d, root, 0, NULL);

    for (int i = 0; i < n && i < MAX_MONITORS; i++) {
        int rank_x = 0, rank_y = 0;
        for (int j = 0; j < n; j++) {
            if (info[j].x_org < info[i].x_org) rank_x++;
            if (info[j].y_org < info[i].y_org) rank_y++;
        }

	int px = origin_x + (int)((info[i].x_org - x0) * scale) + rank_x * gap;
	int py = origin_y + (int)((info[i].y_org - y0) * scale) + rank_y * gap;
	minimap_px[i] = px;
	minimap_py[i] = py;
        int pw = MAX(40, (int)(info[i].width  * scale));
        int ph = MAX(30, (int)(info[i].height * scale));

        XSetWindowAttributes attr;
        attr.override_redirect = True;
        attr.background_pixel  = 0x111111;
        attr.border_pixel      = 0x444444;
        attr.event_mask        = ExposureMask;

        minimap_win[i] = XCreateWindow(d, root, px, py, pw, ph, 0,
                                        CopyFromParent, InputOutput, CopyFromParent,
                                        CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
                                        &attr);
        XMapWindow(d, minimap_win[i]);
        XRaiseWindow(d, minimap_win[i]);
    }

    XFree(info);
}

static void minimap_init(Display *dpy) {
    if (mm_inited) return;

    Visual *visual = DefaultVisual(dpy, DefaultScreen(dpy));
    Colormap cmap  = DefaultColormap(dpy, DefaultScreen(dpy));

    XRenderColor xr = {
        .red = 0x0000,
        .green = 0x0000,
        .blue = 0x0000,
        .alpha = 0xFFFF
    };

    XftColorAllocValue(dpy, visual, cmap, &xr, &mm_color);

    mm_font = XftFontOpenName(dpy, DefaultScreen(dpy), "monospace:size=12");
    mm_inited = 1;
}

static void minimap_draw_one(Window panel, int mon, int mon_w, int mon_h, int mon_x, int mon_y) {
    if (!panel) return;

    minimap_init(d);

    unsigned int mw, mh;
    int mxw, myw;
    win_size(panel, &mxw, &myw, &mw, &mh);

    Visual *visual = DefaultVisual(d, DefaultScreen(d));
    Colormap cmap  = DefaultColormap(d, DefaultScreen(d));

    if (mm_draw) XftDrawDestroy(mm_draw);
    mm_draw = XftDrawCreate(d, panel, visual, cmap);

    XClearWindow(d, panel);
    XRaiseWindow(d, panel);

    float minx =  1e9f, miny =  1e9f;
    float maxx = -1e9f, maxy = -1e9f;
    int   any  = 0;

    for win {
        if (c->monitor != mon) continue;
        any  = 1;
        minx = MIN(minx, c->cx);
        miny = MIN(miny, c->cy);
        maxx = MAX(maxx, c->cx + c->width);
        maxy = MAX(maxy, c->cy + c->height);
    }

    float vx0 = mon_x + canvas.pan_x[mon];
    float vy0 = mon_y +canvas.pan_y[mon];
    float vx1 = vx0 + mon_w;
    float vy1 = vy0 + mon_h;

    if (!any) { minx = vx0; miny = vy0; maxx = vx1; maxy = vy1; }
    minx = MIN(minx, vx0); miny = MIN(miny, vy0);
    maxx = MAX(maxx, vx1); maxy = MAX(maxy, vy1);

    float bw = MAX(maxx - minx, 1.0f);
    float bh = MAX(maxy - miny, 1.0f);
    float scale = MIN(((float)mw - 8) / bw, ((float)mh - 8) / bh);

#define MM_X(v) (int)(4 + ((v) - minx) * scale)
#define MM_Y(v) (int)(4 + ((v) - miny) * scale)

    XSetForeground(d, minimap_gc, 0x555555);
    XDrawRectangle(d, panel, minimap_gc,
                    MM_X(vx0), MM_Y(vy0),
                    MAX(1, (int)((vx1 - vx0) * scale)),
                    MAX(1, (int)((vy1 - vy0) * scale)));

    for win {
        if (c->monitor != mon) continue;

      	int rx = MM_X(c->cx);
        int ry = MM_Y(c->cy);

        int rw = MAX(2, (int)(c->width  * scale));
        int rh = MAX(2, (int)(c->height * scale));
	
        XSetForeground(d, minimap_gc, c == cur ? 0xffffff : 0x505050);
        XFillRectangle(d, panel, minimap_gc,
                        MM_X(c->cx), MM_Y(c->cy), rw, rh);

	char *buf = client_get_title(c->w);;
        if (!buf) buf = "";

	XGlyphInfo ext;
	XftTextExtentsUtf8(d, mm_font, (FcChar8 *)buf, strlen(buf), &ext);
	int bx = rx + rw / 2, by = ry + ext.height + 5;
	int x = bx - ext.xOff / 2;

	XSetForeground(d, minimap_gc, 0x000000);
	XftDrawStringUtf8(mm_draw, &mm_color, mm_font, x, by, (FcChar8 *)buf, strlen(buf));
    }

#undef MM_X
#undef MM_Y
    XFlush(d);
}

void minimap_update(void) {
    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(d, &n);

    if (!info) return;

    for (int i = 0; i < n; i++) {
	minimap_draw_one(minimap_win[i], i, info[i].width, info[i].height, info[i].x_org, info[i].y_org);
    }

    XFree(info);
}

static void always_ot() {
    if (!minimap)
        return;

    for (int i = 0; i < MAX_MONITORS; i++)
        if (minimap_win[i]) XRaiseWindow(d, minimap_win[i]);
}

void toggle_minimap(const Arg arg) {
    minimap = !minimap;

    for (int i = 0; i < MAX_MONITORS; i++) {
	if (!minimap_win[i]) continue;

	if (minimap)
	    XMoveWindow(d, minimap_win[i], minimap_px[i], minimap_py[i]);
	else
	    XMoveWindow(d, minimap_win[i], -800, minimap_py[i]);
    }
    XFlush(d);
}

static void hud_create(void) {
    int hud_w = 320, hud_h = 30;
    int hud_x = (sw - hud_w) / 2;
    int hud_y = 4;

    XSetWindowAttributes attr;
    attr.override_redirect = True;
    attr.background_pixel  = BlackPixel(d, DefaultScreen(d));
    attr.border_pixel      = 0;
    attr.event_mask        = ExposureMask;

    hud_win = XCreateWindow(d, root, hud_x, hud_y, hud_w, hud_h, 0, CopyFromParent, InputOutput, CopyFromParent, CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &attr);

    XMapWindow(d, hud_win);
    XRaiseWindow(d, hud_win);
}

void hud_update(void) {
    if (!hud_win) return;

    int   mon = mon_at_ptr();
    float px  = canvas.pan_x[mon];
    float py  = canvas.pan_y[mon];

    char buf[80];
    snprintf(buf, sizeof(buf), "X: %d  Y: %d", (int)px, (int)py);

    XClearWindow(d, hud_win);
    XRaiseWindow(d, hud_win);

    Visual   *visual = DefaultVisual(d, DefaultScreen(d));
    Colormap  cmap   = DefaultColormap(d, DefaultScreen(d));

    XftDraw *draw = XftDrawCreate(d, hud_win, visual, cmap);

    XftFont *font = XftFontOpenName( d, DefaultScreen(d), "monospace:size=12" );

    if (font) {
	XRenderColor xr = { .red   = 65535, .green = 65535, .blue  = 65535, .alpha = 65535 };
	
	XftColor color;
	XftColorAllocValue(d, visual, cmap, &xr, &color);
	
	XGlyphInfo ext;
	XftTextExtentsUtf8( d, font, (FcChar8 *)buf, strlen(buf), &ext);
	
	unsigned int hw, hh;
	int hx, hy;
	win_size(hud_win, &hx, &hy, &hw, &hh);
	
	int x = ((int)hw - ext.xOff) / 2;
	int y = (hh + font->ascent) / 2.2;
	
	XftDrawStringUtf8(draw, &color, font, x, y, (FcChar8 *)buf, strlen(buf));
	
	XftColorFree(d, visual, cmap, &color);
	XftFontClose(d, font);
    }
    XftDrawDestroy(draw);
    XFlush(d);
}

Window titlebar_create(client *c)
{
    int x, y;
    unsigned int w, h;

    win_size(c->w, &x, &y, &w, &h);

    XSetWindowAttributes attr = {
	.override_redirect = True,
	.background_pixel = 0x222222,
	.border_pixel = 0,
	.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask
    };

    Window titlebar = XCreateWindow(d, root, x, y - TITLEBAR_HEIGHT, w, TITLEBAR_HEIGHT, 0, CopyFromParent, InputOutput, CopyFromParent, CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask, &attr);

    XMapWindow(d, titlebar);

    return titlebar;
}

void titlebar_del (client *c) {
    if (!c) return;
    
    if (c->titlebar) {
	XDestroyWindow(d, c->titlebar);
	c->titlebar = 0;
    }
}

client *client_from_titlebar(Window w){
    for win {
	if (c->titlebar == w)
	    return c;
    }

    return NULL;
}

int is_titlebar (Window w) {
    return client_from_titlebar(w) != NULL;
}

void client_move(client *c, int x, int y) {
    if (!c) return;

    c->x = x;
    c->y = y;

    XMoveWindow(d, c->w, x, y);

    if (c->titlebar)
        XMoveWindow(d, c->titlebar, x, y - TITLEBAR_HEIGHT);
}

void configure(client *c) {
    XConfigureEvent ce;

    ce.type = ConfigureNotify;
    ce.display = d;
    ce.event = c->w;
    ce.window = c->w;
    ce.x = c->x;
    ce.y = c->y;
    ce.width = c->width;
    ce.height = c->height;
    ce.border_width = 0;
    ce.above = None;
    ce.override_redirect = False;
    XSendEvent(d, c->w, False, StructureNotifyMask, (XEvent *)&ce);
}

void resizeclient(client *c, int w, int h) {
    XWindowChanges wc;

    c->oldwidth = c->width; 
    c->width = wc.width = w;

    c->oldheight = c->height;
    c->height = wc.height = h;

    XConfigureWindow(d, c->w, CWWidth | CWHeight, &wc);
    configure(c);

    if (c->titlebar) {
        XResizeWindow(d, c->titlebar, w, TITLEBAR_HEIGHT);
        titlebar_draw(c);
    }

    XSync(d, False);
}

void client_resize(client *c, unsigned int w, unsigned int h) {
    if (!c) return;
    int nw = (int)w, nh = (int)h;
    if (applysizehints(c, &nw, &nh))
        resizeclient(c, nw, nh);
    minimap_update();
}

void updatesizehints(client *c) {
    long msize;
    XSizeHints size;

    if (!XGetWMNormalHints(d, c->w, &size, &msize))
        size.flags = PSize;

    if (size.flags & PBaseSize) { c->basew = size.base_width; c->baseh = size.base_height; }
    else if (size.flags & PMinSize) { c->basew = size.min_width; c->baseh = size.min_height; }
    else c->basew = c->baseh = 0;

    if (size.flags & PResizeInc) { c->incw = size.width_inc; c->inch = size.height_inc; }
    else c->incw = c->inch = 0;

    if (size.flags & PMaxSize) { c->maxw = size.max_width; c->maxh = size.max_height; }
    else c->maxw = c->maxh = 0;

    if (size.flags & PMinSize) { c->minw = size.min_width; c->minh = size.min_height; }
    else if (size.flags & PBaseSize) { c->minw = size.base_width; c->minh = size.base_height; }
    else c->minw = c->minh = 0;

    if (size.flags & PAspect) {
        c->mina = (float)size.min_aspect.y / size.min_aspect.x;
        c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
    } else c->mina = c->maxa = 0.0;
}

int applysizehints(client *c, int *w, int *h) {
    int baseismin;

    *w = *w < 1 ? 1 : *w;
    *h = *h < 1 ? 1 : *h;

    baseismin = (c->basew == c->minw) && (c->baseh == c->minh);
    if (!baseismin) { *w -= c->basew; *h -= c->baseh; }

    if (c->mina > 0 && c->maxa > 0) {
        if (c->maxa < (float)*w / *h)
            *w = *h * c->maxa + 0.5;
        else if (c->mina < (float)*h / *w)
            *h = *w * c->mina + 0.5;
    }

    if (c->incw) *w -= *w % c->incw;
    if (c->inch) *h -= *h % c->inch;

    *w = MAX(*w + (baseismin ? 0 : c->basew), c->minw > 0 ? c->minw : 1);
    *h = MAX(*h + (baseismin ? 0 : c->baseh), c->minh > 0 ? c->minh : 1);

    if (c->maxw) *w = MIN(*w, c->maxw);
    if (c->maxh) *h = MIN(*h, c->maxh);

    return *w != c->width || *h != c->height;
}

char *client_get_title(Window w)
{
    XClassHint hint;
    char *name = NULL;

    if (XGetClassHint(d, w, &hint))
    {
        if (hint.res_class)
        {
            name = copystr(hint.res_class);
            XFree(hint.res_name);
            XFree(hint.res_class);
            return name;
        }

        if (hint.res_name)
        {
            name = copystr(hint.res_name);
            XFree(hint.res_name);
            return name;
        }
    }

    Atom netname = XInternAtom(d, "_NET_WM_NAME", False);
    Atom utf8    = XInternAtom(d, "UTF8_STRING", False);

    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;

    if (XGetWindowProperty(d, w, netname, 0, 1024, False, utf8,
        &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop)
    {
        name = copystr((char *)prop);
        XFree(prop);
        return name;
    }

    return copystr("unknown");
}

void titlebar_draw(client *c) {
    if (!c || !c->titlebar)
        return;

    char buf[256];

    snprintf(buf, sizeof(buf), "%s", client_get_title(c->w));

    XClearWindow(d, c->titlebar);

    Visual   *visual = DefaultVisual(d, DefaultScreen(d));
    Colormap  cmap   = DefaultColormap(d, DefaultScreen(d));

    XftDraw *draw = XftDrawCreate(d, c->titlebar, visual, cmap);

    XftFont *font = XftFontOpenName( d, DefaultScreen(d), "monospace:size=12" );

    if (font) {
	XRenderColor xr = { .red   = 65535, .green = 65535, .blue  = 65535, .alpha = 65535 };
	
	XftColor color;
	XftColorAllocValue(d, visual, cmap, &xr, &color);
	
	XGlyphInfo ext;
	XftTextExtentsUtf8( d, font, (FcChar8 *)buf, strlen(buf), &ext);
	
	XftDrawStringUtf8(draw, &color, font, 8, font->ascent + 1, (FcChar8 *)buf, strlen(buf));
	
	XftColorFree(d, visual, cmap, &color);
	XftFontClose(d, font);
    }

    XftDrawDestroy(draw);
    XFlush(d);
}

void canvas_apply_all(void) {
    for win {
        int   m  = c->monitor;
        float px = canvas.pan_x[m];
        float py = canvas.pan_y[m];
        float z  = canvas.zoom[m];
        client_move(c,
                    canvas_to_screen(c->cx, px, z),
                    canvas_to_screen(c->cy, py, z));
    }
    XFlush(d);
    hud_update();
    minimap_update();
}

void canvas_pan(int mon, float dx, float dy) {
    if (mon < 0 || mon >= MAX_MONITORS) return;
    canvas.pan_x[mon] += dx;
    canvas.pan_y[mon] += dy;
    canvas_apply_all();
}

void canvas_pan_key(const Arg arg) {
    int   mon  = mon_at_ptr();
    float step = (float)PAN_STEP / canvas.zoom[mon];
    switch (arg.i) {
        case 0: canvas_pan(mon, -step,  0);    break;
        case 1: canvas_pan(mon,  step,  0);    break;
        case 2: canvas_pan(mon,  0,    -step); break;
        case 3: canvas_pan(mon,  0,     step); break;
    }
}

void canvas_reset(const Arg arg) {
    int mon = mon_at_ptr();
    canvas.pan_x[mon] = 0;
    canvas.pan_y[mon] = 0;
    canvas.zoom[mon]  = 1.0f;
    canvas_apply_all();
}

void win_focus(client *c) {
    cur = c;
    XSetInputFocus(d, cur->w, RevertToParent, CurrentTime);
    XChangeProperty(d, root,
                    XInternAtom(d, "_NET_ACTIVE_WINDOW", False),
                    XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&cur->w, 1);
}

void canvas_focus(client *c) {
    if (!c) return;

    int m = c->monitor;

    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(d, &n);
    if (!info) { win_focus(c); return; }

    int mx = 0, my = 0, mw = sw, mh = sh;
    if (m < n) {
        mx = info[m].x_org;
        my = info[m].y_org;
        mw = info[m].width;
        mh = info[m].height;
    }
    XFree(info);

    unsigned int cw, ch;
    win_size(c->w, &(int){0}, &(int){0}, &cw, &ch);

    float z = canvas.zoom[m];
    float target_sx = mx + (mw - (int)cw) / 2.0f;
    float target_sy = my + (mh - (int)ch) / 2.0f;

    canvas.pan_x[m] = c->cx - target_sx / z;
    canvas.pan_y[m] = c->cy - target_sy / z;

    canvas_apply_all();
    XRaiseWindow(d, c->w);
    win_focus(c);
}

void win_prev(const Arg arg) { if (cur) canvas_focus(cur->prev); }
void win_next(const Arg arg) { if (cur) canvas_focus(cur->next); }

void notify_destroy(XEvent *e) {
    win_del(e->xdestroywindow.window);
    minimap_update();
    if (list)
        win_focus(list->prev);
    else {
        cur = NULL;
        XSetInputFocus(d, root, RevertToPointerRoot, CurrentTime);
    }
}

void notify_unmap(XEvent *e) {
    Window w = e->xunmap.window;

    int managed = 0;
    for win
        if (c->w == w) { managed = 1; break; }
    if (!managed) return;

    win_del(w);
    if (list)
        win_focus(list->prev);
    else {
        cur = NULL;
        XSetInputFocus(d, root, RevertToPointerRoot, CurrentTime);
    }
}

void notify_enter(XEvent *e) {
    while (XCheckTypedEvent(d, EnterNotify, e));
    for win
        if (c->w == e->xcrossing.window)
            win_focus(c);
    minimap_update();
}

void notify_property(XEvent *e) {
    XPropertyEvent *ev = &e->xproperty;

    for win {
        if (c->w == ev->window) {

            Atom visible =
                XInternAtom(d,
                    "_NET_WM_VISIBLE_NAME",
                    False);

            Atom name =
                XInternAtom(d,
                    "_NET_WM_NAME",
                    False);

            if (ev->atom == XA_WM_NAME || ev->atom == visible || ev->atom == name)
            {
                titlebar_draw(c);
            } else if (ev->atom == XA_WM_NORMAL_HINTS) {
                updatesizehints(c);
            }

            break;
        }
    }
}

void notify_motion(XEvent *e) {
    while (XCheckTypedEvent(d, MotionNotify, e));

    if (pan_active) {
        float dx = e->xbutton.x_root - pan_start_x;
        float dy = e->xbutton.y_root - pan_start_y;
        float z  = canvas.zoom[pan_mon];
        canvas.pan_x[pan_mon] = pan_origin_x - dx / z;
        canvas.pan_y[pan_mon] = pan_origin_y - dy / z;
        canvas_apply_all();
        return;
    }

    if (!mouse.subwindow || (cur && cur->f))
        return;

    int xd = e->xbutton.x_root - mouse.x_root;
    int yd = e->xbutton.y_root - mouse.y_root;

    if (mouse.button == 1) {
        int new_sx = wx + xd;
        int new_sy = wy + yd;
        client_move(cur, new_sx, new_sy);
	minimap_update();
        if (cur) {
            cur->monitor = mon_at_win(cur->w);
            int   m = cur->monitor;
            float z = canvas.zoom[m];
            cur->cx = (float)new_sx / z + canvas.pan_x[m];
            cur->cy = (float)new_sy / z + canvas.pan_y[m];
        }
    } else if (mouse.button == 3) {
        client_resize(cur, MAX(1, ww + xd), MAX(1, wh + yd));
    }
}

void key_press(XEvent *e) {
    KeySym keysym = XkbKeycodeToKeysym(d, e->xkey.keycode, 0, 0);
    for (unsigned int i = 0; i < sizeof(keys) / sizeof(*keys); ++i)
        if (keys[i].keysym == keysym &&
            mod_clean(keys[i].mod) == mod_clean(e->xkey.state))
            keys[i].function(keys[i].arg);
}

void button_press(XEvent *e) {
    if (e->xbutton.button == 2) {
        pan_active   = 1;
        pan_mon      = mon_at_ptr();
        pan_start_x  = e->xbutton.x_root;
        pan_start_y  = e->xbutton.y_root;
        pan_origin_x = canvas.pan_x[pan_mon];
        pan_origin_y = canvas.pan_y[pan_mon];
        return;
    }

    if (!e->xbutton.subwindow) return;
    win_size(e->xbutton.subwindow, &wx, &wy, &ww, &wh);
    XRaiseWindow(d, e->xbutton.subwindow);
    mouse = e->xbutton;

    if (TITLEBAR == 1){
	client *c = client_from_titlebar(e->xbutton.subwindow);

	if (c) {
	    cur = c;
	    mouse = e->xbutton;

	    win_size(c->w, &wx, &wy, &ww, &wh);
	    XRaiseWindow(d, c->w);
	}
    }
    return;
}

void button_release(XEvent *e) {
    if (e->xbutton.button == 2) { pan_active = 0; return; }
    mouse.subwindow = 0;
}

void win_add(Window w) {
    client *c;
    if (!(c = (client *)calloc(1, sizeof(client)))) exit(1);

    c->w       = w;
    c->monitor = mon_at_win(w);

    int sx = 0, sy = 0;
    unsigned int dw2, dh2;
    win_size(w, &sx, &sy, &dw2, &dh2);
    int   m = c->monitor;
    float z = canvas.zoom[m];
    c->cx = (float)sx / z + canvas.pan_x[m];
    c->cy = (float)sy / z + canvas.pan_y[m];

    c->x = sx;
    c->y = sy;
    c->width  = (int)dw2;
    c->height = (int)dh2;
    updatesizehints(c);

    if (TITLEBAR){
	c->titlebar = titlebar_create(c);

	titlebar_draw(c);
    }

    if (list) {
        list->prev->next = c;
        c->prev          = list->prev;
        list->prev       = c;
        c->next          = list;
    } else {
        list       = c;
        list->prev = list->next = list;
    }
}

void win_del(Window w) {
    client *x = NULL;
    for win if (c->w == w) x = c;
    if (!list || !x) return;
    titlebar_del(x);
    if (x->prev == x) list = NULL;
    if (list == x)    list = x->next;
    if (x->next) x->next->prev = x->prev;
    if (x->prev) x->prev->next = x->next;
    free(x);
}

void win_kill(const Arg arg) {
    if (!cur) return;
    Atom *protocols;
    int   n;
    Atom  wm_delete = XInternAtom(d, "WM_DELETE_WINDOW", False);
    Atom  wm_protos = XInternAtom(d, "WM_PROTOCOLS",     False);
    if (XGetWMProtocols(d, cur->w, &protocols, &n)) {
        int ok = 0;
        for (int i = 0; i < n; i++)
            if (protocols[i] == wm_delete) { ok = 1; break; }
        XFree(protocols);
        if (ok) {
            XEvent ev = {.xclient = {
                .type = ClientMessage, .window = cur->w,
                .message_type = wm_protos, .format = 32,
                .data.l[0] = wm_delete, .data.l[1] = CurrentTime,
            }};
            XSendEvent(d, cur->w, False, NoEventMask, &ev);
            return;
        }
    }
    XKillClient(d, cur->w);
}

void win_center(const Arg arg) {
    if (!cur) return;
    win_size(cur->w, &(int){0}, &(int){0}, &ww, &wh);

    int px_ptr, py_ptr, di;
    unsigned int du;
    Window dw;
    XQueryPointer(d, root, &dw, &dw, &px_ptr, &py_ptr, &di, &di, &du);

    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(d, &n);
    int mx = 0, my = 0, mw = sw, mh = sh;
    if (info) {
        for (int i = 0; i < n; i++) {
            if (px_ptr >= info[i].x_org &&
                px_ptr <  info[i].x_org + info[i].width &&
                py_ptr >= info[i].y_org &&
                py_ptr <  info[i].y_org + info[i].height) {
                mx = info[i].x_org; my = info[i].y_org;
                mw = info[i].width; mh = info[i].height;
                break;
            }
        }
        XFree(info);
    }

    int sx = mx + (mw - (int)ww) / 2;
    int sy = my + (mh - (int)wh) / 2;
    client_move(cur, sx, sy);

    cur->monitor = mon_at_win(cur->w);
    int   m = cur->monitor;
    float z = canvas.zoom[m];
    cur->cx = (float)sx / z + canvas.pan_x[m];
    cur->cy = (float)sy / z + canvas.pan_y[m];
}

void win_fs(const Arg arg) {
    if (!cur) return;
    if (!cur->f)
        win_size(cur->w, &cur->wx, &cur->wy, &cur->ww, &cur->wh);

    int px_ptr, py_ptr, di;
    unsigned int du;
    Window dw;
    XQueryPointer(d, root, &dw, &dw, &px_ptr, &py_ptr, &di, &di, &du);

    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(d, &n);
    int mx = 0, my = 0, mw = sw, mh = sh;
    if (info) {
        for (int i = 0; i < n; i++) {
            if (px_ptr >= info[i].x_org &&
                px_ptr <  info[i].x_org + info[i].width &&
                py_ptr >= info[i].y_org &&
                py_ptr <  info[i].y_org + info[i].height) {
                mx = info[i].x_org; my = info[i].y_org;
                mw = info[i].width; mh = info[i].height;
                break;
            }
        }
        XFree(info);
    }

    if ((cur->f = cur->f ? 0 : 1)) {
        XMoveResizeWindow(d, cur->w, mx, my, mw, mh);
        if (cur->titlebar) XUnmapWindow(d, cur->titlebar);
    } else {
        XMoveResizeWindow(d, cur->w, cur->wx, cur->wy, cur->ww, cur->wh);
        if (cur->titlebar) {
            XMoveResizeWindow(d, cur->titlebar, cur->wx, cur->wy - TITLEBAR_HEIGHT, cur->ww, TITLEBAR_HEIGHT);
            XMapWindow(d, cur->titlebar);
            titlebar_draw(cur);
        }
    }
}

void win_round_corners(Window w, int rad) {
    unsigned int rw, rh;
    unsigned int dia = 2 * (unsigned int)rad;
    win_size(w, &(int){0}, &(int){0}, &rw, &rh);
    if (rw < dia || rh < dia) return;
    Pixmap mask = XCreatePixmap(d, w, rw, rh, 1);
    if (!mask) return;
    XGCValues xgcv;
    GC shape_gc = XCreateGC(d, mask, 0, &xgcv);
    if (!shape_gc) { XFreePixmap(d, mask); return; }
    XSetForeground(d, shape_gc, 0);
    XFillRectangle(d, mask, shape_gc, 0, 0, rw, rh);
    XSetForeground(d, shape_gc, 1);
    XFillArc(d, mask, shape_gc, 0,        0,        dia, dia, 0, 23040);
    XFillArc(d, mask, shape_gc, rw-dia-1, 0,        dia, dia, 0, 23040);
    XFillArc(d, mask, shape_gc, 0,        rh-dia-1, dia, dia, 0, 23040);
    XFillArc(d, mask, shape_gc, rw-dia-1, rh-dia-1, dia, dia, 0, 23040);
    XFillRectangle(d, mask, shape_gc, rad, 0,   rw - dia, rh);
    XFillRectangle(d, mask, shape_gc, 0,   rad, rw,       rh - dia);
    XShapeCombineMask(d, w, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreePixmap(d, mask);
    XFreeGC(d, shape_gc);
}

void configure_request(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;
    int sx = ev->x, sy = ev->y;
    unsigned int mask = ev->value_mask;
    client *target = NULL;
    if (list) {
        client *t2 = NULL, *c2 = list;
        for (; c2 && t2 != list->prev; t2 = c2, c2 = c2->next) {
            if (c2->w == ev->window) {
                target = c2;
                int m = c2->monitor;
                sx   = canvas_to_screen(c2->cx, canvas.pan_x[m], canvas.zoom[m]);
                sy   = canvas_to_screen(c2->cy, canvas.pan_y[m], canvas.zoom[m]);
                mask = (mask & ~(CWX | CWY)) | CWX | CWY;
                break;
            }
        }
    }
    XConfigureWindow(d, ev->window, mask,
                     &(XWindowChanges){
                         .x = sx, .y = sy,
                         .width  = ev->width,  .height = ev->height,
                         .sibling = ev->above,  .stack_mode = ev->detail,
                     });

    if (target && target->titlebar) {
        XMoveResizeWindow(d, target->titlebar, sx, sy - TITLEBAR_HEIGHT,
                           ev->width, TITLEBAR_HEIGHT);
        titlebar_draw(target);
    }

}

void map_request(XEvent *e) {
    Window w = e->xmaprequest.window;
    XSelectInput(d, w, StructureNotifyMask | EnterWindowMask | PropertyChangeMask);
    win_size(w, &wx, &wy, &ww, &wh);
    win_add(w);
    minimap_update();
    cur = list->prev;
    if (wx + wy == 0) win_center((Arg){0});
    {
        int sx = 0, sy = 0;
        unsigned int dw2, dh2;
        win_size(w, &sx, &sy, &dw2, &dh2);
        cur->monitor = mon_at_win(w);
        int   m = cur->monitor;
        float z = canvas.zoom[m];
        cur->cx = (float)sx / z + canvas.pan_x[m];
        cur->cy = (float)sy / z + canvas.pan_y[m];
    }
    XMapWindow(d, w);
    win_focus(list->prev);
}

void mapping_notify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;
    if (ev->request == MappingKeyboard || ev->request == MappingModifier) {
        XRefreshKeyboardMapping(ev);
        input_grab(root);
    }
}

void run(const Arg arg) {
    if (fork()) return;
    if (d) close(ConnectionNumber(d));
    setsid();
    execvp((char *)arg.com[0], (char **)arg.com);
}

void input_grab(Window root) {
    unsigned int i, j;
    unsigned int modifiers[] = {0, LockMask, numlock, numlock | LockMask};
    XModifierKeymap *modmap  = XGetModifierMapping(d);
    KeyCode code;

    for (i = 0; i < 8; i++)
        for (int k = 0; k < modmap->max_keypermod; k++)
            if (modmap->modifiermap[i * modmap->max_keypermod + k] ==
                XKeysymToKeycode(d, 0xff7f))
                numlock = (1 << i);

    XUngrabKey(d, AnyKey, AnyModifier, root);

    for (i = 0; i < sizeof(keys) / sizeof(*keys); i++)
        if ((code = XKeysymToKeycode(d, keys[i].keysym)))
            for (j = 0; j < sizeof(modifiers) / sizeof(*modifiers); j++)
                XGrabKey(d, code, keys[i].mod | modifiers[j], root, True,
                         GrabModeAsync, GrabModeAsync);

    for (i = 1; i < 4; i++)
        for (j = 0; j < sizeof(modifiers) / sizeof(*modifiers); j++)
            XGrabButton(d, i, MOD | modifiers[j], root, True,
                        ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                        GrabModeAsync, GrabModeAsync, 0, 0);

    XFreeModifiermap(modmap);
}

void ws_focusnext(const Arg arg) {
    int px, py, di;
    unsigned int du;
    Window dw;
    XQueryPointer(d, root, &dw, &dw, &px, &py, &di, &di, &du);

    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(d, &n);
    if (!info || n < 2) { XFree(info); return; }

    int cur_mon = 0;
    for (int i = 0; i < n; i++) {
        if (px >= info[i].x_org && px < info[i].x_org + info[i].width &&
            py >= info[i].y_org && py < info[i].y_org + info[i].height) {
            cur_mon = i; break;
        }
    }
    int next = (cur_mon + 1) % n;
    XWarpPointer(d, None, root, 0, 0, 0, 0,
                 info[next].x_org + info[next].width  / 2,
                 info[next].y_org + info[next].height / 2);
    minimap_update();
    XFree(info);
    XFlush(d);
}

void move_nextmon(const Arg arg) {
    int px_ptr, py_ptr, di;
    unsigned int du;
    Window dw;
    XQueryPointer(d, root, &dw, &dw, &px_ptr, &py_ptr, &di, &di, &du);

    int n;
    XineramaScreenInfo *info = XineramaQueryScreens(d, &n);
    if (!info || n < 2) { XFree(info); return; }

    int cur_mon = -1;
    for (int i = 0; i < n; i++) {
        if (px_ptr >= info[i].x_org && px_ptr < info[i].x_org + info[i].width &&
            py_ptr >= info[i].y_org && py_ptr < info[i].y_org + info[i].height) {
            cur_mon = i; break;
        }
    }
    if (cur_mon == -1) { XFree(info); return; }

    int next = (cur_mon + 1) % n;
    XWarpPointer(d, None, root, 0, 0, 0, 0,
                 info[next].x_org + info[next].width  / 2,
                 info[next].y_org + info[next].height / 2);

    if (cur) {
        unsigned int cw, ch, bw, depth;
        int cwx, cwy;
        Window rr;
        XGetGeometry(d, cur->w, &rr, &cwx, &cwy, &cw, &ch, &bw, &depth);
        int new_sx = info[next].x_org + (info[next].width  - (int)cw) / 2;
        int new_sy = info[next].y_org + (info[next].height - (int)ch) / 2;
        client_move(cur, new_sx, new_sy);
	minimap_update();
        cur->monitor = next;
        float z = canvas.zoom[next];
        cur->cx = (float)new_sx / z + canvas.pan_x[next];
        cur->cy = (float)new_sy / z + canvas.pan_y[next];
    }

    XFree(info);
    XFlush(d);
}

int main(void) {
    XEvent ev;
    if (!(d = XOpenDisplay(0))) exit(1);
    signal(SIGCHLD, SIG_IGN);
    XSetErrorHandler(xerror);

    int s = DefaultScreen(d);
    root  = RootWindow(d, s);
    sw    = XDisplayWidth(d, s);
    sh    = XDisplayHeight(d, s);

    XSelectInput(d, root, SubstructureRedirectMask);
    XDefineCursor(d, root, XCreateFontCursor(d, 68));
    input_grab(root);


    if (UI_HUD) {
	if (CORDS) {
	// Coordinates
	    hud_create();
	    hud_update();
	}

	minimap_create();
	if (!minimap) {
	    for (int i = 0; i < MAX_MONITORS; i++) {
		if (minimap_win[i])
		    XMoveWindow(d, minimap_win[i], -800, minimap_py[i]);
	    }
	}
    }


    while (1 && !XNextEvent(d, &ev)) {
        if ((ev.type == MapNotify || ev.type == ConfigureNotify) && hud_win)
            XRaiseWindow(d, hud_win);
        if (events[ev.type])
            events[ev.type](&ev);
	if (UI_HUD)
	    always_ot();
	else
	    printf("No HUD");
    }
}
