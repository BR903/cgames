# Top-level makefile for cgames

all: csokoban-build cmines-build cblocks-build
install: csokoban-install cmines-install cblocks-install

clean: csokoban-clean cmines-clean cblocks-clean
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

cmines-build:
	make -C cmines
cmines-install:
	make -C cmines install
cmines-clean:
	make -C cmines clean

cblocks-build:
	make -C cblocks
cblocks-install:
	make -C cblocks install
cblocks-clean:
	make -C cblocks clean
