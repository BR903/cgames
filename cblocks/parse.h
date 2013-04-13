/* parse.h: Functions for parsing the puzzle file syntax.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */
#ifndef	_parse_h_
#define	_parse_h_

#include	<stdio.h>
#include	"fileread.h"

/* Read a single map from the current position of fp and use it to
 * initialize the given gamesetup structure.
 */
extern int readlevelmap(FILE *fp, gamesetup *game);

#endif
