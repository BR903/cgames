/* answers.c: Functions for reading and saving puzzle solutions.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"gen.h"
#include	"cblocks.h"
#include	"dirio.h"
#include	"movelist.h"
#include	"fileread.h"
#include	"play.h"
#include	"answers.h"

/* The directory containing the user's solution files.
 */
char   *savedir = NULL;

/* FALSE if savedir's existence is unverified.
 */
int	savedirchecked = FALSE;

/* Read a sequence of movecount moves from fp. Moves are grouped into
 * steps. Each step begins with the coordinates of the top-left cell
 * of the block to be moved, followed by a sequence of the characters
 * h, j, k, and l for left, down, up and right.
 */
static int readanswer(FILE *fp, cell const *startingmap,
		      actlist *moves, int movecount)
{
    static cell	map[MAXWIDTH * MAXHEIGHT];
    action	move;
    int		ch = EOF;
    int		y, x, n, r;

    setmovelist(moves, movecount);
    memcpy(map, startingmap, sizeof map);
    n = movecount;
    while (n && fscanf(fp, "%d,%d:", &y, &x) == 2) {
	move.y = y;
	move.x = x;
	move.id = blockid(map[y * XSIZE + x]);
	while (n) {
	    do {
		ch = fgetc(fp);
		if (ch == EOF)
		    return FALSE;
	    } while (isspace(ch));
	    if (isdigit(ch)) {
		ungetc(ch, fp);
		break;
	    }
	    switch (ch) {
	      case 'h':	move.dir = WEST;  --x;	break;
	      case 'j':	move.dir = SOUTH; ++y;	break;
	      case 'k':	move.dir = NORTH; --y;	break;
	      case 'l':	move.dir = EAST;  ++x;	break;
	    }
	    moves->list[--n] = move;
	    move.y = y;
	    move.x = x;
	}
	map[y * XSIZE + x] = move.id;
    }

    r = TRUE;
    while (ch != EOF && ch != '\n') {
	if (ch == '.')
	    r = FALSE;
	ch = fgetc(fp);
    }
    if (n) {
	initmovelist(moves);
	return FALSE;
    }
    return r;
}

/* Write the given list of moves out to fp. Line breaks are inserted
 * where possible to avoid going past the 72nd column.
 */
static int saveanswer(FILE *fp, actlist const *moves, int inc)
{
    static char const	dir[] = { 'k', 'l', 'j', 'h' };
    action const       *move;
    char		buf[256];
    char	       *p;
    int			lastid, xpos, firstentry;
    int			i;

    lastid = -1;
    firstentry = TRUE;
    p = buf;
    xpos = 0;
    move = moves->list + moves->count;
    for (i = 0 ; i < moves->count ; ++i) {
	--move;
	if (move->id != lastid) {
	    if (!firstentry) {
		if (xpos + (p - buf) >= 72) {
		    fputc('\n', fp);
		    xpos = 0;
		} else {
		    fputc(' ', fp);
		    ++xpos;
		}
	    }
	    if (p > buf) {
		fwrite(buf, 1, p - buf, fp);
		xpos += p - buf;
		p = buf;
		firstentry = FALSE;
	    }
	    p += sprintf(buf, "%d,%d:", move->y, move->x);
	    lastid = move->id;
	}
	*p++ = dir[move->dir];
    }
    if (p > buf) {
	if (xpos + (p - buf) >= 72)
	    fputc('\n', fp);
	else if (!firstentry)
	    fputc(' ', fp);
	fwrite(buf, 1, p - buf, fp);
    }
    if (inc)
	fwrite(" ...", 1, 4, fp);
    fputc('\n', fp);
    fflush(fp);
    return TRUE;
}

/* Read the solutions for game, if any, from the current file
 * position, and store them in game as "redo" lists. The actual move
 * sequences are always preceded by a line of the form "N moves, M
 * steps", where N and M specify the number of moves and steps
 * contained in the following sequence. If there are two solutions,
 * the first one uses fewer moves, and the second one uses fewer
 * steps. If there is only one solution, it is used for both. In the
 * case of no solution, the entry will consist of a single line
 * beginning with a dash. The solutions for each puzzle are separated
 * from each other by blank lines and/or lines beginning with a
 * semicolon (i.e., comments).
 */
int readanswers(FILE *fp, gamesetup *game)
{
    char	buf[256];
    int		n;

    initmovelist(&game->answer);
    if (!fp)
	return TRUE;

    for (;;) {
	n = getnline(fp, buf, sizeof buf);
	if (n < 0)
	    return FALSE;
	if (*buf != '\n' && *buf != ';')
	    break;
    }
    if (*buf == '-')
	return TRUE;

    if (sscanf(buf, "%d steps, %d moves", &game->beststepcount, &n) < 2
			|| !readanswer(fp, game->map, &game->answer, n)) {
	game->beststepcount = 0;
	return FALSE;
    }

    return TRUE;
}

/* Write out all the solutions for series. Since each file contains
 * solutions for all the puzzles in one series, saving a new solution
 * requires that the function create the entire file's contents
 * afresh. In the case where only part of the original file has been
 * parsed, the unexamined contents are copied verbatim into a
 * temporary memory buffer, and then appended to the new file.
 */
int saveanswers(gameseries *series)
{
    gamesetup  *game;
    char       *tbuf = NULL;
    int		tbufsize = 0;
    fpos_t	pos;
    int		i, n;

    if (!*savedir)
	return TRUE;

    currentfilename = series->filename;

    if (series->answerfp && !series->allanswersread) {
	n = 0;
	while (!feof(series->answerfp)) {
	    if (tbufsize >= n) {
		n = n ? n * 2 : BUFSIZ;
		if (!(tbuf = realloc(tbuf, n)))
		    memerrexit();
		i = fread(tbuf + tbufsize, 1, n - tbufsize, series->answerfp);
		if (i <= 0) {
		    if (i < 0)
			fileerr(NULL);
		    break;
		}
		tbufsize += i;
	    }
	}
    }

    if (!series->answerfp || series->answersreadonly) {
	if (!savedirchecked) {
	    savedirchecked = TRUE;
	    if (!finddir(savedir)) {
		currentfilename = savedir;
		return fileerr(NULL);
	    }
	}
	if (series->answerfp)
	    fclose(series->answerfp);
	else
	    series->allanswersread = TRUE;
	series->answerfp = openfileindir(savedir, series->filename, "w+");
	if (!series->answerfp) {
	    *savedir = '\0';
	    return fileerr(NULL);
	}
    }

    fseek(series->answerfp, 0, SEEK_SET);
    for (i = 0, game = series->games ; i < series->count ; ++i, ++game) {
	fprintf(series->answerfp, "; Puzzle %d\n", i + 1);
	if (!game->answer.count) {
	    fputs("---\n", series->answerfp);
	    continue;
	}
	fprintf(series->answerfp, "%d steps, %d moves\n",
				  game->beststepcount,
				  game->answer.count);
	saveanswer(series->answerfp, &game->answer, game->beststepcount == 0);
    }
    if (tbufsize) {
	fgetpos(series->answerfp, &pos);
	if (*tbuf != ';' && *tbuf != '\n')
	    fprintf(series->answerfp, "; Puzzle %d\n", i + 1);
	fwrite(tbuf, 1, tbufsize, series->answerfp);
	fsetpos(series->answerfp, &pos);
    }
    if (tbuf)
	free(tbuf);

    return TRUE;
}
