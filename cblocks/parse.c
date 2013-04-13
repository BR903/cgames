/* parse.c: Functions for parsing the puzzle file syntax.
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
#include	"userio.h"
#include	"fileread.h"
#include	"parse.h"

/* Temporary storage of the information in a puzzle file.
 */
typedef	struct filemapinfo {
    short	ysize;			/* height of the map */
    short	xsize;			/* width of the map */
    int		beststepknown;		/* shortest solution size known */
    char	key;			/* character for the key block */
    char	etch;			/* TRUE if goal should be etched */
    char       *map;			/* the map proper */
    char       *goal;			/* the goal proper */
    char	name[64];		/* name of the puzzle */
    char	charset[256];		/* block chars and equivalencies */
    char	colors[256];		/* block colorings */
} filemapinfo;

/* Read a pictorial map out of the given file, using the dimensions
 * stored in fmi. If fillcharset is TRUE, block characters are added
 * to fmi's charset; otherwise, block characters are required to be
 * listed there already. The return value is the map, or NULL if an
 * error was encountered.
 */
static char *readpicture(FILE *fp, filemapinfo *fmi, int fillcharset)
{
    char	buf[256];
    char       *map;
    char       *p;
    int		ok = TRUE;
    int 	y, x, n;

    if (fmi->ysize <= 0 || fmi->xsize <= 0)
	ok = FALSE;
    if (!(map = malloc(fmi->ysize * fmi->xsize)))
	memerrexit();
    memset(map, ' ', fmi->ysize * fmi->xsize);
    p = map;
    for (y = 0 ; y < fmi->ysize ; ++y) {
	n = getnline(fp, buf, sizeof buf);
	if (n < 0 || *buf == ';')
	    break;
	if (!ok)
	    continue;
	for (x = 0 ; x < n && buf[x] && buf[x] != ';' ; ++x) {
	    if (buf[x] != ' ' && buf[x] != '.') {
		if (buf[x] == '\t') {
		    x |= 7;
		    continue;
		}
		if (x >= fmi->xsize || !isgraph(buf[x])) {
		    ok = FALSE;
		    break;
		}
		p[x] = buf[x];
		if (fillcharset)
		    fmi->charset[(int)buf[x]] = buf[x];
		else if (!fmi->charset[(int)buf[x]])
		    ok = FALSE;
	    }
	}
	p += fmi->xsize;
    }
    if (!ok) {
	free(map);
	map = NULL;
    }
    return map;
}

/* Read a single "line" out of the given file (which can span multiple
 * lines if a map is involved) and store the data provided in fmi.
 * Only simple syntax checks are done on the line. The return value is
 * an error message if an error is found, an empty string if no errors
 * were found, or NULL if the end of the puzzle was reached.
 */
static char const *readmapinfoline(FILE *fp, filemapinfo *fmi)
{
    char	buf[256];
    char       *statement, *args;
    char       *tmp;
    int		r, g, b;
    int		n;

    for (;;) {
	n = getnline(fp, buf, sizeof buf);
	if (n < 0)
	    return NULL;
	if (!n || *buf == ';')
	    continue;
	while (isspace(buf[n - 1]))
	    --n;
	buf[n] = '\0';
	if (n)
	    break;
    }

    for (statement = buf ; isspace(*statement) ; ++statement) ;
    if ((args = strchr(statement, ' ')))
	*args++ = '\0';
    if (!strcmp(statement, "end")) {
	return NULL;
    } else if (!strcmp(statement, "etchtarget")) {
	fmi->etch = TRUE;
    } else if (!strcmp(statement, "display")) {
	if (!args)
	    return "display line with no text";
	for ( ; isspace(*args) ; ++args) ;
	strncpy(fmi->name, args, sizeof fmi->name - 1);
	fmi->name[sizeof fmi->name - 1] = '\0';
    } else if (!strcmp(statement, "size")) {
	if (!args || sscanf(args, "%hd %hd", &fmi->xsize, &fmi->ysize) < 2)
	    return "incorrect parameters to size";
    } else if (!strcmp(statement, "step")) {
	if (!args || sscanf(args, "%d", &fmi->beststepknown) < 1)
	    return "incorrect parameter to step";
    } else if (!strcmp(statement, "key")) {
	if (!args || !*args)
	    return "missing parameter to key line";
	if (!fmi->charset[(int)*args])
	    return "invalid block ID in key line";
	fmi->key = *args;
    } else if (!strcmp(statement, "color")) {
	for ( ; isspace(*args) ; ++args ) ;
	n = *args;
	if (!fmi->charset[n])
	    return "color line for an invalid block ID";
	if (sscanf(args + 1, "%d %d %d", &r, &g, &b) < 3)
	    return FALSE;
	fmi->colors[n] = getrgbindex(r, g, b);
    } else if (!strcmp(statement, "equiv")) {
	if (!args || !*args)
	    return "missing parameters to equiv line";
	n = args[0];
	if (!fmi->charset[(int)args[0]])
	    return "invalid block ID in equiv line";
	while (*++args) {
	    if (!fmi->charset[(int)args[0]])
		return "invalid block ID in equiv line";
	    fmi->charset[(int)args[-1]] = args[0];
	}
	fmi->charset[(int)args[-1]] = n;
    } else if (!strcmp(statement, "initial")) {
	if (!(fmi->map = readpicture(fp, fmi, TRUE)))
	    return "invalid syntax in initial map";
    } else if (!strcmp(statement, "target")) {
	if (!fmi->map)
	    return "the initial section must precede the target section";
	if (!(fmi->goal = readpicture(fp, fmi, FALSE)))
	    return "invalid syntax in target map";
    } else if (!strcmp(statement, "hint")) {
	if (!fmi->map)
	    return "the initial section must precede the hint section";
	if ((tmp = readpicture(fp, fmi, FALSE)))
	    free(tmp);
    } else if (!strcmp(statement, "image")) {
	/* nop */
    } else if (!strcmp(statement, "label")) {
	/* nop */
    } else if (!strcmp(statement, "labeloffset")) {
	/* nop */
    } else
	return "unrecognized line";
    return "";
}

/* Recursively replace all connected occurrences of a block character
 * with the given block ID.
 */
static void fillblockid(cell *pos, cell rawid, cell numid)
{
    *pos &= ~(MARK | BLOCKID_MASK);
    *pos |= numid;
    if ((pos[-XSIZE] & (MARK | BLOCKID_MASK)) == rawid)
	fillblockid(pos - XSIZE, rawid, numid);
    if ((pos[+1] & (MARK | BLOCKID_MASK)) == rawid)
	fillblockid(pos + 1, rawid, numid);
    if ((pos[+XSIZE] & (MARK | BLOCKID_MASK)) == rawid)
	fillblockid(pos + XSIZE, rawid, numid);
    if ((pos[-1] & (MARK | BLOCKID_MASK)) == rawid)
	fillblockid(pos - 1, rawid, numid);
}

/* Join neighboring cells containing part of the same object, where
 * an object can be a wall, a block, a door, or an etched goal.
 */
static void connectcells(gamesetup *game, cell *map)
{
    cell       *p;
    int		y, x, n;

    p = map;
    for (y = 0 ; y < game->ysize ; ++y, p += XSIZE) {
	for (x = 0 ; x < game->xsize ; ++x) {
	    if ((n = blockid(p[x]))) {
		if (y && blockid(p[x - XSIZE]) == n) {
		    p[x] |= EXTENDNORTH;
		    p[x - XSIZE] |= EXTENDSOUTH;
		}
		if (x && blockid(p[x - 1]) == n) {
		    p[x] |= EXTENDWEST;
		    p[x - 1] |= EXTENDEAST;
		}
	    }
	    if ((n = p[x] & (GOAL | DOORSTAMP_MASK))) {
		if (y && (p[x - XSIZE] & n)) {
		    p[x] |= FEXTENDNORTH;
		    p[x - XSIZE] |= FEXTENDSOUTH;
		}
		if (x && (p[x - 1] & n)) {
		    p[x] |= FEXTENDWEST;
		    p[x - 1] |= FEXTENDEAST;
		}
	    }
	}
    }
}

/* Read the specification for a single puzzle out of the given file
 * and initialize the gamesetup structure for that puzzle. Blocks and
 * other objects are identified and located, and the map and goal
 * image are rendered into their final states.
 */
int readlevelmap(FILE *fp, gamesetup *game)
{
    filemapinfo		info = { 0 };
    unsigned char	idset[256] = { 0 };
    char const	       *p1;
    cell	       *p2;
    int			errorcount;
    int			rawid, nextid, id;
    int 		y, x, n;

    memset(game, 0, sizeof *game);
    errorcount = 0;
    while ((p1 = readmapinfoline(fp, &info)) != NULL) {
	if (*p1) {
	    fileerr(p1);
	    ++errorcount;
	}
    }
    if (errorcount || !info.ysize || !info.xsize)
	return FALSE;
    else if (!info.map || !info.goal)
	return fileerr("Puzzle specification contains errors");
    else if (info.ysize > MAXHEIGHT - 2 || info.xsize > MAXWIDTH - 2)
	return fileerr("ignoring map which exceeds maximum dimensions");
    game->ysize = info.ysize + 2;
    game->xsize = info.xsize + 2;
    game->beststepknown = info.beststepknown;
    strcpy(game->name, info.name);
    p2 = game->map;
    for (y = 1, p1 = info.map ; y <= info.ysize ; ++y) {
	p2 += XSIZE;
	for (x = 1 ; x <= info.xsize ; ++x, ++p1) {
	    if (*p1 == '#') {
		p2[x] = WALLID;
		info.goal[p1 - info.map] = '#';
	    }
	    else if (*p1 == '%')
		p2[x] = DOORSTAMP_MASK;
	    else if (*p1 != ' ')
		p2[x] = MARK | *p1;
	}
    }
    nextid = FIRSTID;
    p2 = game->map;
    for (y = 1, p1 = info.map ; y <= info.ysize ; ++y) {
	p2 += XSIZE;
	for (x = 1 ; x <= info.xsize ; ++x, ++p1) {
	    if (!(p2[x] & MARK))
		continue;
	    rawid = blockid(p2[x]);
	    id = rawid == info.key ? KEYID : nextid++;
	    p2[x] &= ~(MARK | BLOCKID_MASK);
	    p2[x] |= id;
	    if (rawid != '$') {
		fillblockid(p2 + x, MARK | rawid, id);
		idset[rawid] = id;
	    }
	}
    }
    game->blockcount = nextid;
    for (n = 0 ; n < (int)(sizeof info.charset) ; ++n) {
	if (idset[n]) {
	    game->equivs[(int)idset[n]] = idset[(int)info.charset[n]];
	    game->colors[(int)idset[n]] = info.colors[n];
	}
    }
    p2 = game->goal;
    for (y = 1, p1 = info.goal ; y <= info.ysize ; ++y) {
	p2 += XSIZE;
	for (x = 1 ; x <= info.xsize ; ++x, ++p1) {
	    if (*p1 == '#')
		p2[x] = WALLID;
	    else if (*p1 == '%')
		p2[x] = DOORSTAMP_MASK;
	    else if (*p1 != ' ') {
		p2[x] = idset[(int)*p1];
		if (info.etch)
		    game->map[p2 + x - game->goal] |= GOAL;
	    }
	}
    }

    for (x = 0 ; x < game->xsize ; ++x)
	game->map[x] = game->goal[x] = WALLID;
    for (y = 1 ; y < game->ysize - 1 ; ++y)
	game->map[y * XSIZE] = game->goal[y * XSIZE]
			     = game->map[y * XSIZE + game->xsize - 1]
			     = game->goal[y * XSIZE + game->xsize - 1]
			     = WALLID;
    for (x = 0 ; x < game->xsize ; ++x)
	game->map[y * XSIZE + x] = game->goal[y * XSIZE + x] = WALLID;

    connectcells(game, game->map);
    connectcells(game, game->goal);

    return TRUE;
}
