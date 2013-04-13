/* cblocks.h: Basic game-related definitions.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_cblocks_h_
#define	_cblocks_h_

#include	<limits.h>

/* The maximum dimensions of a puzzle.
 */
#define	MAXWIDTH	32
#define	MAXHEIGHT	32

/* The width of one row in a map array.
 */
#define	XSIZE		MAXWIDTH

/* The four directions.
 */
#define	NORTH		0
#define	EAST		1
#define	SOUTH		2
#define	WEST		3

/* Reverse a direction.
 */
#define	backwards(d)	((d) ^ 2)

/* Bitmasks for isolating parts of a cell's value.
 */
#define	BLOCKID_MASK	0x000000FF
#define	DOORSTAMP_MASK	0x3FFF0000

/* Special block IDs.
 */
#define	WALLID		0x00000001
#define	KEYID		0x00000002
#define	FIRSTID		0x00000003
#define	LASTID		0x000000FF

/* Bitflags added to objects indicating how they adjoin across cells.
 */
#define	EXTENDNORTH	0x00000100
#define	EXTENDEAST	0x00000200
#define	EXTENDSOUTH	0x00000400
#define	EXTENDWEST	0x00000800

/* Bitflags added to objects that can be passed over indicating how
 * they adjoin across cells.
 */
#define	FEXTENDNORTH	0x00001000
#define	FEXTENDEAST	0x00002000
#define	FEXTENDSOUTH	0x00004000
#define	FEXTENDWEST	0x00008000

/* Other cell bitflags.
 */
#define	GOAL		0x40000000
#define	MARK		0x80000000

/* Extract a cell's block ID.
 */
#define	blockid(c)	((int)((c) & BLOCKID_MASK))

/* Extract a door cell's timestamp.
 */
#define	doortime(c)	((int)(((c) & DOORSTAMP_MASK) >> 16))

/* The cells of our map are stored in a regular unsigned 32-bit value.
 */
#if UINT_MAX >= 0xFFFFFFFF
typedef	unsigned int	cell;
#else
typedef	unsigned long	cell;
#endif

#endif
