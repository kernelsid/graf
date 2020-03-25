/*
 * Copyright (c) 1991, 1992, 1994, 1996, 1997
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
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

#include "flags.h"
#include "xwin.h"
#include "dataset.h"

#ifdef notdef
#include "bitmaps/dot.11"
#endif

#include "bitmaps/mark1.11"
#include "bitmaps/mark2.11"
#include "bitmaps/mark3.11"
#include "bitmaps/mark4.11"
#include "bitmaps/mark5.11"
#include "bitmaps/mark6.11"
#include "bitmaps/mark7.11"
#include "bitmaps/mark8.11"
#include "bitmaps/mark9.11"

extern void WriteValue(char *str, double val, int expn);
extern int DataSetHidden(LocalWin *wi, int setno);
extern Window NewWindow(BBox *bbp, struct plotflags *flags, LocalWin *parent);
extern int GetColor(char *name);
extern void xSetWindowName(char* name);
extern char *progname;

int TransformCompute(LocalWin *wi);

/* All marks have same dimensions. */
#define mark_w mark1_width
#define mark_h mark1_height
#define mark_center_x mark1_y_hot
#define mark_center_y mark1_x_hot
static char *marks[] = {
	mark1_bits,
	mark2_bits,
	mark9_bits,
	mark3_bits,
	mark4_bits,
	mark5_bits,
	mark6_bits,
	mark7_bits,
	mark8_bits,
};
#define MARKNO(n)  ((n) % (sizeof(marks) / sizeof(marks[0])))

#define SPACE 		10
#define TICKLENGTH	5
#define PADDING 	2
#define PIXVALUE(set) 	(AllAttrs[(set) % MAXATTR].pixelValue)

static int pad = TICKLENGTH + PADDING + mark_w/2;  /* pad inside outline */
static char *date_formats[] = {"", "%Y", "%Y-%m-%d", "%Y-%m-%d %H:%M:",
			       "%Y-%m-%d %H:%M:%S."};

/* The following used to be defined in terms of the maximum and minimum
 * value a "short" can hold.  But using those values can tickle bugs in
 * the X server, which is in the habit of adding offsets to coordinates
 * and assuming that the result still fits in a short.  So we use quite
 * conservative values.  As long as these are significantly larger than
 * the number of pixels on a screen, they should suffice.
 */
#define MAX_X11_COOR	16384
#define MIN_X11_COOR	-16384

/*
 * To properly clip the line between a visible point and one which is outside
 * the X11 coordinate space we need to map the invisible point to the point on
 * the boundary where that line intersects.  To calculate that boundary point
 * we need to have the coordinates of the visible point (op) as well as the
 * invisible point (vp).  The second clip calculation in each of the two cases
 * should only rarely happen for points near the corner of the coordinate
 * space.
 *
 * Since a line between two invisible points may also cross the visible space,
 * this function may also be called with two invisible points.  This requires
 * the code to protect itself against various overflow conditions.  The result
 * should always be a point within the coordinate space.
 *
 * The return value is True if vp was beyond the X11 coordinate space.
 */
static inline int
SCREENXY(LocalWin *ws, Value *vp, Value *op, XPoint *xp)
{
	double x = ws->XOrgX +
		(vp->x - ws->UsrOrgX) / ws->XUnitsPerPixel;
	double y = ws->XOppY - 
		(vp->y - ws->UsrOrgY) / ws->YUnitsPerPixel;
	double xabs = fabs(x);
	double yabs = fabs(y);
	int beyond = False;

	if (xabs > MAX_X11_COOR || yabs > MAX_X11_COOR) {
		double ox = ws->XOrgX +
			(op->x - ws->UsrOrgX) / ws->XUnitsPerPixel;
		double oy = ws->XOppY - 
			(op->y - ws->UsrOrgY) / ws->YUnitsPerPixel;
		double mx, my;

		beyond = True;
		if (xabs > yabs) {
			mx = copysign(MAX_X11_COOR, x);
			if (x == ox || fabs(my = (y - oy) * (mx - ox) / (x - ox) + oy) > MAX_X11_COOR) {
				my = copysign(MAX_X11_COOR, y);
				if (y == oy || fabs(mx = (x - ox) * (my - oy) / (y - oy) + ox) > MAX_X11_COOR)
					mx = copysign(MAX_X11_COOR, x);
			}
		} else {
			my = copysign(MAX_X11_COOR, y);
			if (y == oy || fabs(mx = (x - ox) * (my - oy) / (y - oy) + ox) > MAX_X11_COOR) {
				mx = copysign(MAX_X11_COOR, x);
				if (x == ox || fabs(my = (y - oy) * (mx - ox) / (x - ox) + oy) > MAX_X11_COOR)
					my = copysign(MAX_X11_COOR, y);
			}
		}
		x = mx;
		y = my;
	}
	xp->x = rint(x);
	xp->y = rint(y);
	return beyond;
}

/*
 * SCREENX and SCREENY are only valid when the points are known to be within
 * the X11 coordinate limits.  Otherwise, SCREENXY must be used in order to
 * properly clip.
 */
static inline int
SCREENX(LocalWin *ws, double userX)
{
	return ws->XOrgX +
		rint((userX - ws->UsrOrgX) / ws->XUnitsPerPixel);
}

static inline int
SCREENY(LocalWin *ws, double userY)
{
	return ws->XOppY - 
		rint((userY - ws->UsrOrgY) / ws->YUnitsPerPixel);
}

static inline int
SCREENXS(LocalWin *ws, double userX)
{
	double x = userX / ws->XUnitsPerPixel;

	if (x < MIN_X11_COOR)
		return MIN_X11_COOR;
	if (x > MAX_X11_COOR)
		return MAX_X11_COOR;
	return rint(x);
}

static inline int
SCREENYS(LocalWin *ws, double userY)
{
	double y = userY / ws->YUnitsPerPixel;

	if (y < MIN_X11_COOR)
		return MIN_X11_COOR;
	if (y > MAX_X11_COOR)
		return MAX_X11_COOR;
	return rint(y);
}

#define TRANX(xval) \
	(((double) ((xval) - wi->XOrgX)) * wi->XUnitsPerPixel + wi->UsrOrgX)

#define TRANY(yval) \
	(wi->UsrOppY - (((double) ((yval) - wi->XOrgY)) * wi->YUnitsPerPixel))

#define nlog10(x)	(x == 0.0 ? 0.0 : log10(x))

static GC echoGC;
static GC fitGC;
static GC textGC;
static GC text2GC;
static GC text3GC;
static GC infoGC;
static GC titleGC;
static GC copyGC;

double slope_scale = 1.0;
char *graph_title = NULL;
static int maxName = 0;

static int
WriteDate(str, val)
	char *str;		/* String to write into */
	double val;		/* Value to print */
{
	time_t xsec = (time_t)lround(val);
	int usec = (int)fmod(round(val*1000000), 1000000);
	struct tm xtm;
	if (localTime)
		localtime_r(&xsec, &xtm);
	else
		gmtime_r(&xsec, &xtm);
	if (xtm.tm_hour == 0 && xtm.tm_min == 0 && xtm.tm_sec == 0 &&
	    usec == 0) {
		if (xtm.tm_mon == 0 && xtm.tm_mday == 1) {
			if (str)
				strftime(str, 6, "%Y", &xtm);
			return 1;
		} else {
			if (str)
				strftime(str, 6, "%m/%d", &xtm);
			return 2;
		}
	} else if (lround(fmod(val, 60) * 1000000) == 0) {
		if (str)
			strftime(str, 6, "%H:%M", &xtm);
		return 3;
	} else if (fmod(round(val*1000000), 1000) == 0.0) {
		if (str)
			(void)sprintf(str, "%06.3f", fmod(val, 60));
		return 4;
	} else {
		if (str)
			(void)sprintf(str, "%06u", usec);
		return 5;
	}
}


/*
 * This routine draws grid line labels in engineering notation, the grid
 * lines themselves, and unit labels on the axes.
 */
static void
DrawGridAndAxis(Window win, LocalWin *wi)
{
	int expX, expY;		/* Engineering powers */
	int exp;
	int y, x, w, n;
	int height;
	int len;
	int textLevelOffset = 0;
	int isDST = 0;
	int Xmonths = 0;
	double Xincr, Yincr, Xstart, Ystart, Yindex, Xindex, larger;
	double Xoffset, Yoffset, Xbase, Ybase, Xend, Yend;
	char value[32];
	double RoundUp();
	double MaskDigit();

	/*
	 * Grid display powers are computed by taking the log of the largest
	 * numbers and rounding down to the nearest multiple of 3.
	 */
	if (fabs(wi->UsrOrgX) > fabs(wi->UsrOppX))
		larger = fabs(wi->UsrOrgX);
	else
		larger = fabs(wi->UsrOppX);

	expX = ((int) floor(nlog10(larger) / 3.0)) * 3;
	if (fabs(wi->UsrOrgY) > fabs(wi->UsrOppY))
		larger = fabs(wi->UsrOrgY);
	else
		larger = fabs(wi->UsrOppY);

	expY = ((int) floor(nlog10(larger) / 3.0)) * 3;

	/*
	 * The grid line increments are determined by the number of labels
	 * that will fit in the vertical and horizontal space.
	 */
	height = axisFont->ascent + axisFont->descent;
	Yincr = (SPACE + height) * wi->YUnitsPerPixel;
	Yincr = RoundUp(Yincr);

	if (dateXFlag)
		w = XTextWidth(axisFont, "000000", 6); /* > 00.000 or HH:MM */
	else
		w = XTextWidth(axisFont, "-000.00", 7);
	Xincr = (SPACE + w) * wi->XUnitsPerPixel;
	if (!dateXFlag)
		Xincr = RoundUp(Xincr);

	/*
	 * If the grid line labels will not have sufficient resolution in
	 * two decimal places to represent the grid line increment, then
	 * adjust the grid display powers and change the grid line labels
	 * to be offsets from a starting value to be shown in the axis label.
	 * The offset value is masked to show up to two integer digits in the
	 * grid label, with a possible third digit for overflow, to simplify
	 * mental addition of the offset and the label.
	 *
	 * For date labels on the X axis, find what span of dates is covered in
	 * order to determine the level of detail in the labels: 2015 12/31
	 * 23:59 59.999 999999.  For any given tick mark, the level used will
	 * be the lowest that is not zero.
	 */

	Ystart = ceil(wi->UsrOrgY / Yincr) * Yincr;
	if (Ystart == -0.0) Ystart = 0.0;
	Yoffset = 0.0;
	exp = (int) floor(nlog10(Yincr));
	if (!logYFlag && exp < expY - 2) {
		wi->YPrecisionOffset = ((expY - exp) / 3) * 3;
		expY -= wi->YPrecisionOffset;
		Yoffset = Ystart;
		if (Ystart < 0) {
			n = (int) floor((wi->UsrOppY - Ystart) / Yincr);
			Yoffset += n * Yincr;
		}
		Ybase = MaskDigit(&Yoffset, expY + 2) * 100.0;
		Ystart -= Yoffset;
	}
	if (dateXFlag) {
		if (Xincr > 60*60) {
			time_t step = 1440*60;
			if (Xincr > 720*60) {
				Xincr = floor((Xincr+step-1)/step)*step;
				if (Xincr > 14*1440*60) {
					Xmonths = 1;
					if (Xincr > 28*1440*60 &&
					    Xincr < 32*1440*60)
						Xincr = 32*1440*60;
				}
				if (Xincr > 30*1440*60)
					Xmonths = (int) ceil(Xincr /
							     (30.5*1440*60));
			} else {
				if (Xincr > 240*60) {
					Xincr = 720*60;
				} else if (Xincr > 120*60) {
					Xincr = 240*60;
				} else {
					Xincr = 120*60;
				}
				step = Xincr;
			}
			time_t xsec = (time_t)wi->UsrOrgX;
			xsec += step - 1;
			struct tm xtm;
			if (localTime)
				localtime_r(&xsec, &xtm);
			else
				gmtime_r(&xsec, &xtm);
			if (Xmonths && xtm.tm_mday != 1) {
				xtm.tm_mday = 1;
				if (++xtm.tm_mon > 11)
					++xtm.tm_year;
			}
			if ((Xmonths >= 2 && Xmonths <= 4) || Xmonths == 6) {
				int rem = xtm.tm_mon % Xmonths;
				if (rem != 0) {
					xtm.tm_mon += Xmonths - rem;
					if (xtm.tm_mon > 11) {
						xtm.tm_mon -= 12;
						++xtm.tm_year;
					}
				}
			}
			xtm.tm_hour -= xtm.tm_hour % (step/(60*60));
			xtm.tm_min = xtm.tm_sec = 0;
			xtm.tm_isdst = -1;
			Xstart = localTime ? mktime(&xtm) : timegm(&xtm);
		} else if (Xincr > 1) {
			if (Xincr > 30*60) {
				Xincr = 60*60;
			} else if (Xincr > 10*60) {
				Xincr = 30*60;
			} else if (Xincr > 5*60) {
				Xincr = 10*60;
			} else if (Xincr > 60) {
				Xincr = 5*60;
			} else if (Xincr > 60) {
				Xincr = 5*60;
			} else if (Xincr > 30) {
				Xincr = 60;
			} else if (Xincr > 10) {
				Xincr = 30;
			} else if (Xincr > 5) {
				Xincr = 10;
			} else if (Xincr > 2) {
				Xincr = 5;
			} else
				Xincr = 2;
			Xstart = ceil(wi->UsrOrgX / Xincr) * Xincr;
		} else {
			Xincr = fmax(RoundUp(Xincr), 0.000001);
			Xstart = ceil(wi->UsrOrgX / Xincr) * Xincr;
		}
		if (localTime) {
			time_t xsec = (time_t)Xstart;
			struct tm xtm;
			localtime_r(&xsec, &xtm);
			isDST = xtm.tm_isdst;
		}
		n = (int) floor((wi->UsrOppX - Xstart) / Xincr);
		Xend = Xstart + n * Xincr;
		Xoffset = 0.0;
		expX = 9;
		exp = (int) floor(nlog10(Xincr));
		wi->XPrecisionOffset = ((expX - exp) / 3) * 3;
	} else {
		Xstart = ceil(wi->UsrOrgX / Xincr) * Xincr;
		if (Xstart == -0.0) Xstart = 0.0;
		Xoffset = 0.0;
		exp = (int) floor(nlog10(Xincr));
		if (!logXFlag && exp < expX - 2) {
			wi->XPrecisionOffset = ((expX - exp) / 3) * 3;
			expX -= wi->XPrecisionOffset;
			Xoffset = Xstart;
			if (Xstart < 0) {
				n = (int) floor((wi->UsrOppX - Xstart) / Xincr);
				Xoffset += n * Xincr;
			}
			Xbase = MaskDigit(&Xoffset, expX + 2) * 100.0;
			Xstart -= Xoffset;
		}
		Xend = wi->UsrOppX - Xoffset;
	}

	/*
	 * With the powers computed, we can draw the axis labels.
	 */
	if (expY || logYFlag || Yoffset != 0.0) {
		char powerbuf[100];
		char *power = powerbuf;

		if (logYFlag) {
			if (expY || Yoffset != 0.0)
				power += sprintf(power, " x 10^(y");
			else
				power += sprintf(power, " x 10^y");
		}
		if (expY) {
			if (Yoffset == 0.0)
				power += sprintf(power, " x 10^%d", expY);
			else
				power += sprintf(power, " + %.0f x 10^%d",
						 Ybase, expY);
		} else if (Yoffset != 0.0)
			power += sprintf(power, " + %.0f", Ybase);
		if (logYFlag && (expY || Yoffset != 0.0))
			power += sprintf(power, ")");
		XDrawString(display, win, textGC, w/2, axisFont->ascent+PADDING,
				 powerbuf, strlen(powerbuf));
	}
	if (dateXFlag) {
		time_t xsec = (time_t)Xstart;
		struct tm xtmin, xtmax;
		int width;
		char datebuf[MAXIDENTLEN];
		char *labptr = datebuf;
		char *format = date_formats[WriteDate(0, Xstart) - 1];
		if (localTime)
			localtime_r(&xsec, &xtmin);
		else
			gmtime_r(&xsec, &xtmin);
		labptr += strftime(labptr, MAXIDENTLEN, format, &xtmin);
		len = labptr - datebuf;
		width = XTextWidth(axisFont, datebuf, len);
		x = SCREENX(wi, Xstart) - width / 2;
		if (x < 17)
			x = 17;
		y = wi->height - PADDING;
		XDrawString(display, win, textGC, x, y, datebuf, len);
		xsec = (time_t)Xend;
		if (localTime)
			localtime_r(&xsec, &xtmax);
		else
			gmtime_r(&xsec, &xtmax);
		labptr = datebuf;
		format = date_formats[WriteDate(0, Xend) - 1];
		labptr += strftime(labptr, MAXIDENTLEN, format, &xtmax);
		len = labptr - datebuf;
		width = XTextWidth(axisFont, datebuf, len);
		x = SCREENX(wi, Xend) - width / 2;
		if (x + width > wi->width - 17)
		x = wi->width - width - 17;
		XDrawString(display, win, textGC, x, y, datebuf, len);
		if (xtmin.tm_year == xtmax.tm_year) {
			++textLevelOffset;
			if (xtmin.tm_yday == xtmax.tm_yday) {
				++textLevelOffset;
				if (xtmin.tm_hour == xtmax.tm_hour &&
				    xtmin.tm_min == xtmax.tm_min) {
					++textLevelOffset;
					if (xtmin.tm_sec == xtmax.tm_sec)
						++textLevelOffset;
				}
			}
		}
	} else if (expX || logXFlag || Xoffset != 0.0) {
		char powerbuf[100];
		char *power = powerbuf;

		if (logXFlag) {
			if (expX || Xoffset != 0.0)
				power += sprintf(power, " x 10^(X");
			else
				power += sprintf(power, " x 10^x");
		}
		if (expX) {
			if (Xoffset == 0.0)
				power += sprintf(power, " x 10^%d", expX);
			else
				power += sprintf(power, " + %.0f x 10^%d",
						 Xbase, expX);
		} else if (Xoffset != 0.0)
			power += sprintf(power, " + %.0f", Xbase);
		if (logXFlag && (expX || Xoffset != 0.0))
			power += sprintf(power, ")");
		len = strlen(powerbuf);
		x = wi->width - XTextWidth(axisFont, powerbuf, len) - 17;
		if (x > wi->XOppX)
			x = wi->XOppX;
		y = wi->height - PADDING;
		XDrawString(display, win, textGC, x, y, powerbuf, len);
	}
	/*
	 * Now, we can figure out the grid line labels and grid lines
	 */
	Yend = wi->UsrOppY - Yoffset;
	for (Yindex = Ystart; Yindex <= Yend; Yindex += Yincr) {
		y = SCREENY(wi, Yindex + Yoffset);

		/* Write the axis label */
		WriteValue(value, Yindex, expY);
		len = strlen(value);
		x = wi->XOrgX - XTextWidth(axisFont, value, len) - PADDING;
		XDrawString(display, win, textGC, x, y + axisFont->ascent/2,
			    value, len);

		/* Draw the grid line or tick mark */
		if (wi->flags.tick) {
			XDrawLine(display, win, textGC, wi->XOrgX, y,
				  wi->XOrgX + TICKLENGTH, y);
			XDrawLine(display, win, textGC, wi->XOppX - TICKLENGTH,
				  y, wi->XOppX, y);
		} else
			XDrawLine(display, win, textGC, wi->XOrgX, y,
				  wi->XOppX, y);
	}
	y = wi->height - height;
	int Xnum = 0;
	double Xshift = 0;
	for (Xindex = Xstart; Xindex <= Xend; ++Xnum) {
		x = SCREENX(wi, Xindex + Xoffset);

		/* Write the axis label */
		int textLevel = 1;
		if (dateXFlag)
			textLevel = WriteDate(value, Xindex);
		else
			WriteValue(value, Xindex, expX);
		len = strlen(value);
		w = XTextWidth(axisFont, value, len);
		textLevel -= textLevelOffset;
		GC gc = textLevel <= 1 ? textGC :
			textLevel == 2 ? text2GC : text3GC;
		XDrawString(display, win, gc, x - w/2, y,
				 value, len);

		/* Draw the grid line or tick marks */
		if (wi->flags.tick) {
			XDrawLine(display, win, textGC, x, wi->XOrgY,
				  x, wi->XOrgY + TICKLENGTH);
			XDrawLine(display, win, textGC, x,
				  wi->XOppY - TICKLENGTH, x, wi->XOppY);
		} else
			XDrawLine(display, win, textGC, x, wi->XOrgY, x,
				  wi->XOppY);
		if (!Xmonths)
			Xindex = Xstart + Xincr*Xnum + Xshift; /* Avoid error accum. */
		if (dateXFlag) {
			time_t xsec = (time_t)Xindex;
			struct tm xtm;
			if (localTime)
				localtime_r(&xsec, &xtm);
			else
				gmtime_r(&xsec, &xtm);
			if (Xmonths) {
				xtm.tm_mon += Xmonths;
				if (xtm.tm_mon > 11) {
					xtm.tm_mon -= 12;
					++xtm.tm_year;
				}
				xtm.tm_isdst = -1;
				Xindex = localTime ? mktime(&xtm) :
					timegm(&xtm);
			} else {
				if (localTime && (Xincr > 60*60)) {
					if (isDST && !xtm.tm_isdst) {
						Xshift += 60*60;
						Xindex += 60*60;
					} else if (!isDST && xtm.tm_isdst) {
						Xshift -= 60*60;
						Xindex -= 60*60;
						if (Xincr < 240*60)
							Xindex += Xincr;
					}
				}
			}
			if (localTime) {
				xsec = (time_t)Xindex;
				localtime_r(&xsec, &xtm);
				isDST = xtm.tm_isdst;
			}
		}
	}

	/* Center the title near the top of the graph */
	if (graph_title) {
		char win_name[MAXIDENTLEN];
		snprintf(win_name, MAXIDENTLEN, "%s - %s", progname,
			 graph_title);
		xSetWindowName(win_name);
		len = strlen(graph_title);
		w = XTextWidth(titleFont, graph_title, len);
		x = (wi->width - w) / 2;
		if (x < 0)
			x = 0;
		XDrawImageString(display, win, titleGC, x,
				 titleFont->ascent + PADDING/2,
				 graph_title, len);
	}

	/* Check to see if he wants an outline */
	if (wi->flags.outline)
		XDrawRectangle(display, win, textGC, wi->clip.x, wi->clip.y,
			       wi->clip.width, wi->clip.height);
}

void
DrawSet(LocalWin *wi, Window win, struct data_set *p)
{
	Value *vp, *bp, *ep, *op;
	XPoint *ver, *verend;
	double loX, loY, hiX, hiY;
#define MAXPOINTS 512
	XPoint Xpoints[MAXPOINTS];
	XRectangle Xrectangles[MAXPOINTS];
	XRectangle *rec = Xrectangles;
	struct plotflags* flags = &wi->dflags[p->setno];
	int beyond;

	loX = wi->UsrOrgX; loY = wi->UsrOrgY;
	hiX = wi->UsrOppX; hiY = wi->UsrOppY;
	vp = p->dvec;
	ep = &p->dvec[p->numPoints];
	ver = Xpoints;
	verend = &Xpoints[MAXPOINTS];
	op = vp;
	XSetClipRectangles(display, (GC)p->GC, 0, 0, 
			   &wi->clip, 1, Unsorted);

	while (vp < ep) {
		while (ver < verend && vp < ep) {
			beyond = SCREENXY(wi, vp, op, ver);
			if (flags->rectangle) {
				int w = SCREENXS(wi, vp->w);
				int h = SCREENYS(wi, vp->h);
				if (w < 0) {
					rec->width = -w;
					rec->x = ver->x + w;
					if (rec->x < MIN_X11_COOR) {
						rec->width = MIN_X11_COOR -
							rec->x;
						rec->x = MIN_X11_COOR;
					}
				} else {
					rec->width = w;
					rec->x = ver->x;
				}
				if (h < 0) {
					rec->height = -h;
					rec->y = ver->y;
				} else {
					rec->height = h;
					rec->y = ver->y - h;
					if (rec->y < MIN_X11_COOR) {
						rec->height = MIN_X11_COOR -
							rec->y;
						rec->y = MIN_X11_COOR;
					}
				}
				++rec;
			}
			if (flags->mark && !flags->pixmarks &&
			    vp->x >= loX && vp->x <= hiX &&
			    vp->y >= loY && vp->y <= hiY) {
#ifdef notdef
				XFillArc(display, win, (GC)p->GC,
					 ver->x - 3, ver->y - 3, 7, 7,
					 0, 360*64);
#endif
				XSetClipOrigin(display, (GC)p->mGC,
					       ver->x - mark_center_x,
					       ver->y - mark_center_y);
				XFillRectangle(display, win, (GC)p->mGC,
					       ver->x - mark_center_x, 
					       ver->y - mark_center_y,
					       mark_w, mark_h);
			}
			/* Draw bar elements if requested */
			if (flags->bar)
				XDrawLine(display, win, (GC)p->GC,
					  ver->x, SCREENY(wi, 0.0),
					  ver->x, ver->y);

			/* Draw error bars if requested */
			if (flags->errorbar) {
				int w = SCREENXS(wi, vp->w);
				int h = SCREENYS(wi, vp->h);
				int min, max;
				if (w < 0)
					w = -w;
				min = ver->x - w/2;
				max = ver->x + w/2;
				if (min < MIN_X11_COOR)
					min = MIN_X11_COOR;
				if (max > MAX_X11_COOR)
					max = MAX_X11_COOR;
				XDrawLine(display, win, (GC)p->GC,
					  min, ver->y, max, ver->y);
				if (h < 0)
					h = -h;
				min = ver->y - h/2;
				max = ver->y + h/2;
				if (min < MIN_X11_COOR)
					min = MIN_X11_COOR;
				if (max > MAX_X11_COOR)
					max = MAX_X11_COOR;
				XDrawLine(display, win, (GC)p->GC,
					  ver->x, min, ver->x, max);
			}
			/*
			 * If the last point was an invisible point being drawn
			 * a second time, then ignore the fact that it was
			 * invisible so we will move on to the next point.
			 */
			if (op > vp)
				beyond = False;
			/*
			 * Save this point as the "other point" in case the next
			 * point is out of the X11 coordinate space and needs to
			 * be clipped on the line from this point to it.
			 */
			op = vp;
			/*
			 * If this point wasn't visible, spin until we find one
			 * that is not offscreen on the same edge.  Then back
			 * up so the invisible point immediately before it will
			 * be drawn on the next loop so the line drawn between
			 * those two invisible points will be totally
			 * offscreen.  We don't back up if the preceding
			 * invisible point is the one we just drew unless it
			 * was outside the X11 coordinate space in which case
			 * we need to draw it again clipped to the coordinate
			 * space boundary on the line to the next point (which
			 * we save as op).  If the next point is visible, or
			 * the line between the points crosses the visible
			 * space, then the line from this point to the next
			 * point will be in the correct direction and clipped
			 * at the edge(s).
			 */
			bp = vp;
			if (vp->x < loX && vp->x + vp->w < loX) {
				while (vp->x < loX && vp->x + vp->w < loX &&
				       vp < ep)
					++vp;
				if (beyond)
					op = vp;
				if (vp < ep)
					--vp;
			} else if (vp->x > hiX && vp->x + vp->w > hiX) {
				while (vp->x > hiX && vp->x + vp->w > hiX &&
				       vp < ep)
					++vp;
				if (beyond)
					op = vp;
				if (vp < ep)
					--vp;
			} else if (vp->y < loY && vp->y + vp->h < loY) {
				while (vp->y < loY && vp->y + vp->h < loY &&
				       vp < ep)
					++vp;
				if (beyond)
					op = vp;
				if (vp < ep)
					--vp;
			} else if (vp->y > hiY && vp->y + vp->h > hiY) {
				while (vp->y > hiY && vp->y + vp->h > hiY &&
				       vp < ep)
					++vp;
				if (beyond)
					op = vp;
				if (vp < ep)
					--vp;
			}
			if (vp > bp || beyond)
			    --vp;
			++vp;
			++ver;
		}
		if (flags->rectangle) {
			if (flags->rectangle > 1)
				XFillRectangles(display, win, (GC)p->GC,
						Xrectangles, rec - Xrectangles);
			XDrawRectangles(display, win, (GC)p->GC,
					Xrectangles, rec - Xrectangles);
		}
		if (!flags->nolines)
			XDrawLines(display, win, (GC)p->GC, Xpoints, 
				   ver - Xpoints, CoordModeOrigin);
		else if (flags->mark && flags->pixmarks)
			XDrawPoints(display, win, (GC)p->GC, Xpoints,
				    ver - Xpoints, CoordModeOrigin);
		/*
		 * If we break data across two calls, continue it from the
		 * last point of the last chunk.
		 */
		Xpoints[0] = verend[-1];
		ver = &Xpoints[1];
		rec = Xrectangles;
	}

	XSetClipMask(display, (GC)p->GC, None);
}

/*
 * This routine draws the data sets themselves using the macros for
 * translating coordinates.
 */
static void
DrawData(LocalWin *wi, Window win)
{
	register struct data_set *p;

	for (p = datasets; p != 0; p = p->next) {
		if (DataSetHidden(wi, p->setno))
			continue;
		DrawSet(wi, win, p);
	}
}

/*
 * This routine draws a single data set so it will be rendered on top of any
 * others already drawn.
 */
void
DrawSetNo(LocalWin *wi, Window win, int setno)
{
	register struct data_set *p;

	for (p = datasets; p != 0; p = p->next) {
		if (p->setno != setno)
			continue;
		DrawSet(wi, win, p);
		if (wi->cache != 0)
			XCopyArea(display, win, wi->cache, copyGC,
				  0, 0, wi->width, wi->height, 0, 0);
		break;
	}
}

/*
 * This draws a legend of the data sets displayed.  Only those that will fit
 * are drawn.
 */
static void
DrawLegend(LocalWin *wi, Window win)
{
	int spot, maxlen, len, height, x0, x1;
	struct data_set *p;

	height = axisFont->ascent + axisFont->descent;
	maxlen = 0;
	x0 =  wi->XOppX + PADDING + mark_w;
	x1 = x0 + maxName;
	spot = wi->XOrgY + PADDING;
	for (p = datasets; p != 0; p = p->next) {
		XDrawLine(display, win, (GC)p->GC, x0, spot, x1, spot);
		XDrawString(display, win, textGC, x0, spot + height + 2,
			    p->setName, strlen(p->setName));
		XSetClipOrigin(display, p->mGC, x0 - mark_w,
			       spot + height - mark_h);
		XFillRectangle(display, win, p->mGC, x0 - mark_w, 
			       spot + height - mark_h, mark_w, mark_h);
		spot += 2 * height + PADDING;
	}
}

static char *help[] = {
    "\bMouse Functions",
    "",
    "Hold shift-ctrl-left: show (X,Y) coordinates at cursor",
    "Hold shift-left: if on a data point, also show set label, ordinal, comment",
    "Hold left: also decode X coordinate as GMT/local timestamp if after 1999",
    "Drag left: show a triangle labeled with delta X, delta Y and slope values",
    " (displayed string is set as X11 primary selection to paste in some other",
    " X11 window using middle mouse button)",
    "",
    "Drag middle: show rubber-band line with slope value",
    "Hold shift-middle on data point: draw regression line, show formula for it",
    " (formula set as X11 primary selection; window redraw erases the line)",
    "",
    "Drag right: create new window with zoomed view of points spanned",
    "Drag shift-right: write xgraph.tmp containing subset of points spanned",
    "",
    "Scroll wheel up/down/left/right: pan the graph view",
    "Shift scroll wheel up/down: zoom the graph view in/out at the cursor",
    "",
    "",
    "\bKeystroke Commands",
    "",
    "These commands are not case sensitive and can be typed anywhere.",
    "",
    "    0 - 9	Toggles hiding of the Nth dataset; with ctrl, N+10th",
    "    E	Toggles showing error bars (if dataset includes them)",
    "    F	Toggle drawing filled rectangles (for four-value data sets)",
    "    G or T	Toggles showing the grid versus just tick marks",
    "    H	Toggles drawing a vertical line from Y=0 to each data point",
    "    L	Toggles displaying lines between points",
    "    M	Toggles showing data point marks",
    "    O	Toggles showing graph outline",
    "    P	Toggles between single pixel and icon data point marks",
    "    Q	Closes the current window",
    "    R	Toggle drawing rectangles (for four-value data sets)",
    "    ? or F1	Pop up this Help window",
    "    Arrows	Pan the graph view; up/down with shift zooms in/out",
    "    Ctrl-C	Quits the program",
    "    Ctrl-D	Closes the current window",
    "    Ctrl-H	Pop up this Help window",
    "    Ctrl-L	Redraw the window",
    "",
    "When hiding of a dataset is toggled off, the dataset is redrawn on top",
    "of the other datasets.  Ctrl-L will restore the original drawing order.",
    0
};
static int help_width, tab_width;

/*
 * Scan the help text to measure how large a window is needed to contain it.
 * Also measure the space needed before the tab stop for the keystroke list.
 */
int
MeasureHelp()
{
	char **p;
	char *s, *tab;
	int len, aftertab, width, height, w, h;
	XFontStruct *font;

	h = axisFont->ascent + axisFont->descent;
	tab_width = 0;
	aftertab = 0;
	width = 0;
	height = PADDING * 4;
	
	for (p = help; *p; ++p) {
		s = *p;
		if (!*s) {
			height += h/2;	/* half spacing for blank lines */
			continue;
		}
		if (*s == '\b') {
			++s;
			font = titleFont;
		} else
			font = axisFont;
		tab = index(s, '\t');
		if (tab) {
			w = XTextWidth(font, s, tab-s);
			if (w > tab_width)
				tab_width = w;
			s = tab + 1;
		}
		len = strlen(s);
		height += h;
		w = XTextWidth(font, s, len);
		if (tab) {
			if (w > aftertab)
				aftertab = w;
		} else {
			if (w > width)
				width = w;
		}
	}
	tab_width += PADDING*5;
	if (tab_width + aftertab > width)
		width = tab_width + aftertab;
	width += PADDING*4;
	help_width = width;
	return (width << 16) | height;
}

/*
 * This draws help text explaining the available runtime operations
 * in a separate Help window when invoked by '?', Ctrl-H or F1.
 */
static void
DrawHelp(LocalWin *wi, Window win)
{
	char **p;
	char *s, *tab;
	int len, w, h, x, y;
	GC gc;

	h = axisFont->ascent + axisFont->descent;
	y = PADDING;

	for (p = help; *p; ++p) {
		s = *p;
		if (!*s) {
			y += h/2;	/* half spacing for blank lines */
			continue;
		}
		y += h;
		len = strlen(s);
		if (*s == '\b') {
			++s;
			--len;
			w = XTextWidth(titleFont, s, len);
			x = (help_width - w) / 2;
			gc = titleGC;
		} else {
			x = PADDING * 2;
			gc = textGC;
			tab = index(s, '\t');
			if (tab) {
				XDrawString(display, win, gc, x, y, s, tab-s);
				x += tab_width;
				s = tab + 1;
				len = strlen(s);
			}
		}
		XDrawString(display, win, gc, x, y, s, len);
	}
}

int cflag;

/*
 * Draws the data in the window.  Does not clear the window. The data is
 * scaled so that all of the data will fit. Grid lines are drawn at the
 * nearest power of 10 in engineering notation.  Draws axis numbers along
 * bottom and left hand edges.
 */
void
DrawWindow(Window win, LocalWin *wi)
	           		/* Window to draw in */
	             	/* Window information */
{
	if (wi->help) {
		DrawHelp(wi, win);
		return;
	}

	/* Figure out the transformation constants */
	if (TransformCompute(wi)) {

		/* Draw the legend */
		DrawLegend(wi, win);

		/* Draw the axis unit labels,  grid lines,  and grid labels */
		DrawGridAndAxis(win, wi);

		/* Draw the data sets themselves */
		DrawData(wi, win);
	}
	if (cflag == 0) {
		if (wi->cache == 0)
			wi->cache = XCreatePixmap(display, win, wi->width,
						  wi->height, depth);
		XCopyArea(display, win, wi->cache, copyGC, 
			  0, 0, wi->width, wi->height, 0, 0);
	}
}

void
RedrawWindow(Window win, LocalWin *wi)
{
	if (wi->cache == 0)
		DrawWindow(win, wi);
	else
		XCopyArea(display, wi->cache, win, copyGC, 
			  0, 0, wi->width, wi->height, 0, 0);
}

void
ResizeWindow(Window win, LocalWin *wi)
{
	if (wi->cache != 0) {
		XFreePixmap(display, wi->cache);
		wi->cache = 0;
	}
	XClearWindow(display, win);
	DrawWindow(win, wi);
}

void
PanWindow(LocalWin *wi, unsigned long direction)
{
	Window win = wi->win;
	double opposite = naturalScroll ? -1.0 : 1.0;
	double Xspan = wi->hiX - wi->loX;
	double Yspan = wi->hiY - wi->loY;
	double Xjump = opposite * Xspan / 10.0;
	double Yjump = opposite * Yspan / 10.0;
	double Xshift = opposite * Xspan / 100.0;
	double Yshift = opposite * Yspan / 100.0;

	switch(direction) {
	case Button4:
		wi->loY += Yshift;
		wi->hiY += Yshift;
		break;

	case Button5:
		wi->loY -= Yshift;
		wi->hiY -= Yshift;
		break;

	case Button5+1:
		wi->loX -= Xshift;
		wi->hiX -= Xshift;
		break;

	case Button5+2:
		wi->loX += Xshift;
		wi->hiX += Xshift;
		break;

	case XK_Up:
		wi->loY += Yjump;
		wi->hiY += Yjump;
		break;

	case XK_Down:
		wi->loY -= Yjump;
		wi->hiY -= Yjump;
		break;

	case XK_Left:
		wi->loX -= Xjump;
		wi->hiX -= Xjump;
		break;

	case XK_Right:
		wi->loX += Xjump;
		wi->hiX += Xjump;
		break;
	}
	if (wi->loX < overall_bb.loX) {
		double fix = overall_bb.loX - wi->loX;
		wi->loX += fix;
		wi->hiX += fix;
	}
	if (wi->hiX > overall_bb.hiX) {
		double fix = wi->hiX - overall_bb.hiX;
		wi->loX -= fix;
		wi->hiX -= fix;
	}
	if (wi->loY < overall_bb.loY) {
		double fix = overall_bb.loY - wi->loY;
		wi->loY += fix;
		wi->hiY += fix;
	}
	if (wi->hiY > overall_bb.hiY) {
		double fix = wi->hiY - overall_bb.hiY;
		wi->loY -= fix;
		wi->hiY -= fix;
	}
	XClearWindow(display, win);
	DrawWindow(win, wi);
}

void
ZoomWindow(XButtonEvent *b, LocalWin *wi, unsigned long direction)
{
	Window win = wi->win;
	double shift, inout, north, south, west, east, portion;
	double loX, hiX, loY, hiY;

	if (direction == Button5+1 || direction == Button5+2)
		return;
	/* Calculate amount to change in all four directions from point. */
	if (b) {
		shift = (wi->UsrOppY - wi->UsrOrgY) / 50.0;
		north = shift * (b->y - wi->XOrgY - pad) / wi->clip.height;
		south = shift * (wi->XOppY - b->y - pad) / wi->clip.height;
		shift = (wi->UsrOppX - wi->UsrOrgX) / 50.0;
		west = shift * (b->x - wi->XOrgX - pad) / wi->clip.width;
		east = shift * (wi->XOppX - b->x - pad) / wi->clip.width;
	} else {
		north = south = (wi->hiY - wi->loY) / 10.0;
		west = east = (wi->hiX - wi->loX) / 10.0;
	}
	inout = 1.0;
	if (direction == Button5 || direction == XK_Down)
		inout = -1.0;
	loX = wi->loX + inout * west;
	hiX = wi->hiX - inout * east;
	loY = wi->loY + inout * south;
	hiY = wi->hiY - inout * north;

	/* Don't allow zooming out past overall bounding box.  Some of this
	 * may seem redundant, but it's there to avoid error accumulation. */
	if ((hiX - loX) > (overall_bb.hiX - overall_bb.loX)) {
		if ((hiY - loY) > (overall_bb.hiY - overall_bb.loY)) {
			loX = overall_bb.loX;
			hiX = overall_bb.hiX;
			loY = overall_bb.loY;
			hiY = overall_bb.hiY;
		} else {
			portion = ((overall_bb.hiX - overall_bb.loX) -
				   (wi->hiX - wi->loX)) /
				((hiX - loX) - (wi->hiX - wi->loX));
			loX = overall_bb.loX;
			hiX = overall_bb.hiX;
			loY = wi->loY + inout * south * portion;
			hiY = wi->hiY - inout * north * portion;
			if (loY < overall_bb.loY) {
				hiY += overall_bb.loY - loY;
				loY = overall_bb.loY;
				if (hiY > overall_bb.hiY)
					hiY = overall_bb.hiY;
			} else if (hiY > overall_bb.hiY) {
				loY -= loY - overall_bb.loY;
				hiY = overall_bb.hiY;
				if (loY < overall_bb.loY)
					loY = overall_bb.loY;
			}
		}
	} else if ((hiY - loY) > (overall_bb.hiY - overall_bb.loY)) {
		portion = ((overall_bb.hiY - overall_bb.loY) -
			   (wi->hiY - wi->loY)) /
			((hiY - loY) - (wi->hiY - wi->loY));
		loY = overall_bb.loY;
		hiY = overall_bb.hiY;
		loX = wi->loX + inout * west * portion;
		hiX = wi->hiX - inout * east * portion;
		if (loX < overall_bb.loX) {
			hiX += overall_bb.loX - loX;
			loX = overall_bb.loX;
			if (hiX > overall_bb.hiX)
				hiX = overall_bb.hiX;
		} else if (hiX > overall_bb.hiX) {
			loX -= loX - overall_bb.loX;
			hiX = overall_bb.hiX;
			if (loX < overall_bb.loX)
				loX = overall_bb.loX;
		}
	}

	/* If we have zoomed out to hit an edge, shift to the edge. */
	if (loX < overall_bb.loX) {
		double fix = overall_bb.loX - loX;
		loX += fix;
		hiX += fix;
	} else if (hiX > overall_bb.hiX) {
		double fix = hiX - overall_bb.hiX;
		loX -= fix;
		hiX -= fix;
	}
	if (loY < overall_bb.loY) {
		double fix = overall_bb.loY - loY;
		loY += fix;
		hiY += fix;
	} else if (hiY > overall_bb.hiY) {
		double fix = hiY - overall_bb.hiY;
		loY -= fix;
		hiY -= fix;
	}
	wi->loX = loX;
	wi->hiX = hiX;
	wi->loY = loY;
	wi->hiY = hiY;
	XClearWindow(display, win);
	DrawWindow(win, wi);
}

/*
 * This routine figures out how to draw the axis labels and grid lines.  The
 * axes must be linear, but logarithmic graphs are supported by taking the log
 * of the data points as they are read and then labeling the axis with the
 * resulting exponent values plus a label "x 10^y" at the top or bottom
 * indicating that the labels are to be interpreted as exponents.  This routine
 * also figures out the necessary transformation information for the display of
 * the points (it touches XOrgX, XOrgY, UsrOrgX, UsrOrgY, and UnitsPerPixel).
 */
int 
TransformCompute(LocalWin *wi)
	             		/* Window information */
{
	register struct data_set *p;
	double bbCenX, bbCenY, bbHalfWidth, bbHalfHeight;
	int maxwid, height;

	/*
	 * First,  we figure out the origin in the X window.  Above the space
	 * we have the Y axis unit label. To the left of the
	 * space we have the Y axis grid labels.
	 */
	maxwid = XTextWidth(axisFont, "-000.00", 7) + PADDING*2;
	height = titleFont->ascent + titleFont->descent + PADDING;
	wi->XOrgX = maxwid;
	wi->XOrgY = height;
	/*
	 * Now we find the lower right corner.  Below the space we have the X
	 * axis grid labels.  To the right of the space we have the X axis
	 * unit label and the legend.  We assume the worst case size for the
	 * unit label.
	 */
	maxName = 0;
	for (p = datasets; p != 0; p = p->next) {
		int tmpSize;

		tmpSize = XTextWidth(axisFont, p->setName, strlen(p->setName));
		if (tmpSize > maxName)
			maxName = tmpSize;
	}
	height = axisFont->ascent + axisFont->descent + PADDING;
	wi->XOppX = wi->width - maxName - PADDING*2 - mark_w;
	wi->XOppY = wi->height - height - PADDING - mark_h;

	if ((wi->XOrgX + pad >= wi->XOppX - pad) ||
	    (wi->XOrgX + pad >= wi->XOppY - pad)) {
		(void)fprintf(stderr, "Drawing area is too small\n");
		return 0;
	}
	/*
	 * We now have a bounding box for the drawing region. Figure out the
	 * units per pixel using the data set bounding box.
	 */
	wi->XUnitsPerPixel = (wi->hiX - wi->loX) /
				(double)(wi->XOppX - wi->XOrgX - 2*pad);
	wi->YUnitsPerPixel = (wi->hiY - wi->loY) /
				(double)(wi->XOppY - wi->XOrgY - 2*pad);

	/*
	 * Find origin in user coordinate space.  We keep the center of the
	 * original bounding box in the same place.
	 */
	bbCenX = (wi->loX + wi->hiX) / 2.0;
	bbCenY = (wi->loY + wi->hiY) / 2.0;
	bbHalfWidth = ((double)(wi->XOppX - wi->XOrgX)) / 
		2.0 * wi->XUnitsPerPixel;
	bbHalfHeight = ((double)(wi->XOppY - wi->XOrgY)) / 
		2.0 * wi->YUnitsPerPixel;
	wi->UsrOrgX = bbCenX - bbHalfWidth;
	wi->UsrOrgY = bbCenY - bbHalfHeight;
	wi->UsrOppX = bbCenX + bbHalfWidth;
	wi->UsrOppY = bbCenY + bbHalfHeight;

	wi->clip.x = wi->XOrgX;
	wi->clip.y = wi->XOrgY;
	wi->clip.width = wi->XOppX - wi->XOrgX;
	wi->clip.height = wi->XOppY - wi->XOrgY;

	/*
	 * Everything is defined so we can now use the SCREENX and SCREENY
	 * transformations.
	 */
	return 1;
}

/*
 * Calculate the least-squares regression (linear fit) for a data set.  We
 * subtract off the loX and loY values to avoid exceeding the resolution of
 * double-precision floating point for cases such as timestamps for the X axis.
 */
static void calculate_regression(LocalWin *wi, struct data_set *p)
{
	double x, y, m, b;
	double d;
	double sx = 0, sy = 0, sx2 = 0, sxy = 0;
	int n = p->numPoints;
	Value *vp = p->dvec;
	Value *ep = &p->dvec[n];
	double lx = p->bb.loX;
	double ly = p->bb.loY;

	for ( ; vp < ep; ++vp) {
		x = vp->x - lx;
		y = vp->y - ly;
		sx += x;
		sy += y;
		sx2 += x*x;
		sxy += x*y;
	}
	d = (n * sx2 - sx * sx);
	m = (n * sxy - sx * sy) / d;
	b = (sy - m * sx) / n - m * lx + ly;
	if (d == 0.0) {
		p->x1 = SCREENX(wi, p->bb.loX);
		p->y1 = SCREENY(wi, p->bb.loY);
		p->x2 = SCREENX(wi, p->bb.hiX);
		p->y2 = SCREENY(wi, p->bb.hiY);
	} else {
		x = p->bb.loX;
		y = x * m + b;
		if (y < p->bb.loY) {
			y = p->bb.loY;
			x = (y - b) / m;
		} else if (y > p->bb.hiY) {
			y = p->bb.hiY;
			x = (y - b) / m;
		}
		p->x1 = SCREENX(wi, x);
		p->y1 = SCREENY(wi, y);
		x = p->bb.hiX;
		y = x * m + b;
		if (y < p->bb.loY) {
			y = p->bb.loY;
			x = (y - b) / m;
		} else if (y > p->bb.hiY) {
			y = p->bb.hiY;
			x = (y - b) / m;
		}
		p->x2 = SCREENX(wi, x);
		p->y2 = SCREENY(wi, y);
	}
	p->m = m;
	p->b = b;
	p->regression = 1;
}

static double inline dist(double ux, double uy, double x, double y)
{
	double dx = ux - x, dy = uy - y;
	return (dx*dx + dy*dy);
}

static struct data_set *closest_point(
	LocalWin *wi,
	double ux, double uy,
	int* cpt,
	char **comment)
{
	struct data_set *p, *mset;
	Value *v;
	int pt;
	double mdist;
	double xthresh = 8 * wi->XUnitsPerPixel;
	double ythresh = 8 * wi->YUnitsPerPixel;

	mset = 0;
	*cpt = 0;
	mdist = 4. * (xthresh*xthresh + ythresh*ythresh);
	for (p = datasets; p != 0; p = p->next) {
		if (DataSetHidden(wi, p->setno))
			continue;
		v = p->dvec;
		for (pt = p->numPoints; --pt >= 0;) {
			if (fabs(v->x - ux) < xthresh &&
			    fabs(v->y - uy) < ythresh)
			    if (dist(ux, uy, v->x, v->y) < mdist) {
				mdist = dist(ux, uy, v->x, v->y);
				mset = p;
				*cpt = p->numPoints - pt - 1;
				if (comment != NULL)
					*comment = v->comment;
			    }
			++v;
		}
	}
	return (mset);
}

void
DoIdentify(XButtonEvent *e, LocalWin *wi, Cursor cur, int noPoints)
{
	Window w = e->window;
	XEvent ev;
	Pixmap pm;
	double ux, uy;
	struct data_set *mset = 0;
	int mpt;
	char *comment = NULL;
	int labx, laby;
	int width, height;
	int len;
	int st;
	char labstr[256];
	char *labptr = labstr;

	/*
	 * see if we have point(s) within 8 pixels of the cursor. If so,
	 * output the index and coordinates of the the closest point.
	 * Otherwise, output the coordinates of the cursor.
	 */
	labx = e->x;
	laby = e->y;
	ux = TRANX(labx);
	uy = TRANY(laby);
	if (! noPoints)
		mset = closest_point(wi, ux, uy, &mpt, &comment);
	if (mset != 0) {
		/* if we have more than one set, identify set and point */
		if (datasets->next != 0)
			labptr += sprintf(labptr, "%s-", mset->setName);
		labptr += sprintf(labptr, "%d: ", mpt);
		ux = mset->dvec[mpt].x;
		uy = mset->dvec[mpt].y;
	}
	labptr += sprintf(labptr, "(%.*g, %.*g)", 
			  precision + wi->XPrecisionOffset, ux,
			  precision + wi->YPrecisionOffset, uy);
	if (comment)
		(void)sprintf(labptr, " %s", comment);

	/* center label below cursor position, but avoid edge clipping */
	len = strlen(labstr);
	width = XTextWidth(infoFont, labstr, len);
	height = infoFont->ascent + infoFont->descent;
	labx -= width / 2;
	laby += height;
	if (labx + width > wi->width)
		labx = wi->width - width;
	if (labx < 0)
		labx = 0;

	/* save what's under the label, then output the label */
	pm = XCreatePixmap(display, w, width, height, depth);
	if (! pm)
		return;

	st = XGrabPointer(display, w, True, ButtonReleaseMask,
			   GrabModeAsync, GrabModeAsync, w, cur, CurrentTime);
	if (st != GrabSuccess) {
		XFreePixmap(display, pm);
		XBell(display, 0);
		return;
	}
	XCopyArea(display, w, pm, infoGC, labx, laby, width, height, 0, 0);
	XDrawImageString(display, w, infoGC, labx, laby + infoFont->ascent,
			 labstr, len);

	/* wait till button released */
	XWindowEvent(display, w, ButtonReleaseMask, &ev);

	/* erase the label */
	XCopyArea(display, pm, w, infoGC, 0, 0, width, height, labx, laby);

	XFreePixmap(display, pm);
	XUngrabPointer(display, CurrentTime);
}

void
DoSlope(XButtonEvent *e, LocalWin *wi, Cursor cur)
{
	Window w = e->window;
	XEvent ev;
	Pixmap pm = 0;
	double ux, uy, sxy;
	int sx, sy, cx, cy;
	int labx, laby;
	int width, height;
	int len;
	char labstr[256];
	int st;

	labx = laby = width = height = 0;

	sxy = wi->YUnitsPerPixel / wi->XUnitsPerPixel;
	st = XGrabPointer(display, w, True,
		      ButtonReleaseMask|PointerMotionMask|PointerMotionHintMask,
		      GrabModeAsync, GrabModeAsync, w, cur, CurrentTime);
	if (st != GrabSuccess)
		return;

	sx = e->x;
	sy = e->y;
	cx = sx;
	cy = sy;
	XDrawLine(display, w, echoGC, sx, sy, cx, cy);
	while (1) {
		Window dummy;
		int dummyX;
		u_int mask;

		XNextEvent(display, &ev);
		if (ev.xany.type != MotionNotify &&
		    ev.xany.type != ButtonRelease)
			continue;

		/* erase the old label and line */
		if (pm) {
			XCopyArea(display, pm, w, infoGC, 0, 0, width, height,
				  labx, laby);
			XFreePixmap(display, pm);
			pm = 0;
		}
		XDrawLine(display, w, echoGC, sx, sy, cx, cy);

		/* we're done if anything but a motion event */
		if (ev.xany.type == ButtonRelease)
			break;

		/*
		 * draw a new target line.  if we're far enough from the
		 * start, label it with its slope.
		 */
		XQueryPointer(display, w, &dummy, &dummy, &dummyX, &dummyX,
			      &cx, &cy, &mask);
		XDrawLine(display, w, echoGC, sx, sy, cx, cy);
		if (abs(cx - sx) < 16 && abs(cy - sy) < 16)
			continue;

		ux = cx - sx;
		uy = sy - cy;
		if (ux != 0.)
			(void)sprintf(labstr, "%.*g", 
				      precision, uy / ux * sxy * slope_scale);
		else
			(void)sprintf(labstr, "-inf-");

		/* center label below cursor position */
		len = strlen(labstr);
		width = XTextWidth(infoFont, labstr, len);
		height = infoFont->ascent + infoFont->descent;
		labx = cx - width / 2;
		laby = cy + height / 2;

		/* save what's under the label, then output the label */
		pm = XCreatePixmap(display, w, width, height, depth);
		if (! pm) 
			break;
		XCopyArea(display, w, pm, infoGC, labx, laby, width, height,
			  0, 0);
		XDrawImageString(display, w, infoGC, labx,
				 laby + infoFont->ascent, labstr, len);
	}
	XUngrabPointer(display, CurrentTime);
}

void
DoRegression(XButtonEvent *e, LocalWin *wi, Cursor cur)
{
	Window w = e->window;
	XEvent ev;
	Pixmap pm;
	double ux, uy;
	struct data_set *mset = 0;
	int mpt;
	char *comment = NULL;
	int labx, laby;
	int width, height;
	int len;
	int st;
	char labstr[256];
	char *labptr = labstr;
	int prec;

	/*
	 * see if we have point(s) within 8 pixels of the cursor. If so,
	 * output the index and coordinates of the the closest point.
	 * Otherwise, output the coordinates of the cursor.
	 */
	labx = e->x;
	laby = e->y;
	ux = TRANX(labx);
	uy = TRANY(laby);
	if (datasets->next == 0)
		mset = datasets;
	else
		mset = closest_point(wi, ux, uy, &mpt, &comment);
	if (mset == 0) {
		labptr += sprintf(labptr, "click data point");
	} else {
		if (mset->regression == 0) {
			calculate_regression(wi, mset);
		}
		XDrawLine(display, w, fitGC, mset->x1, mset->y1,
			  mset->x2, mset->y2);
		/* if we have more than one set, include set name */
		if (datasets->next != 0)
			labptr += sprintf(labptr, "%s: ", mset->setName);
		prec = wi->XPrecisionOffset > wi->XPrecisionOffset ? 
			wi->XPrecisionOffset : wi->XPrecisionOffset;
		if (isnan(mset->m))
			labptr += sprintf(labptr, "-inf-");
		else if (mset->b < 0.0)
			labptr += sprintf(labptr, "y = %.*g * x - %.*g",
					  precision, mset->m, prec, -mset->b);
		else
			labptr += sprintf(labptr, "y = %.*g * x + %.*g",
					  precision, mset->m, prec, mset->b);
	}
	/* center label below cursor position, but avoid edge clipping */
	len = labptr - labstr;
	width = XTextWidth(infoFont, labstr, len);
	height = infoFont->ascent + infoFont->descent;
	labx -= width / 2;
	laby += height;
	if (labx + width > wi->width)
		labx = wi->width - width;
	if (labx < 0)
		labx = 0;

	/* prepare to return the label as X selection if asked */
	strncpy(selection, labstr, MAXIDENTLEN);
	selection[MAXIDENTLEN - 1] = '\0';
	selectionSetTime = e->time;
	selectionClearTime = CurrentTime;
	XSetSelectionOwner(display, XA_PRIMARY, w, e->time);

	/* save what's under the label, then output the label */
	pm = XCreatePixmap(display, w, width, height, depth);
	if (! pm)
		return;

	st = XGrabPointer(display, w, True, ButtonReleaseMask,
			   GrabModeAsync, GrabModeAsync, w, cur, CurrentTime);
	if (st != GrabSuccess) {
		XFreePixmap(display, pm);
		XBell(display, 0);
		return;
	}
	XCopyArea(display, w, pm, infoGC, labx, laby, width, height, 0, 0);
	XDrawImageString(display, w, infoGC, labx, laby + infoFont->ascent,
			 labstr, len);

	/* wait till button released */
	XWindowEvent(display, w, ButtonReleaseMask, &ev);

	/* erase the label */
	XCopyArea(display, pm, w, infoGC, 0, 0, width, height, labx, laby);

	XFreePixmap(display, pm);
	XUngrabPointer(display, CurrentTime);
}

void
DoDistance(XButtonEvent *e, LocalWin *wi, Cursor cur)
{
	Window w = e->window;
	XEvent ev;
	Pixmap pmh = 0;
	Pixmap pmv = 0;
	Pixmap pms = 0;
	double ux, uy;
	int sx, sy, cx, cy;
	int hlabx, hlaby, hwidth, hheight;
	int vlabx, vlaby, vwidth, vheight;
	int slabx, slaby, swidth, sheight;
	int len;
	int st;
	char labstr[MAXIDENTLEN];
	char *labptr = labstr;
	XPoint boxPoints[4];
	struct data_set *mset;
	int mpt;
	int identifying = True;
	char *comment = NULL;

	hlabx = hlaby = hwidth = hheight = vlabx = vlaby = vwidth = vheight = 0;

	st = XGrabPointer(display, w, True,
		      ButtonReleaseMask|PointerMotionMask|PointerMotionHintMask,
		      GrabModeAsync, GrabModeAsync, w, cur, CurrentTime);
	if (st != GrabSuccess)
		return;

	sx = e->x; sy = e->y;
	boxPoints[0].x = sx;
	boxPoints[0].y = sy;
	boxPoints[1].x = 0;
	boxPoints[1].y = 0;
	boxPoints[2].x = 0;
	boxPoints[2].y = 0;
	boxPoints[3].x = 0;
	boxPoints[3].y = 0;
	XDrawLines(display, w, echoGC, boxPoints, 4, CoordModePrevious);

	/* identify the point under the cursor */
	ux = TRANX(sx);
	uy = TRANY(sy);
	mset = closest_point(wi, ux, uy, &mpt, &comment);

	if (mset != 0) {
		/* if we have more than one set, identify set and point */
		if (datasets->next != 0)
			labptr += sprintf(labptr, "%s-", mset->setName);
		labptr += sprintf(labptr, "%d: ", mpt);
		ux = mset->dvec[mpt].x;
		uy = mset->dvec[mpt].y;
	}
	if (datasets->bb.loX > 946684800) {
		/* x axis appears to be a timestamp, print as date & time */
		time_t xsec = (time_t)ux;
		struct tm xtm;
		int prec = wi->XPrecisionOffset - 6;
		
		if (localTime)
			localtime_r(&xsec, &xtm);
		else
			gmtime_r(&xsec, &xtm);
		labptr += strftime(labptr, MAXIDENTLEN - (labptr - labstr),
				   "(%Y%m%d-%H%M%S", &xtm);
		labptr += sprintf(labptr, "=%.*f", 
				  prec > 0 ? prec : 0, ux);
	} else
		labptr += sprintf(labptr, "(%.*g", 
				  precision + wi->XPrecisionOffset, ux);
	labptr += sprintf(labptr, ", %.*g)", 
			  precision + wi->YPrecisionOffset, uy);
	if (comment)
		(void)sprintf(labptr, " %s", comment);

	/* center label at cursor position, but avoid edge clipping */
	hwidth = XTextWidth(infoFont, labstr, strlen(labstr));
	hheight = infoFont->ascent + infoFont->descent;
	hlabx = sx - hwidth / 2;
	hlaby = sy + hheight;
	if (hlabx + hwidth > wi->width)
		hlabx = wi->width - hwidth;
	if (hlabx < 0)
		hlabx = 0;

	/* save what's under the label, then output the label */
	pmh = XCreatePixmap(display, w, hwidth, hheight, depth);
	if (pmh) {
		XCopyArea(display, w, pmh, infoGC, hlabx, hlaby,
			  hwidth, hheight, 0, 0);
		XDrawImageString(display, w, infoGC, hlabx,
				 hlaby + infoFont->ascent,
				 labstr, strlen(labstr));
		/* prepare to return the label as X selection if asked */
		strncpy(selection, labstr, MAXIDENTLEN);
		selection[MAXIDENTLEN - 1] = '\0';
		selectionSetTime = e->time;
		selectionClearTime = CurrentTime;
		XSetSelectionOwner(display, XA_PRIMARY, w, e->time);
	}
	while (1) {
		Window dummy;
		int dummyX;
		u_int mask;

		XNextEvent(display, &ev);
		if (ev.xany.type != MotionNotify &&
		    ev.xany.type != ButtonRelease)
			continue;

		/* don't erase point identification until significant motion */
		XQueryPointer(display, w, &dummy, &dummy, &dummyX, &dummyX,
			      &cx, &cy, &mask);
		if (identifying && ev.xany.type == MotionNotify &&
		    abs(cx - sx) < 5 && abs(cy - sy) < 5)
			continue;
		identifying = False;

		/* erase the old label and line */
		if (pms) {
			XCopyArea(display, pms, w, infoGC, 0, 0,
				  swidth, sheight, slabx, slaby);
			XFreePixmap(display, pms);
			pms = 0;
		}
		if (pmv) {
			XCopyArea(display, pmv, w, infoGC, 0, 0,
				  vwidth, vheight, vlabx, vlaby);
			XFreePixmap(display, pmv);
			pmv = 0;
		}
		if (pmh) {
			XCopyArea(display, pmh, w, infoGC, 0, 0,
				  hwidth, hheight, hlabx, hlaby);
			XFreePixmap(display, pmh);
			pmh = 0;
		}
		XDrawLines(display, w, echoGC, boxPoints, 4, CoordModePrevious);

		/* we're done if anything but a motion event */
		if (ev.xany.type != MotionNotify)
			break;

		/*
		 * draw a new target lines.  if we're far enough from the
		 * start, label x & y lines with dist from start.
		 */
		boxPoints[1].x = cx - e->x;
		boxPoints[2].y = cy - e->y;
		boxPoints[3].x = -boxPoints[1].x;
		boxPoints[3].y = -boxPoints[2].y;
		XDrawLines(display, w, echoGC, boxPoints, 4, CoordModePrevious);

		if (abs(cx - sx) >= 16) {
			char labstr2[256];
			int p1 = 0, p2 = 0;
			double tx = TRANX(e->x);
			double ty = TRANY(e->y);
			int h;

			mset = closest_point(wi, tx, ty, &p1, 0);
			tx = TRANX(cx);
			ty = TRANY(cy);
			if (mset && mset == closest_point(wi, tx, ty, &p2, 0))
				sprintf(labstr2, "%d", p2 - p1);
			else
				labstr2[0] = 0;

			ux = (cx - sx) * wi->XUnitsPerPixel;
			sprintf(labstr, "%.*g",
				precision + wi->XPrecisionOffset, ux);

			/* right just label below cursor position */
			hwidth = XTextWidth(infoFont, labstr, strlen(labstr));
			if (labstr2[0] && (h = XTextWidth(infoFont,
					labstr2, strlen(labstr2))) > hwidth)
				hwidth = h;
			h = infoFont->ascent + infoFont->descent;
			hlabx = (sx + cx - hwidth) / 2;
			hheight = h;
			if (labstr2[0])
				hheight *= 2;
			if (sy < cy)
				hlaby = sy - hheight - h / 2;
			else
				hlaby = sy + h / 2;
			if (hlaby < 0)
				hlaby = 0;

			/* save what's under the label, then output the label */
			pmh = XCreatePixmap(display, w, hwidth, hheight, depth);
			if (! pmh) 
				break;
			XCopyArea(display, w, pmh, infoGC, hlabx, hlaby,
				  hwidth, hheight, 0, 0);
			XDrawImageString(display, w, infoGC, hlabx,
					 hlaby + infoFont->ascent,
					 labstr, strlen(labstr));
			if (labstr2[0])
				XDrawImageString(display, w, infoGC, hlabx,
						 hlaby + h + infoFont->ascent,
						 labstr2, strlen(labstr2));
		}
		if (abs(cy - sy) >= 16) {
			ux = (sy - cy) * wi->YUnitsPerPixel;
			(void)sprintf(labstr, "%.*g",
				      precision + wi->XPrecisionOffset, ux);

			/* center label vertically to left of cursor */
			len = strlen(labstr);
			vwidth = XTextWidth(infoFont, labstr, len);
			vheight = infoFont->ascent + infoFont->descent;
			if (sx < cx)
				vlabx = cx + 8;
			else
				vlabx = cx - vwidth - 8;
			vlaby = (sy + cy - vheight) / 2;

			/* save what's under the label, then output the label */
			pmv = XCreatePixmap(display, w, vwidth, vheight, depth);
			if (! pmv) 
				break;
			XCopyArea(display, w, pmv, infoGC, vlabx, vlaby,
				  vwidth, vheight, 0, 0);
			XDrawImageString(display, w, infoGC, vlabx,
					 vlaby + infoFont->ascent, labstr, len);
		}
		if (abs(cx - sx) >= 16 && abs(cy - sy) >= 16) {
			double sxy = wi->YUnitsPerPixel / wi->XUnitsPerPixel;
			ux = cx - sx;
			uy = sy - cy;
			if (ux != 0.)
				(void)sprintf(labstr, "%.*g", precision,
					      uy / ux * sxy * slope_scale);
			else
				(void)sprintf(labstr, "-inf-");

			/* right just label at slope line midpoint */
			len = strlen(labstr);
			swidth = XTextWidth(infoFont, labstr, len);
			sheight = infoFont->ascent + infoFont->descent;
			if (sx < cx)
				slabx = (sx + cx) / 2 - swidth - 8;
			else
				slabx = (sx + cx) / 2 + 8;
			slaby = (sy + cy - sheight) / 2;

			/* save what's under the label, then output the label */
			pms = XCreatePixmap(display, w, swidth, sheight, depth);
			if (! pms) 
				break;
			XCopyArea(display, w, pms, infoGC, slabx, slaby,
				  swidth, sheight, 0, 0);
			XDrawImageString(display, w, infoGC, slabx,
					 slaby + infoFont->ascent, labstr, len);
		}
	}
	XUngrabPointer(display, CurrentTime);
}

#define DRAWBOX \
	if (curX < startX) { \
		echoBox.x = curX; \
		echoBox.width = startX - curX; \
	} else { \
		echoBox.x = startX; \
		echoBox.width = curX - startX; \
	} \
	if (curY < startY) { \
		echoBox.y = curY; \
		echoBox.height = startY - curY; \
	} else { \
		echoBox.y = startY; \
		echoBox.height = curY - startY; \
	} \
	XDrawRectangles(display, win, echoGC, &echoBox, 1);

static BBox *
RubberBox(XButtonEvent *evt, LocalWin *wi, Cursor cur)
{
	static XRectangle echoBox;
	Window win, dummy;
	XEvent theEvent;
	int startX, startY, curX, curY, newX, newY;
	int dummyX;
	u_int mask;
	int st;
	static BBox r;

	win = evt->window;
	st = XGrabPointer(display, win, True,
			    ButtonPressMask|ButtonReleaseMask|
			       PointerMotionMask|PointerMotionHintMask,
			    GrabModeAsync, GrabModeAsync, win, cur,
			    CurrentTime);
	if (st != GrabSuccess) {
		XBell(display, 0);
		return ((BBox *)0);
	}
	startX = evt->x;
	startY = evt->y;
	XQueryPointer(display, win, &dummy, &dummy, &dummyX, &dummyX,
		      &curX, &curY, &mask);

	/* Draw first box */
	DRAWBOX;
	while (1) {
		XNextEvent(display, &theEvent);
		switch (theEvent.xany.type) {
		case MotionNotify:
			XQueryPointer(display, win, &dummy, &dummy,
				      &dummyX, &dummyX, &newX, &newY, &mask);
			/* Undraw the old one */
			DRAWBOX;
			/* Draw the new one */
			curX = newX;
			curY = newY;
			DRAWBOX;
			break;
		case ButtonRelease:
			goto out;
		}
	}
out:
	DRAWBOX;
	XUngrabPointer(display, CurrentTime);
	if ((startX - curX != 0) && (startY - curY != 0)) {
		/* Figure out relative bounding box */
		r.loX = TRANX(startX);
		r.loY = TRANY(startY);
		r.hiX = TRANX(curX);
		r.hiY = TRANY(curY);
		if (r.loX > r.hiX) {
			double temp;

			temp = r.hiX;
			r.hiX = r.loX;
			r.loX = temp;
		}
		if (r.loY > r.hiY) {
			double temp;

			temp = r.hiY;
			r.hiY = r.loY;
			r.loY = temp;
		}
		/* Limit the new bounding box within overall_bb */
		if (r.loX < overall_bb.loX)
			r.loX = overall_bb.loX;
		if (r.hiX > overall_bb.hiX)
			r.hiX = overall_bb.hiX;
		if (r.loY < overall_bb.loY)
			r.loY = overall_bb.loY;
		if (r.hiY > overall_bb.hiY)
			r.hiY = overall_bb.hiY;
		/* physical aspect ratio */
		r.asp = (r.hiX - r.loX) / (r.hiY - r.loY);
		return (&r);
	}
	return ((BBox *)0);
}

int
HandleZoom(XButtonEvent *evt, LocalWin *wi, Cursor cur)
{
	BBox *b = RubberBox(evt, wi, cur);

	return b != 0 && NewWindow(b, &wi->flags, wi);
}

void
DoWriteSubset(XButtonEvent *evt, LocalWin *wi, Cursor cur)
{
	BBox *b;
	register double lx, ly, ux, uy;
	register Value *v;
	register struct data_set *p;
	register int pt;
	FILE *f;

	if ((b = RubberBox(evt, wi, cur)) == 0)
		return;

	if ((f = fopen("xgraph.tmp", "w")) == 0) {
		perror("xgraph.tmp");
		return;
	}

	lx = b->loX; ly = b->loY; ux = b->hiX; uy = b->hiY;
	for (p = datasets; p != 0; p = p->next) {
		(void)fprintf(f, "# %s\n", p->setName);
		v = p->dvec;
		for (pt = p->numPoints; --pt >= 0; ++v) {
			if (lx <= v->x && v->x <= ux &&
			    ly <= v->y && v->y <= uy) {
				(void)fprintf(f, "%.15g\t%.15g", v->x, v->y);
				if (v->w != 0) {
					(void)fprintf(f, "\t%.15g", v->w);
					if (v->h != 0)
						(void)fprintf(f, "\t%.15g",
							      v->h);
				} else if (v->h != 0)
					(void)fprintf(f, "\t0\t%.15g", v->h);
				if (v->comment)
					(void)fprintf(f, "\t# %s\n",
						      v->comment);
				else
					(void)fprintf(f, "\n");
			}
		}
	}
	(void)fclose(f);
}

/* Default color names */
static char *colornames[] = {
	"red", "blue", "limegreen", "magenta", "orange",
	"darkturquoise", "brown", "dimgray", "gold", "black"
};
#define COLORNAME(n) \
	colornames[(n) % (sizeof(colornames) / sizeof(colornames[0]))]


static void
colorGCs(Window win)
{
	struct data_set *p;
	XGCValues v;
	GC x;

	v.function = GXcopy;
	for (p = datasets; p != 0; p = p->next) {
		v.foreground = GetColor(COLORNAME(p->setno));
		x = XCreateGC(display, win, GCForeground|GCFunction, &v);
		p->GC = (void *)x;
	}
}

static char dash[] = {
	0,
	2, 1, 1,
	2, 4, 1,
	2, 2, 2,
	2, 3, 1,
	2, 1, 3,
	2, 4, 2,
	2, 4, 4,
	2, 4, 7,
	-1,
};

static void
bwGCs(Window win)
{
	int len;
	int n = 0;
	struct data_set *p;
	XGCValues v;
	GC x;

	v.function = GXcopy;
	v.foreground = normPixel;
	for (p = datasets; p != 0; p = p->next) {
		len = dash[n++];
		if (len < 0) {
			n = 0;
			len = dash[n++];
		}
		if (len == 0)
			v.line_style = LineSolid;
		else
			v.line_style = LineOnOffDash;

		x = XCreateGC(display, win, 
			      GCForeground|GCFunction|GCLineStyle, &v);
		if (len > 0) {
			XSetDashes(display, x, 0, &dash[n], len);
			n += len;
		}
		p->GC = (void *)x;
	}
}

static void
markGCs(Window win)
{
	struct data_set *p;
	XGCValues v;
	GC x;

	v.function = GXcopy;
	for (p = datasets; p != 0; p = p->next) {
		v.foreground = GetColor(COLORNAME(p->setno));
		v.clip_mask = XCreateBitmapFromData(display, win,
						    marks[MARKNO(p->setno)], 
						    mark_w, mark_h);
		x = XCreateGC(display, win, 
			      GCForeground|GCClipMask, &v);
		p->mGC = (void *)x;
	}
}

void
initGCs(Window win)
{
	XGCValues v;
	
	v.foreground = zeroPixel ^ bgPixel;
	v.foreground = normPixel ^ bgPixel;
	v.function = GXxor;
	echoGC = XCreateGC(display, win, GCForeground|GCFunction, &v);
	
	v.foreground = normPixel;
	v.line_width = 2;
	v.function = GXcopy;
	fitGC = XCreateGC(display, win, GCForeground|GCFunction|GCLineWidth, &v);
	
	v.foreground = normPixel;
	v.background = bgPixel;
	v.function = GXcopy;
	v.font = axisFont->fid;
	textGC = XCreateGC(display, win, 
			   GCFont|GCForeground|GCBackground|GCFunction, &v);
	
	v.foreground = text2Color;
	v.background = bgPixel;
	v.function = GXcopy;
	v.font = axisFont->fid;
	text2GC = XCreateGC(display, win, 
			   GCFont|GCForeground|GCBackground|GCFunction, &v);
	
	v.foreground = text3Color;
	v.background = bgPixel;
	v.function = GXcopy;
	v.font = axisFont->fid;
	text3GC = XCreateGC(display, win, 
			   GCFont|GCForeground|GCBackground|GCFunction, &v);
	
	v.foreground = normPixel;
	v.background = bgPixel;
	v.function = GXcopy;
	v.font = infoFont->fid;
	infoGC = XCreateGC(display, win, 
			   GCFont|GCForeground|GCBackground|GCFunction, &v);

	v.foreground = normPixel;
	v.background = bgPixel;
	v.function = GXcopy;
	v.font = titleFont->fid;
	titleGC = XCreateGC(display, win, 
			    GCFont|GCForeground|GCBackground|GCFunction, &v);
	if (bwFlag)
		bwGCs(win);
	else
		colorGCs(win);
	markGCs(win);

	v.function = GXcopy;
	v.foreground = normPixel;
	v.background = bgPixel;
	copyGC = XCreateGC(display, win,
			   GCForeground|GCBackground|GCFunction, &v);
}
