/* movelist.h: Functions for manipulating lists of moves.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_movelist_h_
#define	_movelist_h_

/* A move is stored as a block identifier, block coordinates, and a
 * direction, plus one bit indicating if a door was opened.
 */
typedef	struct action { int id:12, dir:3, door:1, y:8, x:8; } action;

/* A list of moves.
 */
typedef struct actlist {
    int		allocated;	/* number of elements allocated */
    int		count;		/* size of the actual array */
    action     *list;		/* the array */
} actlist;

/* Initialize or reinitialize list as empty.
 */
extern void initmovelist(actlist *list);

/* Initialize list as having size elements.
 */
extern void setmovelist(actlist *list, int size);

/* Append move to the end of list.
 */
extern void addtomovelist(actlist *list, action move);

/* Make to an independent copy of from.
 */
extern void copymovelist(actlist *to, actlist const *from);

/* Deallocate list.
 */
extern void destroymovelist(actlist *list);

#endif
