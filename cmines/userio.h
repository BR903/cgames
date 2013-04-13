/* userio.h: Functions that need to know the nature of the user interface.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_userio_h_
#define	_userio_h_

#include	"cmines.h"

enum {
    status_ignore, status_normal, status_lost, status_won, status_besttime
};

/* The name of the program; used for error messages.
 */
extern char const      *programname;


/* Initialize the user interface for our program. If updatetimeflag is
 * TRUE, the timer display will be updated asynchronously; by default
 * it is only updated on input. If showsmileyflag is TRUE, emoticons
 * are used to highlight the player's current status. silenceflag, if
 * TRUE, suppresses the ringing of the terminal bell.
 */
extern int ioinitialize(int updatetimerflag, int showsmileyflag,
			int silenceflag, int allowoffclicks);

/* Wait for a keypress.
 */
extern int input(void);

/* Ring the bell.
 */
extern void ding(void);

/* Exits with an error message.
 */
extern void die(char const *fmt, ...);

/* Read the timer.
 */
extern int gettimer(void);

/* Set the timer's current state. If action is negative, the timer is
 * turned off and removed from the display. If action is positive, the
 * timer begins counting. If action is zero, the timer stops counting.
 */
extern void settimer(int action);

/* Move the cursor to the given position in the field.
 */
extern void setcursorpos(int pos);

/* Update the display. field contains the game field in its current
 * state; ysize and xsize indicate the field's dimensions. minecount
 * is the number of mines in the field and flagcount is the number of
 * cells that have been flagged. status is set to one of the values in
 * the enumeration above and indicates the current status of the game.
 */
extern void displaygame(cell const *field, int ysize, int xsize,
			int minecount, int flagcount, int status);

/* Change the display to show information about the various key
 * commands. The keys array is a list of keycount double-strings that
 * describe the various game actions; each element contains two
 * concatenated NUL-terminated strings, which are displayed in
 * separate columns. The setups array is a list of setupcount strings
 * that name the different configurations available. besttimes is a
 * list, also with setupcount elements, that provides the best times
 * on record for each configuration.
 */
extern void displayhelp(int keycount, char const *keys[],
			int setupcount, char const *setups[], int besttimes[]);

/* A callback function, called from within input() in order to
 * decipher mouse activity. y and x provide the current position of
 * the mouse. button indicates which mouse button has been released,
 * or depressed if mstate is negative. The return value is either a
 * character to be returned from input(), or zero to ignore the
 * activity.
 */
extern int mousecallback(int pos, int button);

#endif
