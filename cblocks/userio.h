/* userio.h: Functions that need to know the nature of the user interface.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_userio_h_
#define	_userio_h_

#include	"cblocks.h"

/* Special codes to identify multibyte keystrokes.
 */
#define	ARROW_N		(1024 + NORTH)
#define	ARROW_E		(1024 + EAST)
#define	ARROW_S		(1024 + SOUTH)
#define	ARROW_W		(1024 + WEST)

/* Initialize the user interface for our program. This functions
 * checks to make sure it can do the necessary I/O, and sets
 * fieldheight and fieldwidth, but does not do anything to the
 * interface (such as altering terminal modes). If silence is TRUE,
 * then the beep() function will have no effect.
 */
extern int ioinitialize(int silence);

/* Wait for a keypress.
 */
extern int input(void);

/* Ring the bell.
 */
extern void ding(void);

/* Translate a general RGB value (each number in the range 0-255)
 * to one of the available colors.
 */
extern char getrgbindex(int r, int g, int b);

/* Update the display. map contains the game map in its current state;
 * ysize and xsize indicate the map's dimensions. recording, macro,
 * and save are boolean values indicating whether macro recording is
 * on, whether macro playback is on, and whether the stack contains
 * any saved states, respectively. seriesname and levelname are
 * identifying strings that have been previously modified by
 * textfit(). index is a one-based identifier of the current puzzle
 * across all of the series. boxcount is the number of boxes present,
 * and storecount is the number of boxes currently stored in cells
 * containing goals. movecount and pushcount are the number of moves
 * and pushes made so far. bestmovecount and bestpushcount indicate
 * the user's best solutions to date, or are zero if no such solutions
 * exist. FALSE is returned if the game cannot be displayed.
 */
extern int displaygame(cell const *map, int ysize, int xsize,
		       char const *seriesname, char const *levelname,
		       int level, char const *colors,
		       int currblockid, int ycursor, int xcursor,
		       int saved, int movecount, int stepcount,
		       int beststepcount, int bestmovecount,
		       int beststepknown);

/* Change the display to show information about the various key
 * commands. keys is an array of keycount double-strings; each element
 * contains two concatenated NUL-terminated strings, which are
 * displayed in separate columns.
 */
extern void displayhelp(char const *keys[], int keycount);

/* Display a prompt urging the user to press some key in order to
 * continue (or to exit if endofsession is TRUE).
 */
extern void displayendmessage(int endofsession);

/* A callback function, called from within input() in order to
 * decipher mouse activity. y and x provide the current position of
 * the mouse. button indicates which mouse button has been released,
 * or depressed if mstate is negative. The return value is either a
 * character to be returned from input(), or zero to ignore the
 * activity.
 */
extern int mousecallback(int y, int x, int button);

#endif
