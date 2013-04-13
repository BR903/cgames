/* answers.h: Functions for reading and saving puzzle solutions.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_answers_h_
#define	_answers_h_

#include	<stdio.h>
#include	"fileread.h"

/* The directory containing the user's solution files.
 */
extern char    *savedir;

/* FALSE if savedir's existence is unverified.
 */
extern int	savedirchecked;

/* Read the solutions for game, if any, from the current file
 * position.
 */
extern int readanswers(FILE *fp, gamesetup *game);

/* Write out all the solutions for series.
 */
extern int saveanswers(gameseries *series);

#endif
