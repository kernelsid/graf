/*
 * Copyright (c) 1991, 1997
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

typedef struct local_win {
	struct local_win *next;
	Window win;
	int width, height;		/* Current size of window */
	double loX, loY, hiX, hiY;	/* Local bounding box of window */
	int XOrgX, XOrgY;		/* Origin of bounding box on screen */
	int XOppX, XOppY;		/* Other point defining bounding box */
	double UsrOrgX, UsrOrgY;	/* bounding box origin in user space */
	double UsrOppX, UsrOppY;	/* Other point of bounding box */
	double XUnitsPerPixel;		/* X Axis scale factor */
	double YUnitsPerPixel;		/* Y Axis scale factor */
	int XPrecisionOffset;		/* Extra precision for X axis offset */
	int YPrecisionOffset;		/* Extra precision for X axis offset */
	struct plotflags flags;
	Pixmap cache;		/* off-screen redraw cache */
	XRectangle clip;
	XSizeHints sizehints;
	unsigned long hide;		/* bit =1 if assoc. dataset hidden */
	struct plotflags dflags[32];
} LocalWin;

extern XFontStruct *axisFont;	/* Font for axis labels */
extern XFontStruct *infoFont;	/* Font for info popup labels */
 
extern int depth;
extern Display *display;
extern int zeroPixel;
extern int normPixel;
extern int text2Color;
extern int text3Color;
extern int bgPixel;
extern int bwFlag;
extern char *markcolor;
extern int precision;
extern char selection[];
extern Time selectionSetTime;
extern Time selectionClearTime;
