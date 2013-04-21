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
#include 	"cblocks.h"
#include	"userio.h"

#ifdef NOMOUSE

/* Stubs and empty macros to provide a no-op mouse interface.
 */
#define	mousegetchar	getchar
#define	openmouse()	FALSE
#define	closemouse()	((void)0)

static int mousefd = -1;

#else

/* The GPM mouse interface.
 */
#include	<gpm.h>

/* Aliases for GPM internals.
 */
#define	mousefd		gpm_fd
#define	mousegetchar	Gpm_Getchar

/* The parameters of the gpm hook.
 */
static Gpm_Connect	gpmconnection;
#define	openmouse()	(gpm_zerobased = TRUE, \
			 gpm_handler = mousehandler, \
			 gpmconnection.eventMask = GPM_DOWN|GPM_DRAG|GPM_UP, \
			 gpmconnection.defaultMask = GPM_MOVE | GPM_HARD, \
			 gpmconnection.minMod = 0, \
			 gpmconnection.maxMod = 0, \
			 Gpm_Open(&gpmconnection, 0) >= 0)
#define	closemouse()	(Gpm_Close())

#endif


/* If NSIG is not provided by signal.h, blithely assume 32 is big enough.
 */
#ifndef	NSIG
#define	NSIG		32
#endif

/* The predefined height of the logical console font.
 */
#define	FONTHEIGHT	32

/* The number of columns we reserve on the right side of the display
 * for text.
 */
#define	SIDEBARWIDTH	20

/* Flags to indicate which parts of the screen have been completed.
 */
#define	DONE_MAP	0x01
#define	DONE_STATS	0x02
#define	DONE_SERIESNAME	0x04
#define	DONE_LEVELNAME	0x08
#define	DONE_ALL	0x0F

/* Attributes used in the translation of the map to its display.
 */
#define	ATTR_NORTH	0x0001
#define	ATTR_EAST	0x0002
#define	ATTR_SOUTH	0x0004
#define	ATTR_WEST	0x0008
#define	ATTR_SHAPE	0x000F
#define	ATTR_WHITEBLOCK	0x0990
#define	ATTR_COLORBLOCK	0x0090
#define	ATTR_FRAME	0x0640
#define	ATTR_GOAL	0x2990
#define	ATTR_CLOSEDOOR	0x0190
#define	ATTR_OPENDOOR	0x0290
#define	ATTR_SELECTED	0x1000

/* Extract the different aspects of an attribute.
 */
#define	attr_level(a)		(((a) >> 12) & 7)
#define	attr_fgcolor(a)		(((a) >> 8) & 15)
#define	attr_bgcolor(a)		(((a) >> 4) & 15)
#define	attr_colors(f, b)	((((f) & 15) << 8) | (((b) & 15) << 4))

/* Given a console font and a character, calculate a pointer to the
 * data for that character.
 */
#define	fontchar(font, ch)	((font) + FONTHEIGHT * (unsigned char)(ch))

/* Reset and erase the console screen.
 */
#define	erasescreen()		(write(STDOUT_FILENO, "\033c\033[H\033[J", 8))

/* The following array contains the character numbers in the console
 * font that will be overwritten with images. The numbers were chosen
 * as being relatively unlikely to collide with anything that will be
 * used while the program is running. (They correspond to the mixed
 * single-and-double line-drawing characters in the PC character set.)
 * However, since a console font can have arbitrary mapping, there's
 * no guaranteee that these characters don't contain accented letters,
 * or even part of the ASCII subset. A better approach might be to
 * read the currently installed map and choose characters that are not
 * being used for anything in the Latin-1 range. However, this could
 * still present a problem if the user is using (say) Russian in the
 * other VCs. There is no good general solution; thus the relatively
 * simple approach used here.
 */

/* The indexes of the characters in the font that will be replaced
 * with images for displaying various parts of objects.
 */
static char const linechars[][2] = {
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
 * containing some kind of object.
 */
static char const linecells[][2] = {
    { 210, 190 },	/* isolated object */
    { 209, 189 },	/* object extends N */
    { 210, 206 },	/* object extends E */
    { 209, 213 },	/* object extends N E */
    { 208, 214 },	/* object extends S */
    { 207, 188 },	/* object extends N S */
    { 208, 212 },	/* object extends S E */
    { 207, 211 },	/* object extends N S E */
    { 206, 190 },	/* object extends W */
    { 205, 189 },	/* object extends N W */
    { 206, 206 },	/* object extends W E */
    { 205, 213 },	/* object extends N W E */
    { 204, 214 },	/* object extends S W */
    { 203, 188 },	/* object extends N S W */
    { 204, 212 }, 	/* object extends S W E */
    { 203, 211 }	/* object extends N S W E */
};

/* TRUE if the terminal is currently in raw mode.
 */
static int			inrawmode = FALSE;

/* TRUE if the console font has been changed.
 */
static int			usingfont = FALSE;

/* TRUE if the program has hooked into the mouse.
 */
static int			usingmouse = FALSE;

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
    int			i, j, k;

    for (i = 0 ; i < 2 ; ++i) {
	for (j = 0 ; j < 4 ; ++j) {
	    left = fontchar(chardata, linechars[i * 4 + j][0]);
	    right = fontchar(chardata, linechars[i * 4 + j][1]);
	    imprint(left, right, walltop[((j & 1) << 1) | i], 2);
	    for (k = 2 ; k < fontheight - 2 ; ++k)
		imprint(left + k, right + k, wallside[i], 1);
	    imprint(left + fontheight - 2, right + fontheight - 2,
		    wallbottom[(j & 2) | i], 2);
	}
    }
}

/*
 * Output functions
 */

/* Translate a general RGB value (each number in the range 0-255)
 * to one of the eight available colors.
 */
char getrgbindex(int r, int g, int b)
{
    return (r >= 96 ? 1 : 0) | (g >= 96 ? 2 : 0) | (b >= 96 ? 4 : 0);
}

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
    fieldheight = size.ws_row;
    fieldwidth = (sidebar - 1) / 2 - 2;
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

/* Copy the map into screengrid, translating cell contents into
 * display attributes.
 */
static void maptogrid(short screengrid[MAXHEIGHT][MAXWIDTH],
		      cell const *map, char const *colors,
		      int ysize, int xsize, int currblock, int movecount)
{
    cell const *p;
    int		y, x, n;

    memset(screengrid, 0, MAXHEIGHT * MAXWIDTH * sizeof screengrid[0][0]);
    p = map;
    for (y = 0 ; y < ysize ; ++y, p += XSIZE) {
	for (x = 0 ; x < xsize ; ++x) {
	    n = blockid(p[x]);
	    if (n) {
		if (n == WALLID)
		    screengrid[y][x] = ATTR_FRAME;
		else {
		    if (colors[n])
			screengrid[y][x] = attr_colors(colors[n], 9);
		    else
			screengrid[y][x] = ATTR_WHITEBLOCK;
		    if (currblock && n == currblock)
			screengrid[y][x] |= ATTR_SELECTED;
		}
		if (p[x] & EXTENDNORTH)
		    screengrid[y][x] |= ATTR_NORTH;
		if (p[x] & EXTENDEAST)
		    screengrid[y][x] |= ATTR_EAST;
		if (p[x] & EXTENDSOUTH)
		    screengrid[y][x] |= ATTR_SOUTH;
		if (p[x] & EXTENDWEST)
		    screengrid[y][x] |= ATTR_WEST;
	    } else if (p[x] & (GOAL | DOORSTAMP_MASK)) {
		if (p[x] & GOAL)
		    screengrid[y][x] = ATTR_GOAL;
		else if (doortime(p[x]) > movecount)
		    screengrid[y][x] = ATTR_CLOSEDOOR;
		else
		    screengrid[y][x] = ATTR_OPENDOOR;
		if (p[x] & FEXTENDNORTH)
		    screengrid[y][x] |= ATTR_NORTH;
		if (p[x] & FEXTENDEAST)
		    screengrid[y][x] |= ATTR_EAST;
		if (p[x] & FEXTENDSOUTH)
		    screengrid[y][x] |= ATTR_SOUTH;
		if (p[x] & FEXTENDWEST)
		    screengrid[y][x] |= ATTR_WEST;
	    }
	}
    }
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
		char const *seriesname, char const *levelname, int index,
		char const *colors, int currblock, int ycursor, int xcursor,
		int saves, int movecount, int stepcount,
		int beststepcount, int bestmovecount, int beststepknown)
{
    static char const  *levelstr[] = { "22", "1", "2" };
    static short	screengrid[MAXHEIGHT][MAXWIDTH];
    char		buf[SIDEBARWIDTH + 1];
    short	       *p;
    int			done;
    int			attr, lastattr;
    int			nameindex = 0;
    int			y, x;

    out("\033[H");

    if (ysize > fieldheight || xsize > fieldwidth) {
	out("Level %d won't fit on the screen.\033[J", index);
	out(NULL);
	return FALSE;
    }

    maptogrid(screengrid, map, colors, ysize, xsize, currblock, movecount);

    p = screengrid[0];
    for (y = 0, done = 0 ; done != DONE_ALL && y <= lastline ; ++y) {
	if (y)
	    out("\n");
	if (y < ysize) {
	    lastattr = 0;
	    for (x = 0 ; x < xsize ; ++x) {
		attr = screengrid[y][x] & ~ATTR_SHAPE;
		if (!attr) {
		    out(lastattr ? "\033[39;49m  " : "  ");
		    lastattr = 0;
		    continue;
		}
		if (lastattr != attr) {
		    out("\033[%s;3%c;4%cm", levelstr[attr_level(attr)],
					    '0' + attr_fgcolor(attr),
					    '0' + attr_bgcolor(attr));
		    lastattr = attr;
		}
		outpair(linecells[screengrid[y][x] & ATTR_SHAPE]);
	    }
	    out("\033[22;39;49m");
	} else {
	    x = 0;
	    done |= DONE_MAP;
	}

	switch (y) {
	  case 0:
	    if (index) {
		sprintf(buf, "# %d", index);
		out("%*s", sidebar + SIDEBARWIDTH - x * 2 - 1, buf);
	    }
	    break;
	  case 1:
	    out("%*s  Steps: %d", sidebar - x * 2 - 1, "", stepcount);
	    if (saves)
		out("    S");
	    break;
	  case 2:
	    out("%*s  Moves: %d", sidebar - x * 2 - 1, "", movecount);
	    break;
	  case 3:
	    if (beststepknown)
		out("%*sMinimum: %d steps", sidebar - x * 2 - 1, "",
					    beststepknown);
	    break;
	  case 4:
	    if (beststepcount) {
		out("%*s   Best: %d", sidebar - x * 2 - 1, "", beststepcount);
		if (beststepcount < 10000)
		    out(" steps");
	    }
	    break;
	  case 5:
	    if (beststepcount && bestmovecount) {
		if (bestmovecount < 10000)
		    out("%*s%d moves", sidebar - x * 2 + 8, "", bestmovecount);
		else
		    out("%*d moves", sidebar - x * 2 + 12, bestmovecount);
	    }
	    break;
	  case 6:
	    done |= DONE_STATS;
	    nameindex = 0;
	    break;
	  default:
	    if (!(done & DONE_SERIESNAME)) {
		if (seriesname && seriesname[nameindex]) {
		    out("%*s", sidebar - x * 2 - 1, "");
		    nameindex = lineout(seriesname, nameindex);
		} else {
		    done |= DONE_SERIESNAME;
		    nameindex = 0;
		}
	    } else if (!(done & DONE_LEVELNAME)) {
		if (levelname && levelname[nameindex]) {
		    out("%*s", sidebar - x * 2 - 1, "");
		    nameindex = lineout(levelname, nameindex);
		} else {
		    done |= DONE_LEVELNAME;
		    nameindex = 0;
		}
	    }
	    break;
	}
	p += XSIZE;
	out("\033[K");
    }

    out("\033[J");
    if (ycursor && xcursor)
	out("\033[%d;%dH", ycursor + 1, xcursor * 2 + 1);
    else
	out("\033[%d;%dH", lastline, sidebar + SIDEBARWIDTH);
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
    fd_set		in, empty;
    int			max, n;
    unsigned char	ch;

    for (;;) {
	if (mousefd >= 0) {
	    FD_ZERO(&in);
	    FD_ZERO(&empty);
	    FD_SET(STDIN_FILENO, &in);
	    FD_SET(mousefd, &in);
	    max = (STDIN_FILENO > mousefd ? STDIN_FILENO : mousefd) + 1;
	    n = select(max, &in, &empty, &empty, NULL);
	    if (n > 0)
		return mousegetchar();
	    else if (n == 0 || errno != EINTR)
		return EOF;
	} else {
	    n = read(STDIN_FILENO, &ch, 1);
	    if (n > 0)
		return ch;
	    else if (n == 0 || errno != EINTR)
		return EOF;
	}
	if (redrawrequest) {
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
		  case 'A':	return ARROW_N;
		  case 'B':	return ARROW_S;
		  case 'C':	return ARROW_E;
		  case 'D':	return ARROW_W;
		  default:	ding();	goto getnewkey;
		}
	    }
	}
    }
    return key;
}

#ifndef NOMOUSE

/* Handle mouse activity. The screen coordinates are translated into a
 * field's cell position and mousecallback() is called. The return
 * value supplies the keystroke to translate the mouse event into, or
 * zero if no keystroke should be generated.
 */
static int mousehandler(Gpm_Event *event, void *clientdata)
{
    int	state;

    (void)clientdata;
    GPM_DRAWPOINTER(event);
    if ((event->type & GPM_DOWN) && (event->buttons & GPM_B_LEFT))
	state = -1;
    else if ((event->type & GPM_UP) && (event->buttons & GPM_B_LEFT))
	state = +1;
    else if ((event->type & GPM_DOWN) && (event->buttons & GPM_B_RIGHT))
	state = -2;
    else if ((event->type & GPM_UP) && (event->buttons & GPM_B_RIGHT))
	state = +2;
    else if ((event->type & GPM_DRAG))
	state = 0;
    else
	return 0;
    return mousecallback(event->y, event->x / 2, state);
}

#endif

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
    usingmouse = openmouse();
    erasescreen();
    return TRUE;
}

/* Reset the console to the state the program found it in.
 */
static void shutdown(void)
{
    if (usingmouse) {
	closemouse();
	usingmouse = FALSE;
    }
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

/* Install signal handlers and initialize the console. Make sure the
 * program is running on a Linux console, and the console screen is
 * not too small.
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
    makewalls(newfontdata);

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
    fprintf(stderr, "\n%s: ", programname);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    exit(EXIT_FAILURE);
}
