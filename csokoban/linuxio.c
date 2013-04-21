/* userio.c: User interface functions for the Linux console.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<string.h>
#include	<ctype.h>
#include	<signal.h>
#include	<errno.h>
#include	<unistd.h>
#include	<termios.h>
#include	<sys/ioctl.h>
#include	<linux/kd.h>
#include	"gen.h"
#include 	"csokoban.h"
#include	"userio.h"

/* If NSIG is not provided by signal.h, blithely assume 32 is big enough.
 */
#ifndef	NSIG
#define	NSIG	32
#endif

/* The predefined height of the logical console font.
 */
#define	FONTHEIGHT	32

/* The number of columns we reserve on the right side of the display
 * for text.
 */
#define	SIDEBARWIDTH	18

/* Flags to indicate which parts of the screen have been completed.
 */
#define	DONE_MAP	0x01
#define	DONE_STATS	0x02
#define	DONE_SERIESNAME	0x04
#define	DONE_LEVELNAME	0x08
#define	DONE_ALL	0x0F

/* Given a console font and a character, calculate a pointer to the
 * data for that character.
 */
#define	fontchar(font, ch)	((font) + FONTHEIGHT * (unsigned char)(ch))

/* Reset and erase the console screen.
 */
#define	erasescreen()		(write(STDOUT_FILENO, "\033c\033[H\033[J", 8))

/* The following three arrays contain the character numbers in the
 * console font that will be overwritten with images. The numbers were
 * chosen as being relatively unlikely to collide with anything that
 * will be used while the program is running. (They correspond to the
 * mixed single-and-double line-drawing characters in the PC character
 * set.) However, since a console font can have arbitrary mapping,
 * there's no guaranteee that these characters don't contain accented
 * letters, or even part of the ASCII subset. A better approach might
 * be to read the currently installed map and choose characters that
 * are not being used for anything in the Latin-1 range. However, this
 * could still present a problem if the user is using (say) Russian in
 * the other VCs. There is no good general solution; thus the
 * relatively simple approach used here.
 */

/* The indexes of the characters in the font that will be replaced
 * with images for displaying various parts of walls.
 */
static char const wallchars[][2] = {
    { 203, 211 },	/* left side extends N S W, right side extends N S E */
    { 204, 212 },	/* left side extends S W, right side extends S E */
    { 205, 213 },	/* left side extends N W, right side extends N E */
    { 206, 206 },	/* left side extends W, right side extends E */
    { 207, 188 },	/* left side extends N S, right side extends N S */
    { 208, 214 },	/* left and right sides extend S */
    { 209, 189 },	/* left and right sides extend N */
    { 210, 190 }	/* left and right sides do no extend */
};

/* The indexes of the character pairs used to display a cell
 * containing some kind of wall.
 */
static char const wallcells[][2] = {
    { 210, 190 },	/* isolated wall */
    { 209, 189 },	/* wall extends N */
    { 208, 214 },	/* wall extends S */
    { 207, 188 },	/* wall extends N S */
    { 206, 190 },	/* wall extends W */
    { 205, 189 },	/* wall extends N W */
    { 204, 214 },	/* wall extends S W */
    { 203, 188 },	/* wall extends N S W */
    { 210, 206 },	/* wall extends E */
    { 209, 213 },	/* wall extends N E */
    { 208, 212 },	/* wall extends S E */
    { 207, 211 },	/* wall extends N S E */
    { 206, 206 },	/* wall extends W E */
    { 205, 213 },	/* wall extends N W E */
    { 204, 212 }, 	/* wall extends S W E */
    { 203, 211 }	/* wall extends N S W E */
};

/* The indexes of the character pairs used to display a cell
 * containing something besides a wall.
 */
static char const screencells[][2] = {
    { 181, 182 },	/* FLOOR */
    { 198, 183 },	/* GOAL */
    { 199, 184 },	/* BOX */
    { 200, 185 },	/* BOX + GOAL */
    { 201, 186 },	/* PLAYER */
    { 202, 187 }	/* PLAYER + GOAL */
};

/* TRUE if the terminal is currently in raw mode.
 */
static int			inrawmode = FALSE;

/* TRUE if the console font has been changed.
 */
static int			usingfont = FALSE;

/* TRUE if the display needs to be redrawn now.
 */
static int			redrawrequest = FALSE;

/* Signal handlers that were installed on startup that may need to be
 * reinstalled.
 */
static struct sigaction		prevhandler[NSIG];

/* Signal handlers that this program installs that may need to be
 * temporary uninstalled.
 */
static struct sigaction		myhandler[NSIG];

/* The console font's unicode map data.
 */
static struct unimapdesc	unimap = { 0, NULL };

/* The description of the console font in use at startup.
 */
static struct consolefontdesc	origfont = { 0, 0, NULL };

/* The program's new console font.
 */
static char		       *newfontdata = NULL;

/* The height of the console font.
 */
static int			fontheight = 0;

/* The total number of bytes in the console font data.
 */
static int			fullfontsize = 0;

/* The dimensions of the largest map that can be displayed.
 */
static int			fieldheight, fieldwidth;

/* The index of the bottommost line.
 */
static int			lastline;

/* The index of the leftmost column in the textual display area.
 */
static int			sidebar;

/* FALSE if the program is allowed to ring the bell.
 */
static int			silence = FALSE;

/* The name of the program and the file currently being accessed,
 * for use in error messages (declared in gen.h).
 */
char const		       *programname = NULL;
char const		       *currentfilename = NULL;

/*
 * Console font access functions
 */

/* Retrieve and remember the current console font.
 */
static int getfontdata(void)
{
    ioctl(STDOUT_FILENO, GIO_UNIMAP, &unimap);
    unimap.entries = malloc(unimap.entry_ct * sizeof(struct unipair));
    if (ioctl(STDOUT_FILENO, GIO_UNIMAP, &unimap))
	return errno;
    ioctl(STDOUT_FILENO, GIO_FONTX, &origfont);
    fontheight = origfont.charheight;
    fullfontsize = origfont.charcount * FONTHEIGHT;
    origfont.chardata = malloc(fullfontsize);
    if (ioctl(STDOUT_FILENO, GIO_FONTX, &origfont))
	return errno;
    return 0;
}

/* Change the console font character data to chardata, while
 * preserving the existing height and unicode mapping.
 */
static int setfontdata(char *chardata)
{
    struct consolefontdesc	fd = { 0, 0, NULL };
    struct unimapinit		adv = { 0, 0, 0 };

    fd.charcount = origfont.charcount;
    fd.charheight = origfont.charheight;
    fd.chardata = chardata ? chardata : origfont.chardata;
    if (ioctl(STDOUT_FILENO, PIO_FONTX, &fd))
	return errno;
    ioctl(STDOUT_FILENO, PIO_UNIMAPCLR, &adv);
    ioctl(STDOUT_FILENO, PIO_UNIMAP, &unimap);
    usingfont = chardata != NULL;
    return 0;
}

/*
 * Font mangling functions
 */

/* Mangle the font data for two different characters, pointed to by
 * left and right. image points to an array of height strings, each
 * one 16 characters in length and containing the characters 0, 1, and
 * space. Each string is split down the middle and applied separately
 * to one scanline of left and right. The character 0 turns a pixel
 * off, 1 turns a pixel on, and space leaves the pixel unchanged.
 */
static void imprint(char *left, char *right,
		    char const (*image)[16], int height)
{
    int	off, on, i, j;

    for (i = 0 ; i < height ; ++i) {
	on = 0x00;
	off = 0xFF;
	for (j = 0 ; j < 8 ; ++j) {
	    if (image[i][j] == '0')
		off &= ~(128 >> j);
	    else if (image[i][j] == '1')
		on |= 128 >> j;
	}
	left[i] &= off;
	left[i] |= on;
	on = 0x00;
	off = 0xFF;
	for (j = 0 ; j < 8 ; ++j) {
	    if (image[i][8 + j] == '0')
		off &= ~(128 >> j);
	    else if (image[i][8 + j] == '1')
		on |= 128 >> j;
	}
	right[i] &= off;
	right[i] |= on;
    }
}

/* Draw the floor pattern on the font data at left and right. If
 * goaled is TRUE, the floor pattern for goal cells is used.
 */
static void makeflooring(char *left, char *right, int goaled)
{
    static char const floor[2][4] = {
	{ 0x00, 0x08, 0x06, 0x08 },
	{ 0x00, 0x20, 0xC0, 0x20 }
    };
    static char const goal[5][16] = {
	"      111       ",
	"     1   1      ",
	"    1     1     ",
	"     1   1      ",
	"      111       "
    };
    int	i, j;

    for (i = 0 ; i * 4 < fontheight ; ++i) {
	for (j = 0 ; j < 4 ; ++j) {
	    left[i * 4 + j] = floor[i & 1][j];
	    right[i * 4 + j] = floor[1 - (i & 1)][j];
	}
    }
    if (goaled)
	imprint(left + 4, right + 4, goal, 5);
}

/* Draw the image of a box on the font data at left and right. If
 * goaled is TRUE, the image of a stored box is used.
 */
static void makeboxes(char *left, char *right, int goaled)
{
    static char const boxends[2][16] =
	{ "  111111111111  ",
	  "  000000000000  " };
    static char const boxbody[2][2][16] = {
	{ " 11110010011000 ",
	  " 11001101100100 " },
	{ " 11111111111100 ",
	  " 11111111111100 " }
    };
    int	i, j;

    makeflooring(left, right, goaled);
    j = fontheight / 7;
    for (i = j + 1 ; i < fontheight - j - 1 ; i += 2)
	imprint(left + i, right + i, boxbody[goaled], 2);
    imprint(left + j, right + j, boxends + 0, 1);
    imprint(left + i, right + i, boxends + 1, 1);
}

/* Draw the image of the player on the font data at left and right. If
 * goaled is TRUE, the goal cell is used for the floor pattern behind
 * the player.
 */
static void makeplayers(char *left, char *right, int goaled)
{
    static char const playerbody[1][16] =
	{ "      0110      " };
    static char const playersides[6][16] =
	{ " 00          00 ",
	  "0110        0110",
	  "0111111111111110",
	  "0111111111111110",
	  "0110        0110",
	  " 00          00 " };
    static char const playerends[2][3][16] = {
	{ "     000000     ",
	  "    01111110    ",
	  "     001100     " },
	{ "     001100     ",
	  "    01111110    ",
	  "     000000     " }
    };
    int	i, j;

    makeflooring(left, right, goaled);
    for (i = 0 ; i < fontheight ; ++i)
	imprint(left + i, right + i, playerbody, 1);
    imprint(left, right, playerends[0], 3);
    j = fontheight / 2 - 3;
    imprint(left + j, right + j, playersides, 6);
    j = fontheight - 3;
    imprint(left + j, right + j, playerends[1], 3);
}

/* Draw all the different kinds of wall parts onto selected characters
 * in chardata. The wallchars array contains the actual character
 * indexes that are used.
 */
static void makewalls(char *chardata)
{
    static char const walltop[4][2][16] = {
	{ "1000000000000011",
	  "0000000000000000" },
	{ "1100000000000100",
	  "1100000000000100" },
	{ "1111111111111111",
	  "0000000000000000" },
	{ "1111111111111110",
	  "1100000000000100" }
    };
    static char const wallside[2][1][16] = {
	{ "0000000000000000" },
	{ "1100000000000100" }
    };
    static char const wallbottom[4][2][16] = {
	{ "1000000000000011",
	  "0100000000000101" },
	{ "1100000000000100",
	  "1100000000000100" },
	{ "1111111111111111",
	  "0000000000000000" },
	{ "1111111111111110",
	  "1000000000000001" }
    };
    char      *left, *right;
    int		i, j, k;

    for (i = 0 ; i < 2 ; ++i) {
	for (j = 0 ; j < 4 ; ++j) {
	    left = fontchar(chardata, wallchars[i * 4 + j][0]);
	    right = fontchar(chardata, wallchars[i * 4 + j][1]);
	    imprint(left, right, walltop[((j & 1) << 1) | i], 2);
	    for (k = 2 ; k < fontheight - 2 ; ++k)
		imprint(left + k, right + k, wallside[i], 1);
	    imprint(left + fontheight - 2, right + fontheight - 2,
		    wallbottom[(j & 2) | i], 2);
	}
    }
}

/* Put all of the special character images into chardata.
 */
static void createfontchars(char *chardata)
{
    makeflooring(fontchar(chardata, screencells[0][0]),
		 fontchar(chardata, screencells[0][1]), 0);
    makeflooring(fontchar(chardata, screencells[GOAL][0]),
		 fontchar(chardata, screencells[GOAL][1]), 1);
    makeboxes(fontchar(chardata, screencells[BOX][0]),
	      fontchar(chardata, screencells[BOX][1]), 0);
    makeboxes(fontchar(chardata, screencells[BOX | GOAL][0]),
	      fontchar(chardata, screencells[BOX | GOAL][1]), 1);
    makeplayers(fontchar(chardata, screencells[PLAYER][0]),
		fontchar(chardata, screencells[PLAYER][1]), 0);
    makeplayers(fontchar(chardata, screencells[PLAYER | GOAL][0]),
		fontchar(chardata, screencells[PLAYER | GOAL][1]), 1);
    makewalls(chardata);
}

/*
 * Output functions
 */

/* Switch the terminal into the mode that was in place at startup
 * (presumably cooked mode) if raw is FALSE, or raw mode if raw is
 * TRUE. Note that signals are not turned off, however (what ncurses
 * calls cbreak mode).
 */
static int setrawmode(int raw)
{
    static struct termios	origterm;
    static struct termios	rawterm;
    static int			termloaded = FALSE;

    if (!termloaded) {
	if (tcgetattr(STDIN_FILENO, &origterm))
	    return errno;
	rawterm = origterm;
	rawterm.c_lflag &= ~(ECHO | ICANON);
	termloaded = TRUE;
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, raw ? &rawterm : &origterm))
	return errno;
    inrawmode = raw;
    return 0;
}

/* Retrieve the current screen size and compute the size and placement
 * of the various parts. FALSE is returned if the screen is too small
 * to display anything.
 */
static int measurescreen(void)
{
    struct winsize	size;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size))
	return FALSE;
    lastline = size.ws_row;
    sidebar = size.ws_col - SIDEBARWIDTH;
    fieldheight = size.ws_row + 2;
    fieldwidth = (sidebar - 1) / 2 + 2;
    if (sidebar < 1)
	return FALSE;
    return TRUE;
}

/* Enqueue characters for output formatted according to fmt. If fmt is
 * NULL, the accumulated characters are flushed to the screen. The
 * cursor is always left in the bottom right.
 */
static void out(char const *fmt, ...)
{
    static char	out[8192];
    static int	size = 0;
    va_list	args;

    if (fmt) {
	va_start(args, fmt);
	size += vsprintf(out + size, fmt, args);
	va_end(args);
    } else {
	size += sprintf(out + size, "\033[%d;%dH", lastline,
						   sidebar + SIDEBARWIDTH);
	write(STDOUT_FILENO, out, size);
	size = 0;
    }
}

/* Output a single line's worth of a string without breaking up words.
 */
static int lineout(char const *str, int index)
{
    int	n;

    while (isspace(str[index]))
	++index;
    if (str[index]) {
	n = strlen(str + index);
	if (n < SIDEBARWIDTH) {
	    out("%*s", SIDEBARWIDTH, str + index);
	    index += n;
	} else {
	    if (n > SIDEBARWIDTH)
		n = SIDEBARWIDTH;
	    while (!isspace(str[index + n]) && n >= 0)
		--n;
	    if (n < 0) {
		out("%.*s", SIDEBARWIDTH, str + index);
		index += SIDEBARWIDTH;
		while (str[index] && !isspace(str[index]))
		    ++index;
	    } else {
		out("%*.*s", SIDEBARWIDTH, n, str + index);
		index += n + 1;
	    }
	}
    }
    return index;
}

/* Display two characters directly from the current console font. The
 * user-defined Unicode area U+F000 to U+F0FF is defined on the Linux
 * console to map directly to the current font character. The function
 * handles the conversion to UTF-8.
 */
static void outpair(char const pair[2])
{
    out("\xEF%c%c\xEF%c%c",
	0x80 | ((unsigned char)pair[0] >> 6), 0x80 | (pair[0] & 0x3F),
	0x80 | ((unsigned char)pair[1] >> 6), 0x80 | (pair[1] & 0x3F));
}

/* Display the game state on the console. The map is placed in the
 * upper left corner, and the textual information appears in the upper
 * right corner.
 */
int displaygame(cell const *map, int ysize, int xsize,
		int recording, int macro, int save,
		char const *seriesname, char const *levelname, int index,
		int boxcount, int storecount, int movecount, int pushcount,
		int bestmovecount, int bestpushcount)
{
    char	buf[SIDEBARWIDTH + 1];
    cell const *p;
    int		done = 0;
    int		nameindex = 0;
    int		y, x;

    out("\033[H");

    if (ysize > fieldheight || xsize > fieldwidth) {
	out("Level %d won't fit on the screen.\033[J", index);
	out(NULL);
	return FALSE;
    }

    p = map;
    for (y = 1 ; done != DONE_ALL && y <= lastline ; ++y) {
	p += XSIZE;
	if (y > 1)
	    out("\n");
	if (y < ysize - 1) {
	    for (x = 1 ; x < xsize - 1 ; ++x) {
		if (p[x] == EMPTY)
		    out("  ");
		else if (p[x] & WALL)
		    outpair(wallcells[p[x] >> 4]);
		else
		    outpair(screencells[p[x] & 0x0F]);
	    }
	} else {
	    x = 1;
	    done |= DONE_MAP;
	}
	switch (y) {
	  case 1:
	    sprintf(buf, "# %d", index);
	    out("%*s", sidebar + SIDEBARWIDTH - x * 2, buf);
	    break;
	  case 2:
	    out("%*s Boxes: %-3d", sidebar - x * 2, "", boxcount);
	    if (recording)
		out("    R");
	    else if (macro)
		out("    M");
	    break;
	  case 3:
	    out("%*sStored: %-3d", sidebar - x * 2, "", storecount);
	    if (save)
		out("    S");
	    break;
	  case 4:
	    out("%*s Moves: %d", sidebar - x * 2, "", movecount);
	    break;
	  case 5:
	    out("%*sPushes: %d", sidebar - x * 2, "", pushcount);
	    break;
	  case 6:
	    if (bestmovecount && bestpushcount) {
		out("%*s  Best: %d", sidebar - x * 2, "", bestmovecount);
		if (bestmovecount < 100000)
		    out(" moves");
	    }
	    break;
	  case 7:
	    if (bestmovecount && bestpushcount) {
		out("%*s", sidebar - x * 2, "");
		if (bestpushcount < 10000)
		    out("        %d pushes", bestpushcount);
		else
		    out("     %7d pushes", bestpushcount);
	    }
	    break;
	  case 8:
	    done |= DONE_STATS;
	    nameindex = 0;
	    break;
	  default:
	    if (!(done & DONE_SERIESNAME)) {
		if (seriesname && seriesname[nameindex]) {
		    out("%*s", sidebar - x * 2, "");
		    nameindex = lineout(seriesname, nameindex);
		} else {
		    done |= DONE_SERIESNAME;
		    nameindex = 0;
		}
	    } else if (!(done & DONE_LEVELNAME)) {
		if (levelname && levelname[nameindex]) {
		    out("%*s", sidebar - x * 2, "");
		    nameindex = lineout(levelname, nameindex);
		} else {
		    done |= DONE_LEVELNAME;
		    nameindex = 0;
		}
	    }
	    break;
	}
	out("\033[K");
    }

    out("\033[J");
    out(NULL);
    return TRUE;
}

/* Display information about the various key commands. Each element in
 * the keys array contains two sequential NUL-terminated strings,
 * describing the key and the associated command respectively. The
 * strings are arranged to line up into two columns.
 */
void displayhelp(char const *keys[], int keycount)
{
    int keywidth, descwidth;
    int	i, n;

    keywidth = 1;
    descwidth = 0;
    for (i = 0 ; i < keycount ; ++i) {
	n = strlen(keys[i]);
	if (n > keywidth)
	    keywidth = n;
	n = strlen(keys[i] + n + 1);
	if (n > descwidth)
	    descwidth = n;
    }

    out("\033[H");
    n = keywidth + descwidth + 2;
    if (n < 4)
	n = 4;
    out("%*s\033[K\n", n / 2 + 2, "Keys");
    for (i = 0 ; i < n ; ++i)
	out("\xE2\x94\x80");
    out("\033[K\n");
    for (i = 0 ; i < keycount ; ++i) {
	n = strlen(keys[i]);
	out("%-*s  %-*s\033[K\n", keywidth, keys[i],
				  descwidth, keys[i] + n + 1);
    }

    out("\033[J\n\nPress any key to return.");
    out(NULL);
}

/* Display a closing message appropriate to the completion of a
 * puzzle, or the completion of the last puzzle.
 */
void displayendmessage(int endofsession)
{
    out("\033[%d;%dHPress ENTER\033[%d;%dH%s",
	lastline - 1, sidebar + 1, lastline, sidebar + 1,
	endofsession ? "  to exit" : "to continue");
    out(NULL);
}

/*
 * Input functions
 */

/* Read a single character. If redrawrequest has been set to TRUE,
 * reset the display and return a faked Ctrl-L.
 */
static int getkey(void)
{
    unsigned char	in;
    int			n;

    for (;;) {
	n = read(STDIN_FILENO, &in, 1);
	if (n > 0)
	    return in;
	else if (n == 0 || errno != EINTR)
	    return EOF;
	else if (redrawrequest) {
	    redrawrequest = FALSE;
	    erasescreen();
	    measurescreen();
	    return '\f';
	}
    }
}

/* Retrieve and return a single keystroke. Arrow-key sequences are
 * translated into their vi counterparts; all other ESC-prefixed
 * sequences are ignored as such.
 */
int input(void)
{
    static int	lastkey = 0;
    int		key;

    if (lastkey) {
	key = lastkey;
	lastkey = 0;
    } else {
      getnewkey:
	key = getkey();
	if (key == '\033') {
	    lastkey = getkey();
	    if (lastkey == '[' || lastkey == 'O') {
		lastkey = 0;
		switch (getkey()) {
		  case 'A':	return 'k';
		  case 'B':	return 'j';
		  case 'C':	return 'l';
		  case 'D':	return 'h';
		  default:	ding();	goto getnewkey;
		}
	    }
	}
    }
    return key;
}

/*
 * Top-level functions
 */

/* Alter the console I/O for use by the program.
 */
static int startup(void)
{
    if (setfontdata(newfontdata))
	return FALSE;
    if (setrawmode(TRUE))
	return FALSE;
    erasescreen();
    return TRUE;
}

/* Reset the console to the state the program found it in.
 */
static void shutdown(void)
{
    if (usingfont) {
	erasescreen();
	setfontdata(NULL);
    }
    if (inrawmode)
	setrawmode(FALSE);
}

/* Handler for signals that require the program to reset the console
 * and reinitialize it from scratch. The signal is chained to avoid
 * suppressing any default behavior.
 */
static void chainandredraw(int signum)
{
    sigset_t	mask, prevmask;

    shutdown();
    sigemptyset(&mask);
    sigaddset(&mask, signum);
    sigprocmask(SIG_UNBLOCK, &mask, &prevmask);
    sigaction(signum, prevhandler + signum, NULL);
    raise(signum);
    sigaction(signum, myhandler + signum, NULL);
    sigprocmask(SIG_SETMASK, &prevmask, NULL);
    startup();
    redrawrequest = TRUE;
}

/* Handler for signals that require the program to exit. The console
 * is returned to its original state and the signal is chained to
 * produce the default action.
 */
static void bailout(int signum)
{
    sigset_t	mask;

    shutdown();
    sigemptyset(&mask);
    sigaddset(&mask, signum);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    sigaction(signum, prevhandler + signum, NULL);
    raise(signum);
}

/* Install signal handlers and initialize the user interface. Make
 * sure the program is running on a Linux console, and the console
 * screen is not too small.
 */
int ioinitialize(int silenceflag)
{
    struct sigaction	act;

    silence = silenceflag;

    if (!measurescreen()) {
	if (errno)
	    die("Couldn't access the console!");
	else
	    die("The console screen is too darned small!");
    }

    atexit(shutdown);

    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    act.sa_handler = chainandredraw;
    myhandler[SIGTSTP] = act;
    sigaction(SIGTSTP, &act, prevhandler + SIGTSTP);
    myhandler[SIGWINCH] = act;
    sigaction(SIGWINCH, &act, prevhandler + SIGWINCH);

    act.sa_handler = bailout;
    sigaction(SIGINT, &act, prevhandler + SIGINT);
    sigaction(SIGQUIT, &act, prevhandler + SIGQUIT);
    sigaction(SIGTERM, &act, prevhandler + SIGTERM);
    sigaction(SIGSEGV, &act, prevhandler + SIGSEGV);
    sigaction(SIGPIPE, &act, prevhandler + SIGPIPE);
    sigaction(SIGILL, &act, prevhandler + SIGILL);
    sigaction(SIGBUS, &act, prevhandler + SIGBUS);
    sigaction(SIGFPE, &act, prevhandler + SIGFPE);

    if (getfontdata())
	return fileerr(NULL);
    if (!(newfontdata = malloc(fullfontsize)))
	memerrexit();
    memcpy(newfontdata, origfont.chardata, fullfontsize);
    createfontchars(newfontdata);

    return startup();
}

/*
 * Miscellaneous interface functions
 */

/* Ring the bell.
 */
void ding(void)
{
    if (!silence)
	write(STDERR_FILENO, "\a", 1);
}

/* Display an appropriate error message on stderr; use msg if errno
 * is zero.
 */
int fileerr(char const *msg)
{
    fputc('\n', stderr);
    fputs(currentfilename ? currentfilename : programname, stderr);
    fputs(": ", stderr);
    fputs(msg ? msg : errno ? strerror(errno) : "unknown error", stderr);
    fputc('\n', stderr);
    return FALSE;
}

/* Display a formatted message on stderr and exit cleanly.
 */
void die(char const *fmt, ...)
{
    va_list	args;

    shutdown();
    va_start(args, fmt);
    fprintf(stderr, "%s: ", programname);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    exit(EXIT_FAILURE);
}
