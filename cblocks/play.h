/* play.h: Functions for changing the state of the game.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_play_h_
#define	_play_h_

#include	"cblocks.h"
#include	"movelist.h"
#include	"fileread.h"

/* The collection of data corresponding to the game's state.
 */
typedef	struct gamestate {
    gamesetup  *game;			/* the puzzle specification */
    int		level;			/* the level number of the game */
    short	ycurrpos;		/* the player's current position */
    short	xcurrpos;		/* the player's current position */
    short	currblock;		/* the player's current position */
    int		movecount;		/* number of moves made so far */
    int		stepcount;		/* number of pushes made so far */
    actlist	undo;			/* the list of moves */
    actlist	redo;			/* the list of recently undone moves */
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
 * contain the user's saved solution.
 */
extern void initgamestate(void);

/* Execute a new move in the current game. If the move is illegal,
 * FALSE will be returned and the state is unchanged.
 */
extern int newmove(int dir);

/* Undo the last move. FALSE is returned if there is no last move.
 */
extern int undomove(void);

/* Undo the last n moves. FALSE is returned if there is no last move.
 */
extern int undomoves(int n);

/* Undo the last step. FALSE is returned if there is no last step.
 */
extern int undostep(void);

/* Reinstate the last undone move. FALSE is returned if the previous
 * action was not an undo.
 */
extern int redomove(void);


/* Redo the last n undone moves. FALSE is returned if there are no
 * moves to redo.
 */
extern int redomoves(int n);

/* Redo the last undone step. FALSE is return if there is no step to
 * redo.
 */
extern int redostep(void);

/* Return TRUE if the current state has completed the puzzle.
 */
extern int checkfinished(void);

/* Change the current block, cycling through the complete set.
 */
extern void rotatefromcurrblock(void);

/* Change the current block to the block that most nearly lies next to
 * it in the given direction.
 */
extern int shiftfromcurrblock(int dir);

extern int movecursor(int dir);

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

/* Display the puzzle's goal to the user.
 */
extern void displaygoal(void);

/* Replace the user's solution with the just-executed solution (taken
 * from the undo list) if it beats the existing solution for least
 * number of steps. FALSE is returned if no solution was replaced. If
 * saveinc is TRUE, then the user's "solution" is not actually
 * complete, in which case it will only be saved if no complete
 * solution is currently saved.
 */
extern int replaceanswer(int saveinc);

/* Print to stdout a series of images of the current game as the
 * user's solution is followed.
 */
extern int displaygamesolution(void);

#endif
