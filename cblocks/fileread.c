/* fileread.c: Functions for reading the puzzle files.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	"gen.h"
#include	"cblocks.h"
#include	"movelist.h"
#include	"dirio.h"
#include	"userio.h"
#include	"answers.h"
#include	"parse.h"
#include	"fileread.h"

/* Mini-structure for our findfiles() callback.
 */
typedef	struct seriesdata {
    gameseries *list;
    int		count;
} seriesdata;

/* The path of the directory containing the puzzle files.
 */
char   *datadir = NULL;

/* Read one full line from fp and store the first len characters in
 * buf.
 */
int getnline(FILE *fp, char *buf, int len)
{
    int	ch, n;

    if (!fgets(buf, len, fp))
	return -1;
    n = strlen(buf);
    if (n == len - 1 && buf[n] != '\n') {
	do
	    ch = fgetc(fp);
	while (ch != EOF && ch != '\n');
    } else if (buf[n - 1] == '\n')
	buf[--n] = '\0';
    return n;
}

/* Examine the top of the file for series and extract the display
 * string, if present. FALSE is returned if the file appears to be
 * invalid or bereft of maps.
 */
static int readseriesheader(gameseries *series)
{
    char	buf[256];
    int		ch;
    int		n;

    for (;;) {
	ch = fgetc(series->mapfp);
	if (ch == ';') {
	    getnline(series->mapfp, buf, sizeof buf);
	    if (*buf == ';') {
		for (n = 1 ; isspace(buf[n]) ; ++n) ;
		strncpy(series->name, buf + n, sizeof series->name - 1);
		series->name[sizeof series->name - 1] = '\0';
		n = strlen(series->name);
		for (--n ; isspace(series->name[n]) ; --n) ;
		series->name[n + 1] = '\0';
		return TRUE;
	    }
	} else if (ch != '\n')
	    break;
    }

    if (ch == EOF) {
	fclose(series->mapfp);
	series->mapfp = NULL;
	series->allmapsread = TRUE;
	return FALSE;
    }
    ungetc(ch, series->mapfp);
    return TRUE;
}

/* A callback function that initializes a gameseries structure for
 * filename and adds it to the list stored under the second argument.
 */
static int getseriesfile(char *filename, void *data)
{
    static int	allocated = 0;
    seriesdata *sdata = (seriesdata*)data;
    gameseries *series;

    while (sdata->count >= allocated) {
	++allocated;
	if (!(sdata->list = realloc(sdata->list,
				    allocated * sizeof *sdata->list)))
	    memerrexit();
    }
    series = sdata->list + sdata->count++;
    series->filename = filename;
    series->mapfp = NULL;
    series->answerfp = NULL;
    series->allmapsread = FALSE;
    series->allanswersread = FALSE;
    series->allocated = 0;
    series->count = 0;
    series->games = NULL;
    *series->name = '\0';
    return 1;
}

/* A callback function to compare two gameseries structures by
 * comparing their filenames.
 */
static int gameseriescmp(void const *a, void const *b)
{
    return strcmp(((gameseries*)a)->filename, ((gameseries*)b)->filename);
}

/* Search the game file directory and generate an array of gameseries
 * structures corresponding to the puzzle files found there.
 */
int getseriesfiles(char *filename, gameseries **list, int *count)
{
    seriesdata	s;

    s.list = NULL;
    s.count = 0;
    if (*filename && isfilename(filename)) {
	if (getseriesfile(filename, &s) <= 0 || !s.count)
	    die("Couldn't access \"%s\"", filename);
	*datadir = '\0';
	*savedir = '\0';
    } else {
	if (!findfiles(datadir, &s, getseriesfile) || !s.count)
	    die("Couldn't find any data files in \"%s\".", datadir);
	if (s.count > 1)
	    qsort(s.list, s.count, sizeof *s.list, gameseriescmp);
    }
    *list = s.list;
    *count = s.count;
    return TRUE;
}

/* Read the puzzle file corresponding to series, and the corresponding
 * solutions in the answer file, until at least level maps have been
 * successfully parsed, or the end is reached. The files are opened if
 * they have not been already. No files are opened if the requested
 * level is already in memory.
 */
int readlevelinseries(gameseries *series, int level)
{
    int		n;

    if (level < 0)
	return FALSE;
    if (series->count > level)
	return TRUE;

    if (!series->allmapsread) {
	if (!series->mapfp) {
	    currentfilename = series->filename;
	    series->mapfp = openfileindir(datadir, series->filename, "r");
	    if (!series->mapfp)
		return fileerr(NULL);
	    if (!readseriesheader(series))
		return fileerr("file contains no maps");
	    if (*savedir) {
		series->answerfp = openfileindir(savedir, 
						 series->filename, "r");
		if (series->answerfp) {
		    savedirchecked = TRUE;
		    series->answersreadonly = TRUE;
		}
	    } else
		series->answerfp = NULL;
	}
	while (!series->allmapsread && series->count <= level) {
	    while (series->count >= series->allocated) {
		n = series->allocated ? series->allocated * 2 : 16;
		if (!(series->games = realloc(series->games,
					      n * sizeof *series->games)))
		    memerrexit();
		memset(series->games + series->allocated, 0,
		       (n - series->allocated) * sizeof *series->games);
		series->allocated = n;
	    }
	    if (readlevelmap(series->mapfp, series->games + series->count)) {
		series->games[series->count].seriesname = series->name;
		if (!series->allanswersread)
		    readanswers(series->answerfp,
				series->games + series->count);
		++series->count;
	    }
	    if (feof(series->mapfp)) {
		fclose(series->mapfp);
		series->mapfp = NULL;
		series->allmapsread = TRUE;
	    }
	    if (series->answerfp && feof(series->answerfp))
		series->allanswersread = TRUE;
	}
    }
    return series->count > level;
}
