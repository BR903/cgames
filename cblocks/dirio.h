/* dirio.h: Directory access functions.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_dirio_h_
#define	_dirio_h_

#include	<stdio.h>

/* Return a buffer big enough to hold a pathname.
 */
extern char *getpathbuffer(void);

/* Copy a pathname, assuming the destination is of size PATH_MAX.
 */
extern int copypath(char *to, char const *from);

/* Return TRUE if name appears to be the name of a file.
 */
extern int isfilename(char const *name);

/* Create the directory dir if it doesn't already exist.
 */
extern int finddir(char const *dir);

/* Open a file, using dir as the directory if filename is not a path.
 */
extern FILE *openfileindir(char const *dir, char const *filename,
			   char const *mode);

/* Call filecallback once for every file in dir; the first argument to
 * the callback function is an allocated buffer containing the
 * filename. If the callback's return value is zero, the buffer is
 * deallocated normally; if the return value is positive, the callback
 * function inherits the buffer and the responsibility of freeing it.
 * If the return value is negative, the function stops scanning the
 * directory and returns to the original caller.
 */
extern int findfiles(char const *dir, void *data,
		     int (*filecallback)(char*, void*));

#endif
