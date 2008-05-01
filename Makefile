ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro)
endif
 
export TOPDIR	:=	$(CURDIR)
 
export DATESTRING	:=	$(shell date +%Y)$(shell date +%m)$(shell date +%d)

default: release

all: release dist

release: nds-release gba-release cube-release wii-release

ogc-release: cube-release wii-release

nds-release:
	$(MAKE) -C nds BUILD=release

gba-release:
	$(MAKE) -C gba BUILD=release

cube-release:
	$(MAKE) -C libogc PLATFORM=cube BUILD=cube_release

wii-release:
	$(MAKE) -C libogc PLATFORM=wii BUILD=wii_release

debug: nds-debug gba-debug cube-debug wii-debug

ogc-debug: cube-debug wii-debug

nds-debug:
	$(MAKE) -C nds BUILD=debug

gba-debug:
	$(MAKE) -C gba BUILD=debug


cube-debug:
	$(MAKE) -C libogc PLATFORM=cube BUILD=wii_debug

wii-debug:
	$(MAKE) -C libogc PLATFORM=wii BUILD=cube_debug

clean: nds-clean gba-clean ogc-clean

nds-clean:
	$(MAKE) -C nds clean

gba-clean:
	$(MAKE) -C gba clean

ogc-clean:
	$(MAKE) -C libogc clean

dist-bin: nds-dist-bin gba-dist-bin ogc-dist-bin

nds-dist-bin: nds-release distribute/$(DATESTRING)
	$(MAKE) -C nds dist-bin

gba-dist-bin: gba-release distribute/$(DATESTRING)
	$(MAKE) -C gba dist-bin

ogc-dist-bin: ogc-release distribute/$(DATESTRING)
	$(MAKE) -C libogc dist-bin

dist-src: distribute/$(DATESTRING)
	@tar --exclude=*CVS* -cvjf distribute/$(DATESTRING)/libfat-src-$(DATESTRING).tar.bz2 \
	source include Makefile \
	nds/Makefile nds/include \
	gba/Makefile gba/include \
	libogc/Makefile libogc/include

dist: dist-bin dist-src

distribute/$(DATESTRING):
	@[ -d $@ ] || mkdir -p $@


install: nds-install gba-install ogc-install

nds-install: nds-release
	make -C nds install

gba-install: gba-release
	make -C gba install

ogc-install: cube-release wii-release
	make -C libogc install
