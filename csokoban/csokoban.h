/* csokoban.h: Basic game-related definitions.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_csokoban_h_
#define	_csokoban_h_

/* The maximum dimensions of a puzzle.
 */
#define	MAXWIDTH	32
#define	MAXHEIGHT	32

/* The width of one row in a map array.
 */
#define	XSIZE		MAXWIDTH

/* An empty cell.
 */
#define	EMPTY		0

/* Non-empty cells. Note that PLAYER and BOX can be combined with GOAL.
 */
#define	GOAL		0x01
#define	PLAYER		0x02
#define	BOX		0x04
#define	WALL		0x08

/* Bitflags added to walls indicating how they adjoin with neighboring
 * walls.
 */
#define	EXTENDNORTH	0x10
#define	EXTENDSOUTH	0x20
#define	EXTENDWEST	0x40
#define	EXTENDEAST	0x80

/* Temporary bitflag used by improvemap().
 */
#define	CELLMARK	0x10

/* Bitflag indicating the cell can be reached by the player.
 */
#define	FLOOR		0x20

/* The cells of our map are stored in a single byte.
 */
typedef	unsigned char	cell;

#endif
