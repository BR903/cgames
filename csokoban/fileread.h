/* fileread.h: Functions for reading the puzzle files.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_fileread_h_
#define	_fileread_h_

#include	<stdio.h>
#include	"csokoban.h"
#include	"movelist.h"

/* The collection of data maintained for each puzzle.
 */
typedef	struct gamesetup {
    short	ysize;			/* height of the map */
    short	xsize;			/* width of the map */
    short	boxcount;		/* total number of boxes */
    short	goalcount;		/* total number of goal locations */
    short	storecount;		/* number of boxes stored at start */
    yx		start;			/* position of player at start */
    int		movebestcount;		/* least number of moves to finish */
    int		pushbestcount;		/* least number of pushes to finish */
    int		movebestpushcount;	/* number of pushes in former */
    int		pushbestmovecount;	/* number of moves in latter */
    dyxlist	moveanswer;		/* solution with least moves */
    dyxlist	pushanswer;		/* solution with least pushes */
    int		level;			/* index of puzzle in series */
    char const *seriesname;		/* pointer to the name of the series */
    char	name[64];		/* name of the puzzle */
    cell	map[MAXHEIGHT * MAXWIDTH]; /* the map proper */
} gamesetup;

/* The collection of data maintained for each file of puzzles.
 */
typedef	struct gameseries {
    int		allocated;		/* number of elements allocated */
    int		count;			/* actual size of array */
    gamesetup  *games;			/* the list of puzzles */
    char       *filename;		/* the name of the files */
    FILE       *mapfp;			/* the file containing the puzzles */
    FILE       *answerfp;		/* the file of the user's solutions */
    int		allmapsread;		/* TRUE if mapfp has reached EOF */
    int		allanswersread;		/* TRUE if answerfp has reached EOF */
    int		answersreadonly;	/* TRUE if answerfp is open readonly */
    char	name[64];		/* the series's name */
} gameseries;

/* The path of the directory containing the files of puzzles.
 */
extern char    *datadir;

/* This function differs from fgets() in that it always reads a
 * complete line from the file; any characters beyond what will fit in
 * buf are lost. The return value is the length of the string stored
 * in buf, or -1 if fgets() returned NULL.
 */
extern int getnline(FILE *fp, char *buf, int len);

/* Find the puzzle files and allocate an array of gameseries
 * structures for them.
 */
extern int getseriesfiles(char *filename, gameseries **list, int *count);

/* Read the given puzzle file up to puzzle number level.
 */
extern int readlevelinseries(gameseries *series, int level);

#endif
