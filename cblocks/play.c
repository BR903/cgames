/* play.c: Functions for changing the state of the game.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<string.h>
#include	"gen.h"
#include	"cblocks.h"
#include	"userio.h"
#include	"play.h"

/* The bits of a cell that move with a block.
 */
#define	BLOCK_MASK	(BLOCKID_MASK | EXTENDNORTH | EXTENDEAST	\
				      | EXTENDSOUTH | EXTENDWEST)

/* Set the timestamp on a door cell.
 */
#define	stampdoor(c, m)	((cell)(((c) & ~DOORSTAMP_MASK)	\
				| (((m) << 16) & DOORSTAMP_MASK)))

/* One entry on the saved-state stack.
 */
typedef	struct gamestack gamestack;
struct gamestack {
    gamestack  *next;		/* pointer to the next entry */
    gamestate	state;		/* the saved state */
};

/* Arrays for translating a direction into deltas.
 */
static int const dirdelta[] = { -XSIZE, +1, +XSIZE, -1 };
static int const dirydelta[] = { -1, 0, +1, 0 };
static int const dirxdelta[] = { 0, +1, 0, -1 };

/* The stack of saved states.
 */
static gamestack       *stack = NULL;

/* The current state of the current game.
 */
static gamestate	state;

/* Make a copy of from that shares no memory with to.
 */
static void copygamestate(gamestate *to, gamestate const *from)
{
    *to = *from;
    copymovelist(&to->undo, &from->undo);
    copymovelist(&to->redo, &from->redo);
}

/* Free allocated memory associated with s.
 */
static void freegamestate(gamestate *s)
{
    destroymovelist(&s->undo);
    destroymovelist(&s->redo);
}

/* Initialize the current state to the starting position of the
 * current puzzle, and reset the macro array and the stack.
 */
void initgamestate(void)
{
    memcpy(state.map, state.game->map, sizeof state.map);
    state.currblock = state.game->equivs[KEYID] ? KEYID : FIRSTID;
    state.ycurrpos = state.xcurrpos = 0;
    initmovelist(&state.undo);
    copymovelist(&state.redo, &state.game->answer);
    state.movecount = 0;
    state.stepcount = 0;
}

/* Set the current puzzle to be game, with the given level number.
 */
void selectgame(gamesetup *game, int level)
{
    state.game = game;
    state.level = level;
}

/*
 * Movement support functions
 */

/* Take a vector from (x0, y0) to (x, y) and return the two orthogonal
 * directions it decomposes into, with the larger one given first.
 */
static void raydirections(int y0, int x0, int y, int x, int *dir1, int *dir2)
{
    int	dy, dx;
    int	ydir, xdir;

    dy = y - y0;
    dx = x - x0;
    if (dy > 0) {
	if (dx > 0) {
	    ydir = SOUTH;
	    xdir = EAST;
	} else if (dx < 0) {
	    ydir = SOUTH;
	    xdir = WEST;
	    dx = -dx;
	} else {
	    *dir1 = SOUTH;
	    *dir2 = -1;
	    return;
	}
    } else if (dy < 0) {
	if (dx > 0) {
	    ydir = NORTH;
	    xdir = EAST;
	} else if (dx < 0) {
	    ydir = NORTH;
	    xdir = WEST;
	    dx = -dx;
	} else {
	    *dir1 = NORTH;
	    *dir2 = -1;
	    return;
	}
	dy = -dy;
    } else {
	if (dx > 0)
	    *dir1 = EAST;
	else if (dx < 0)
	    *dir1 = WEST;
	else
	    *dir1 = -1;
	*dir2 = -1;
	return;
    }

    if (dy > dx) {
	*dir1 = ydir;
	*dir2 = xdir;
    } else {
	*dir1 = xdir;
	*dir2 = ydir;
    }
}

/* Fill in an action structure, using the current location of block id
 * to fill in the x-y fields.
 */
static action makeaction(int id, int dir)
{
    cell const *map;
    action	move;
    int 	y, x;

    move.id = id;
    move.dir = dir;
    move.door = FALSE;
    map = state.map;
    for (y = 1, map += XSIZE ; y < state.game->ysize - 1 ; ++y, map += XSIZE) {
	for (x = 1 ; x < state.game->xsize - 1 ; ++x) {
	    if (blockid(map[x]) == id) {
		move.y = y;
		move.x = x;
		return move;
	    }
	}
    }
    return move;
}

/* Given block id, return the first block that lies next to it in the
 * direction defined by qmin, qmax, qinc, and qmul. pmin, pmax, pinc,
 * and pmul define the perpendicular direction, which is scanned first
 * to find the leading edge of the original block. This edge is then
 * advanced in the q direction until a block is found.
 */
static int shiftfromblock(short pmin, short pmax, short pinc, short pmul,
			  short qmin, short qmax, short qinc, short qmul,
			  int id)
{
    short	edges[MAXWIDTH > MAXHEIGHT ? MAXWIDTH : MAXHEIGHT];
    short	p, q;
    int		n;

    for (p = pmin ; p != pmax ; p += pinc) {
	edges[p] = qmax;
	for (q = qmin ; q != qmax ; q += qinc)
	    if (blockid(state.map[p * pmul + q * qmul]) == id)
		edges[p] = q + qinc;
    }

    do {
	n = 0;
	for (p = pmin ; p != pmax ; p += pinc) {
	    if (edges[p] != qmax) {
		++n;
		id = blockid(state.map[p * pmul + edges[p] * qmul]);
		if (id && id != WALLID)
		    return id;
		edges[p] += qinc;
	    }
	}
    } while (n);

    return -1;
}

/* Return TRUE if block id can move in direction dir.
 */
static int canmove(int id, int dir)
{
    cell const *map;
    int		d = dirdelta[dir];
    int		dy = dirydelta[dir];
    int		dx = dirxdelta[dir];
    int		y, x, n;

    map = state.map;
    for (y = 1, map += XSIZE ; y < state.game->ysize - 1 ; ++y, map += XSIZE) {
	for (x = 1 ; x < state.game->xsize - 1 ; ++x) {
	    if (blockid(map[x]) != id)
		continue;
	    if (y + dy < 1 || x + dx < 1 || y + dy >= state.game->ysize - 1
					 || x + dx >= state.game->xsize - 1)
		return FALSE;
	    n = blockid(map[x + d]);
	    if (n && n != id)
		return FALSE;
	    if (doortime(map[x + d]) > state.movecount && id != KEYID)
		return FALSE;
	}
    }
    return TRUE;
}

/* Change the map by moving block id in direction dir.
 */
static int moveblock(int id, int dir)
{
    cell       *map;
    int		d = dirdelta[dir];
    int 	y, x, r;

    r = FALSE;
    if (d < 0) {
	map = state.map + XSIZE;
	for (y = 1 ; y < state.game->ysize ; ++y, map += XSIZE) {
	    for (x = 1 ; x < state.game->xsize ; ++x) {
		if (blockid(map[x]) != id)
		    continue;
		map[x + d] |= map[x] & BLOCK_MASK;
		map[x] &= ~BLOCK_MASK;
		if (id == KEYID) {
		    if (doortime(map[x + d]) > state.movecount + 1) {
			map[x + d] = stampdoor(map[x + d],
					       state.movecount + 1);
			r = TRUE;
		    }
		}
	    }
	}
    } else {
	map = state.map + (state.game->ysize - 1) * XSIZE;
	for (y = state.game->ysize - 1 ; y > 0 ; --y, map -= XSIZE) {
	    for (x = state.game->xsize - 1 ; x > 0 ; --x) {
		if (blockid(map[x]) != id)
		    continue;
		map[x + d] |= map[x] & BLOCK_MASK;
		map[x] &= ~BLOCK_MASK;
		if (id == KEYID) {
		    if (doortime(map[x + d]) > state.movecount + 1) {
			map[x + d] = stampdoor(map[x + d],
					       state.movecount + 1);
			r = TRUE;
		    }
		}
	    }
	}
    }
    return r;
}

/* Apply a legal move to the current state, adding it to the undo list.
 */
static void domove(action move)
{
    move.door = moveblock(move.id, move.dir);
    state.currblock = move.id;
    state.ycurrpos = state.xcurrpos = 0;
    ++state.movecount;
    if (!state.undo.count ||
		move.id != state.undo.list[state.undo.count - 1].id)
	++state.stepcount;
    addtomovelist(&state.undo, move);
}

/* Reset the timestamps on all door cells not currently open.
 */
static void resetdoors(void)
{
    cell       *map;
    int		y, x;

    map = state.map;
    for (y = 1, map += XSIZE ; y < state.game->ysize - 1 ; ++y, map += XSIZE)
	for (x = 1 ; x < state.game->xsize - 1 ; ++x)
	    if (doortime(map[x]) > state.movecount)
		map[x] |= DOORSTAMP_MASK;
}

/*
 * Exported movement functions
 */

int movecursor(int dir)
{
    cell const *map;
    int		ypos = 0, xpos = 0, y, x;

    if (state.ycurrpos) {
	ypos = state.ycurrpos;
	xpos = state.xcurrpos;
    } else if (state.currblock) {
	map = state.map + XSIZE;
	for (y = 1 ; !ypos && y < state.game->ysize - 1 ; ++y, map += XSIZE) {
	    for (x = 1 ; !xpos && x < state.game->xsize - 1 ; ++x) {
		if (blockid(map[x]) == state.currblock) {
		    ypos = y;
		    xpos = x;
		}
	    }
	}
    }
    if (!ypos || !xpos)
	return FALSE;

    ypos += dirydelta[dir];
    xpos += dirxdelta[dir];
    if (ypos < 1 || xpos < 1 || ypos >= state.game->ysize - 1
			     || xpos >= state.game->xsize - 1)
	return FALSE;

    state.currblock = blockid(state.map[ypos * XSIZE + xpos]);
    state.ycurrpos = ypos;
    state.xcurrpos = xpos;
    return TRUE;
}

/* Unapply the last move on the undo list, reversing what was done in
 * domove() and adding the move to the redo list.
 */
int undomove(void)
{
    action	move;

    if (!state.undo.count)
	return FALSE;

    move = state.undo.list[--state.undo.count];
    addtomovelist(&state.redo, move);
    moveblock(move.id, backwards(move.dir));
    state.currblock = move.id;
    state.ycurrpos = state.xcurrpos = 0;
    --state.movecount;
    if (!state.undo.count ||
		move.id != state.undo.list[state.undo.count - 1].id)
	--state.stepcount;
    if (move.door)
	resetdoors();

    return TRUE;
}

/* Undo the last n moves.
 */
int undomoves(int n)
{
    if (!state.undo.count)
	return FALSE;
    while (n-- && undomove()) ;
    return TRUE;
}

/* Undo all of the last moves that were part of one step.
 */
int undostep(void)
{
    int	id, n;

    if (!state.undo.count)
	return FALSE;
    n = state.undo.count;
    id = state.undo.list[n - 1].id;
    for ( ; n > 0 && state.undo.list[n - 1].id == id ; --n) ;
    return undomoves(state.undo.count - n);
}

/* Redo the last undone move.
 */
int redomove(void)
{
    if (!state.redo.count)
	return FALSE;
    domove(state.redo.list[--state.redo.count]);
    return TRUE;
}

/* Redo the last n moves.
 */
int redomoves(int n)
{
    if (!state.redo.count)
	return FALSE;
    while (n-- && redomove()) ;
    return TRUE;
}

/* Redo all of the last moves that were part of one step.
 */
int redostep(void)
{
    int	id, n;

    if (!state.redo.count)
	return FALSE;
    n = state.redo.count;
    id = state.redo.list[n - 1].id;
    for ( ; n > 0 && state.redo.list[n - 1].id == id ; --n) ;
    return redomoves(state.redo.count - n);
}

/* Check a move for validity in the current state. If it is valid, it
 * is applied via domove(), otherwise return FALSE. If the move is
 * equivalent to an undo or a redo, then use that instead; otherwise,
 * the redo list is reset.
 */
int newmove(int dir)
{
    action	move;

    if (!state.currblock || !canmove(state.currblock, dir))
	return FALSE;
    if (state.undo.count) {
	move = state.undo.list[state.undo.count - 1];
	if (move.id == state.currblock && move.dir == backwards(dir)
				       && !move.door)
	    return undomove();
    }
    if (state.redo.count) {
	move = state.redo.list[state.redo.count - 1];
	if (move.id == state.currblock && move.dir == dir)
	    return redomove();
    }
    domove(makeaction(state.currblock, dir));
    state.redo.count = 0;
    return TRUE;
}

/* Change the current block, cycling through the list of block IDs.
 */
void rotatefromcurrblock(void)
{
    cell const *map;
    int		y, x;
    int		id, n;

    if (!state.currblock)
	return;
    id = state.currblock + 256;
    map = state.map;
    for (y = 1, map += XSIZE ; y < state.game->ysize - 1 ; ++y, map += XSIZE) {
	for (x = 1 ; x < state.game->xsize - 1 ; ++x) {
	    n = blockid(map[x]);
	    if (n && n != WALLID && n != state.currblock) {
		if (n < state.currblock)
		    n += 256;
		if (n < id)
		    id = n;
	    }
	}
    }
    state.currblock = id % 256;
    state.ycurrpos = state.xcurrpos = 0;
}

/* Change the current block to the block that most nearly lies next to
 * it in the given direction.
 */
int shiftfromcurrblock(int dir)
{
    int	id;

    if (!state.currblock)
	return FALSE;

    if (dir == NORTH)
	id = shiftfromblock(state.game->xsize - 1, -1, -1, 1,
			    state.game->ysize - 1, -1, -1, XSIZE,
			    state.currblock);
    else if (dir == EAST)
	id = shiftfromblock(0, state.game->ysize, +1, XSIZE,
			    0, state.game->xsize, +1, 1,
			    state.currblock);
    else if (dir == SOUTH)
	id = shiftfromblock(0, state.game->xsize, +1, 1,
			    0, state.game->ysize, +1, XSIZE,
			    state.currblock);
    else if (dir == WEST)
	id = shiftfromblock(state.game->ysize - 1, -1, -1, XSIZE,
			    state.game->xsize - 1, -1, -1, 1,
			    state.currblock);
    else
	id = -1;

    if (id < 0)
	return FALSE;
    state.currblock = id;
    state.ycurrpos = state.xcurrpos = 0;
    return TRUE;
}

/*
 * State-saving functions
 */

/* Save the current state of the game on a stack.
 */
void savestate(void)
{
    gamestack  *save;

    if (!(save = malloc(sizeof *save)))
	memerrexit();
    copygamestate(&save->state, &state);
    save->next = stack;
    stack = save;
}

/* Replace the current state with the last saved state.
 */
int restorestate(void)
{
    gamestack  *next;

    if (!stack)
	return FALSE;
    freegamestate(&state);
    state = stack->state;
    next = stack->next;
    free(stack);
    stack = next;
    return TRUE;
}

/* Discard all saved states from the stack.
 */
void freesavedstates(void)
{
    gamestack  *next;

    while (stack) {
	freegamestate(&stack->state);
	next = stack->next;
	free(stack);
	stack = next;
    }
}

/*
 * Miscellaneous functions
 */

/* Print the current map to stdout.
 */
static void outputmapstate(void)
{
    static char	charids[32] = "ONBHUEZXDQ8MWKRGAVS523694PFTYCL7";
    cell const *map;
    int		spaces, id;
    int		y, x;
    char	obj;

    if (state.stepcount)
	printf("== step %d\n", state.stepcount);
    map = state.map;
    for (y = 0 ; y < state.game->ysize ; ++y, map += XSIZE) {
	spaces = 0;
	for (x = 0 ; x < state.game->xsize ; ++x) {
	    id = blockid(map[x]);
	    if (id) {
		obj = id == WALLID ? '#' : id == KEYID
				   ? '0' : charids[(id - FIRSTID) % 32];
	    } else if ((map[x] & DOORSTAMP_MASK) &&
				doortime(map[x]) > state.movecount + 1) {
		obj = '%';
	    } else {
		++spaces;
		continue;
	    }
	    printf("%*s%c", spaces, "", obj);
	    spaces = 0;
	}
	putchar('\n');
    }
}

/* Print to stdout a series of images of the map as the moves of a
 * user's solution are applied.
 */
int displaygamesolution(void)
{
    action     *move;
    int		lastid = -1;
    int		i;

    if (!state.redo.count)
	return FALSE;
    move = state.redo.list + state.redo.count;
    for (i = 0 ; i < state.redo.count ; ++i) {
	--move;
	if (move->id != lastid) {
	    lastid = move->id;
	    outputmapstate();
	}
	domove(*move);
    }
    outputmapstate();
    return TRUE;
}

/* Return TRUE if the current map is equivalent to the goal.
 */
int checkfinished(void)
{
    int	i, j, k;
    int	y, x, n;

    for (y = 1, n = XSIZE ; y < state.game->ysize - 1 ; ++y, n += XSIZE) {
	for (x = 1 ; x < state.game->xsize - 1 ; ++x) {
	    if (!state.game->goal[n + x])
		continue;
	    i = blockid(state.game->goal[n + x]);
	    if (i == WALLID)
		continue;
	    j = blockid(state.map[n + x]);
	    if (!j)
		return FALSE;
	    if (i != j) {
		k = state.game->equivs[i];
		while (k != j) {
		    if (k == i)
			return FALSE;
		    k = state.game->equivs[k];
		}
	    }
	}
    }
    return TRUE;
}

/* Display the current game state to the user.
 */
int drawscreen(int index)
{
    return displaygame(state.map, state.game->ysize, state.game->xsize,
		       state.game->seriesname, state.game->name, index + 1,
		       state.game->colors, state.currblock,
		       state.ycurrpos, state.xcurrpos,
		       !!stack, state.movecount, state.stepcount,
		       state.game->beststepcount, state.game->answer.count,
		       state.game->beststepknown);
}

/* Display the puzzle's goal.
 */
void displaygoal(void)
{
    static cell	savedmap[MAXHEIGHT * MAXWIDTH];
    int		currblock, ycurrpos, xcurrpos;

    memcpy(savedmap, state.map, sizeof savedmap);
    memcpy(state.map, state.game->goal, sizeof state.map);
    currblock = state.currblock;
    ycurrpos = state.ycurrpos;
    xcurrpos = state.xcurrpos;
    state.currblock = state.ycurrpos = state.xcurrpos = 0;
    drawscreen(-1);
    state.currblock = currblock;
    state.ycurrpos = ycurrpos;
    state.xcurrpos = xcurrpos;
    memcpy(state.map, savedmap, sizeof savedmap);
}

/* Handle commands from the mouse. While the mouse is being dragged,
 * the function attempts to move the current block towards the cursor.
 */
int mousecallback(int y, int x, int mstate)
{
    static int	startpos, lastpos;
    static int	startmovecount = -1;
    int		pos, dir, altdir;
    int		retval, n;

    if (mstate == -2)
	return startmovecount < 0 ? 'X' : 0;
    else if (mstate == +2)
	return 0;

    pos = y * XSIZE + x;
    if (mstate == -1) {
	if (y < 1 || x < 1 || y >= state.game->ysize - 1
			   || x >= state.game->xsize - 1)
	    return 0;
	n = blockid(state.map[pos]);
	if (!n || n == WALLID)
	    return 0;
	state.currblock = n;
	state.ycurrpos = state.xcurrpos = 0;
	startpos = lastpos = pos;
	startmovecount = state.undo.count;
	return '\f';
    }

    if (startmovecount < 0)
	return 0;

    if (y < 0 || x < 0 || y >= state.game->ysize || x >= state.game->xsize) {
	if (startmovecount < state.undo.count) {
	    undomoves(state.undo.count - startmovecount);
	    lastpos = startpos;
	}
	if (mstate == +1)
	    startmovecount = -1;
	return '\f';
    }

    retval = 0;
    while (pos != lastpos) {
	raydirections(lastpos / XSIZE, lastpos % XSIZE, y, x, &dir, &altdir);
	if (dir < 0 || !canmove(state.currblock, dir)) {
	    if (altdir < 0 || !canmove(state.currblock, altdir))
		dir = -1;
	    else
		dir = altdir;
	}
	if (dir < 0 || !newmove(dir))
	    break;
	lastpos += dirdelta[dir];
	retval = '\f';
    }

    if (mstate == +1)
	startmovecount = -1;
    return retval;
}

/* Compare the solution currently sitting in the undo list with the
 * user's best solutions (if any). If this solution beats what's
 * there, replace them. If this solution has the same number of moves
 * as the least-moves solution, but fewer steps, then the replacement
 * will be done, and likewise for the least-steps solution. Note that
 * the undo list contains the moves in backwards order, so the list
 * needs to be reversed when it is copied. TRUE is returned if any
 * solution was replaced.
 */
int replaceanswer(int saveinc)
{
    int	i;

    if (state.game->beststepcount) {
	if (saveinc)
	    return FALSE;
	if (state.stepcount > state.game->beststepcount
		|| (state.stepcount == state.game->beststepcount
			&& state.movecount >= state.game->answer.count))
	    return FALSE;
	state.game->beststepcount = state.stepcount;
    } else {
	if (!saveinc)
	    state.game->beststepcount = state.stepcount;
    }

    initmovelist(&state.game->answer);
    i = state.undo.count;
    while (i--)
	addtomovelist(&state.game->answer, state.undo.list[i]);

    return TRUE;
}
