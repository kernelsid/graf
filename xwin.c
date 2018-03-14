/*
 * Copyright (c) 1991, 1992, 1993, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Lawrence Berkeley Laboratory,
 * Berkeley, CA.  The name of the University may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * This code is derived from the original X10 xgraph written by
 * David Harrison University of California,  Berkeley 1986, 1987
 * 
 * Heavily hacked by Van Jacobson and Steven McCanne, UCB/LBL: added mouse
 * functions to id points and compute slopes and distances.  added keystroke
 * commands to toggle most of the visual attributes of a window (grid, line, 
 * markers, etc.).
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#include "flags.h"
#include "xwin.h"
#include "dataset.h"

#define DEFAULT_FONT "-*-helvetica-bold-r-*-*-12-*"
#define TITLE_FONT "-*-helvetica-bold-r-*-*-14-*"

static int DoKeyPress(Window, LocalWin *, XKeyEvent *, int type);
static int DoButton(XButtonEvent *b, LocalWin *wi, Cursor zc);
static void DoSelection(XSelectionRequestEvent *);
void xdefaults(struct plotflags *);
int DataSetHidden(LocalWin *wi, int setno);
static Window main_win;
static Window help_win;
static Atom wm_delete_window;

/* VARARGS */
extern void error(char* cp, ...);
extern void DataSetBBox(BBox *b);
extern void initGCs(Window win);
extern void RedrawWindow(Window win, LocalWin *wi);
extern void ResizeWindow(Window win, LocalWin *wi);
extern void DoIdentify(XButtonEvent *e, LocalWin *wi, Cursor cur, int noPoints);
extern void DoSlope(XButtonEvent *e, LocalWin *wi, Cursor cur);
extern void DoRegression(XButtonEvent *e, LocalWin *wi, Cursor cur);
extern void DoDistance(XButtonEvent *e, LocalWin *wi, Cursor cur);
extern void DoWriteSubset(XButtonEvent *evt, LocalWin *wi, Cursor cur);
extern int HandleZoom(XButtonEvent *evt, LocalWin *wi, Cursor cur);
extern void DrawWindow(Window win, LocalWin *wi);
extern void DrawSetNo(LocalWin *wi, Window win, int setno);
extern int MeasureHelp();

extern char *progname;
char *geometry = "=600x512";	/* Geometry specification */
Display *display;
int dateXFlag;		/* Date X axis */
int localTime;		/* Show timestamps as local time not GMT */
int logXFlag;		/* Logarithmic X axis */
int logYFlag;		/* Logarithmic Y axis */
int bwFlag = -1;	/* Black and white flag */
int bgPixel;		/* Background color */
int bdrSize;		/* Width of border */
int bdrPixel;		/* Border color */
int zeroPixel;		/* Color of the zero axis */
int normPixel;		/* Foreground (Axis) color */
int text2Color;		/* Second-level date label color */
int text3Color;		/* Third-level date label color */
XFontStruct *axisFont;	/* Font for axis labels */
XFontStruct *infoFont;	/* Font for info popup labels */
XFontStruct *titleFont;	/* Font for graph and help window titles */
static Cursor crossCursor;
static Cursor zoomCursor;
int precision = 4;
char selection[MAXIDENTLEN];	/* X selection buffer */
Time selectionSetTime;		/* Time at which selection text was set */
Time selectionClearTime;	/* Time at which selection text was cleared */
Atom xa_COMPOUND_TEXT;
Atom xa_MULTIPLE;
Atom xa_TARGETS;
Atom xa_TEXT;
Atom xa_TIMESTAMP;

int screen;
Visual *visual;
int depth;

#define MAX(a,b)	((a) > (b) ? (a) : (b))
#define MIN(a,b)	((a) < (b) ? (a) : (b))

LocalWin *wi_hash[64];
#define WI_HASH(win) (&wi_hash[(int)(win) & 63])

void
add_winfo(Window win, LocalWin *wi)
{
	LocalWin **p = WI_HASH(win);

	wi->win = win;
	wi->next = *p;
	*p = wi;
}

LocalWin *
win_to_info(register Window win)
{
	register LocalWin *wi;

	for (wi = *WI_HASH(win); wi != 0; wi = wi->next)
		if (wi->win == win)
			break;
	return (wi);
}

static void
copy_dflags(LocalWin *wi, LocalWin *parent)
{
	memcpy(wi->dflags, parent->dflags, sizeof(wi->dflags));
}

static void
get_dflags(LocalWin *wi, struct plotflags* flags)
{
	int i;

	for (i = 0; i < 32; ++i)
		if (! DataSetHidden(wi, i)) {
			*flags = wi->dflags[i];
			return;
		}
}

static void
set_dflags(LocalWin *wi, struct plotflags* flags)
{
	int i;

	for (i = 0; i < 32; ++i)
		if (! DataSetHidden(wi, i))
			wi->dflags[i] = *flags;
}

#define MINDIM 100

void
initSizeHints(LocalWin *wi)
{
	u_long gmask;
	u_int width, height;

	wi->sizehints.x = 0; wi->sizehints.y = 0;
	width = height = 128;
	gmask = XParseGeometry(geometry, &wi->sizehints.x, &wi->sizehints.y,
			       &width, &height);
	wi->sizehints.width = MAX(MINDIM, width);
	wi->sizehints.height = MAX(MINDIM, height);

	if (gmask & (XValue|YValue))
		wi->sizehints.flags = USPosition;
	else
		wi->sizehints.flags = PPosition;
	if (gmask & (WidthValue|HeightValue))
		wi->sizehints.flags = USSize;
	else
		wi->sizehints.flags = PSize;
}

/*
 */
Window
createWindow(LocalWin *wi, u_long wamask, XSetWindowAttributes wattr)
{
	Window win;
	XWMHints wmhints;

	win = XCreateWindow(display, RootWindow(display, screen),
			    wi->sizehints.x, wi->sizehints.y,
			    (u_int)wi->sizehints.width, 
			    (u_int)wi->sizehints.height,
			    (u_int)bdrSize,
			    depth, InputOutput, visual,	
			    wamask, &wattr);
	if (win == 0)
		return win;

	XStoreName(display, win, progname);
	XSetIconName(display, win, progname);
	XSetNormalHints(display, win, &wi->sizehints);
	
	wmhints.flags = InputHint | StateHint;
	wmhints.input = True;
	wmhints.initial_state = NormalState;
	XSetWMHints(display, win, &wmhints);

	return win;
}

/*
 * Creates a special window to display help text describing the runtime
 * operations available using the keyboard and mouse.  Only one such window
 * will be created.
 */
Window 
HelpWindow(LocalWin *parent)
{
	u_long wamask;
	XSetWindowAttributes wattr;
	Window win;
	LocalWin *wi;
	struct plotflags flags = { 0 };
	int size, w, h;
	Window root, decs, ch, *kids;
	unsigned int nkids;	
        int rx, ry;
        unsigned int rw, rh;
        unsigned int bw;
        unsigned int d;

	wi = (LocalWin *)malloc(sizeof(LocalWin));
	if (wi == 0)
		error("out of memory");
		
	
	size = MeasureHelp();
	w = size >> 16;
	h = size & 0xFFFF;

	wi->help = 1;
	wi->flags = flags;
	wi->width = wi->height = -1;
	wi->cache = 0;
	wi->sizehints = parent->sizehints;
	wi->sizehints.width = w;
	wi->sizehints.height = h;
	wi->sizehints.flags |= PMinSize|PMaxSize|PBaseSize;
	wi->sizehints.min_width = w;
	wi->sizehints.min_height = h;
	wi->sizehints.max_width = w;
	wi->sizehints.max_height = h;
	wi->sizehints.base_width = w;
	wi->sizehints.base_height = h;

	if (XQueryTree(display, parent->win, &root, &decs, &kids, &nkids) &&
	    XGetGeometry(display, decs, &root, &rx, &ry, &rw, &rh, &bw, &d) &&
	    XTranslateCoordinates(display, decs, root, rw, 0, &rx, &ry, &ch)) {
		wi->sizehints.flags |= PPosition;
		wi->sizehints.x = rx + 1;
		wi->sizehints.y = ry;
	}

	wamask = CWBackPixel|CWBorderPixel|CWEventMask;
	wattr.background_pixel = bgPixel;
	wattr.border_pixel = bdrPixel;
	wattr.event_mask = ExposureMask|KeyReleaseMask;

	win = createWindow(wi, wamask, wattr);
	if (win != 0) {
		char win_name[MAXIDENTLEN];
		snprintf(win_name, MAXIDENTLEN, "%s - Help", progname);
		XStoreName(display, win, win_name);
		add_winfo(win, wi);
		XMapWindow(display, win);
		XSetWMProtocols(display, win, &wm_delete_window, 1);
	}
	return win;
}

/*
 * Creates and maps a new window.  This includes allocating its local
 * structure and associating it with the XId for the window. The aspect ratio
 * is specified as the ratio of width over height.
 */
Window 
NewWindow(BBox *bbp, struct plotflags *flags, LocalWin *parent)
{
	u_long wamask;
	XSetWindowAttributes wattr;
	Window win;
	LocalWin *wi;
	double pad;
	int exp;
	double man;
	double epsilon;

	wi = (LocalWin *)malloc(sizeof(LocalWin));
	if (wi == 0)
		error("out of memory");
		
	wi->help = 0;
	wi->flags = *flags;

	wi->loX = bbp->loX;
	wi->hiX = bbp->hiX;
	wi->loY = bbp->loY;
	wi->hiY = bbp->hiY;

	wi->width = wi->height = -1;
	wi->cache = 0;
	wi->XPrecisionOffset = 0;
	wi->YPrecisionOffset = 0;

	/* Increase the padding for aesthetics */
	if (wi->hiX - wi->loX == 0.0) {
		if (dateXFlag)
			pad = 30*60;
		else
			pad = MAX(0.5, fabs(wi->hiX / 2.0));
		wi->hiX += pad;
		wi->loX -= pad;
	} else {
		epsilon = ldexp(frexp(wi->loX, &exp), exp - 49);
		if (wi->hiX - wi->loX < epsilon) {
			wi->hiX += epsilon;
			wi->loX -= epsilon;
		}
	}			
	if (wi->hiY - wi->loY == 0) {
		pad = MAX(0.5, fabs(wi->hiY / 2.0));
		wi->hiY += pad;
		wi->loY -= pad;
	} else {
		epsilon = ldexp(frexp(wi->loY, &exp), exp - 50);
		if (wi->hiY - wi->loY < 2*epsilon) {
			wi->hiY += epsilon;
			wi->loY -= epsilon;
		}
	}

	if (parent == 0) {
		initSizeHints(wi);
		wi->hide = 0;
		set_dflags(wi, flags);
	} else {
		wi->sizehints = parent->sizehints;
		wi->sizehints.width = parent->width;
		wi->sizehints.height = parent->height;
		wi->hide = parent->hide;
		copy_dflags(wi, parent);
	}

	wamask = CWBackPixel|CWBorderPixel|CWCursor|CWEventMask;
	wattr.background_pixel = bgPixel;
	wattr.border_pixel = bdrPixel;
	wattr.cursor = crossCursor;
	wattr.event_mask = ExposureMask|KeyPressMask|KeyReleaseMask|
		ButtonPressMask;

	win = createWindow(wi, wamask, wattr);
	if (win != 0) {
		add_winfo(win, wi);
		XMapWindow(display, win);
		XSetWMProtocols(display, win, &wm_delete_window, 1);
	}
	return win;
}

void
DeleteWindow(Window win, LocalWin *wi)
{
	register LocalWin **wp;

	for (wp = WI_HASH(win); *wp != 0; wp = &(*wp)->next)
		if (*wp == wi)
			break;
	*wp = wi->next;

	if (wi->cache != 0)
		XFreePixmap(display, wi->cache);
	free((char *)wi);
	XDestroyWindow(display, win);
}

/*
 * Given a standard color name, this routine fetches the associated pixel
 * value using XGetHardwareColor.  If it can't find or allocate the color,
 * it returns NO_COLOR.
 */
int 
GetColor(char *name)
{
	XColor def, sdef;
	int st;

	if (bwFlag) 
		return BlackPixel(display, screen);

	st = XAllocNamedColor(display, DefaultColormap(display, screen),
				name, &sdef, &def);
	return (st != BadColor) ? sdef.pixel : BlackPixel(display, screen);
}

void
xmain(struct plotflags *flags, int xlimits, int ylimits, double loX, double loY,
      double hiX, double hiY)
{
	BBox bb;
	int num_wins;

	if (axisFont == 0)
		error("%s: font not found (try -f)", DEFAULT_FONT);

	DataSetBBox(&bb);
	bb.asp = 1.0;
	if (xlimits) {
		bb.loX = loX;
		bb.hiX = hiX;
	}
	if (ylimits) {
		bb.loY = loY;
		bb.hiY = hiY;
	}
	zoomCursor = XCreateFontCursor(display, XC_sizing);
	crossCursor = XCreateFontCursor(display, XC_crosshair);
	wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
	main_win = NewWindow(&bb, flags, (LocalWin *)0);
	if (main_win == 0)
		error("cannot map window");
	initGCs(main_win);
	num_wins = 1;
	help_win = 0;

	while (num_wins > 0) {
		XEvent e;
		Window win;
		LocalWin *wi;

		XNextEvent(display, &e);
		win = e.xany.window;
		wi = win_to_info(win);
		switch (e.type) {
		case Expose:
			if (e.xexpose.count <= 0) {
				XWindowAttributes attr;

				XGetWindowAttributes(display, e.xany.window,
						     &attr);
				if (wi->width == attr.width &&
				    wi->height == attr.height)
					RedrawWindow(win, wi);
				else {
					wi->width = attr.width;
					wi->height = attr.height;
					ResizeWindow(win, wi);
				}
			}
			break;

		case KeyPress:
		case KeyRelease:
			num_wins += DoKeyPress(win, wi, &e.xkey, e.type);
			break;

		case ButtonPress:
			num_wins += DoButton(&e.xbutton, wi, zoomCursor);
			break;

		case SelectionClear:
			/* Nothing much to do since selection buffer is static*/
			selectionClearTime = e.xselectionclear.time;
			break;
		
		case SelectionRequest:
			DoSelection((XSelectionRequestEvent *)&e);
			break;

		case ClientMessage:
			/* Delete this window */
			DeleteWindow(win, wi);
			if (win == help_win)
				help_win = 0;
			else
				--num_wins;
			break;

		default:
			/* XXX ignore unknown events  */
#ifdef notdef
			fprintf(stderr, "unknown event type: 0x%x\n", e.type);
#endif
			break;
		}
	}
}

static int
DoButton(XButtonEvent *b, LocalWin *wi, Cursor zc)
{
	int windelta = 0;

	switch (b->button) {

	case Button1:
		if (b->state & ShiftMask)
			DoIdentify(b, wi, crossCursor, b->state & ControlMask);
		else 
			DoDistance(b, wi, crossCursor);
		break;

	case Button2:
		if (b->state & ShiftMask)
			DoRegression(b, wi, crossCursor);
		else
			DoSlope(b, wi, crossCursor);
		break;

	case Button3:
		if (b->state & ShiftMask)
			DoWriteSubset(b, wi, zc);
		else
			windelta = HandleZoom(b, wi, zc);
		break;

	}
	return windelta;
}

int
ToggleSet(LocalWin *wi, int setno)
{
	int bit = setno & (8 * sizeof(wi->hide) - 1);

	wi->hide ^= 1 << bit;
	return (wi->hide & (1 << bit)) != 0;
}

int
DataSetHidden(LocalWin *wi, int setno)
{
	int bit = setno & (8 * sizeof(wi->hide) - 1);

	return (wi->hide & (1 << bit)) != 0;
}

static int
DoKeyPress(Window win, LocalWin *wi, XKeyEvent *xk, int type)
{
	struct plotflags flags;
	int k, n;
	int setno;
#define	MAXKEYS 32
	char keys[MAXKEYS];
	KeySym keysym;

	n = XLookupString(xk, keys, sizeof(keys), &keysym, 
			  (XComposeStatus *)0);

	if (n == 0 && keysym == XK_F1) {	/* Translate F1 to '?' */
		keys[0] = '?';
		n = 1;
	}
	get_dflags(wi, &flags);
	for (k = 0; k < n; ++k) {
		if (type == KeyRelease) {
			switch (keys[k]) {
			case '\003':
				exit(0);

			case '\004':
			case 'q':
			case 'Q':
				/* Delete this window */
				DeleteWindow(win, wi);
				if (win == help_win) {
					help_win = 0;
					return 0;
				} else
					return -1;
			}
			continue;
		}

		/* In some environments, digit keys with Ctrl get translated */
		switch (keys[k]) {
		case '\000':
			keys[k] = '2';
			break;

		case '\033':
		case '\034':
		case '\035':
		case '\036':
		case '\037':
			keys[k] = '3' + keys[k] - '\033';
			break;

		case '\177':
			keys[k] = '8';
			break;
		}		

		switch (keys[k]) {

		case 'g':
		case 'G':
		case 't':
		case 'T':
			if ((wi->flags.tick ^= 1) == 1)
				XClearWindow(display, win);
			DrawWindow(win, wi);
			break;

		case 'l':
		case 'L':
			flags.nolines ^= 1;
			set_dflags(wi, &flags);
			XClearWindow(display, win);
			DrawWindow(win, wi);
			break;

		case 'M':
		case 'm':
			flags.mark ^= 1;
			set_dflags(wi, &flags);
			XClearWindow(display, win);
			DrawWindow(win, wi);
			break;

		case 'p':
		case 'P':
			flags.pixmarks ^= 1;
			set_dflags(wi, &flags);
			XClearWindow(display, win);
			DrawWindow(win, wi);
			break;

		case 'h':
		case 'H':
			flags.bar ^= 1;
			set_dflags(wi, &flags);
			XClearWindow(display, win);
			DrawWindow(win, wi);
			break;

		case 'o':
		case 'O':
			if ((wi->flags.outline ^= 1) == 0)
				XClearWindow(display, win);
			DrawWindow(win, wi);
			break;

		case 's':
		case 'S':
			flags.spline ^= 1;
			XClearWindow(display, win);
			set_dflags(wi, &flags);
			DrawWindow(win, wi);
			break;

		case 'r':
		case 'R':
			if (flags.rectangle)
				flags.rectangle = 0;
			else
				flags.rectangle = 1;
			XClearWindow(display, win);
			set_dflags(wi, &flags);
			DrawWindow(win, wi);
			break;

		case 'f':
		case 'F':
			if (flags.rectangle == 2)
				flags.rectangle = 0;
			else
				flags.rectangle = 2;
			XClearWindow(display, win);
			set_dflags(wi, &flags);
			DrawWindow(win, wi);
			break;

		case 'e':
		case 'E':
			flags.errorbar ^= 1;
			set_dflags(wi, &flags);
			XClearWindow(display, win);
			DrawWindow(win, wi);
			break;

		case '\f':
			XClearWindow(display, win);
			DrawWindow(win, wi);
			break;

		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			setno = keys[k] - '0' +
				  (xk->state & ControlMask ? 10 : 0);
			if (ToggleSet(wi, setno)) {
				XClearWindow(display, win);
				DrawWindow(win, wi);
				break;
			}
			DrawSetNo(wi, win, setno);
			break;

		case '?':
		case '\b':
			if (help_win) {
				XRaiseWindow(display, help_win);
			} else {
				help_win = HelpWindow(wi);
			}
			break;
		}
	}
	return 0;
}

static int
ConvertSelection(Window requestor, Atom target, Atom property)
{
	if (property == None)
		return False;
	if (target == XA_STRING || target == xa_COMPOUND_TEXT) {
		XChangeProperty(display, requestor, property, target,
				8, PropModeReplace, (unsigned char*)selection,
				strlen(selection));
		return True;
	}
	if (target == xa_TEXT) {
		XChangeProperty(display, requestor, property, XA_STRING,
				8, PropModeReplace, (unsigned char*)selection,
				strlen(selection));
		return True;
	}
	/*
	 * The ICCM documentation says that the following targets are required
	 * to be handled, but I could not find any client that requests them so
	 * the following code is untested.
	 */
	if (target == xa_TARGETS) {
		Atom targetList[] = {xa_TARGETS, xa_MULTIPLE, xa_TIMESTAMP,
				     XA_STRING, xa_TEXT, xa_COMPOUND_TEXT};

		XChangeProperty(display, requestor, property, XA_ATOM, 32,
				PropModeReplace, (unsigned char *)targetList,
				sizeof(targetList)/sizeof(Atom));
		return True;
	}
	if (target == xa_TIMESTAMP) {
		XChangeProperty(display, requestor, property, XA_INTEGER,
				32, PropModeReplace,
				(unsigned char *)&selectionSetTime, 1);
		return True;
	}
	if (target == xa_MULTIPLE) {
		Atom actual_type;
		int actual_format;
		unsigned long nitems;
		unsigned long remaining;
		unsigned char *data;
		int i, ret, nerrors = 0;
		struct atom_pair {
			Atom target;
			Atom property;
		} *m;

		ret = XGetWindowProperty(display, requestor, property, 0,
					 4*2*100, False, XA_ATOM, &actual_type,
					 &actual_format, &nitems, &remaining,
					 &data);
		if (ret != Success || nitems == 0)
			return False;
		if (remaining > 0 || (nitems & 1) || actual_type != XA_ATOM ||
		    actual_format != 32) {
			XFree(data);
			return False;
		}
		nitems >>= 1;
		m = (struct atom_pair *)data;
		for (i = 0; i < nitems; ++i, ++m) {
			if (!ConvertSelection(requestor, m->target,
					      m->property)) {
				m->property = None;
				++nerrors;
			}
		}
		if (nerrors > 0) {
			XChangeProperty(display, requestor, property, XA_ATOM,
					32, PropModeReplace, data, nitems * 2);
		}
		XFree(data);
		return True;
	}
	return False;
}

static void
DoSelection(XSelectionRequestEvent *re)
{
	XSelectionEvent se;

	se.type = SelectionNotify;
	se.requestor = re->requestor;
	se.selection = re->selection;
	se.target = re->target;
	se.property = re->property;
	se.time = re->time;

	if ((se.time == CurrentTime || se.time >= selectionSetTime) &&
	    (selectionClearTime == CurrentTime ||
	     se.time < selectionClearTime)) {
		if (!ConvertSelection(se.requestor, se.target, se.property))
			se.property = None;
	}
	else se.property = None;

	XSendEvent(display, se.requestor, 0, 0, (XEvent *)&se);
}

void
xinit(char *dispname, struct plotflags *flags)
{
	display = XOpenDisplay(dispname);
	if (display == 0)
		error("can't open display \"%s\"", XDisplayName(dispname));

	screen = DefaultScreen(display);
	visual = DefaultVisual(display, screen);
	depth = DefaultDepth(display, screen);
	if (bwFlag == -1)
		bwFlag = depth < 4;
	bdrSize = 2;
	bdrPixel = BlackPixel(display, screen);
	axisFont = XLoadQueryFont(display, DEFAULT_FONT);
	infoFont = axisFont;
	titleFont = XLoadQueryFont(display, TITLE_FONT);

	/* Depends critically on whether the display has color */
	if (bwFlag) {
		bgPixel = WhitePixel(display, screen);
		zeroPixel = BlackPixel(display, screen);
		normPixel = BlackPixel(display, screen);
		text2Color = BlackPixel(display, screen);
		text3Color = BlackPixel(display, screen);
	} else {
		bgPixel = WhitePixel(display, screen);
		zeroPixel = GetColor("LightGray");
		normPixel = BlackPixel(display, screen);
		text2Color = GetColor("Gray60");
		text3Color = GetColor("Gray75");
	}
	xdefaults(flags);

	xa_COMPOUND_TEXT = XInternAtom(display, "COMPOUND_TEXT", True);
	xa_MULTIPLE = XInternAtom(display, "MULTIPLE", True);
	xa_TARGETS = XInternAtom(display, "TARGETS", True);
	xa_TEXT = XInternAtom(display, "TEXT", True);
	xa_TIMESTAMP = XInternAtom(display, "TIMESTAMP", True);
}

static void
xdef_color(char *name, int *pix)
{
	char *value = XGetDefault(display, progname, name);

	if (value != 0) {
		int color = GetColor(value);

		if (color != -1)
			*pix = color;
	}
}

static void
xdef_int(char *id, int *p)
{
	char *v = XGetDefault(display, progname, id);

	if (v != 0)
		*p = atoi(v);
}

static void
xdef_font(char *id, XFontStruct **f)
{
	char *v = XGetDefault(display, progname, id);
	if (v == 0)
		return;
	*f = XLoadQueryFont(display, v);
	if (*f == 0)
		error("cannot load font %s of %s", id, v);
}

static void
xdef_flag(char *id, char *flag)
{
	char *value = XGetDefault(display, progname, id);

	if (value != 0) {
		if (strcmp(value, "off") == 0 || 
		    strcmp(value, "false") == 0)
			*flag = 0;
		else
			*flag = 1;
	}
}

static void
xdef_string(char *id, char **sp)
{
	char *v = XGetDefault(display, progname, id);

	if (v != 0)
		*sp = v;
}

/*
 * Reads X default values which override the hard-coded defaults.
 */
void
xdefaults(struct plotflags *flags)
{
	xdef_color("Background", &bgPixel);
	xdef_int("BorderSize", &bdrSize);
	xdef_color("Border", &bdrPixel);
	xdef_color("Foreground", &normPixel);
	xdef_color("ZeroColor", &zeroPixel);
	xdef_color("Text2Color", &text2Color);
	xdef_color("Text3Color", &text3Color);
	xdef_font("LabelFont", &axisFont);
	xdef_font("InfoFont", &infoFont);
	xdef_font("TitleFont", &titleFont);
	xdef_flag("Spline", &flags->spline);
	xdef_flag("Ticks", &flags->tick);
	xdef_flag("Markers", &flags->mark);
	xdef_flag("Outline", &flags->outline);
	xdef_flag("NoLines", &flags->nolines);
	xdef_flag("PixelMarkers", &flags->pixmarks);
	xdef_string("geometry", &geometry);
	xdef_int("precision", &precision);
	xdef_flag("Rectangles", &flags->rectangle);
	xdef_flag("ErrorBars", &flags->errorbar);
}

void
xsetfont(char *name)
{
	infoFont = XLoadQueryFont(display, name);
	if (infoFont == 0)
		error("cannot find font %s", name);
}

void
xSetWindowName(char* name)
{
	XStoreName(display, main_win, name);
}
