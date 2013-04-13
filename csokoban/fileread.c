/* fileread.c: Functions for reading the puzzle files.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"gen.h"
#include	"csokoban.h"
#include	"movelist.h"
#include	"dirio.h"
#include	"userio.h"
#include	"answers.h"
#include	"fileread.h"

/* Mini-structure for our findfiles() callback.
 */
typedef	struct seriesdata {
    gameseries *list;
    int		count;
} seriesdata;

/* The character representations used in the puzzle files, keyed to
 * the numerical values defined in csokoban.h.
 */
static char const *filecells = " " /* EMPTY  */  "." /* GOAL          */
			       "@" /* PLAYER */  "+" /* GOAL + PLAYER */
			       "$" /* BOX    */  "*" /* GOAL + BOX    */
			       " "		 " "
			       "#" /* WALL   */  ;

/* The path of the directory containing the puzzle files.
 */
char   *datadir = NULL;

/* Read one full line from fp and store the first len characters in
 * buf.
 */
int getnline(FILE *fp, char *buf, int len)
{
    int	ch, n;

    if (!fgets(buf, len, fp))
	return -1;
    n = strlen(buf);
    if (n == len - 1 && buf[n] != '\n') {
	do
	    ch = fgetc(fp);
	while (ch != EOF && ch != '\n');
    } else
	buf[n--] = '\0';
    return n;
}

/* Recursively flood-fill an area surrounded by WALLs with an absence
 * of FLOORs.
 */
static void pullflooring(cell *map, yx pos)
{
    if (map[pos] & WALL)
	return;
    map[pos] &= ~FLOOR;
    if (pos >= XSIZE && (map[pos - XSIZE] & (WALL | FLOOR)) == FLOOR)
	pullflooring(map, pos - XSIZE);
    if (pos <= MAXHEIGHT * MAXWIDTH - XSIZE &&
			(map[pos + XSIZE] & (WALL | FLOOR)) == FLOOR)
	pullflooring(map, pos + XSIZE);
    if (pos % XSIZE > 0 && (map[pos - 1] & (WALL | FLOOR)) == FLOOR)
	pullflooring(map, pos - 1);
    if (pos % XSIZE < MAXWIDTH - 1 && (map[pos + 1] & (WALL | FLOOR)) == FLOOR)
	pullflooring(map, pos + 1);
}

/* Add data not explicitly defined in the file's representation.
 * Neighboring WALL cells are "joined" so that they can be drawn in
 * outline, and every cell that is "inside" is given a FLOOR. Finally,
 * invisible WALLs are added around the outer edge (so as to insure
 * that the player is not allowed to leave the map and go walking
 * through the heap).
 */
static int improvemap(gamesetup *game)
{
    cell       *map;
    yx		initpos;
    int 	y, x;

    initpos = 0;
    map = game->map;
    for (y = 0 ; y < game->ysize ; ++y, map += XSIZE) {
	for (x = 0 ; x < game->xsize ; ++x) {
	    if (map[x] & WALL)
		continue;
	    map[x] |= FLOOR;
	    if (map[x] & PLAYER) {
		if (initpos)
		    return fileerr("multiple players in map");
		initpos = &map[x] - game->map;
	    }
	}
    }
    if (!initpos)
	return fileerr("no player in map");

    pullflooring(game->map, 0);

    map = game->map;
    for (y = 1, map += XSIZE ; y < game->ysize - 1 ; ++y, map += XSIZE) {
	for (x = 1 ; x < game->xsize - 1 ; ++x) {
	    if (!(map[x] & WALL))
		continue;
	    if (map[x + XSIZE] & WALL)
		map[x + XSIZE] |= EXTENDNORTH;
	    if (map[x - XSIZE] & WALL)
		map[x - XSIZE] |= EXTENDSOUTH;
	    if (map[x + 1] & WALL)
		map[x + 1] |= EXTENDWEST;
	    if (map[x - 1] & WALL)
		map[x - 1] |= EXTENDEAST;
	}
    }

    map = game->map;
    memset(map, WALL, game->xsize);
    for (y = 1, map += XSIZE ; y < game->ysize - 1 ; ++y, map += XSIZE)
	map[0] = map[game->xsize - 1] = WALL;
    memset(map, WALL, game->xsize);

    return TRUE;
}

/* Read a single map from the current position of fp and use it to
 * initialize the given gamesetup structure. Maps in the file are
 * separated by blank lines and/or lines beginning with a semicolon,
 * equal sign, or single quote. A semicolon appearing inside a line
 * in a map causes the remainder of the line to be ignored.
 */
static int readlevelmap(FILE *fp, gamesetup *game)
{
    char	buf[256];
    char       *p, *q;
    int		badmap = FALSE;
    int		y, x, n, ch;

    game->name[0] = '\0';
    memset(game->map, EMPTY, sizeof game->map);
    game->xsize = 1;

    for (y = 1 ; y < MAXHEIGHT ; ++y) {
	ch = fgetc(fp);
	if (ch == EOF) {
	    if (y > 1)
		break;
	    else
		return FALSE;
	} else if (ch == '\n' || ch == ';' || ch == '=' || ch == '\'') {
	    if (y > 1) {
		ungetc(ch, fp);
		break;
	    }
	    --y;
	    if (ch == '\n')
		continue;
	    n = getnline(fp, buf, sizeof buf);
	    if (n < 0)
		return FALSE;
	    if (ch == ';' && *buf == ';') {
		for (x = 1 ; isspace(buf[x]) ; ++x) ;
		n -= x;
		if (n >= (int)(sizeof game->name))
		    n = sizeof game->name - 1;
		memcpy(game->name, buf + x, n);
		for (--n ; isspace(game->name[n]) ; --n) ;
		game->name[n + 1] = '\0';
	    }
	    continue;
	}
	buf[0] = ch;
	getnline(fp, buf + 1, sizeof buf - 1);
	if (badmap)
	    continue;
	x = 1;
	p = buf;
	for ( ; x < MAXWIDTH && *p && *p != '\n' && *p != ';' ; ++x, ++p) {
	    if (*p == '\t') {
		x |= 7;
		continue;
	    }
	    if (!(q = strchr(filecells, *p))) {
		fileerr("unrecognized character in level");
		badmap = TRUE;
		break;
	    }
	    game->map[y * XSIZE + x] = q - filecells;
	}
	if (game->xsize <= x) {
	    game->xsize = x + 1;
	    if (game->xsize > MAXWIDTH)
		break;
	}
    }

    game->ysize = y + 1;
    if (game->ysize > MAXHEIGHT || game->xsize > MAXWIDTH)
	return fileerr("ignoring map which exceeds maximum dimensions");

    if (!improvemap(game))
	return FALSE;

    game->boxcount = 0;
    game->goalcount = 0;
    game->storecount = 0;
    for (n = 0 ; n < game->ysize * XSIZE ; ++n) {
	if (game->map[n] & PLAYER)
	    game->start = n;
	else if (game->map[n] & BOX) {
	    ++game->boxcount;
	    if (game->map[n] & GOAL)
		++game->storecount;
	}
	if (game->map[n] & GOAL)
	    ++game->goalcount;
    }

    return TRUE;
}

/* Examine the top of the file for series and extract the display
 * string, if present. FALSE is returned if the file appears to be
 * invalid or bereft of maps.
 */
static int readseriesheader(gameseries *series)
{
    char	buf[256];
    int		ch;
    int		n;

    for (;;) {
	ch = fgetc(series->mapfp);
	if (ch == ';') {
	    getnline(series->mapfp, buf, sizeof buf);
	    if (*buf == ';') {
		for (n = 1 ; isspace(buf[n]) ; ++n) ;
		strncpy(series->name, buf + n, sizeof series->name - 1);
		series->name[sizeof series->name - 1] = '\0';
		n = strlen(series->name);
		for (--n ; isspace(series->name[n]) ; --n) ;
		series->name[n + 1] = '\0';
		return TRUE;
	    }
	} else if (ch != '\n')
	    break;
    }

    if (ch == EOF) {
	fclose(series->mapfp);
	series->mapfp = NULL;
	series->allmapsread = TRUE;
	return FALSE;
    }
    ungetc(ch, series->mapfp);
    return TRUE;
}

/* A callback function that initializes a gameseries structure for
 * filename and adds it to the list stored under the second argument.
 */
static int getseriesfile(char *filename, void *data)
{
    static int	allocated = 0;
    seriesdata *sdata = (seriesdata*)data;
    gameseries *series;

    while (sdata->count >= allocated) {
	++allocated;
	if (!(sdata->list = realloc(sdata->list,
				    allocated * sizeof *sdata->list)))
	    memerrexit();
    }
    series = sdata->list + sdata->count++;
    series->filename = filename;
    series->mapfp = NULL;
    series->answerfp = NULL;
    series->allmapsread = FALSE;
    series->allanswersread = FALSE;
    series->allocated = 0;
    series->count = 0;
    series->games = NULL;
    *series->name = '\0';
    return 1;
}

/* A callback function to compare two gameseries structures by
 * comparing their filenames.
 */
static int gameseriescmp(void const *a, void const *b)
{
    return strcmp(((gameseries*)a)->filename, ((gameseries*)b)->filename);
}

/* Search the game file directory and generate an array of gameseries
 * structures corresponding to the puzzle files found there.
 */
int getseriesfiles(char *filename, gameseries **list, int *count)
{
    seriesdata	s;

    s.list = NULL;
    s.count = 0;
    if (*filename && isfilename(filename)) {
	if (getseriesfile(filename, &s) <= 0 || !s.count)
	    die("Couldn't access \"%s\"", filename);
	*datadir = '\0';
	*savedir = '\0';
    } else {
	if (!findfiles(datadir, &s, getseriesfile) || !s.count)
	    die("Couldn't find any data files in \"%s\".", datadir);
	if (s.count > 1)
	    qsort(s.list, s.count, sizeof *s.list, gameseriescmp);
    }
    *list = s.list;
    *count = s.count;
    return TRUE;
}

/* Read the puzzle file corresponding to series, and the corresponding
 * solutions in the answer file, until at least level maps have been
 * successfully parsed, or the end is reached. The files are opened if
 * they have not been already. No files are opened if the requested
 * level has already been loaded into memory.
 */
int readlevelinseries(gameseries *series, int level)
{
    int	n;

    if (level < 0)
	return FALSE;
    if (series->count > level)
	return TRUE;

    if (!series->allmapsread) {
	if (!series->mapfp) {
	    currentfilename = series->filename;
	    series->mapfp = openfileindir(datadir, series->filename, "r");
	    if (!series->mapfp)
		return fileerr(NULL);
	    if (!readseriesheader(series))
		return fileerr("file contains no maps");
	    if (*savedir) {
		series->answerfp = openfileindir(savedir, 
						 series->filename, "r");
		if (series->answerfp) {
		    savedirchecked = TRUE;
		    series->answersreadonly = TRUE;
		}
	    } else
		series->answerfp = NULL;
	}
	while (!series->allmapsread && series->count <= level) {
	    while (series->count >= series->allocated) {
		n = series->allocated ? series->allocated * 2 : 16;
		if (!(series->games = realloc(series->games,
					      n * sizeof *series->games)))
		    memerrexit();
		memset(series->games + series->allocated, 0,
		       (n - series->allocated) * sizeof *series->games);
		series->allocated = n;
	    }
	    if (readlevelmap(series->mapfp, series->games + series->count)) {
		series->games[series->count].seriesname = series->name;
		if (!series->allanswersread)
		    readanswers(series->answerfp,
				series->games + series->count);
		++series->count;
	    }
	    if (feof(series->mapfp)) {
		fclose(series->mapfp);
		series->mapfp = NULL;
		series->allmapsread = TRUE;
	    }
	    if (series->answerfp && feof(series->answerfp))
		series->allanswersread = TRUE;
	}
    }
    return series->count > level;
}
