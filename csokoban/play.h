/* play.h: Functions for changing the state of the game.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_play_h_
#define	_play_h_

#include	"csokoban.h"
#include	"movelist.h"
#include	"fileread.h"

/* The collection of data corresponding to the game's state.
 */
typedef	struct gamestate {
    gamesetup  *game;			/* the puzzle specification */
    int		level;			/* the level number of the game */
    yx		player;			/* the player's current position */
    short	storecount;		/* number of boxes on goal cells */
    int		movecount;		/* number of moves made so far */
    int		pushcount;		/* number of pushes made so far */
    dyxlist	undo;			/* the list of moves */
    dyxlist	redo;			/* the list of recently undone moves */
    cell	map[MAXHEIGHT * MAXWIDTH]; /* the game's map */
} gamestate;

/* Set the current puzzle to be game, with the given level number.
 * After calling this function, initgamestate() must be called before
 * using any other functions in this module.
 */
extern void selectgame(gamesetup *game, int level);

/* Initialize the current state to the starting position of the
 * current puzzle. All macros, saved positions, and the undo list will
 * be erased and deallocated. The redo list will be initialized to
 * contain the user's saved solution. If the user has two solutions,
 * then the solution with the least moves is used if usemoves is TRUE,
 * otherwise the solution with the least pushes is used.
 */
extern void initgamestate(int usemoves);

/* Execute a new move in the current game. If the move is illegal,
 * FALSE will be returned and the state is unchanged.
 */
extern int newmove(yx delta);

/* Undo the latest move. FALSE is returned if there is no latest move.
 */
extern int undomove(void);

/* Reinstate the last undone move. FALSE is returned if the previous
 * action was not an undo.
 */
extern int redomove(void);

/* Return TRUE if the current state has completed the puzzle.
 */
extern int checkfinished(void);

/* Toggle macro recording on and off. All moves made while recording
 * will be saved in a list associated with the position of the player
 * at the time recording begun.
 */
extern void setmacro(void);

/* Turn on macro playback. Return FALSE if no macro is associated with
 * the player's current position.
 */
extern int startmacro(void);

/* Execute one move from the macro currently being played back.
 * Return FALSE when the last move in the macro is reached, or if the
 * move was not valid. In either case, macro playback will be
 * automatically turned off.
 */
extern int macromove(void);

/* Return TRUE if macro playback is currently on.
 */
extern int isplaying(void);

/* Save the current state of the game on a stack.
 */
extern void savestate(void);

/* Replace the current state with the last saved state. Return FALSE
 * if there were no states to save.
 */
extern int restorestate(void);

/* Discard all saved states from the stack.
 */
extern void freesavedstates(void);

/* Display the current game state to the user.
 */
extern int drawscreen(int index);

/* Replace the user's solutions with the just-executed solution (taken
 * from the undo list) if it beats either or both of them for least
 * number of moves/pushes. FALSE is returned if no solution was
 * replaced.
 */
extern int replaceanswers(void);

#endif
