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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "flags.h"
#include "dataset.h"
#include "libst.h"
#include "token.h"

#ifndef MAXFLOAT 
#define MAXFLOAT HUGE
#endif

#define INITSIZE 512

struct data_set *datasets;
static struct data_set **lastSet = &datasets;
static int nsets = 0;

static int setnum;
static char *comment1 = NULL;
static char *setname[64];
#define SETNUM (sizeof(setname) / sizeof(setname[0]))

static char *stringspace = NULL;
static int remaining = 0;
static size_t pagesize;

void
setsetname(name)
	register char *name;
{
	if (setnum < SETNUM) {
		if (strlen(name) > MAXSETNAME)
			name[MAXSETNAME] = '\0';
		setname[setnum++] = name;
	}
}

void
DataSetBBox(b)
	BBox *b;
{
	struct data_set *p = datasets;

	if (p == 0)
		abort();

	*b = p->bb;
	for (p = p->next; p != 0; p = p->next) {
		if (b->loX > p->bb.loX)
			b->loX = p->bb.loX;
		if (b->hiX < p->bb.hiX)
			b->hiX = p->bb.hiX;
		if (b->loY > p->bb.loY)
			b->loY = p->bb.loY;
		if (b->hiY < p->bb.hiY)
			b->hiY = p->bb.hiY;
	}
}

static void
nameDataSet(struct data_set *p, char *name)
{
	if (p->setno < SETNUM && setname[p->setno] != NULL)
		p->setName = setname[p->setno];
	else if (name != NULL)
		p->setName = name;
	else {
		(void)sprintf(p->namebuf, "Set %d", p->setno);
		p->setName = p->namebuf;
	}
}

static struct data_set *
nextDataSet(char *name)
{
	struct data_set *p = (struct data_set *)malloc(sizeof(*p));
	
	if (p == 0)
		error("out of memory");

	p->setno = nsets++;
	nameDataSet(p, name);
	p->numPoints = 0;
	p->allocSize = INITSIZE;
	p->dvec = (Value *)malloc((unsigned)(INITSIZE * sizeof(*p->dvec)));
	if (p->dvec == 0)
		error("out of memory");

	p->bb.loX = MAXFLOAT;
	p->bb.loY = MAXFLOAT;
	p->bb.hiX = -MAXFLOAT;
	p->bb.hiY = -MAXFLOAT;

	*lastSet = p;
	lastSet = &p->next;
	p->next = 0;

	return p;
}

static void
growDataSet(p)
	struct data_set *p;
{
	unsigned int size;

	p->allocSize *= 2;
	size = p->allocSize * sizeof(*p->dvec);
	p->dvec = (Value *)realloc((char *)p->dvec, size);
	if (p->dvec == 0)
		error("out of memory");
}

static char *
getStringSpace(len)
	int len;
{
	char *str;

	/*
	 * We don't bother remembering the allocated memory to free it
	 * since (nonempty) datasets are never free()'d.
	 */

	if (len > remaining) {
		if (len > pagesize)
			error("excessively large string allocation");
		stringspace = (char *)malloc(pagesize);
		remaining = pagesize;
		if (stringspace == 0)
			error("out of memory");
	}
	str = stringspace;
	stringspace += len;
	remaining -= len;
	return str;
}

static int
scanline(v, comment, eof)
	double *v;
	char **comment;
	int *eof;
{
	extern char *yytext;
	extern int yyleng;
	char *str;
	int len;
	int n = 0;

	*comment = NULL;
	while (1) {
		int token = yylex();

		switch (token) {
		case 0:
			*eof = 1;
			return (n);

		case '\n':
			return (n);

		case TOK_NUM:
			if (0 <= n && n < 4)
				v[n++] = lex_num;
			break;

		case TOK_COMMENT:
			len = yyleng;
			if (len > MAXCOMMENT)
				len = MAXCOMMENT;
			str = getStringSpace(len+1);
			strncpy(str, yytext, len);
			*(str + len) = '\0';
			*comment = str;
			break;

		default:
			n = -1;
			break;
		}
	}
	/*NOTREACHED*/
	return (-1);
}

/*
 * Reads in the data sets from the supplied stream.
 */
void 
ReadData(filename, ulaw)
	char *filename;
	int ulaw;
{
	extern FILE *yyin;
	register int n, npts;
	register double x, y, w, h;
	register Value *v;
	int eof = 0;
	struct data_set *p;
	char *stripdir();

	pagesize = getpagesize();
	if (filename == 0) {
		p = nextDataSet(0);
		yyin = stdin;
	} else {
		p = nextDataSet(stripdir(filename));
		yyin = fopen(filename, "r");
		if (yyin == NULL) {
			perror(filename);
			exit(1);
		}
	}
	v = p->dvec;
	npts = 0;
	while (1) {
		double coord[4];

		if (npts >= p->allocSize) {
			growDataSet(p);
			v = &p->dvec[npts];
		}
		if (ulaw) {
			n = getc(yyin);
			if (n == EOF)
				break;
			v->x = npts;
			v->y = st_ulaw_to_linear(n);
			v->w = v->h = 0.;
			goto have_v;
		}
		n = scanline(coord, &v->comment, &eof);
		if (n < 0)
			continue;

		if (n == 0) {
			if (eof)
				break;
			/* blank line */
			if (npts != 0) {
				p->numPoints = npts;
				p = nextDataSet(v->comment);
				v = p->dvec;
				npts = 0;
			} else {
				/*
				 * If 2 or more comment lines precede the first
				 * dataset, take the first comment line as the
				 * graph title if none was given on command line.
				 */
				if (comment1) {
					if(!graph_title && nsets == 1)
					    graph_title = comment1;
				} else
					comment1 = v->comment;
				if (v->comment)
				/*
				 * Take dataset name from comment on last blank line
				 * preceding data.
				 */
				nameDataSet(p, v->comment);
			}
			continue;
		}
		switch (n) {

		default:
			error("%s: one to four numbers per line", filename);
			continue;

		case 1:
			v->x = npts;
			v->y = coord[0];
			v->w = v->h = 0.;
			break;

		case 2:
			v->x = coord[0];
			v->y = coord[1];
			v->w = v->h = 0.;
			break;

		case 3:
			v->x = coord[0];
			v->y = coord[1];
			v->w = v->h = coord[2];
			break;

		case 4:
			v->x = coord[0];
			v->y = coord[1];
			v->w = coord[2];
			v->h = coord[3];
			break;
		}
have_v:
		x = v->x;
		y = v->y;
		w = v->w;
		h = v->h;
		if (logXFlag) {
			if (x <= 0.0)
				continue;
			v->x = x = log10(x);
		}
		if (logYFlag) {
			if (y <= 0.0)
				continue;
			v->y = y = log10(y);
		}
		/* Update bounding box */
		if (x < p->bb.loX)
			p->bb.loX = x;
		if (x + w > p->bb.hiX)
			p->bb.hiX = x + w;
		if (y < p->bb.loY)
			p->bb.loY = y;
		if (y + h > p->bb.hiY)
			p->bb.hiY = y + h;
		
		++v; ++npts;
	}
	p->numPoints = npts;
	/* Discard last dataset if empty */
	if (npts == 0) {
		for (lastSet = &datasets;
		     *lastSet != p;
		     lastSet = &(*lastSet)->next) ;
		*lastSet = 0;
		free(p->dvec);
		free(p);
		--nsets;
	}
	fclose(yyin);
}
