/* play.c: Functions for changing the state of the game.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<string.h>
#include	"gen.h"
#include	"csokoban.h"
#include	"userio.h"
#include	"play.h"

/* One entry on the saved-state stack.
 */
typedef	struct gamestack gamestack;
struct gamestack {
    gamestack  *next;		/* pointer to the next entry */
    gamestate	state;		/* the saved state */
};

/* The stack of saved states.
 */
static gamestack       *stack = NULL;

/* The array of macros.
 */
static dyxlist		macros[MAXHEIGHT * MAXWIDTH];

/* The macro currently being recorded or played back.
 */
static dyxlist	       *macro = NULL;

/* TRUE while a macro is being recorded.
 */
static int		recording = FALSE;

/* The number of moves executed from the macro currently being played
 * back.
 */
static int		macroplay = -1;

/* The current state of the current game.
 */
static gamestate	state;

/*
 * Game state handling functions
 */

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
void initgamestate(int usemoves)
{
    int	i;

    memcpy(state.map, state.game->map, sizeof state.map);
    state.player = state.game->start;
    initmovelist(&state.undo);
    if (!state.game->moveanswer.count)
	copymovelist(&state.redo, &state.game->pushanswer);
    else if (!state.game->pushanswer.count || usemoves)
	copymovelist(&state.redo, &state.game->moveanswer);
    else
	copymovelist(&state.redo, &state.game->pushanswer);
    state.movecount = 0;
    state.pushcount = 0;
    state.storecount = state.game->storecount;
    recording = FALSE;
    for (i = 0 ; i < (int)(sizeof macros / sizeof *macros) ; ++i)
	if (macros[i].count)
	    macros[i].count = 0;
}

/* Set the current puzzle to be game, with the given level number.
 */
void selectgame(gamesetup *game, int level)
{
    state.game = game;
    state.level = level;
}

/*
 * Basic movement functions
 */

/* Apply a legal move to the current state, adding it to the undo list
 * and any macro being recorded. (This function contains the actual
 * sokoban game logic. Everything else in this program is just
 * housekeeping.)
 */
static void domove(dyx move)
{
    yx	j;

    state.map[state.player] &= ~PLAYER;
    state.player += move.yx;
    state.map[state.player] |= PLAYER;
    ++state.movecount;
    if (move.box) {
	j = state.player + move.yx;
	state.map[state.player] &= ~BOX;
	state.map[j] |= BOX;
	if (state.map[state.player] & GOAL)
	    --state.storecount;
	if (state.map[j] & GOAL)
	    ++state.storecount;
	++state.pushcount;
    }

    addtomovelist(&state.undo, move);
    if (recording)
	addtomovelist(macro, move);
}

/* Unapply the last move on the undo list, reversing what was done in
 * domove() and adding the move to the redo list.
 */
int undomove(void)
{
    dyx	move;
    yx	j;

    if (!state.undo.count)
	return FALSE;

    move = state.undo.list[--state.undo.count];
    addtomovelist(&state.redo, move);
    if (move.box) {
	j = state.player + move.yx;
	state.map[j] &= ~BOX;
	state.map[state.player] |= BOX;
	if (state.map[j] & GOAL)
	    --state.storecount;
	if (state.map[state.player] & GOAL)
	    ++state.storecount;
	--state.pushcount;
    }
    state.map[state.player] &= ~PLAYER;
    state.player -= move.yx;
    state.map[state.player] |= PLAYER;
    --state.movecount;
    if (recording && macro->count) {
	if (--macro->count == 0)
	    recording = FALSE;
    }
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

/* Check a move for validity in the current state. If it is valid, it
 * is applied via domove(), otherwise return FALSE. If the move is
 * equivalent to an undo or a redo, then use that instead; otherwise,
 * the redo list is reset.
 */
int newmove(yx delta)
{
    dyx	move;
    int	b;
    yx	j;

    j = state.player + delta;
    if (state.map[j] & WALL)
	return FALSE;
    if (state.undo.count) {
	move = state.undo.list[state.undo.count - 1];
	if (!move.box && move.yx == -delta)
	    return undomove();
    }

    b = state.map[j] & BOX ? TRUE : FALSE;
    if (b && state.map[j + delta] & (BOX | WALL))
	return FALSE;
    if (state.redo.count) {
	move = state.redo.list[state.redo.count - 1];
	if (move.box == b && move.yx == delta)
	    return redomove();
    }

    move.yx = delta;
    move.box = b;
    domove(move);
    state.redo.count = 0;
    return TRUE;
}

/* Return TRUE if the puzzle has been completed. (Note that normally
 * boxcount and goalcount will be the same number. But if they are
 * not, either one should be considered a winning condition.)
 */
int checkfinished(void)
{
    return state.storecount == state.game->boxcount
	|| state.storecount == state.game->goalcount;
}

/*
 * Macro functions
 */

/* Toggle macro recording on and off.
 */
void setmacro(void)
{
    if (recording) {
	recording = FALSE;
	return;
    }
    macro = &macros[state.player];
    initmovelist(macro);
    recording = TRUE;
}

/* Set macro and macroplay so as to begin macro playback.
 */
int startmacro(void)
{
    if (!macros[state.player].count)
	return FALSE;
    if (recording)
	recording = FALSE;
    macro = &macros[state.player];
    macroplay = 0;
    return TRUE;
}

/* Apply one move from the current macro.
 */
int macromove(void)
{
    if (macroplay >= 0)
	if (!newmove(macro->list[macroplay].yx) || ++macroplay >= macro->count)
	    macroplay = -1;
    return macroplay >= 0;
}

/* Return TRUE if macro playback is currently on.
 */
int isplaying(void)
{
    return macroplay >= 0;
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
    char	obj[2] = " ";
    cell       *map;
    int		spaces;
    int		y, x;

    if (state.movecount)
	printf(";;; move %d\n", state.movecount);
    map = state.map;
    for (y = 1, map += XSIZE ; y < state.game->ysize - 1 ; ++y, map += XSIZE) {
	spaces = 0;
	for (x = 1 ; x < state.game->xsize - 1 ; ++x) {
	    if (map[x] & PLAYER)
		obj[0] = map[x] & GOAL ? '+' : '@';
	    else if (map[x] & BOX)
		obj[0] = map[x] & GOAL ? '*' : '$';
	    else if (map[x] & WALL)
		obj[0] = '#';
	    else if (map[x] & GOAL)
		obj[0] = '.';
	    else {
		++spaces;
		continue;
	    }
	    printf("%*s", spaces + 1, obj);
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
    dyx		lastmove = { 0 };
    dyx	       *move;
    int		i;

    if (!state.redo.count)
	return FALSE;
    move = state.redo.list + state.redo.count;
    for (i = 0 ; i < state.redo.count ; ++i) {
	--move;
	if (move->yx != lastmove.yx || move->box != lastmove.box) {
	    lastmove = *move;
	    outputmapstate();
	}
	domove(*move);
    }
    outputmapstate();
    return TRUE;
}

/* Display the current game state to the user.
 */
int drawscreen(int index)
{
    return displaygame(state.map, state.game->ysize, state.game->xsize,
		       recording, macros[state.player].count > 0, !!stack,
		       state.game->seriesname, state.game->name, index + 1,
		       state.game->boxcount, state.storecount,
		       state.movecount, state.pushcount,
		       state.game->movebestcount, state.game->pushbestcount);
}

/* Compare the solution currently sitting in the undo list with the
 * user's best solutions (if any). If this solution beats what's
 * there, replace them. If this solution has the save number of moves
 * as the least-moves solution, but fewer pushes, then the replacement
 * will be done, and likewise for the least-pushes solution. Note that
 * the undo list contains the moves in backwards order, so the list
 * needs to be reversed when it is copied. TRUE is returned if any
 * solution was replaced.
 */
int replaceanswers(int saveinc)
{
    int		i, n;

    if (saveinc && (state.game->movebestcount || state.game->pushbestcount))
	return FALSE;

    n = 0;
    if (!state.game->movebestcount
		|| state.movecount < state.game->movebestcount
		|| (state.movecount == state.game->movebestcount
			&& state.pushcount < state.game->movebestpushcount)) {
	initmovelist(&state.game->moveanswer);
	i = state.undo.count;
	while (i--)
	    addtomovelist(&state.game->moveanswer, state.undo.list[i]);
	if (!saveinc)
	    state.game->movebestcount = state.movecount;
	state.game->movebestpushcount = state.pushcount;
	++n;
    }
    if (!state.game->pushbestcount
		|| state.pushcount < state.game->pushbestcount
		|| (state.pushcount == state.game->pushbestcount
			&& state.movecount < state.game->pushbestmovecount)) {
	initmovelist(&state.game->pushanswer);
	i = state.undo.count;
	while (i--)
	    addtomovelist(&state.game->pushanswer, state.undo.list[i]);
	state.game->pushbestcount = state.pushcount;
	if (!saveinc)
	    state.game->pushbestmovecount = state.movecount;
	++n;
    }

    return n > 0;
}
