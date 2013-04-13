/* userio.c: User interface functions for the Linux console.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#define	__POSIX_SOURCE
#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<string.h>
#include	<ctype.h>
#include	<time.h>
#include	<signal.h>
#include	<unistd.h>
#include	<termios.h>
#include	<sys/time.h>
#include	<sys/ioctl.h>
#include	"cmines.h"
#include	"userio.h"

#ifdef NOMOUSE

/* Stubs and empty macros to provide a no-op mouse interface.
 */

#define	mousegetchar	getchar
#define	openmouse()	FALSE
#define	closemouse()	((void)0)
#define	setlastevent(e)	((void)0)
#define	hidelastevent()	((void)0)
#define	drawmousepos()	((void)0)

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
			 gpmconnection.eventMask = GPM_DOWN | GPM_UP, \
			 gpmconnection.defaultMask = GPM_MOVE | GPM_HARD, \
			 gpmconnection.minMod = 0, \
			 gpmconnection.maxMod = 0, \
			 Gpm_Open(&gpmconnection, 0) >= 0)
#define	closemouse()	(Gpm_Close())

/* The last event received from gpm. The y field is set to -2 if the
 * keyboard has been used since the last gpm event.
 */
static Gpm_Event lastgpm;
#define	setlastevent(e)	(lastgpm = (e))
#define	hidelastevent()	(lastgpm.y = -2)
#define	drawmousepos()	((void)(lastgpm.y > -2 && GPM_DRAWPOINTER(&lastgpm)))

#endif

/* If NSIG is not provided by signal.h, blithely assume 32 is big enough.
 */
#ifndef	NSIG
#define	NSIG	32
#endif

/* Bitflags that correspond to ANSI escape sequences. These are used
 * by displaygame() to keep track of the current state of the output
 * stream.
 */
#define	ATTR_CHARACTER	0x00FF
#define	ATTR_COLOR	0x0F00
#define	ATTR_VT100CHAR	0x1000

#define	COLORVAL(a)	(((a) >> 8) & 7)

#define	ATTR_RED	0x0100
#define	ATTR_GREEN	0x0200
#define	ATTR_BLUE	0x0400
#define	ATTR_YELLOW	(ATTR_RED | ATTR_GREEN)
#define	ATTR_MAGENTA	(ATTR_RED | ATTR_BLUE)
#define	ATTR_CYAN	(ATTR_GREEN | ATTR_BLUE)
#define	ATTR_WHITE	(ATTR_RED | ATTR_GREEN | ATTR_BLUE)
#define	ATTR_BRIGHT	0x0800

/* Reset and erase the console screen.
 */
#define	erasescreen()	(write(STDOUT_FILENO, "\033c\033[H\033[J", 8))

/* The characters and attributes used to display all the different
 * kinds of cells of the field.
 */
#ifdef USE_LATIN1
static int const	coveredcell = 183 | ATTR_WHITE;
#else
static int const	coveredcell = '~' | ATTR_WHITE | ATTR_VT100CHAR;
#endif
static int const	minecell = '*' | ATTR_RED;
static int const	boomcell = '*' | ATTR_RED | ATTR_BRIGHT;
static int const	flagcell = '+' | ATTR_WHITE;
static int const	badflagcell = 'x' | ATTR_RED | ATTR_BRIGHT;

static int const	numbercell[9] = { ' ' | ATTR_WHITE,
					  '1' | ATTR_BLUE,
					  '2' | ATTR_GREEN,
					  '3' | ATTR_RED,
					  '4' | ATTR_MAGENTA,
					  '5' | ATTR_YELLOW,
					  '6' | ATTR_CYAN,
					  '7' | ATTR_BLUE | ATTR_BRIGHT,
					  '8' | ATTR_RED | ATTR_BRIGHT };

/* The emoticons used to indicate the current status.
 */
static char const *smiley[] = { "   ", ":-)", "8-|", ":-D", "B-)" };

/* starttime either is zero if the timer is not running, is negative
 * if the timer has been turned off, or else holds the time at which
 * the timer began running.
 */
static time_t		starttime = -1;

/* The timer's current value.
 */
static int		timer = 0;

/* TRUE if the terminal is currently in raw mode.
 */
static int		inrawmode = FALSE;

/* TRUE if the program has hooked into the mouse.
 */
static int		usingmouse = FALSE;

/* TRUE if the display needs to be redrawn now.
 */
static int		redrawrequest = FALSE;

/* Signal handlers that were installed on startup that may need to be
 * reinstalled.
 */
static struct sigaction	prevhandler[NSIG];

/* Signal handlers that this program installs that may need to be
 * temporary uninstalled.
 */
static struct sigaction	myhandler[NSIG];

/* The index of the bottommost line.
 */
static int		lastline;

/* The index of the column where the timer is displayed.
 */
static int		timercolumn;

/* TRUE if the timer should be updated asynchronously.
 */
static int		updatetimer = FALSE;

/* TRUE if a mouse click to the right of a cell is accepted.
 */
static int		allowoffclicks = FALSE;

/* TRUE if the current status should be indicated with emoticons.
 */
static int		showsmileys = FALSE;

/* FALSE if the program is allowed to ring the bell.
 */
static int		silence = FALSE;

/* The name of the program (for use in error messages).
 */
char const	       *programname = "";

/*
 * Timer functions
 */

/* Return the current value of the timer.
 */
int gettimer(void)
{
    return timer;
}

/* Set the timer's current state. If action is positive, the timer
 * begins counting. If action is zero, the timer stops counting. If
 * action is negative, the timer is turned off and removed from the
 * display.
 */
void settimer(int action)
{
    if (action < 0) {
	timer = 0;
	starttime = -1;
    } else if (action > 0)
	starttime = time(NULL) - timer;
    else
	starttime = 0;
}

/*
 * Output functions
 */

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
    timercolumn = size.ws_col - 6;
    if (timercolumn <= 1)
	return FALSE;
    return TRUE;
}

/* Switch the terminal into the mode that was in place at startup
 * (presumably cooked mode) if raw is FALSE, or raw mode if raw is
 * TRUE. Keyboard signals are not turned off.
 */
static int setrawmode(int raw)
{
    static struct termios	origterm;
    static struct termios	rawterm;
    static int			termloaded = FALSE;

    if (!termloaded) {
	if (tcgetattr(STDIN_FILENO, &origterm))
	    return FALSE;
	rawterm = origterm;
	rawterm.c_lflag &= ~(ECHO | ICANON);
	termloaded = TRUE;
    }
    if (tcsetattr(STDIN_FILENO, TCSANOW, raw ? &rawterm : &origterm))
	return FALSE;
    inrawmode = raw;
    return TRUE;
}

/* Enqueue characters for output formatted according to fmt. If fmt is
 * NULL, the accumulated characters are flushed to the screen, and the
 * mouse cursor is redrawn immediately afterwards if it was visible.
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
    } else if (size) {
	write(STDOUT_FILENO, out, size);
	size = 0;
	drawmousepos();
    }
}

/* Update the timer display without altering the cursor position.
 * Output is flushed whether or not the timer is currently being
 * displayed.
 */
static void displaytimer(void)
{
    if (starttime > 0)
	timer = time(NULL) - starttime;
    if (starttime >= 0)
	out("\033[s\033[1;%dH%7d\033[u", timercolumn, timer);
    out(NULL);
}

/* Display the game state on the console. The field is placed in the
 * upper left corner, and status information appears in the upper
 * right corner.
 */
void displaygame(cell const *field, int ysize, int xsize,
		 int minecount, int flagcount, int status)
{
    cell const *p;
    int		lastattr;
    int		y, x, ch;

    out("\033[H\033[22;39m");

    if (ysize + 2 > lastline ||
			xsize * 2 + (showsmileys ? 11 : 7) > timercolumn) {
	out("\033(BThe screen is too small to show the game.\033[J");
	out(NULL);
	return;
    }

    out("\033(0lq");
    for (x = 0 ; x < xsize ; ++x)
	out("qq");
    out("k \033(B%-4d", minecount - flagcount);
    if (showsmileys && status != status_ignore)
	out("%s", smiley[status]);
    out("\033[K\n\033(0");

    for (y = 0, p = field ; y < ysize ; ++y, p += XSIZE) {
	lastattr = ATTR_VT100CHAR | ATTR_WHITE;
	out("x ");
	for (x = 0 ; x < xsize ; ++x) {
	    if (p[x] & MINED) {
		if (p[x] & FLAGGED)
		    ch = flagcell;
		else if (p[x] & EXPOSED)
		    ch = boomcell;
		else
		    ch = status == status_normal ? coveredcell : minecell;
	    } else if (p[x] & FLAGGED)
		ch = status == status_normal ? flagcell : badflagcell;
	    else if (p[x] & EXPOSED)
		ch = numbercell[p[x] & NEIGHBOR_MASK];
	    else
		ch = coveredcell;
	    if ((ch ^ lastattr) & ATTR_VT100CHAR)
		out("\033(%c", ch & ATTR_VT100CHAR ? '0' : 'B');
	    if ((ch ^ lastattr) & ATTR_COLOR)
		out("\033[%s;3%cm", ch & ATTR_BRIGHT ? "1" : "22",
				    '0' + COLORVAL(ch));
	    lastattr = ch;
	    out("%c ", ch & ATTR_CHARACTER);
	}
	if (!(lastattr & ATTR_VT100CHAR))
	    out("\033(0");
	if ((lastattr & ATTR_COLOR) != ATTR_WHITE)
	    out("\033[22;39m");
	out("x\033[K\n");
    }
    out("mq");
    for (x = 0 ; x < xsize ; ++x)
	out("qq");
    out("j \033(B\033[J");
    displaytimer();
}

/* Display information about the various key commands. Each element in
 * the keys array contains two sequential NUL-terminated strings,
 * describing the key and the associated command respectively. The
 * strings in the setups array describe the different configurations
 * available. The besttimes array give the best times on record for
 * each configuration. The information is arranged into columns.
 */
void displayhelp(int keycount, char const *keys[],
		 int setupcount, char const *setups[], int besttimes[])
{
    int keywidth, descwidth;
    int	i, n;

    keywidth = 1;
    descwidth = 0;
    for (i = 0 ; i < setupcount ; ++i) {
	n = strlen(setups[i]);
	if (n > descwidth)
	    descwidth = n;
    }
    for (i = 0 ; i < keycount ; ++i) {
	n = strlen(keys[i]);
	if (n > keywidth)
	    keywidth = n;
	n = strlen(keys[i] + n + 1);
	if (n > descwidth)
	    descwidth = n;
    }

    out("\033[H\033[22;39m");
    n = keywidth + descwidth + 2;
    if (n < 14)
	n = 14;
    out("%*s\033[K\n\033(0", n / 2 + 7, "Mouse and Keys");
    for (i = 0 ; i < n ; ++i)
	out("q");
    out("\033(B\033[K\n");
    for (i = 0 ; i < keycount ; ++i) {
	n = strlen(keys[i]);
	out("%-*s  %-*s", keywidth, keys[i],
			  descwidth, keys[i] + n + 1);
	if (i == keycount - 1)
	    out("    Best Times");
	out("\033[K\n");
    }
    out("\033(0%*s\033(B\033[K\n", keywidth + descwidth + 16, "qqqqqqqqqq");
    for (i = 0 ; i < setupcount ; ++i) {
	n = strlen(setups[i]);
	out("%-*c  %-*s", keywidth, tolower(setups[i][0]),
			  descwidth, setups[i]);
	if (besttimes[i])
	    out("%14d", besttimes[i]);
	out("\033[K\n");
    }
    out("\033[J\033[%d;0HPress any key to return.", lastline);
    out(NULL);
}

/* Move the cursor to the given field cell.
 */
void setcursorpos(int pos)
{
    int	y, x;

    if (pos >= 0) {
	y = pos / XSIZE;
	x = pos - y * XSIZE;
	y = y + 2;
	x = x * 2 + 3;
	if (y <= lastline && x <= timercolumn) {
	    out("\033[%d;%dH", y, x);
	    out(NULL);
	}
    }
}

/*
 * Input functions
 */

/* Read a single character. The keyboard and mouse are polled once a
 * second, with the timer being updated between polls. If
 * redrawrequest has been set to TRUE, reset the display and return a
 * faked Ctrl-L.
 */
static int getkey(void)
{
    struct timeval	waitfor;
    fd_set		in, empty;
    int			max;
    char		ch;

    for (;;) {
	if (starttime > 0 && (updatetimer || !usingmouse))
	    displaytimer();
	waitfor.tv_sec = 1;
	waitfor.tv_usec = 0;
	FD_ZERO(&in);
	FD_ZERO(&empty);
	FD_SET(STDIN_FILENO, &in);
	if (mousefd >= 0) {
	    FD_SET(mousefd, &in);
	    max = (STDIN_FILENO > mousefd ? STDIN_FILENO : mousefd) + 1;
	    if (select(max, &in, &empty, &empty, &waitfor) > 0)
		return mousegetchar();
	} else {
	    if (select(STDIN_FILENO + 1, &in, &empty, &empty, &waitfor) > 0) {
		read(STDIN_FILENO, &ch, 1);
		return ch;
	    }
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

    hidelastevent();
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

#ifndef NOMOUSE

/* Handle mouse activity. The screen coordinates are translated into a
 * field's cell position and mousecallback() is called. If the left
 * and right mouse buttons are both depressed, it is automatically
 * translated into a middle button event. The return value supplies
 * the keystroke to translate the mouse event into, or zero if no
 * keystroke should be generated.
 */
static int mousehandler(Gpm_Event *event, void *clientdata)
{
    int	mstate;

    (void)clientdata;
    setlastevent(*event);
    if (!allowoffclicks && (event->x & 1))
	return 0;
    if (event->type & (GPM_DOWN | GPM_UP)) {
	if (event->buttons & GPM_B_MIDDLE)
	    mstate = 3;
	else {
	    mstate = 0;
	    if (event->buttons & GPM_B_LEFT)
		mstate += 1;
	    if (event->buttons & GPM_B_RIGHT)
		mstate += 2;
	}
	if (event->type & GPM_DOWN)
	    mstate = -mstate;
    } else
	return 0;
    return mousecallback((event->y - 1) * XSIZE + (event->x - 2) / 2, mstate);
}

#endif

/*
 * Top-level functions
 */

/* Prepare the console and the mouse for use by the program.
 */
static int startup(void)
{
    if (!setrawmode(TRUE))
	return FALSE;

    usingmouse = openmouse();
    hidelastevent();

    erasescreen();
    return TRUE;
}

/* Reset the console to its natural state.
 */
static void shutdown(void)
{
    if (usingmouse) {
	closemouse();
	usingmouse = FALSE;
    }
    if (inrawmode)
	setrawmode(FALSE);
    write(STDOUT_FILENO, "\n", 1);
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

/* Install signal handlers and initialize the console.
 */
int ioinitialize(int updatetimerflag, int showsmileysflag, int silenceflag,
		 int allowoffclicksflag)
{
    struct sigaction	act;

    updatetimer = updatetimerflag;
    showsmileys = showsmileysflag;
    silence = silenceflag;
    allowoffclicks = allowoffclicksflag;

    atexit(shutdown);

    if (!measurescreen())
	return FALSE;

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

    return startup();
}

/*
 * Miscellaneous interface functions.
 */

/* Ring the bell.
 */
void ding(void)
{
    if (!silence)
	write(STDERR_FILENO, "\a", 1);
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
