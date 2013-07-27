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

#define MAXSETNAME 256	/* same as max filename size */
#define MAXCOMMENT 256
#define MAXIDENTLEN (MAXSETNAME + 256 + MAXCOMMENT)

typedef struct {
	double x;
	double y;
	double w;
	double h;
	char *comment;
} Value;

typedef struct {
	double loX, loY, hiX, hiY, asp;
} BBox;

struct data_set {
	struct data_set *next;
	int setno;		/* set id number */
	char *setName;		/* name of set */
	char namebuf[16];	/* space for generated names of the form
				   "Set %d": we need 11 bytes for the largest
				   int, 1 for null, and 4 for the rest. */
	int numPoints;		/* How many points */
	int allocSize;		/* Allocated size */
	Value *dvec;		/* data values */
	BBox bb;		/* bounding box around data set */
	void *GC;		/* data point graphics context for X */
	void *mGC;		/* marker graphics context */
};

extern struct data_set *datasets;

