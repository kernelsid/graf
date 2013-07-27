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

struct plotflags {
	char spline;		/* Draw lines with spline */
	char tick;		/* Don't draw full grid */
	char mark;		/* Draw marks at pochars */
	char bb;		/* Whether to draw bb */
	char nolines;		/* Don't draw lines */
	char pixmarks;		/* Draw pixel markers */
	char bar;		/* Draw bar graph */
	char rectangle;		/* Draw rectangles */
	char errorbar;		/* Draw error bars */
};

extern int logXFlag;		/* Logarithmic X axis */
extern int logYFlag;		/* Logarithmic Y axis */
extern char *geometry;
extern int cflag;
extern double slope_scale;
extern char *graph_title;
