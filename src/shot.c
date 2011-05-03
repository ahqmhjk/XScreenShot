#include "g_pixbuf.h"
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <strings.h>
#include <sys/param.h>

#include "crosshair.xbm"
#include "crosshair_mask.xbm"

#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5

#define IMAX(a,b) ((a)>(b)?(a):(b))
#define IMIN(a,b) ((a)<(b)?(a):(b))

#ifndef uint32_t
typedef unsigned int uint32_t;
#endif

typedef struct _mwmhints{
	uint32_t flags;
    uint32_t functions;
	uint32_t decorations;
    int32_t  input_mode;
    uint32_t status;
}MWMHints;

static Display *dpy;
static Window win;

static void grab_pointer_position(int *src_x, int *src_y, int *width, int *height);
static void my_init()
{
	dpy = XOpenDisplay(NULL);
}

static void my_close()
{
	if (dpy != NULL)
		XCloseDisplay(dpy);
}

static void createWindow()
{
	int scr = DefaultScreen(dpy);
	int width = DisplayWidth(dpy, scr);
	int height = DisplayHeight(dpy, scr);
	XSetWindowAttributes attr;
	attr.event_mask = ButtonPress | ButtonRelease;
	win = XCreateWindow(dpy, RootWindow(dpy, scr), 0, 0, width, height, 0, 0, InputOnly, DefaultVisual(dpy, scr), CWEventMask, &attr);
}

static void removeTile()
{
	MWMHints mwmhints;
    Atom prop;
    memset(&mwmhints, 0, sizeof(mwmhints));
    prop = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
    mwmhints.flags = MWM_HINTS_DECORATIONS;
    mwmhints.decorations = 0;
    XChangeProperty(dpy, win, prop, prop, 32, PropModeReplace, (unsigned char *) &mwmhints, PROP_MWM_HINTS_ELEMENTS);
}

static void show_forever()
{
#if 1  //实现在linux桌面任意工作区可见
	Atom net_wm_state_sticky=XInternAtom(dpy, "_NET_WM_STATE_STICKY", True);
	Atom net_wm_state = XInternAtom (dpy, "_NET_WM_STATE", False);
  	XChangeProperty (dpy, win, net_wm_state, XA_ATOM, 32, PropModeAppend, (unsigned char *)&net_wm_state_sticky, 1);
#endif
}

static void show_toplevel()
{
#if 1 // 窗口始终置顶
	Atom net_wm_window_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
	Atom net_wm_window_type_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
	XChangeProperty (dpy, win, net_wm_window_type, XA_ATOM, 32, PropModeReplace, (unsigned char *)&net_wm_window_type_dock, 1);  
#endif
}

static void showWindow()
{
	XMapWindow(dpy, win);
}

static void changeCursor(Display *display, Window window)
{
	Colormap cmap;
    Cursor no_ptr;
    XColor black, dummy;
    cmap = DefaultColormap(display, DefaultScreen(display));
    XAllocNamedColor(display, cmap, "black", &black, &dummy);
    Pixmap bm_crosshair = XCreateBitmapFromData(display, window, crosshair_bits, crosshair_width, crosshair_height);
    Pixmap bm_crosshair_mask = XCreateBitmapFromData(display, window, crosshair_mask_bits, crosshair_width, crosshair_height);
    no_ptr = XCreatePixmapCursor(display, bm_crosshair, bm_crosshair_mask, &black, &black, 0, 0);

    XDefineCursor(display, window, no_ptr);
    XFreeCursor(display, no_ptr);
    if (bm_crosshair != None)
    	XFreePixmap(display, bm_crosshair);
	if (bm_crosshair_mask != None)
        XFreePixmap(display, bm_crosshair_mask);
    
	XFreeColors(display, cmap, &black.pixel, 1, 0);
}

static void window_main_loop(int *src_x, int *src_y, int *dest_x, int *dest_y)
{
	XSelectInput(dpy, win, ButtonPressMask | ButtonReleaseMask);
	int i = 0;
	
	while (i != 2) {
		XEvent event;
		XNextEvent(dpy, &event);
		if (event.type == ButtonPress) {
		   	*src_x = event.xbutton.x_root;
			*src_y = event.xbutton.y_root;
			if (event.xbutton.button == 1)
				++i;
		}
		if (event.type == ButtonRelease) {
		   	*dest_x = event.xbutton.x_root;
			*dest_y = event.xbutton.y_root;
			if (event.xbutton.button == 1)	
				++i;
		}
	}
}

void grab_pointer_position(int *src_x, int *src_y, int *width, int *height)
{
	int dest_x, dest_y;
	createWindow();
	removeTile();
	show_forever();
	show_toplevel();
	changeCursor(dpy, win);
	showWindow();
	window_main_loop(src_x, src_y, &dest_x, &dest_y);
	*width = IMAX(*src_x, dest_x) - IMIN(*src_x, dest_x);
	*height = IMAX(*src_y, dest_y) - IMIN(*src_y, dest_y);
}

void grab_window_position(int *src_x, int *src_y, int *width, int *height)
{
   	Cursor cursor;		/* cursor to use when selecting */
    Window root;		/* the current root */
    Window retwin = None;	/* the window that got selected */
    int retbutton = -1;		/* button used to select window */
    int pressed = 0;		/* count of number of buttons pressed */

#define MASK (ButtonPressMask | ButtonReleaseMask)

    root = DefaultRootWindow(dpy);
    cursor = XCreateFontCursor(dpy, XC_pirate);
    if (cursor == None) {
		fprintf (stderr, "%s:  unable to create selection cursor\n", "grab_window_position");
		return;
    }

    XSync (dpy, 0);			/* give xterm a chance */

    if (XGrabPointer (dpy, root, False, MASK, GrabModeSync, GrabModeAsync, None, cursor, CurrentTime) != GrabSuccess) {
		fprintf (stderr, "%s:  unable to grab cursor\n", "grab_window_position");
		return;
    }

    /* from dsimple.c in xwininfo */
    while (retwin == None || pressed != 0) {
		XEvent event;

		XAllowEvents (dpy, SyncPointer, CurrentTime);
		XWindowEvent (dpy, root, MASK, &event);
		switch (event.type) {
	  		case ButtonPress:
	    		if (retwin == None) {
					retbutton = event.xbutton.button;
					retwin = ((event.xbutton.subwindow != None) ? event.xbutton.subwindow : root);
	    		}
	    		pressed++;
	    		continue;
	  		case ButtonRelease:
	    		if (pressed > 0) pressed--;
	    		continue;
		}					/* end switch */
    }						/* end for */
    XUngrabPointer (dpy, CurrentTime);
    XFreeCursor (dpy, cursor);
    XSync (dpy, 0);

	if (retwin != None) {
		XWindowAttributes attr;
		XGetWindowAttributes(dpy, retwin, &attr);
		*src_x = attr.x;
		*src_y = attr.y;
		*width = attr.width;
		*height = attr.height;
	}
	else {
		*src_x = *src_y = *width = *height = 0;
	}
}

void grab_window(const char *fileName, g_save_type type)
{
	my_init();
	int x, y, w, h;
//	grab_pointer_position(&x, &y, &w, &h);
	grab_window_position(&x, &y, &w, &h);
   	XWindowAttributes window_attr;
	Window window = DefaultRootWindow(dpy);
	GPixbuf *dest = g_pixbuf_x_get_from_drawable(dpy, window, x, y, w, h);
	FILE *fp;
	fp = fopen(fileName, "wba");

	g_pixbuf_save(dest, fp, type);
	if (fp) fclose(fp);
	free(dest->pixels);
	free(dest);
	my_close();
}
