# Edit as appropriate for your system.
#
prefix = /usr/local

CC = gcc -ggdb
CFLAGS = -Wall
LDFLAGS = -Wall
LOADLIBES = -lgpm

# Uncomment if your VT-100 bullet character doesn't look right.
#
#CFLAGS += -DUSE_LATIN1

# Uncomment the following line to build the curses-based version.
#
#LOADLIBES += -lncurses

OBJS = cmines.o userio.o

cmines: $(OBJS)

clean:
	rm -f $(OBJS) cmines

install: cmines
	install -d $(prefix)/games
	install -c ./cmines $(prefix)/games/cmines
	install -d $(prefix)/man/man6
	install -c ./cmines.6 $(prefix)/man/man6/cmines.6

cmines.o: cmines.c cmines.h userio.h
userio.o: userio.c cmines.h userio.h
