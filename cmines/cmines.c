/* cmines.h: The guts of the game.
 *
 * Copyright (C) 2000 by Brian Raiter, under the GNU General Public
 * License. No warranty. See COPYING for details.
 */

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>
#include	<ctype.h>
#include	<errno.h>
#include	<getopt.h>
#include	"cmines.h"
#include	"userio.h"

/* When allocation fails.
 */
#define	memerrexit()	(die("out of memory"))

/* A collection of function return values that indicate how the caller
 * should proceed.
 */
enum { RET_NOP, RET_OK, RET_REDRAW, RET_DING, RET_QUIT, RET_LOSE, RET_GIVEUP };

/* The possible states of a game.
 */
enum { S_UNBUILT, S_PLAYING, S_ENDED };

/* The collection of data maintained for each configuration.
 */
typedef	struct gamesetup {
    int		ysize;			/* height of the field */
    int		xsize;			/* width of the field */
    int		minecount;		/* number of mines */
    int		besttime;		/* fastest completion on record */
    char	name[32];		/* configuration's name */
} gamesetup;

/* The collection of data maintained for a game in progress.
 */
typedef	struct gameinfo {
    gamesetup  *setup;			/* the configuration used */
    short	ysize;			/* height of the field */
    short	xsize;			/* width of the field */
    short	cellcount;		/* total number of cells */
    short	minecount;		/* number of mines */
    short	exposedcount;		/* number of mines exposed */
    short	flaggedcount;		/* number of cells flagged */
    int		peekcount;		/* number of cells peeked at */
    int		currpos;		/* position of current cell */
    int		state;			/* state of the game */
    cell	field[MAXHEIGHT * MAXWIDTH]; /* the field proper */
} gameinfo;

/* Structure used to pass data back from readcmdline().
 */
typedef	struct startupdata {
    int		updatetimer;	/* update timer asynchronously if TRUE */
    int		showsmileys;	/* display status emoticons if TRUE */
    int		silence;	/* suppress the terminal bell if TRUE */
    int		allowoffclicks;	/* allow off-by-one clicks if TRUE */
    char const *setupname;	/* name of the starting configuration */
} startupdata;

/* Online help.
 */
static char const      *yowzitch =
	"Usage: cmines [-hvetcr] [-s SETUPFILE]\n"
	"   -h  Display this help\n"
	"   -v  Display version information\n"
	"   -c  Accept mouse clicks on pressing instead of releasing\n"
	"   -r  Accept mouse clicks to the right of a cell\n"
	"   -e  Display game status emoticons\n"
	"   -s  Read setups from SETUPFILE (default is ~/.cminesrc)\n"
	"   -t  Continually update timer\n"
	"(Press ? during the game for further help.)\n";

/* Version information.
 */
static char const      *vourzhon =
    "cmines, version 1.2. Copyright (C) 2000 by Brian Raiter\n"
    "under the terms of the GNU General Public License.\n";

/* Contents of the default setup file.
 */
static char const	defaultsetup[] = "Beginner: 8 8 10\n"
					 "Intermediate: 16 16 40\n"
					 "Expert: 16 30 99\n";

/* The name of the setup file.
 */
static char const      *setupfile;

/* The list of game setups.
 */
static gamesetup       *setups = NULL;

/* The total number of setups available.
 */
static int		setupcount = 0;

/* The game currently being played.
 */
static gameinfo		game;

/* TRUE if mouse-click commands should always occur when the mouse
 * button is pressed, instead of released.
 */
static int		actonpress = FALSE;

/*
 * Setup functions.
 */

/* Allocate and return a buffer containing the pathname composed
 * of the given directory and file.
 */
static char *makepathname(char const *dir, char const *file)
{
    char       *path;
    int		n;

    if (!dir || !*dir)
	dir = ".";
    n = strlen(dir);
    if (!(path = malloc(n + 1 + strlen(file) + 1)))
	memerrexit();
    memcpy(path, dir, n);
    path[n++] = '/';
    strcpy(path + n, file);
    return path;
}

/* Read the list of configurations from the given file and initialize
 * a gamesetup structure for each. If the give file does not exist,
 * create it with the default set of configurations.
 */
static int readsetups(void)
{
    FILE       *fp;
    char	buf[256];
    int		n, y, x, m, t;

    if (!(fp = fopen(setupfile, "r"))) {
	if (!(fp = fopen(setupfile, "w+")))
	    die("%s: %s", setupfile, strerror(errno));
	fwrite(defaultsetup, 1, sizeof defaultsetup - 1, fp);
	rewind(fp);
    }
    while (fgets(buf, sizeof buf, fp)) {
	if (!(setups = realloc(setups, (setupcount + 1) * sizeof *setups)))
	    memerrexit();
	setups[setupcount].name[31] = '\0';
	n = sscanf(buf, "%31[^:]: %d %d %d %d",
			setups[setupcount].name, &y, &x, &m, &t);
	if (n < 4 || y > MAXHEIGHT || x > MAXWIDTH || m >= y * x)
	    die("%s: Configuration #%d (\"%s\") is invalid.",
		setupfile, setupcount + 1, setups[setupcount].name);
	setups[setupcount].ysize = y;
	setups[setupcount].xsize = x;
	setups[setupcount].minecount = m;
	setups[setupcount].besttime = n > 4 ? t : 0;
	++setupcount;
    }
    fclose(fp);

    if (!setupcount)
	die("No game setups found in setup file \"%s\".", setupfile);

    for (n = 0 ; n < setupcount - 1 ; ++n)
	for (m = n + 1 ; m < setupcount ; ++m)
	    if (setups[n].name[0] == setups[m].name[0])
		die("%s: Configurations \"%s\" and \"%s\" "
			"begin with the same initial.",
		    setupfile, setups[n].name, setups[m].name);

    return TRUE;
}

/* Write out a new configuration file.
 */
static int writesetups(char const *filename)
{
    FILE       *fp;
    int		i;

    if (!(fp = fopen(filename, "w")))
	die("%s: %s", filename, strerror(errno));
    for (i = 0 ; i < setupcount ; ++i)
	fprintf(fp, "%s: %d %d %d %d\n", setups[i].name,
					 setups[i].ysize,
					 setups[i].xsize,
					 setups[i].minecount,
					 setups[i].besttime);
    fclose(fp);
    return TRUE;
}

/* Update the best time for the current configuration if gametime
 * is better. TRUE is returned if the record was changed.
 */
static int checkrecord(int gametime)
{
    if (game.setup->besttime && game.setup->besttime <= gametime)
	return FALSE;
    game.setup->besttime = gametime;
    return writesetups(setupfile);
}

/* Initialize a game based on the given configuration.
 */
static void setupgame(gamesetup *setup)
{
    if (setup) {
	game.setup = setup;
	game.ysize = setup->ysize;
	game.xsize = setup->xsize;
	game.minecount = setup->minecount;
	game.cellcount = game.ysize * game.xsize;
    }
    game.exposedcount = 0;
    game.flaggedcount = 0;
    game.peekcount = 0;
    game.currpos = (game.ysize / 2) * XSIZE + game.xsize / 2;
    game.state = S_UNBUILT;
    memset(game.field, 0, sizeof game.field);
    settimer(-1);
}

/* Fill in an empty field with the appropriate number of mines
 * randomly, except that the given cell is never mined. The neighbor
 * count of each cell is set to the number of its neighbors that are
 * mined.
 */
static void makenewfield(int except)
{
    static int	deck[MAXHEIGHT * MAXWIDTH];
    int		pos;
    short	m, n, y, x;

    if (game.state != S_UNBUILT)
	return;

    game.cellcount = game.ysize * game.xsize;
    n = 0;
    for (y = 0, pos = 0 ; y < game.ysize ; ++y, pos += XSIZE - game.xsize)
	for (x = 0 ; x < game.xsize ; ++x, ++pos)
	    if (pos != except)
		deck[n++] = pos;
    while (--n) {
	m = (int)((rand() * (double)(n + 1)) / (double)RAND_MAX);
	pos = deck[n];
	deck[n] = deck[m];
	deck[m] = pos;
    }

    for (n = 0 ; n < game.minecount ; ++n) {
	pos = deck[n];
	game.field[pos] |= MINED;
	y = pos / XSIZE;
	x = pos - y * XSIZE;
	if (x > 0)
	    ++game.field[pos - 1];
	if (x < game.xsize - 1)
	    ++game.field[pos + 1];
	if (y > 0) {
	    ++game.field[pos - XSIZE];
	    if (x > 0)
		++game.field[pos - XSIZE - 1];
	    if (x < game.xsize - 1)
		++game.field[pos - XSIZE + 1];
	}
	if (y < game.ysize - 1) {
	    ++game.field[pos + XSIZE];
	    if (x > 0)
		++game.field[pos + XSIZE - 1];
	    if (x < game.xsize - 1)
		++game.field[pos + XSIZE + 1];
	}
    }

    game.exposedcount = 0;
    game.state = S_PLAYING;
    settimer(+1);
}

/*
 * Game logic.
 */

/* Return the total number of cells neighboring pos that are flagged.
 */
static int countneighborflags(int pos)
{
    int	count = 0;
    int	y, x;

    y = pos / XSIZE;
    x = pos - y * XSIZE;
    if (x > 0)
	if (game.field[pos - 1] & FLAGGED)		++count;
    if (x < game.xsize - 1)
	if (game.field[pos + 1] & FLAGGED)		++count;
    if (y > 0) {
	if (game.field[pos - XSIZE] & FLAGGED)		++count;
	if (x > 0)
	    if (game.field[pos - XSIZE - 1] & FLAGGED)	++count;
	if (x < game.xsize - 1)
	    if (game.field[pos - XSIZE + 1] & FLAGGED)	++count;
    }
    if (y < game.ysize - 1) {
	if (game.field[pos + XSIZE] & FLAGGED)		++count;
	if (x > 0)
	    if (game.field[pos + XSIZE - 1] & FLAGGED)	++count;
	if (x < game.xsize - 1)
	    if (game.field[pos + XSIZE + 1] & FLAGGED)	++count;
    }
    return count;
}

static int exposeneighbors(int pos);

/* Mark the cell at the given coordinates as exposed. Report a
 * losing condition if the cell in mined. If the cell has no
 * mined neighbors, expose all neighboring cells recursively.
 */
static int exposecell(int pos)
{
    if (game.field[pos] & (EXPOSED | FLAGGED))
	return RET_NOP;
    game.field[pos] |= EXPOSED;
    ++game.exposedcount;
    if (game.field[pos] & MINED)
	return RET_LOSE;
    if (!(game.field[pos] & NEIGHBOR_MASK))
	if (exposeneighbors(pos) == RET_LOSE)
	    return RET_LOSE;
    return RET_REDRAW;
}

/* Expose all neighbors of the cell at the given coordinates. Report a
 * losing condition if any of them are mined.
 */
static int exposeneighbors(int pos)
{
    int	y, x;

    y = pos / XSIZE;
    x = pos - y * XSIZE;
    if (x > 0)
	if (exposecell(pos - 1) == RET_LOSE)		    return RET_LOSE;
    if (x < game.xsize - 1)
	if (exposecell(pos + 1) == RET_LOSE)		    return RET_LOSE;
    if (y > 0) {
	if (exposecell(pos - XSIZE) == RET_LOSE)	    return RET_LOSE;
	if (x > 0)
	    if (exposecell(pos - XSIZE - 1) == RET_LOSE)    return RET_LOSE;
	if (x < game.xsize - 1)
	    if (exposecell(pos - XSIZE + 1) == RET_LOSE)    return RET_LOSE;
    }
    if (y < game.ysize - 1) {
	if (exposecell(pos + XSIZE) == RET_LOSE)	    return RET_LOSE;
	if (x > 0)
	    if (exposecell(pos + XSIZE - 1) == RET_LOSE)    return RET_LOSE;
	if (x < game.xsize - 1)
	    if (exposecell(pos + XSIZE + 1) == RET_LOSE)    return RET_LOSE;
    }
    return RET_REDRAW;
}

/*
 * Game actions.
 */

/* Change the current cell.
 */
static int setcurrpos(int pos)
{
    if (pos < 0 || pos >= game.ysize * XSIZE || pos % XSIZE >= game.xsize)
	return RET_NOP;
    game.currpos = pos;
    return RET_OK;
}

/* Shift the current cell.
 */
static int movecurrpos(int delta, int times)
{
    int	pos;

    pos = game.currpos;
    while (times-- && pos >= 0 && pos < game.ysize * XSIZE
			       && pos % XSIZE < game.xsize) {
	game.currpos = pos;
	pos += delta;
    }
    return RET_OK;
}

/* Toggle the current cell's flag.
 */
static int flagcurrcell()
{
    if (game.field[game.currpos] & EXPOSED)
	return RET_NOP;
    if (game.field[game.currpos] & FLAGGED)
	--game.flaggedcount;
    else
	++game.flaggedcount;
    game.field[game.currpos] ^= FLAGGED;
    return RET_REDRAW;
}

/* Expose the current cell.
 */
static int exposecurrcell(void)
{
    if (game.field[game.currpos] & FLAGGED)
	return RET_NOP;
    if (game.state == S_UNBUILT)
	makenewfield(game.currpos);
    return exposecell(game.currpos);
}

/* Expose the neighbors of the current cell.
 */
static int exposeothercells(void)
{
    int	m, n;

    if (!(game.field[game.currpos] & EXPOSED))
	return RET_NOP;
    m = game.field[game.currpos] & NEIGHBOR_MASK;
    n = countneighborflags(game.currpos);
    if (n > m)
	return RET_DING;
    else if (n < m)
	return RET_NOP;
    return exposeneighbors(game.currpos);
}

/* Expose either the current cell or its neighbors.
 */
static int exposecells(void)
{
    if (game.field[game.currpos] & FLAGGED)
	return RET_NOP;
    if (game.state == S_UNBUILT)
	makenewfield(game.currpos);
    return game.field[game.currpos] & EXPOSED ? exposeothercells()
					      : exposecurrcell();
}

/* Expose all unmined cells and flag all mined cells.
 */
static void exposefield(void)
{
    int	pos, y, x;

    pos = game.currpos;

    game.currpos = 0;
    for (y = 0 ; y < game.ysize ; ++y, game.currpos += XSIZE - game.xsize)
	for (x = 0 ; x < game.xsize ; ++x, ++game.currpos)
	    if (!(game.field[game.currpos] & (EXPOSED | FLAGGED))
				&& (game.field[game.currpos] & MINED))
		flagcurrcell();

    game.currpos = pos;
}

/* Peek at the current cell.
 */
static int peekatcell(void)
{
    ++game.peekcount;
    if (game.field[game.currpos] & MINED)
	return RET_DING;
    return exposecurrcell();
}

/*
 * User interface functions.
 */

/* Display the current state of the game. showall is used to expose
 * the field when the game is ended early.
 */
static void drawgamescreen(int status)
{
    displaygame(game.field, game.ysize, game.xsize,
		game.minecount, game.flaggedcount, status);
}

/* Display online help for the game interface. The best times on
 * record are also displayed.
 */
static int drawhelpscreen(void)
{
    static char const **setuplist = NULL;
    static int	       *besttimes = NULL;
    static char const **bestnames = NULL;
    static char const  *keys[] = { "h j k l\0move the cursor",
				   "arrows\0    \"",
				   "H J K L\0move by threes",
				   "SPACE\0expose cells",
				   "left-mouse\0    \"",
				   "dot\0flag cells",
				   "right-mouse\0    \"",
				   "ENTER\0expose neighbors",
				   "middle-mouse\0    \"",
				   "Ctrl-L\0redraw screen",
				   "p\0peek at cell",
				   "q\0quit game",
				   "Q\0quit game and program",
				   "n\0new game"
    };

    int	i;

    if (!bestnames) {
	if (!(setuplist = malloc(setupcount * sizeof(char*))))
	    return RET_DING;
	if (!(besttimes = malloc(setupcount * sizeof(int))))
	    return RET_DING;
    }
    for (i = 0 ; i < setupcount ; ++i) {
	setuplist[i] = setups[i].name;
	besttimes[i] = setups[i].besttime;
    }
    displayhelp(sizeof keys / sizeof *keys, keys,
		setupcount, setuplist, besttimes);
    input();
    return RET_REDRAW;
}

/* Get a key from the user at the completion of the current game.
 */
static int endinput(int ch)
{
    int i;

    ch = tolower(ch);
    for (i = 0 ; i < setupcount ; ++i) {
	if (ch == tolower(setups[i].name[0])) {
	    setupgame(setups + i);
	    return RET_REDRAW;
	}
    }
    switch (ch) {
      case 'q':			 return RET_QUIT;
      case 'n':	setupgame(NULL); return RET_REDRAW;
      case '?':			 return drawhelpscreen();
    }
    return RET_NOP;
}

/* Get a key from the user and perform the indicated command. The
 * return value indicates what should happen as a result. If the game
 * is not in the playing state, endinput() is used to process the key
 * instead.
 */
static int doturn(void)
{
    int	ch, n;

    for (;;) {
	ch = input();
	if (game.state != S_PLAYING) {
	    n = endinput(ch);
	    if (n != RET_NOP || game.state == S_ENDED)
		return n;
	}
	switch (ch) {
	  case 'h':	return setcurrpos(game.currpos - 1);
	  case 'j':	return setcurrpos(game.currpos + XSIZE);
	  case 'k':	return setcurrpos(game.currpos - XSIZE);
	  case 'l':	return setcurrpos(game.currpos + 1);
	  case 'H':	return movecurrpos(-1, 3);
	  case 'J':	return movecurrpos(+XSIZE, 3);
	  case 'K':	return movecurrpos(-XSIZE, 3);
	  case 'L':	return movecurrpos(+1, 3);
	  case ' ':	return exposecells();
	  case '.':	return flagcurrcell();
	  case '\n':	return exposeothercells();
	  case 'p':	return peekatcell();
	  case '?':	return drawhelpscreen();
	  case '0':	return RET_NOP;
	  case '\f':	return RET_REDRAW;
	  case 'q':	return game.state == S_PLAYING ? RET_GIVEUP : RET_QUIT;
	  case 'Q':	return RET_QUIT;
	}
    }
    return RET_NOP;
}

/* The mouse activity decoder. A middle-button press becomes RET, A
 * left-button press-and-release becomes SPC, and a right-button
 * press-and-release becomes dot.
 */
int mousecallback(int pos, int button)
{
    static int	posdown, buttondown = 0;
    int		r = '\f';

    if (button < 0) {
	if (setcurrpos(pos) == RET_OK) {
	    posdown = pos;
	    if ((buttondown | -button) == 3) {
		buttondown = 0;
		r = '\n';
	    } else if (actonpress) {
		buttondown = 0;
		r = button == -1 ? ' ' : '.';
	    } else {
		buttondown = -button;
		r = '\f';
	    }
	}
    } else if (button > 0) {
	setcurrpos(pos);
	if (buttondown == button && posdown == pos)
	    r = button == 1 ? ' ' : '.';
	buttondown = 0;
    }
    return r;
}

/* The main loop, exited only when the user requests to leave the
 * program.
 */
static void playgame(void)
{
    for (;;) {
	if (game.state == S_PLAYING) {
	    if (game.cellcount - game.exposedcount == game.minecount) {
		exposefield();
		game.state = S_ENDED;
		settimer(0);
		drawgamescreen(!game.peekcount && checkrecord(gettimer()) ?
						status_besttime : status_won);
	    }
	}
	if (game.state != S_ENDED)
	    setcursorpos(game.currpos);
	switch (doturn()) {
	  case RET_REDRAW:
	    drawgamescreen(status_normal);
	    break;
	  case RET_DING:
	    ding();
	    break;
	  case RET_LOSE:
	  case RET_GIVEUP:
	    settimer(0);
	    game.state = S_ENDED;
	    drawgamescreen(status_lost);
	    break;
	  case RET_QUIT:
	    settimer(0);
	    drawgamescreen(status_ignore);
	    return;
	}
    }
}

/*
 * Top-level functions
 */

/* Initialize the user-controlled options to their default values and
 * parse the command-line options and arguments.
 */
static void initwithcmdline(int argc, char *argv[], startupdata *start)
{
    int	ch;

    programname = argv[0];

    start->updatetimer = FALSE;
    start->showsmileys = FALSE;
    start->silence = FALSE;
    start->allowoffclicks = FALSE;
    start->setupname = NULL;
    actonpress = FALSE;
    while ((ch = getopt(argc, argv, "cehqrs:tv")) != EOF) {
	switch (ch) {
	  case 's':	setupfile = optarg;		break;
	  case 't':	start->updatetimer = TRUE;	break;
	  case 'e':	start->showsmileys = TRUE;	break;
	  case 'q':	start->silence = TRUE;		break;
	  case 'r':	start->allowoffclicks = TRUE;	break;
	  case 'c':	actonpress = TRUE;		break;
	  case 'h':	fputs(yowzitch, stdout);	exit(EXIT_SUCCESS);
	  case 'v':	fputs(vourzhon, stdout);	exit(EXIT_SUCCESS);
	  default:	die(yowzitch);
	}
    }
    if (optind < argc)
	start->setupname = argv[optind++];
    if (optind < argc)
	die("invalid command-line syntax -- %s", argv[optind]);
    if (!setupfile)
	setupfile = makepathname(getenv("HOME"), ".cminesrc");
}

/* main().
 */
int main(int argc, char *argv[])
{
    startupdata	start;
    int		n;

    srand((unsigned)time(NULL));

    initwithcmdline(argc, argv, &start);

    readsetups();

    n = 0;
    if (start.setupname) {
	for (n = 0 ; n < setupcount ; ++n)
	    if (tolower(start.setupname[0]) == tolower(setups[n].name[0]))
		break;
	if (n == setupcount)
	    die("No such setup as \"%s\"", start.setupname);
    }

    if (!ioinitialize(start.updatetimer, start.showsmileys, start.silence,
		      start.allowoffclicks))
	die("Failed to initialize terminal.");

    setupgame(&setups[n]);
    drawgamescreen(status_normal);

    playgame();

    return EXIT_SUCCESS;
}
