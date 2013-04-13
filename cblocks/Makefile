# Edit these to suit your preferences.
#
prefix = /usr/local
datadir = $(prefix)/share/cblocks

CC = gcc
CFLAGS = -Wall -O '-DDATADIR="$(datadir)"'
LDFLAGS = -Wall -O -s
LOADLIBES = -lgpm

# Uncomment this line to build the curses-based version.
#
#LOADLIBES += -lncurses

OBJS = cblocks.o movelist.o parse.o fileread.o answers.o play.o dirio.o userio.o

cblocks: $(OBJS)

clean:
	rm -f $(OBJS) cblocks

install: cblocks
	install -d $(prefix)/games
	install -c ./cblocks $(prefix)/games/cblocks
	install -d $(prefix)/man/man6
	install -c ./cblocks.6 $(prefix)/man/man6/cblocks.6
	install -d -g games $(datadir)
	install -c ./series/* $(datadir)/

movelist.o: movelist.c gen.h movelist.h
dirio.o   : dirio.c gen.h dirio.h
userio.o  : userio.c gen.h cblocks.h userio.h
parse.o   : parse.c gen.h cblocks.h userio.h fileread.h movelist.h \
            parse.h
fileread.o: fileread.c gen.h cblocks.h movelist.h dirio.h userio.h \
            answers.h fileread.h parse.h
answers.o : answers.c gen.h cblocks.h dirio.h movelist.h fileread.h \
            play.h answers.h
play.o    : play.c gen.h cblocks.h userio.h play.h movelist.h fileread.h
cblocks.o : cblocks.c gen.h cblocks.h movelist.h dirio.h fileread.h \
            answers.h play.h userio.h
