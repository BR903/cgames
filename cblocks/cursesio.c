/* cursesio.c: User interface functions for the ncurses library.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<signal.h>
#include	<errno.h>
#include	<ncurses.h>
#include	<unistd.h>
#include	"gen.h"
#include	"cblocks.h"
#include	"userio.h"

#define	F_NORTH		(1 << NORTH)
#define	F_EAST		(1 << EAST)
#define	F_SOUTH		(1 << SOUTH)
#define	F_WEST		(1 << WEST)

/* The number of columns we reserve on the right side of the display
 * for text.
 */
#define	SIDEBARWIDTH	18

static attr_t		frameattr, goalattr, selectattr,
			closedoorattr, opendoorattr;

/* The dimensions of the largest map that can be displayed.
 */
static int		fieldheight, fieldwidth;

/* The index of the bottommost line.
 */
static int		lastline;

/* The index of the leftmost column in the textual display area.
 */
static int		sidebar;

/* FALSE if the program is allowed to ring the bell.
 */
static int		silence = FALSE;

/* The name of the program and the file currently being accessed,
 * for use in error messages (declared in gen.h).
 */
char const	       *programname = NULL;
char const	       *currentfilename = NULL;

/*
 * Input and output functions
 */

/* Translate a general RGB value (each number in the range 0-255)
 * to one of the eight available colors.
 */
char getrgbindex(int r, int g, int b)
{
    return (r >= 96 ? 1 : 0) | (g >= 96 ? 2 : 0) | (b >= 96 ? 4 : 0);
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
	    printw("%*s", SIDEBARWIDTH, str + index);
	    index += n;
	} else {
	    if (n > SIDEBARWIDTH)
		n = SIDEBARWIDTH;
	    while (!isspace(str[index + n]) && n >= 0)
		--n;
	    if (n < 0) {
		printw("%.*s", SIDEBARWIDTH, str + index);
		index += SIDEBARWIDTH;
		while (str[index] && !isspace(str[index]))
		    ++index;
	    } else {
		printw("%*.*s", SIDEBARWIDTH, n, str + index);
		index += n + 1;
	    }
	}
    }
    return index;
}

static void drawcell(int y, int x, int ext, attr_t attr)
{
    static chtype	acschar[16];
    int			screencell[2][4] = { { 0, 0, 0, 0 }, { 0, 0, 0, 0 } };

    if (!acschar[0]) {
	acschar[0] = ' ';
	acschar[F_NORTH] = acschar[F_SOUTH] =
			   acschar[F_NORTH | F_SOUTH] = ACS_VLINE;
	acschar[F_EAST] = acschar[F_WEST] =
			  acschar[F_EAST | F_WEST] = ACS_HLINE;
	acschar[F_NORTH | F_EAST] = ACS_LLCORNER;
	acschar[F_SOUTH | F_EAST] = ACS_ULCORNER;
	acschar[F_NORTH | F_WEST] = ACS_LRCORNER;
	acschar[F_SOUTH | F_WEST] = ACS_URCORNER;
	acschar[F_NORTH | F_SOUTH | F_EAST] = ACS_LTEE;
	acschar[F_NORTH | F_SOUTH | F_WEST] = ACS_RTEE;
	acschar[F_NORTH | F_EAST | F_WEST] = ACS_BTEE;
	acschar[F_SOUTH | F_EAST | F_WEST] = ACS_TTEE;
	acschar[F_NORTH | F_SOUTH | F_EAST | F_WEST] = ACS_PLUS;
    }

    if (!(ext & EXTENDWEST)) {
	screencell[0][0] |= F_SOUTH;
	screencell[1][0] |= F_NORTH;
    } else {
	if (ext & EXTENDNORTH)
	    screencell[0][0] |= F_NORTH | F_WEST;
	if (ext & EXTENDSOUTH)
	    screencell[1][0] |= F_SOUTH | F_WEST;
    }
    if (!(ext & EXTENDEAST)) {
	screencell[0][2] |= F_SOUTH;
	screencell[1][2] |= F_NORTH;
    } else {
	if (ext & EXTENDNORTH) {
	    screencell[0][2] |= F_NORTH | F_EAST;
	    screencell[0][3] |= F_EAST | F_WEST;
	}
	if (ext & EXTENDSOUTH) {
	    screencell[1][2] |= F_SOUTH | F_EAST;
	    screencell[1][3] |= F_EAST | F_WEST;
	}
    }
    if (!(ext & EXTENDNORTH)) {
	screencell[0][0] |= F_EAST;
	screencell[0][1] |= F_EAST | F_WEST;
	screencell[0][2] |= F_WEST;
	if (ext & EXTENDEAST) {
	    screencell[0][2] |= F_EAST;
	    screencell[0][3] |= F_WEST;
	}
    }
    if (!(ext & EXTENDSOUTH)) {
	screencell[1][0] |= F_EAST;
	screencell[1][1] |= F_EAST | F_WEST;
	screencell[1][2] |= F_WEST;
	if (ext & EXTENDEAST) {
	    screencell[1][2] |= F_EAST;
	    screencell[1][3] |= F_WEST;
	}
    }

    mvaddch(y, x, acschar[screencell[0][0]] | attr);
    addch(acschar[screencell[0][1]] | attr);
    addch(acschar[screencell[0][2]] | attr);
    addch(acschar[screencell[0][3]] | attr);
    mvaddch(y + 1, x, acschar[screencell[1][0]] | attr);
    addch(acschar[screencell[1][1]] | attr);
    addch(acschar[screencell[1][2]] | attr);
    addch(acschar[screencell[1][3]] | attr);
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
    cell const *p;
    char	buf[SIDEBARWIDTH + 1];
    attr_t	attr;
    int		ext;
    int		y, x, n;

    erase();

    if (ysize > fieldheight || xsize > fieldwidth) {
	mvprintw(0, 0, "Level %d won't fit on the screen.", index);
	refresh();
	return FALSE;
    }

    p = map;
    for (y = 0 ; y < ysize ; ++y, p += XSIZE) {
	for (x = 0 ; x < xsize ; ++x) {
	    if (!p[x])
		continue;
	    if ((n = blockid(p[x]))) {
		if (n == WALLID)
		    attr = frameattr;
		else {
		    attr = colors[n] ? COLOR_PAIR(colors[n]) : A_NORMAL;
		    if (n == currblock)
			attr |= selectattr;
		}
		ext = p[x] & (EXTENDNORTH | EXTENDEAST |
			      EXTENDSOUTH | EXTENDWEST);
	    } else {
		if (p[x] & GOAL)
		    attr = goalattr;
		else if (doortime(p[x]) > movecount)
		    attr = closedoorattr;
		else
		    attr = opendoorattr;
		ext = (p[x] & (FEXTENDNORTH | FEXTENDEAST |
			       FEXTENDSOUTH | FEXTENDWEST)) >> 4;
	    }
	    if (attr != A_INVIS)
		drawcell(y * 2, x * 4, ext, attr);
	}
    }

    sprintf(buf, "# %d", index);
    mvprintw(0, sidebar, "%*s", SIDEBARWIDTH, buf);
    mvprintw(1, sidebar, "Steps: %d", stepcount);
    mvprintw(2, sidebar, "Moves: %d", movecount);
    if (beststepknown)
	mvprintw(3, sidebar, " Min.: %d steps", beststepknown);
    if (beststepcount) {
	mvprintw(4, sidebar, " Best: %d", beststepcount);
	if (beststepcount < 100000)
	    addstr(" steps");
	if (bestmovecount) {
	    if (bestmovecount < 100000)
		mvprintw(5, sidebar + 7, "%d moves", bestmovecount);
	    else
		mvprintw(5, sidebar + 4, "%8d moves", bestmovecount);
	}
    }

    y = 7;
    if (seriesname) {
	for (n = 0 ; seriesname[n] && y <= lastline ; ++y) {
	    move(y, sidebar);
	    n = lineout(seriesname, n);
	}
	++y;
    }
    if (levelname) {
	for (n = 0 ; levelname[n] && y <= lastline ; ++y) {
	    move(y, sidebar);
	    n = lineout(levelname, n);
	}
    }

    if (ycursor && xcursor)
	move(ycursor * 2, xcursor * 4 + 1);
    else
	move(lastline, sidebar + SIDEBARWIDTH - 1);

    refresh();
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

    erase();
    n = keywidth + descwidth + 2;
    if (n < 4)
	n = 4;
    mvaddstr(0, n / 2 + 2, "Keys");
    for (i = 0 ; i < n ; ++i)
	mvaddch(1, i, ACS_HLINE);
    for (i = 0 ; i < keycount ; ++i) {
	n = strlen(keys[i]);
	mvaddstr(i + 2, 0, keys[i]);
	mvaddstr(i + 2, keywidth + 2, keys[i] + n + 1);
    }

    mvaddstr(lastline, 0, "Press any key to return.");
    refresh();
    clearok(stdscr, TRUE);
}

/* Display a closing message appropriate to the completion of a
 * puzzle, or the completion of the last puzzle.
 */
void displayendmessage(int endofseries)
{
    mvaddstr(lastline - 1, sidebar + 1, "Press ENTER");
    mvaddstr(lastline, sidebar + 1, endofseries ? "  to exit  "
						: "to continue");
    refresh();
}

/*
 * Input functions
 */

static int mousehandler(void)
{
    static int	tracking = FALSE;
    MEVENT	event;
    int		state;

    if (getmouse(&event) != OK)
	return 0;

    if (event.bstate & BUTTON1_PRESSED) {
	tracking = TRUE;
	state = -1;
    } else if (event.bstate & BUTTON1_RELEASED) {
	tracking = FALSE;
	state = +1;
    } else if (tracking)
	state = 0;
    else
	return 0;

    return mousecallback(event.y / 2, event.x / 4, state);
}

/* Retrieve and return a single keystroke. Arrow keys and other inputs
 * are translated into ASCII equivalents.
 */
int input(void)
{
    for (;;) {
	int key = getch();
	switch (key) {
	  case KEY_UP:		return ARROW_N;
	  case KEY_DOWN:	return ARROW_S;
	  case KEY_LEFT:	return ARROW_W;
	  case KEY_RIGHT:	return ARROW_E;
	  case KEY_ENTER:	return '\n';
	  case '\r':		return '\n';
	  case KEY_BACKSPACE:	return '\b';
	  case KEY_MOUSE:
	    if (!(key = mousehandler()))
		continue;
	    break;
	  case '\f':
	    clearok(stdscr, TRUE);
	    break;
	}
	return key;
    }
}

/*
 * Initialization functions
 */

/* Examine the terminal's capabilities (i.e., color and highlighting)
 * and decide how the elements of the game should be displayed.
 */
static void selectrepresentation(void)
{
    attr_t	attr;

    if (has_colors()) {
	int	pair;
	short	frgnd, bkgnd;

	start_color();
	attr_get(&attr, &pair, NULL);
	pair_content(pair, &frgnd, &bkgnd);
	init_pair(0, COLOR_BLACK, bkgnd);
	init_pair(1, COLOR_RED, bkgnd);
	init_pair(2, COLOR_GREEN, bkgnd);
	init_pair(3, COLOR_YELLOW, bkgnd);
	init_pair(4, COLOR_BLUE, bkgnd);
	init_pair(5, COLOR_MAGENTA, bkgnd);
	init_pair(6, COLOR_CYAN, bkgnd);
	init_pair(7, COLOR_WHITE, bkgnd);
	frameattr = COLOR_PAIR(3);
	goalattr = COLOR_PAIR(4);
	closedoorattr = COLOR_PAIR(1);
	opendoorattr = COLOR_PAIR(2);
	if (termattrs() & A_DIM)
	    goalattr |= A_DIM;
    } else {
	frameattr = A_NORMAL;
	closedoorattr = A_NORMAL;
	goalattr = opendoorattr = termattrs() & A_DIM ? A_DIM : A_INVIS;
    }

    selectattr = termattrs() & A_BOLD ? A_BOLD : A_STANDOUT;
}

/* Reset the terminal state and turn off ncurses.
 */
static void shutdown(void)
{
    if (!isendwin()) {
	clear();
	refresh();
	endwin();
    }
}

/* Prepare the user interface, change the terminal modes, and ensure
 * that the terminal has enough room to display anything.
 */
int ioinitialize(int silenceflag)
{
    int	y, x;

    silence = silenceflag;

    atexit(shutdown);
    if (!initscr())
	die("Couldn't initialize the console!");

    getmaxyx(stdscr, y, x);
    lastline = y - 1;
    sidebar = x - SIDEBARWIDTH;
    fieldheight = y / 2;
    fieldwidth = sidebar / 4;
    if (sidebar < 1)
	die("The terminal screen is too darned small!");

    nonl();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    mousemask(BUTTON1_PRESSED | BUTTON1_RELEASED | REPORT_MOUSE_POSITION,
	      NULL);

    selectrepresentation();

    return TRUE;
}

/*
 * Miscellaneous interface functions
 */

/* Ring the bell.
 */
void ding(void)
{
    if (!silence)
	beep();
}

/* Display an appropriate error message on stderr; use msg if errno
 * is zero.
 */
int fileerr(char const *msg)
{
    if (!isendwin()) {
	endwin();
	fputc('\n', stderr);
    }
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

    if (!isendwin()) {
	endwin();
	fputc('\n', stderr);
    }
    va_start(args, fmt);
    fprintf(stderr, "%s: ", programname);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    exit(EXIT_FAILURE);
}
