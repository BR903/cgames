# Edit these to suit your preferences.
#
prefix = /usr/local
datadir = $(prefix)/share/csokoban

CC = gcc
CFLAGS = -Wall -O '-DDATADIR="$(datadir)"'
LDFLAGS = -Wall -O -s

# Uncomment this line to build the curses-based version.
#
#LOADLIBES = -lncurses

OBJS = csokoban.o movelist.o fileread.o answers.o play.o dirio.o userio.o

csokoban: $(OBJS)

clean:
	rm -f $(OBJS) csokoban

install: csokoban
	install -d $(prefix)/games
	install -c ./csokoban $(prefix)/games/csokoban
	install -d $(prefix)/man/man6
	install -c ./csokoban.6 $(prefix)/man/man6/csokoban.6
	install -d -g games $(datadir)
	install -c ./series/* $(datadir)/

movelist.o: movelist.c gen.h movelist.h
dirio.o   : dirio.c gen.h dirio.h
userio.o  : userio.c gen.h csokoban.h userio.h
fileread.o: fileread.c gen.h csokoban.h movelist.h dirio.h userio.h \
            answers.h fileread.h
answers.o : answers.c gen.h csokoban.h dirio.h movelist.h fileread.h \
            play.h answers.h
play.o    : play.c gen.h csokoban.h userio.h play.h movelist.h fileread.h
csokoban.o: csokoban.c gen.h csokoban.h movelist.h dirio.h fileread.h \
            answers.h play.h userio.h
