/*
 * Copyright (c) 1991, 1993, 1996, 1997
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
#include <unistd.h>
#include <string.h>
#include "flags.h"
#include "dataset.h"

#ifndef lint
static char rcsid[] =
    "@(#) $Header$ (LBL)";
#endif

/* VARARGS */
extern void error(char* cp, ...);
extern void xinit(char *dispname, struct plotflags *flags);
extern void xsetfont(char *name);
extern void setsetname(register char *name);
extern void ReadData(char *filename, int ulaw);
extern void xmain(struct plotflags *flags, int xlimits, int ylimits, double loX, double loY, double hiX, double hiY);

char *progname;

void usage(void);

static char *
dispname(argc, argv)
	int argc;
	char **argv;
{
	int i;

	for (i = 1; i < argc - 1; ++i) 
		if (strcmp(argv[i], "-d") == 0)
			return argv[i + 1];
	return 0;
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int i, op;
	int ulawflag = 0;
	struct plotflags flags;	
	int xlim = 0, ylim = 0;
	double loX, hiX, loY, hiY;
	char *stripdir();
	extern char *optarg;
	extern int optind, opterr;
	extern int bwFlag;

	progname = stripdir(argv[0]);
	memset(&flags, 0, sizeof(flags));
	xinit(dispname(argc, argv), &flags);

	/* Parse the argument list */
	while ((op = getopt(argc, argv, "x:y:cerRsS:tT:mMnN:oBpd:f:g:uDLl:v")) != -1) {
		switch (op) {

		default:
			usage();

		case 'c':
			cflag = 1;
			break;

		case 'e':
			flags.errorbar = 1;
			break;

		case 'r':
			flags.rectangle = 1;
			flags.nolines = 1;
			break;

		case 'R':
			flags.rectangle = 2;
			flags.nolines = 1;
			break;

		case 's':
			flags.spline = 1;
			break;

		case 'S':
			slope_scale = atof(optarg);
			break;

		case 't':
			flags.tick = 1;
			break;

		case 'T':
			graph_title = optarg;
			break;

		case 'm':
			flags.mark = 1;
			break;

		case 'M':
			bwFlag = 1;
			break;

		case 'n':
			flags.nolines = 1;
			break;

		case 'N':
			setsetname(optarg);
			break;

		case 'o':
			flags.bb = 1;
			break;

		case 'B':
			flags.bar = 1;
			break;

		case 'p':
			flags.pixmarks = 1;
			break;

		case 'd':
			break;

		case 'f':
			xsetfont(optarg);
			break;

		case 'g':
			geometry = optarg;
			break;

		case 'u':
			ulawflag = 1;
			break;

		case 'D':
			dateXFlag = 1;
			break;

		case 'L':
			dateXFlag = 1;
			localTime = 1;
			break;

		case 'l':
			if (*optarg == 'x')
				logXFlag = 1;
			else if (*optarg == 'y')
				logYFlag = 1;
			else
				usage();
			break;

		case 'v':
			usage();
			break;

		case 'x':
			xlim = 1;
			if (sscanf(optarg, "%lg,%lg", &loX, &hiX) != 2)
				usage();
			break;

		case 'y':
			ylim = 1;
			if (sscanf(optarg, "%lg,%lg", &loY, &hiY) != 2)
				usage();
			break;
		}
	}
	/* Read the data into the data sets */
	if (optind == argc)
		ReadData((char *)0, ulawflag);
	else
		for (i = optind; i < argc; ++i)
			ReadData(argv[i], ulawflag);
	if (datasets == NULL)
		error("no datapoints");

	xmain(&flags, xlim, ylim, loX, loY, hiX, hiY);
	exit(0);
}

void
usage(void)
{
	static char use[] =
"\
usage: graf [-B] [-c] [-D] [-L] [-e] [-m] [-M] [-n] [-o] [-p] [-r] [-R] [-s]\n\
            [-S] [-t] [-u] [-x x1,x2] [-y y1,y2] [-l x] [-l y] [-N label]\n\
            [-T title] [-d host:display] [-f font] [-g geometry] [ file ... ]\n\
\nFonts must be fixed width.\n\
-B    Draw bar graph\n\
-c    Don't use off-screen pixmaps to enhance screen redraws\n\
-D    Interpret X values as timestamps and label axis with date and time\n\
-e    Draw error bars at each point\n\
-l x  Logarithmic scale for X axis\n\
-l y  Logarithmic scale for Y axis\n\
-L    Show timestamps as local time not GMT, implies -D\n\
-m    Mark each data point with large dot\n\
-M    Forces black and white\n\
-n    Don't draw lines (scatter plot)\n\
-N    Dataset label (repeat for multiple datasets)\n\
-o    Draws bounding box around data\n\
-p    If -m, mark each point with a pixel instead\n\
-r    Draw rectangle at each point (-R for filled)\n\
-s    X Window System's notion of spline curves (not implemented)\n\
-S    Set scale for slope display\n\
-t    Draw tick marks instead of full grid\n\
-T    Graph title\n\
-u    Input files are ulaw encoded\n";
	fputs(use, stderr);
	exit(1);
}
