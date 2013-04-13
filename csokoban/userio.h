/* userio.h: Functions that need to know the nature of the user interface.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_userio_h_
#define	_userio_h_

#include	"csokoban.h"

/* Initialize the user interface. This functions checks to make sure
 * it can do the necessary I/O, and sets fieldheight and fieldwidth,
 * but does not do anything to the interface (such as altering
 * terminal modes).
 */
extern int ioinitialize(int silence);

/* Wait for a keypress.
 */
extern int input(void);

/* Ring the bell.
 */
extern void ding(void);

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
		       int recording, int macro, int save,
		       char const *seriesname, char const *levelname,
		       int level, int boxcount, int storecount,
		       int movecount, int pushcount,
		       int bestmovecount, int bestpushcount);

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

#endif
