/* gen.h: Generic functions and definitions used by all modules.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#ifndef	_gen_h_
#define	_gen_h_

#ifndef	TRUE
#define	TRUE	1
#define	FALSE	0
#endif

/* When allocation fails.
 */
#define	memerrexit()	(die("out of memory"))

/* The name of the program and the file currently being accessed; used
 * for error messages.
 */
extern char const      *programname;
extern char const      *currentfilename;

/* Exits with an error message.
 */
extern void die(char const *fmt, ...);

/* Displays a message appropriate to the last error, or msg if errno
 * is zero. Returns FALSE.
 */
extern int fileerr(char const *msg);

#endif
