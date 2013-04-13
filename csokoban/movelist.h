/* movelist.h: Functions for manipulating lists of moves.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_movelist_h_
#define	_movelist_h_

/* A coordinate is stored in a single short as a map array offset.
 */
typedef short	yx;

/* A move is stored as a delta value plus a boolean indicating if a
 * box was pushed.
 */
typedef	struct dyx { int yx:14, box:2; } dyx;

/* A list of moves.
 */
typedef struct dyxlist {
    int		allocated;	/* number of elements allocated */
    int		count;		/* size of the actual array */
    dyx	       *list;		/* the array */
} dyxlist;

/* Initialize or reinitialize list as empty.
 */
extern void initmovelist(dyxlist *list);

/* Initialize list as having size elements.
 */
extern void setmovelist(dyxlist *list, int size);

/* Append move to the end of list.
 */
extern void addtomovelist(dyxlist *list, dyx move);

/* Make to an independent copy of from.
 */
extern void copymovelist(dyxlist *to, dyxlist const *from);

/* Deallocate list.
 */
extern void destroymovelist(dyxlist *list);

#endif
