ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro)
endif
 
export TOPDIR	:=	$(CURDIR)
 
export DATESTRING	:=	$(shell date +%Y)$(shell date +%m)$(shell date +%d)

default: release

all: release dist

release: 
	make -C nds BUILD=release
	make -C gba BUILD=release

debug:
	make -C nds BUILD=debug
	make -C gba BUILD=debug

clean:
	make -C nds clean
	make -C gba clean

dist-bin: release distribute/$(DATESTRING)
	make -C nds dist-bin
	make -C gba dist-bin

dist-src: distribute/$(DATESTRING)
	@tar --exclude=*CVS* -cvjf distribute/$(DATESTRING)/libfat-src-$(DATESTRING).tar.bz2 \
	source include Makefile \
	nds/Makefile nds/include \
	gba/Makefile gba/include 

dist: dist-bin dist-src

distribute/$(DATESTRING): distribute
	@[ -d $@ ] || mkdir -p $@

distribute:
	@[ -d $@ ] || mkdir -p $@

install: dist
	make -C nds install
	make -C gba install
