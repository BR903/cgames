# Top-level makefile for cgames

all: csokoban-build cmines-build cblocks-build
install: csokoban-install cmines-install cblocks-install

clean: csokoban-clean cmines-clean cblocks-clean
	rm -f config.log config.status

distclean: csokoban-distclean cmines-distclean cblocks-distclean
	rm -f config.cache config.log config.status

csokoban: csokoban-build csokoban-install
cmines: cmines-build cmines-install
cblocks: cblocks-build cblocks-install

csokoban-build:
	make -C csokoban
csokoban-install:
	make -C csokoban install
csokoban-clean:
	make -C csokoban clean
csokoban-distclean:
	make -C csokoban distclean
	rm csokoban/userio.c

cmines-build:
	make -C cmines
cmines-install:
	make -C cmines install
cmines-clean:
	make -C cmines clean
cmines-distclean:
	make -C cmines distclean
	rm cmines/userio.c

cblocks-build:
	make -C cblocks
cblocks-install:
	make -C cblocks install
cblocks-clean:
	make -C cblocks clean
cblocks-distclean:
	make -C cblocks distclean
	rm cblocks/userio.c
