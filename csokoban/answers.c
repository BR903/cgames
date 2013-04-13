/* answers.h: Functions for reading and saving puzzle solutions.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	"gen.h"
#include	"csokoban.h"
#include	"dirio.h"
#include	"movelist.h"
#include	"fileread.h"
#include	"play.h"
#include	"answers.h"

/* The directory containing the user's solution files.
 */
char   *savedir = NULL;

/* Read a sequence of movecount moves from fp. Each move is a single
 * character: h, j, k, or l for left, down, up or right. Capital
 * letters indicate that the move also pushes a box. Whitespace is
 * ignored, save that the sequence is expected to end with a newline.
 */
static int readanswer(FILE *fp, dyxlist *moves, int movecount)
{
    dyx	move;
    int	ch = EOF;

    setmovelist(moves, movecount);
    while (movecount && (ch = fgetc(fp)) != EOF) {
	if (isspace(ch))
	    continue;
	switch (ch) {
	  case 'h':	move.yx = -1;	  move.box = FALSE;	break;
	  case 'j':	move.yx = +XSIZE; move.box = FALSE;	break;
	  case 'k':	move.yx = -XSIZE; move.box = FALSE;	break;
	  case 'l':	move.yx = +1;	  move.box = FALSE;	break;
	  case 'H':	move.yx = -1;	  move.box = TRUE;	break;
	  case 'J':	move.yx = +XSIZE; move.box = TRUE;	break;
	  case 'K':	move.yx = -XSIZE; move.box = TRUE;	break;
	  case 'L':	move.yx = +1;	  move.box = TRUE;	break;
	}
	moves->list[--movecount] = move;
    }
    while (ch != EOF && ch != '\n')
	ch = fgetc(fp);
    if (movecount) {
	initmovelist(moves);
	return FALSE;
    }
    return TRUE;
}

/* Write the given list of moves out to fp, inserting a line break
 * after every 64th character.
 */
static int saveanswer(FILE *fp, dyxlist const *moves)
{
    dyx const  *move;
    int		ch, i;

    move = moves->list + moves->count;
    for (i = 0 ; i < moves->count ; ++i) {
	--move;
	if (move->yx < 0)
	    ch = move->yx == -1 ? 'h' : 'k';
	else
	    ch = move->yx == +1 ? 'l' : 'j';
	if (move->box)
	    ch = toupper(ch);
	fputc(ch, fp);
	if ((i & 63) == 63)
	    fputc('\n', fp);
    }
    if (i & 63)
	fputc('\n', fp);
    fflush(fp);
    return TRUE;
}

/* Read the solutions for game, if any, from the current file
 * position, and store them in game as "redo" lists. The actual move
 * sequences are always preceded by a line of the form "N moves, M
 * pushes", where N and M specify the number of moves and pushes
 * contained in the following sequence. If there are two solutions,
 * the first one uses fewer moves, and the second one uses fewer
 * pushes. If there is only one solution, it is used for both. In the
 * case of no solution, the entry will consist of a single line
 * beginning with a dash. The solutions for each puzzle are separated
 * from each other by blank lines and/or lines beginning with a
 * semicolon (i.e., comments).
 */
int readanswers(FILE *fp, gamesetup *game)
{
    char	buf[256];
    int		m, n;

    initmovelist(&game->moveanswer);
    initmovelist(&game->pushanswer);
    if (!fp)
	return TRUE;

    for (;;) {
	n = getline(fp, buf, sizeof buf);
	if (n < 0)
	    return FALSE;
	if (*buf != '\n' && *buf != ';')
	    break;
    }
    if (*buf == '-')
	return TRUE;

    if (sscanf(buf, "%d moves, %d pushes", &n, &m) < 2
			|| !readanswer(fp, &game->moveanswer, n))
	return FALSE;
    game->movebestcount = n;
    game->movebestpushcount = m;

    if (getline(fp, buf, sizeof buf) <= 0
			|| sscanf(buf, "%d moves, %d pushes", &n, &m) < 2
			|| !readanswer(fp, &game->pushanswer, n)) {
	copymovelist(&game->pushanswer, &game->moveanswer);
	game->pushbestcount = game->movebestpushcount;
	game->pushbestmovecount = game->movebestcount;
	return TRUE;
    }
    game->pushbestcount = m;
    game->pushbestmovecount = n;

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
	if (!series->savedirchecked) {
	    series->savedirchecked = TRUE;
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
	fprintf(series->answerfp, ";Level %d\n", i + 1);
	if (!game->moveanswer.count) {
	    fputs("---\n", series->answerfp);
	    continue;
	}
	fprintf(series->answerfp, "%d moves, %d pushes\n",
				  game->movebestcount,
				  game->movebestpushcount);
	saveanswer(series->answerfp, &game->moveanswer);
	if (game->pushbestcount != game->movebestpushcount
		&& game->pushbestmovecount != game->movebestcount) {
	    fprintf(series->answerfp, "%d moves, %d pushes\n",
				      game->pushbestmovecount,
				      game->pushbestcount);
	    saveanswer(series->answerfp, &game->pushanswer);
	}
    }
    if (tbufsize) {
	fgetpos(series->answerfp, &pos);
	if (*tbuf != ';' && *tbuf != '\n')
	    fprintf(series->answerfp, ";Level %d\n", i + 1);
	fwrite(tbuf, 1, tbufsize, series->answerfp);
	fsetpos(series->answerfp, &pos);
    }
    if (tbuf)
	free(tbuf);

    return TRUE;
}
