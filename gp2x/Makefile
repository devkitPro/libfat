#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM)
endif

include $(DEVKITARM)/gp2x_rules


#---------------------------------------------------------------------------------
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing extra header files
# DATA is a list of directories containing binary files
# LIB is where the built library will be placed
# all directories are relative to this makefile
#---------------------------------------------------------------------------------
BUILD		?=	release
SOURCES		:=	../source
INCLUDES	:=	../include
DATA		:=
LIB		:=	$(TOPDIR)/gp2x/lib

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-mthumb -mthumb-interwork

CFLAGS	:=	-g -Wall -O2\
		-mcpu=arm9tdmi -mtune=arm9tdmi\
 		-fomit-frame-pointer\
		-ffast-math \
		$(ARCH)

CFLAGS	+=	$(INCLUDE) -DGP2X
CXXFLAGS	:=	$(CFLAGS)

ASFLAGS	:=	-g $(ARCH)

ifneq ($(BUILD),debug)
export GP2XBIN	:=	$(LIB)/libfat.a
else
export GP2XBIN	:=	$(LIB)/libfatd.a
CFLAGS	+=	-DFAT_DEBUG
endif


#---------------------------------------------------------------------------------
# any extra libraries we wish to link with the project
#---------------------------------------------------------------------------------
LIBS	:= 
#-lnds9
 
#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= 	$(LIBORCUS)
 
#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export TOPDIR ?= $(CURDIR)/..

export DEPSDIR := $(CURDIR)/$(BUILD)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))
 
export CC		:=	$(PREFIX)gcc
export CXX		:=	$(PREFIX)g++
export AR		:=	$(PREFIX)ar
export OBJCOPY	:=	$(PREFIX)objcopy
 
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
 
#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
 
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)
 
export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)
 
.PHONY: $(BUILD) clean
 
#---------------------------------------------------------------------------------
$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
 
#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr debug gp2x_release $(LIB) include

all: $(GP2XBIN)

dist-bin:
	@mkdir -p include
	@cp $(TOPDIR)/include/fat.h $(TOPDIR)/include/libfatversion.h include
	@tar --exclude=.svn --exclude=*CVS* -cvjf $(TOPDIR)/distribute/$(VERSTRING)/libfat-gp2x-$(VERSTRING).tar.bz2 include lib
	
install:
	@mkdir -p $(DESTDIR)$(DEVKITPRO)/liborcus/lib
	@mkdir -p $(DESTDIR)$(DEVKITPRO)/liborcus/include
	@cp lib/libfat.a $(DESTDIR)$(DEVKITPRO)/liborcus/lib
	@cp $(TOPDIR)/include/fat.h $(TOPDIR)/include/libfatversion.h $(DESTDIR)$(DEVKITPRO)/liborcus/include
 
#---------------------------------------------------------------------------------
else
 
DEPENDS	:=	$(OFILES:.o=.d)
 
#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(GP2XBIN)	:	$(OFILES) $(LIB)
	@rm -f "$(GP2XBIN)"
	@$(AR) rcs "$(GP2XBIN)" $(OFILES)
	@echo built ... $(notdir $@)

$(LIB):
	mkdir $(LIB)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data 
#---------------------------------------------------------------------------------
%.bin.o	:	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)


-include $(DEPENDS)
 
#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------

