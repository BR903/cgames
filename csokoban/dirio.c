/* dirio.c: Directory access functions.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<unistd.h>
#include	<dirent.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	"gen.h"
#include	"dirio.h"

/* Determine a compile-time number to use as the maximum length of a
 * path. Use 1023 if we can't get anything usable from the header
 * files.
 */
#include <limits.h>
#if !defined(PATH_MAX) || PATH_MAX <= 0
#  if defined(MAXPATHLEN) && MAXPATHLEN > 0
#    define PATH_MAX MAXPATHLEN
#  else
#    include <sys/param.h>
#    if !defined(PATH_MAX) || PATH_MAX <= 0
#      if defined(MAXPATHLEN) && MAXPATHLEN > 0
#        define PATH_MAX MAXPATHLEN
#      else
#        define PATH_MAX 1023
#      endif
#    endif
#  endif
#endif


/* Return a buffer big enough to hold a pathname.
 */
char *getpathbuffer(void)
{
    char       *buf;

    if (!(buf = malloc(PATH_MAX + 1)))
	memerrexit();
    return buf;
}

/* Copy a pathname, assuming the destination is of size PATH_MAX.
 */
int copypath(char *to, char const *from)
{
    int	n;

    n = strlen(from);
    if (n > PATH_MAX)
	return FALSE;
    memcpy(to, from, n + 1);
    return TRUE;
}

/* Return TRUE if name appears to be the name of a file.
 */
int isfilename(char const *name)
{
    struct stat	st;

    if (strchr(name, '/'))
	return TRUE;
    if (!stat(name, &st) && !S_ISDIR(st.st_mode))
	return TRUE;
    return FALSE;
}

/* Create the directory dir if it doesn't already exist.
 */
int finddir(char const *dir)
{
    struct stat	st;

    return stat(dir, &st) ? mkdir(dir, 0755) == 0 : S_ISDIR(st.st_mode);
}

/* Open a file, using dir as the directory if filename is not a path.
 */
FILE *openfileindir(char const *dir, char const *filename, char const *mode)
{
    FILE       *fp;
    char	buf[PATH_MAX + 1];
    int		n;

    if (!dir || !*dir || strchr(filename, '/'))
	fp = fopen(filename, mode);
    else {
	n = strlen(dir);
	if (n + 1 + strlen(filename) > PATH_MAX) {
	    errno = ENAMETOOLONG;
	    return NULL;
	}
	memcpy(buf, dir, n);
	buf[n++] = '/';
	strcpy(buf + n, filename);
	fp = fopen(buf, mode);
    }
    return fp;
}

/* Call filecallback once for every file in dir.
 */
int findfiles(char const *dir, void *data,
	      int (*filecallback)(char*, void*))
{
    char	       *filename = NULL;
    DIR		       *dp;
    struct dirent      *dent;
    int			r;

    if (!(dp = opendir(dir))) {
	currentfilename = dir;
	return fileerr(NULL);
    }

    while ((dent = readdir(dp))) {
	if (dent->d_name[0] == '.')
	    continue;
	if (!(filename = realloc(filename, strlen(dent->d_name) + 1)))
	    memerrexit();
	strcpy(filename, dent->d_name);
	r = (*filecallback)(filename, data);
	if (r < 0)
	    break;
	else if (r > 0)
	    filename = NULL;
    }

    if (filename)
	free(filename);
    closedir(dp);
    return TRUE;
}
