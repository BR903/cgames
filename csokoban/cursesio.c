/* cursesio.c: User interface functions for the ncurses library.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<errno.h>
#include	<ncurses.h>
#include	<unistd.h>
#include	"gen.h"
#include	"csokoban.h"
#include	"userio.h"

/* The number of columns we reserve on the right side of the display
 * for text.
 */
#define	SIDEBARWIDTH	18

/* The characters used to draw the cells of the map.
 */
static chtype screencells[][2] = {
    { ' ', ' ' },	/* EMPTY */
    { ':', ':' },	/* GOAL	*/
    { '>', '<' },	/* PLAYER */
    { '>', '<' },	/* PLAYER | GOAL */
    { '[', ']' },	/* BOX */
    { '{', '}' },	/* BOX | GOAL */
    {  0 ,  0  },
    {  0 ,  0  },
    { '#', '#' }	/* WALL */
};

/* The dimensions of the largest map that can be displayed.
 */
static int	fieldheight, fieldwidth;

/* The index of the bottommost line.
 */
static int	lastline;

/* The index of the leftmost column in the textual display area.
 */
static int	sidebar;

/* FALSE if the program is allowed to ring the bell.
 */
static int	silence = FALSE;

/* The name of the program and the file currently being accessed,
 * for use in error messages (declared in gen.h).
 */
char const     *programname = NULL;
char const     *currentfilename = NULL;

/*
 * Input and output functions
 */

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

/* Display the game state on the terminal. The map is placed in the
 * upper left corner, and the status information goes in the upper
 * right corner.
 */
int displaygame(cell const *map, int ysize, int xsize,
		int recording, int macro, int save,
		char const *seriesname, char const *levelname, int index,
		int boxcount, int storecount, int movecount, int pushcount,
		int bestmovecount, int bestpushcount)
{
    cell const *p;
    char	buf[SIDEBARWIDTH + 1];
    int		y, x, n;

    erase();

    if (ysize > fieldheight || xsize > fieldwidth) {
	mvprintw(0, 0, "Level %d won't fit on the screen.", index);
	refresh();
	return FALSE;
    }

    p = map;
    for (y = 1, p += XSIZE ; y < ysize - 1 ; ++y, p += XSIZE) {
	if (y != 1)
	    addch('\n');
	for (x = 1 ; x < xsize - 1 ; ++x) {
	    addch(screencells[p[x] & 0x0F][0]);
	    addch(screencells[p[x] & 0x0F][1]);
	}
    }

    sprintf(buf, "# %d", index);
    mvprintw(0, sidebar, "%*s", SIDEBARWIDTH, buf);
    mvprintw(1, sidebar, " Boxes: %-3d    %c",
			 boxcount,
			 recording ? 'R' : macro ? 'M' : ' ');
    mvprintw(2, sidebar, "Stored: %-3d    %c", storecount, save ? 'S' : ' ');
    mvprintw(3, sidebar, " Moves: %d", movecount);
    mvprintw(4, sidebar, "Pushes: %d", pushcount);
    if (bestmovecount && bestpushcount) {
	mvprintw(5, sidebar, "  Best: %d", bestmovecount);
	if (bestmovecount < 100000)
	    addstr(" moves");
	
	if (bestpushcount < 10000)
	    mvprintw(6, sidebar + 8, "%d pushes", bestpushcount);
	else
	    mvprintw(6, sidebar + 4, "%7d pushes", bestpushcount);
    }

    y = 8;
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

/* Retrieve and return a single keystroke. Arrow keys and other inputs
 * are translated into ASCII equivalents.
 */
int input(void)
{
    for (;;) {
	int key = getch();
	switch (key) {
	  case KEY_UP:		return 'k';
	  case KEY_DOWN:	return 'j';
	  case KEY_LEFT:	return 'h';
	  case KEY_RIGHT:	return 'l';
	  case KEY_ENTER:	return '\n';
	  case '\r':		return '\n';
	  case KEY_BACKSPACE:	return '\b';
	  case '\f':		clearok(stdscr, TRUE);
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
    attr_t	attrs;

    if (has_colors()) {
	start_color();
	init_pair(1, COLOR_CYAN, COLOR_BLACK);
	init_pair(2, COLOR_YELLOW, COLOR_YELLOW);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);
	screencells[GOAL][0] |= COLOR_PAIR(1) | A_BOLD;
	screencells[GOAL][1] |= COLOR_PAIR(1) | A_BOLD;
	screencells[BOX | GOAL][0] = screencells[BOX][0] | COLOR_PAIR(3)
							 | A_BOLD;
	screencells[BOX | GOAL][1] = screencells[BOX][1] | COLOR_PAIR(3)
							 | A_BOLD;
	screencells[WALL][0] = ' ' | COLOR_PAIR(2);
	screencells[WALL][1] = ' ' | COLOR_PAIR(2);
	return;
    }

    attrs = termattrs();
    if (attrs & A_REVERSE) {
	screencells[WALL][0] = ' ' | A_REVERSE;
	screencells[WALL][1] = ' ' | A_REVERSE;
    }
    if (attrs & A_BOLD) {
	screencells[GOAL][0] |= A_BOLD;
	screencells[GOAL][1] |= A_BOLD;
	screencells[BOX | GOAL][0] = screencells[BOX][0] | A_BOLD;
	screencells[BOX | GOAL][1] = screencells[BOX][1] | A_BOLD;
    }
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
    fieldheight = y + 2;
    fieldwidth = (x - 1) / 2 + 2;
    if (sidebar < 1)
	die("The terminal screen is too darned small!");

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
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
