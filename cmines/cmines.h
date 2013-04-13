/* cmines.h: Basic game-related definitions.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_cmines_h_
#define	_cmines_h_

#ifndef	TRUE
#define	TRUE		1
#define	FALSE		0
#endif

/* The maximum dimensions of the field.
 */
#define	MAXWIDTH	64
#define	MAXHEIGHT	64

/* The width of one row in a map array.
 */
#define	XSIZE		MAXWIDTH

/* Direction indicators.
 */
#define	NORTH		0
#define	EAST		1
#define	SOUTH		2
#define	WEST		3

/* Bitflags used to indicate the state of a cell.
 */
#define	MINED		0x10
#define	FLAGGED		0x20
#define	EXPOSED		0x40
#define	NEIGHBOR_MASK	0x0F

/* The cells of the field occupy a single byte.
 */
typedef	unsigned char	cell;

#endif
