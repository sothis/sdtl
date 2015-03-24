PROJECT_NAME	:= sdtlconfig

VERSION		:= $(shell ./version)
UNAMEEXISTS	:= $(shell uname > /dev/null 2>&1; echo $$?)
PWDEXISTS	:= $(shell pwd > /dev/null 2>&1; echo $$?)
GCCEXISTS	:= $(shell gcc --version > /dev/null 2>&1; echo $$?)
CLANGEXISTS	:= $(shell clang --version > /dev/null 2>&1; echo $$?)
#ICCEXISTS	:= $(shell icc --version > /dev/null 2>&1; echo $$?)
GITEXISTS	:= $(shell git --version > /dev/null 2>&1; echo $$?)
TAREXISTS	:= $(shell tar --version > /dev/null 2>&1; echo $$?)
BZIP2EXISTS	:= $(shell bzip2 --help > /dev/null 2>&1; echo $$?)

ifeq ($(VERSION),)
$(error can't determine version string)
endif
ifneq ($(PWDEXISTS), 0)
$(error command 'pwd' not found)
endif
ifneq ($(UNAMEEXISTS), 0)
$(error command 'uname' not found)
endif
ifneq ($(GCCEXISTS), 0)
ifneq ($(CLANGEXISTS), 0)
ifneq ($(ICCEXISTS), 0)
$(error neither 'gcc', 'icc' nor 'clang' found)
endif
endif
endif

PLATFORM	:= $(shell uname)
PWD		:= $(shell pwd)

GCC_MAJOR	:= 0
GCC_MINOR	:= 0
ifeq ($(CONF), debug)
	DEBUG		:= Yes
endif
ifeq ($(CONF), release)
	RELEASE		:= Yes
endif
ifeq ($(CLANGEXISTS), 0)
	HAVE_CLANG	:= Yes
endif
ifeq ($(GCCEXISTS), 0)
	HAVE_GCC	:= Yes
endif
ifeq ($(ICCEXISTS), 0)
	HAVE_ICC	:= Yes
endif
ifndef VERBOSE
	VERB		:= -s
endif

ifeq ($(PLATFORM), Linux)
	PLAT_LINUX	:= Yes
	PLATFORM	:= LINUX
	SO_EXT		:= so
else ifeq ($(PLATFORM), OpenBSD)
	PLAT_OPENBSD	:= Yes
	PLATFORM	:= OPENBSD
	SO_EXT		:= so
else ifeq ($(PLATFORM), FreeBSD)
	PLAT_FREEBSD	:= Yes
	PLATFORM	:= FREEBSD
	SO_EXT		:= so
else ifeq ($(PLATFORM), Darwin)
	PLAT_DARWIN	:= Yes
	PLATFORM	:= DARWIN
	SO_EXT		:= dylib
else ifeq ($(PLATFORM), MINGW32_NT-5.1)
	PLAT_WINNT	:= Yes
	PLATFORM	:= WINNT
	SO_EXT		:= dll
else ifeq ($(PLATFORM), MINGW32_NT-6.2)
	PLAT_WINNT	:= Yes
	PLATFORM	:= WINNT
	SO_EXT		:= dll
else ifeq ($(PLATFORM), MINGW32_NT-6.3)
	PLAT_WINNT	:= Yes
	PLATFORM	:= WINNT
	SO_EXT		:= dll
else
$(error unsupported platform: $(PLATFORM))
endif

ifdef HAVE_GCC
ifndef PLAT_WINNT
# TODO: write shellscript in order to get detailed compiler version information
# and advanced testing possibilities (i.e. greater/less than, not just equality)
	GCC_MAJOR	:= $(shell gcc --version 2>&1 | head -n 1 | \
		cut -d' ' -f3 | cut -d'.' -f1)
	GCC_MINOR	:= $(shell gcc --version 2>&1 | head -n 1 | \
		cut -d' ' -f3 | cut -d'.' -f1)
endif
endif


OUTDIR		:= ./build
BUILDDIR	:= $(OUTDIR)/$(TOOLCHAIN)_$(CONF)

################################################################################

INCLUDES	+= -I./src
INCLUDES	+= -I./include

#SRC		+= ./src/version.c
#.PHONY: ./src/version.c

SRC		+= ./src/conf_test.c

################################################################################


# preprocessor definitions
ifdef RELEASE
DEFINES		+= -DNDEBUG
endif
DEFINES		+= -D__$(PLATFORM)__=1
DEFINES		+= -DVERSION='"$(VERSION)"'
DEFINES		+= -D__$(TOOLCHAIN)__=1


# toolchain configuration
# common flags
CFLAGS		:= -Wall -g

ifeq ($(TOOLCHAIN), gcc)
ifeq ($(GCC_MAJOR), 4)
CFLAGS		+= -fvisibility=hidden
endif
else
CFLAGS		+= -fvisibility=hidden
endif

ifdef PLAT_DARWIN
CFLAGS		+= -mmacosx-version-min=10.7
endif
ifdef M32
CFLAGS		+= -m32
endif

ifdef DEBUG
CFLAGS		+= -O0
endif

ifdef RELEASE
CFLAGS		+= -O3
ifdef PLAT_WINNT
CFLAGS		+= -flto -fwhole-program
else

ifeq ($(GCC_MAJOR), 4)
ifeq ($(GCC_MINOR), 5)
CFLAGS		+= -flto -fuse-linker-plugin
endif
endif
ifeq ($(GCC_MAJOR), 4)
ifeq ($(GCC_MINOR), 6)
CFLAGS		+= -flto -fuse-linker-plugin
endif
endif

endif
endif #RELEASE
CXXFLAGS	:= $(CFLAGS)

# language dependent flags
ifneq ($(TOOLCHAIN), clang)
CFLAGS		+= -std=c99
endif
ifdef RELEASE
CXXFLAGS	+= -fvisibility-inlines-hidden
endif

LDFLAGS		:= $(CFLAGS)
ifdef PLAT_LINUX
#LDFLAGS		+= -static-libgcc
endif
ifdef PLAT_DARWIN
ARFLAGS		:= -static -o
else
ARFLAGS		:= cru
STRIPFLAGS	:= -s
endif


# determine intermediate object filenames
C_SRC		:= $(filter %.c, $(SRC))
CXX_SRC		:= $(filter %.cpp, $(SRC))
CLI_C_SRC	:= $(filter %.c, $(CLI_SRC))

DEPS		:= $(patsubst %.c, $(BUILDDIR)/.obj/%_C.dep, $(C_SRC))
DEPS		+= $(patsubst %.cpp, $(BUILDDIR)/.obj/%_CXX.dep, $(CXX_SRC))
DEPS		+= $(patsubst %.c, $(BUILDDIR)/.obj/%_C.dep, $(CLI_C_SRC))

OBJECTS		:= $(patsubst %.c, $(BUILDDIR)/.obj/%_C.o, $(C_SRC))
OBJECTS		+= $(patsubst %.cpp, $(BUILDDIR)/.obj/%_CXX.o, $(CXX_SRC))

# tools
INSTALL		:= install
STRIP		:=  $(CROSS)strip
ifeq ($(TOOLCHAIN), gcc)
	CC		:= $(CROSS)gcc
	CXX		:= $(CROSS)g++
	ifeq ($(CXX_SRC),)
		LD	:= $(CROSS)gcc
	else
		LD	:= $(CROSS)g++
	endif
endif
ifeq ($(TOOLCHAIN), clang)
	CC		:= clang
	CXX		:= clang++
	ifeq ($(CXX_SRC),)
		LD	:= clang
	else
		LD	:= clang++
	endif
endif
ifeq ($(TOOLCHAIN), icc)
	CC		:= icc -ipo -no-prec-div -static-intel -wd,1338
	CXX		:= icc -ipo -no-prec-div -static-intel -wd,1338
	LD		:= icc -ipo -no-prec-div -static-intel \
				-wd,1338,11021,11000,11001,11006
endif

ifdef PLAT_DARWIN
	AR		:= libtool
else
	AR		:= $(CROSS)ar
endif


print_cp	:= echo $(eflags) "COPY "
print_ar	:= echo $(eflags) "AR   "
print_tar	:= echo $(eflags) "TAR  "
print_ld	:= echo $(eflags) "LD   "
print_as	:= echo $(eflags) "ASM  "
print_cc	:= echo $(eflags) "CC   "
print_cxx	:= echo $(eflags) "CXX  "
print_strip	:= echo $(eflags) "STRIP"
print_inst	:= echo $(eflags) "INST "

# targets
all: release

help:
	@echo "following make targets are available:"
	@echo "  help        - print this"
	@echo "  release     - build release version of $(PROJECT_NAME) (*)"
	@echo "  debug       - build debug version of $(PROJECT_NAME)"
	@echo "  install     - install release version of $(PROJECT_NAME) "\
		"to '$DESTDIR'"
	@echo "  clean       - recursively delete the output directory" \
		"'$(OUTDIR)'"
	@echo ""
	@echo "(*) denotes the default target if none or 'all' is specified"
debug:
	@$(MAKE) CONF=debug $(VERB) -C . all-recursive
release:
	@$(MAKE) CONF=release $(VERB) -C . all-recursive
install: all
	@$(MAKE) CONF=release $(VERB) -C . TOOLCHAIN=gcc install-recursive
.PHONY: install

Release: release
Debug: debug

coverity: clean
	cov-build --dir cov-int make
	tar caf build/$(PROJECT_NAME).xz cov-int

.PHONY: coverity

clean:
	@echo "deleting '$(OUTDIR)'"
	@-rm -rf $(OUTDIR)
	@-rm -rf cov-int

all-recursive:
ifdef HAVE_GCC
	$(MAKE) $(VERB) -C . TOOLCHAIN=gcc final-all-recursive
endif
ifdef HAVE_CLANG
	$(MAKE) $(VERB) -C . TOOLCHAIN=clang final-all-recursive
endif
ifdef HAVE_ICC
	$(MAKE) $(VERB) -C . TOOLCHAIN=icc final-all-recursive
endif

final-all-recursive: $(BUILDDIR)/$(PROJECT_NAME)

install-recursive:
	$(print_inst) $(PROJECT_NAME)
	-mkdir -p $(DESTDIR)/usr/bin
	-mkdir -p $(DESTDIR)/etc/egvpbridge
	$(INSTALL) -s -p -m 0755 -t $(DESTDIR)/usr/bin \
		$(BUILDDIR)/$(PROJECT_NAME)
	$(INSTALL) -p -m 0600 -t $(DESTDIR)/etc/egvpbridge \
		conf/egvpbridge.conf
	$(INSTALL) -p -m 0600 -t $(DESTDIR)/etc/egvpbridge \
		conf/text.xsl
	$(INSTALL) -p -m 0600 -t $(DESTDIR)/etc/egvpbridge \
		conf/html.xsl

deb:
	dpkg-buildpackage -b -D -us -uc
.PHONY: deb

$(BUILDDIR)/$(PROJECT_NAME): $(OBJECTS) $(EXPLICIT_LIBS)
	$(print_ld) $(subst $(PWD)/,./,$(abspath $(@)))
	@-mkdir -p $(dir $(@))
ifdef PLAT_DARWIN
	$(LD) -Wl,-rpath,"@loader_path/" $(MACARCHS) $(LDFLAGS) \
	$(LPATH) $(FRAMEWORKS) -o $(@) $(^) $(LIBRARIES)
else
	@export LD_RUN_PATH='$${ORIGIN}' && $(LD) $(MACARCHS) $(LDFLAGS) \
	$(LPATH) -o $(@) $(OBJECTS) $(EXPLICIT_LIBS) $(LIBRARIES)
endif

$(BUILDDIR)/.obj/%_C.o: %.c
	$(print_cc) $(subst $(PWD)/,./,$(abspath $(<)))
	-mkdir -p $(dir $(@))
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -E -M -MT \
		"$(@) $(@:.o=.dep)" -o $(@:.o=.dep) $(<)
	$(CC) $(CFLAGS) $(MACARCHS) $(DEFINES) $(INCLUDES) -c -o $(@) $(<)

$(BUILDDIR)/.obj/%_C_PIC.o: %.c
	$(print_cc) $(subst $(PWD)/,./,$(abspath $(<)))
	-mkdir -p $(dir $(@))
	$(CC) $(CFLAGS) $(DEFINES) -DPIC $(INCLUDES) -E -M -MT \
		"$(@) $(@:.o=.dep)" -o $(@:.o=.dep) $(<)
	$(CC) -fPIC $(CFLAGS) -DPIC $(MACARCHS) $(DEFINES) \
		$(INCLUDES) -c -o $(@) $(<)

$(BUILDDIR)/.obj/%_CXX.o: %.cpp
	$(print_cxx) $(subst $(PWD)/,./,$(abspath $(<)))
	-mkdir -p $(dir $(@))
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -E -M -MT \
		"$(@) $(@:.o=.dep)" -o $(@:.o=.dep) $(<)
	$(CXX) $(CXXFLAGS) $(MACARCHS) $(DEFINES) $(INCLUDES) -c -o $(@) $(<)

$(BUILDDIR)/.obj/%_CXX_PIC.o: %.cpp
	$(print_cxx) $(subst $(PWD)/,./,$(abspath $(<)))
	-mkdir -p $(dir $(@))
	$(CXX) $(CXXFLAGS) $(DEFINES) -DPIC $(INCLUDES) -E -M -MT \
		"$(@) $(@:.o=.dep)" -o $(@:.o=.dep) $(<)
	$(CXX) -fPIC $(CXXFLAGS) $(MACARCHS) $(DEFINES) -DPIC \
		$(INCLUDES) -c -o $(@) $(<)

-include $(DEPS)
