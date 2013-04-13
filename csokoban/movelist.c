/* movelist.c: Functions for manipulating lists of moves.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<string.h>
#include	"gen.h"
#include	"movelist.h"

/* Initialize or reinitialize list as empty.
 */
void initmovelist(dyxlist *list)
{
    if (!list->allocated || !list->list) {
	list->allocated = 16;
	if (!(list->list = malloc(list->allocated * sizeof *list->list)))
	    memerrexit();
    }
    list->count = 0;
}

/* Initialize list as having size elements.
 */
void setmovelist(dyxlist *list, int size)
{
    if (!list->allocated || !list->list)
	list->allocated = 16;
    while (list->allocated < size)
	list->allocated *= 2;
    if (!(list->list = realloc(list->list,
			       list->allocated * sizeof *list->list)))
	memerrexit();
    list->count = size;
}

/* Append move to the end of list.
 */
void addtomovelist(dyxlist *list, dyx move)
{
    if (list->count >= list->allocated) {
	list->allocated *= 2;
	if (!(list->list = realloc(list->list,
				   list->allocated * sizeof *list->list)))
	    memerrexit();
    }
    list->list[list->count++] = move;
}

/* Make to an independent copy of from.
 */
void copymovelist(dyxlist *to, dyxlist const *from)
{
    to->list = NULL;
    initmovelist(to);
    setmovelist(to, from->count);
    memcpy(to->list, from->list, from->count * sizeof *from->list);
}

/* Deallocate list.
 */
void destroymovelist(dyxlist *list)
{
    if (list->list)
	free(list->list);
    list->allocated = 0;
    list->list = NULL;
}
