# Makefile (for GNU Make / 'gmake')
#
# Project: IcoWM ('icowm'), Iconifying Window Manager
# Author: J. A. Corbal (<jacorbal@gmail.com>)

# Copyright (c) 2026, J. A. Corbal
# All rights reserved.
#
# This file is licensed under the 'ISC License'.
# Read the 'LICENSE' file in the root of this repository for details.

## Project metadata
PROJECT_NAME_PROG = icowm
PROJECT_NAME_SHORT = "IcoWM"
PROJECT_NAME_LONG = "Iconifying Window Manager"
PROJECT_VERSION = "1.0.1"
PROJECT_VERSION_CODENAME = "'ovelya"
LICENSE = "ISC License"
COPYRIGHT = "Copyright (c) 2026"
AUTHOR = "J. A. Corbal"
RELEASE_DATE = "20260923"


## Directories
PWD = $(CURDIR)
I_DIR = $(PWD)/include
S_DIR = $(PWD)/src
T_DIR = $(PWD)/tools
L_DIR = $(PWD)/lib
O_DIR = $(PWD)/obj
B_DIR = $(PWD)/bin
TESTS_DIR = $(PWD)/tests

SHELL=/bin/sh

# 'nproc' is GNU coreutils only and is not present on a stock BSD
# system, where 'sysctl -n hw.ncpu' is the native way to ask for the
# same figure.  This file is the GNU Make one, so 'nproc' is tried
# first, with the BSD spelling behind it for a GNU Make build running on
# a BSD (a common enough combination), and '1' as the final, always-safe
# answer if neither tool is there.  Without the fallbacks 'JOBS' came
# out empty on such a system and 'parallel' below became a bare '-j',
# which is unlimited parallelism rather than none.
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null \
        || echo 1)


## Installation directories
# Every one of these is overridable, so a distribution may move a single
# directory without having to restate the rest, and 'DESTDIR' stages the
# whole tree somewhere else for a package build.
OS ?= $(shell uname -s)

PREFIX ?= /usr/local
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
LOCALEDIR ?= $(DATADIR)/locale
XSESSIONSDIR ?= $(DATADIR)/xsessions
DOCDIR ?= $(DATADIR)/doc/$(PROJECT_NAME_PROG)
EXAMPLEDIR ?= $(DATADIR)/$(PROJECT_NAME_PROG)
# Icon themes are searched by this exact layout, so the two files go
# where a theme expects to find them rather than under a directory of
# this project's choosing: the scalable one is what a session list
# shows, and the symbolic one is for the panels and trays that recolor
# what they draw.
ICONDIR ?= $(DATADIR)/icons/hicolor
# Named once rather than written out at each of the four places that
# reach for it, the path being too long to fit a line whole and neither
# make joining a split one the same way
ICON_SCALABLE = $(ICONDIR)/scalable/apps
ICON_SYMBOLIC = $(ICONDIR)/symbolic/apps

# Manual pages sit directly under the prefix on every BSD, and under
# 'share' on Linux and anything else, which is where the FHS puts them.
# Guessed from 'uname', and overridable like the rest for the systems
# that follow neither.
ifneq (,$(filter %BSD DragonFly,$(OS)))
    MANDIR ?= $(PREFIX)/man
else
    MANDIR ?= $(DATADIR)/man
endif

# Neither '-D' nor an owner is asked for anywhere below: the first is
# a GNU extension BSD's 'install' does not have, and the second names
# a group that is 'root' on Linux and 'wheel' on the BSDs.
# The directories are made separately, and ownership is left to whoever
# runs this.
INSTALL ?= install
INSTALL_PROGRAM ?= $(INSTALL) -m 0755
INSTALL_DATA ?= $(INSTALL) -m 0644
INSTALL_DIR ?= $(INSTALL) -d -m 0755
PKGCONF ?= $(shell command -v pkgconf 2>/dev/null || \
           command -v pkg-config 2>/dev/null || echo pkgconf)


## Compiler & linker options
CCSTD = c99  # c89 | c90, c99, c11, c17, gnu11, gnu17,...
CCOPT = 2    # 0:debug; 1:optimize; 2:optimize more; 3:even more
CCOPTS = -pedantic -pedantic-errors
CCEXTRA = -fdiagnostics-color=always -fdiagnostics-show-location=once

CCWARN_POSIX = -D _POSIX_C_SOURCE=200112L  #-D __STRICT_ANSI__

CCWARN_TINY = $(CCWARN_POSIX) -Wpedantic -Wall -Wextra -Wshadow -Wundef \
              -Werror

CCWARN_MORE = -Wwrite-strings -Wconversion -Wdouble-promotion

CCWARN_MOST = -Wformat -Wuninitialized -Wfloat-equal \
              -Wcast-align -Wpointer-arith -Wstrict-overflow=2 \
              -Wunreachable-code -Wmissing-format-attribute \
              -Wdeprecated

CCWARN_GCC = -Wlogical-op -Wstrict-aliasing=3 -Wduplicated-branches \
             -Wformat-overflow -Wformat-signedness \
             -Wno-suggest-attribute=format

CCWARN_CLANG = -Wbad-function-cast -Wextra-semi-stmt -Wmissing-prototypes \
               -Wswitch-enum -Wcovered-switch-default -Wreserved-identifier \
               -Wdeclaration-after-statement -Wsometimes-uninitialized \
               -Wno-fortify-source -Wno-cast-align -Wno-cast-qual \
               -Wdocumentation

CCWARN = $(CCWARN_TINY) $(CCWARN_MORE) $(CCWARN_MOST)

CCDEPS = -MMD -MP

XCB_CFLAGS = $(shell $(PKGCONF) --cflags \
        xcb xcb-keysyms xcb-util xcb-icccm xcb-ewmh xcb-randr xcb-sync \
        xcb-cursor xcb-render xcb-renderutil 2>/dev/null)
FONT_CFLAGS = $(shell $(PKGCONF) --cflags freetype2 fontconfig 2>/dev/null | \
        sed 's/-I/-isystem /g')
JSON_CFLAGS = $(shell $(PKGCONF) --cflags libcjson 2>/dev/null || \
        $(PKGCONF) --cflags cjson 2>/dev/null)
CCFLAGS_BASE = $(CCOPTS) $(CCWARN) -std=$(CCSTD) $(CCEXTRA) -I $(I_DIR) \
               ${CCDEPS}
CCFLAGS = $(CCFLAGS_BASE) $(XCB_CFLAGS) $(FONT_CFLAGS) $(JSON_CFLAGS)

# The same flags without the dependency-file ones, for the 'headers'
# pass: those would write a '.d' beside the makefile for every header
# checked, and outlive the throwaway source describing it
HDRFLAGS = $(CCOPTS) $(CCWARN) -std=$(CCSTD) $(CCEXTRA) -I $(I_DIR) \
           $(XCB_CFLAGS) $(FONT_CFLAGS) $(JSON_CFLAGS)

# 'icowm-msg' (see 'tools/icowm-msg.c') is a small, deliberately
# self-contained IPC client: it never touches X11 at all, so it has no
# reason to pull in the XCB or font libraries the window manager itself
# needs, only JSON for the wire protocol it speaks.
MSG_CCFLAGS = $(CCFLAGS_BASE) $(JSON_CFLAGS)

XCB_LFLAGS = $(shell $(PKGCONF) --libs \
        xcb xcb-keysyms xcb-util xcb-icccm xcb-ewmh xcb-randr xcb-sync \
        xcb-cursor xcb-render xcb-renderutil 2>/dev/null || \
        printf '%s ' '-lxcb' '-lxcb-keysyms' '-lxcb-util' '-lxcb-icccm' \
        '-lxcb-ewmh' '-lxcb-randr' '-lxcb-sync' \
        '-lxcb-cursor' '-lxcb-render' '-lxcb-render-util')
FONT_LFLAGS = $(shell $(PKGCONF) --libs freetype2 fontconfig 2>/dev/null || \
        printf '%s' '-lfreetype -lfontconfig')
JSON_LFLAGS = $(shell $(PKGCONF) --libs libcjson 2>/dev/null || \
        $(PKGCONF) --libs cjson 2>/dev/null || printf '%s' '-lcjson')
# 'gettext' sits inside the C library on Linux, glibc and musl alike,
# and in a library of its own on every BSD, where it comes from
# 'gettext-runtime' or pkgsrc's 'gettext-lib'.  Probed rather than
# assumed, since either make may run on either system: pkg-config first,
# for those shipping a '.pc' file for it, then a link test for those
# that do not, and nothing at all where the C library answers already.
# Without this the link fails outright on a BSD, every call to 'gettext'
# going unresolved.
INTL_LFLAGS = $(shell $(PKGCONF) --libs intl 2>/dev/null || \
        { printf 'int main(void){return 0;}' | \
          $(CC) -x c - -o /dev/null -lintl 2>/dev/null && \
          printf '%s' '-lintl'; } || printf '%s' '')
OTHR_LFLAGS = -lpthread $(INTL_LFLAGS)
LDFLAGS = -L $(L_DIR) $(XCB_LFLAGS) $(FONT_LFLAGS) $(JSON_LFLAGS) \
          $(OTHR_LFLAGS)
MSG_LDFLAGS = -L $(L_DIR) $(JSON_LFLAGS)


## Data & build information
# 'SOURCE_DATE_EPOCH', when the environment sets it, is the agreed way
# for a distribution to ask for a reproducible build: the same sources
# have to give the same binary, whenever they are compiled.  Two things
# here stand in the way of that, and both step aside when it is set.
#
# The timestamp below is taken from that epoch instead of from the
# clock, spelled for GNU 'date' first and for BSD 'date' second, since
# the two disagree about how an epoch is given.
#
# The build number stops counting and the file stops being written.
# It is a counter of this author's own builds, which is useful here and
# meaningless in a package, where it would only record how many times
# somebody else's machine had compiled the sources and leave a tracked
# file dirty for having done so.
SOURCE_DATE_EPOCH ?=

BUILD_NUMBER_FILE = Build
ifneq (,$(wildcard $(BUILD_NUMBER_FILE)))
    LAST_BUILD_NUMBER := $(shell cat $(BUILD_NUMBER_FILE))
else
    LAST_BUILD_NUMBER := 0
endif

ifeq ($(SOURCE_DATE_EPOCH),)
    BUILD_NUMBER := $(shell echo $$(($(LAST_BUILD_NUMBER) + 1)))
    BUILD_TIMESTAMP := $(shell date -u +'%Y%m%dT%H%M')
else
    BUILD_NUMBER := $(LAST_BUILD_NUMBER)
    BUILD_TIMESTAMP := $(shell date -u -d @$(SOURCE_DATE_EPOCH) \
            +'%Y%m%dT%H%M' 2>/dev/null || \
        date -u -r $(SOURCE_DATE_EPOCH) +'%Y%m%dT%H%M' 2>/dev/null || \
        echo 19700101T0000)
endif

CCFLAGS += -D BUILD_NUMBER=$(BUILD_NUMBER)
CCFLAGS += -D BUILD_TIMESTAMP=\"$(BUILD_TIMESTAMP)\"
CCFLAGS += -D PROJECT_NAME_LONG=\"$(PROJECT_NAME_LONG)\"
CCFLAGS += -D PROJECT_NAME_SHORT=\"$(PROJECT_NAME_SHORT)\"
CCFLAGS += -D PROJECT_NAME_PROG=\"$(PROJECT_NAME_PROG)\"
CCFLAGS += -D PROJECT_VERSION=\"$(PROJECT_VERSION)\"
CCFLAGS += -D PROJECT_VERSION_CODENAME=\"$(PROJECT_VERSION_CODENAME)\"
CCFLAGS += -D AUTHOR=\"$(AUTHOR)\"
CCFLAGS += -D COPYRIGHT=\"$(COPYRIGHT)\"
CCFLAGS += -D LICENSE=\"$(LICENSE)\"
CCFLAGS += -D RELEASE_DATE=\"$(RELEASE_DATE)\"
CCFLAGS += -D I18N_DOMAIN=\"icowm\"
CCFLAGS += -D I18N_LOCALE_DIR=\"$(LOCALEDIR)\"

# 'icowm-msg' only ever prints its name, IcoWM's short name, its
# version, its license, its copyright line, and its author (see
# 'tools/icowm-msg.c'); the rest of the metadata above is icowm's '-v'
# output, not something a small IPC client has any reason to report
# about itself.
MSG_CCFLAGS += -D PROJECT_NAME_SHORT=\"$(PROJECT_NAME_SHORT)\"
MSG_CCFLAGS += -D PROJECT_NAME_PROG=\"$(PROJECT_NAME_PROG)\"
MSG_CCFLAGS += -D PROJECT_VERSION=\"$(PROJECT_VERSION)\"
MSG_CCFLAGS += -D PROJECT_VERSION_CODENAME=\"$(PROJECT_VERSION_CODENAME)\"
MSG_CCFLAGS += -D AUTHOR=\"$(AUTHOR)\"
MSG_CCFLAGS += -D COPYRIGHT=\"$(COPYRIGHT)\"
MSG_CCFLAGS += -D LICENSE=\"$(LICENSE)\"


## Options on 'make'
# Compiler: 'make clean && make CC=clang' or 'make clean && make CC=gcc'
CC = gcc
ifeq ($(CC), clang)
    CCWARN += $(CCWARN_CLANG)
else ifeq ($(CC), gcc)
    CCWARN += $(CCWARN_GCC)
else
    $(error Unsupported compiler '$(CC)': CC only admits 'gcc' or 'clang')
endif

# 'clang' has no '=auto' value for '-flto' (only 'thin'/'full', or
# nothing at all); only 'gcc' knows to parallelize its LTRANS pass
# across every core this way.
ifeq ($(CC), gcc)
    LTO_FLAG = -flto=auto
else
    LTO_FLAG = -flto
endif

# Use 'make clean && make DEBUG=1' to add debugging information
# Use 'make clean && make DEBUG=2' to compile & link with address sanitizer
DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CCFLAGS += -DDEBUG -g3 -ggdb3 -O0
else ifeq ($(DEBUG), 2)
    # No '-fanalyzer' alongside the sanitizer.  The two answer different
    # questions, one before the program runs and one while it does, and
    # gcc 13 loses track of its own instrumentation when asked for both:
    # a loop whose condition calls another translation unit, holding
    # a variable that unit fills through a pointer, is reported as a use
    # of an uninitialized value that no initialization silences.  'make
    # analyze' is where the analyzer belongs, and it still runs there.
    CCFLAGS += -DDEBUG -g3 -ggdb3 -O0 \
               -fsanitize=address -fno-omit-frame-pointer -fPIE
    LDFLAGS += -fsanitize=address -pie
else
    CCFLAGS += -DNDEBUG -O$(CCOPT) $(LTO_FLAG) \
               -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE
    LDFLAGS += $(LTO_FLAG) -Wl,-z,relro,-z,now -Wl,-z,noexecstack -pie
endif

# Symbols are discarded by default; 'make STRIP=0' keeps them.  Any
# debug build keeps them whatever this says, a stripped binary being of
# no use to a debugger or a sanitizer.
STRIP ?= 1
ifeq ($(DEBUG), 0)
    ifneq ($(STRIP), 0)
        LDFLAGS += -s
        MSG_LDFLAGS += -s
    endif
endif

# Use 'make COMPACT=1' to shrink several compile-time array capacities
# throughout the codebase, for building specifically for a severely
# memory-constrained target.
# Independent of restricted-memory mode ('icowm -M <mib>').  It does not
# turn that mode on by itself, and it does not supply a default for '-M
# <mib>' when that flag is left off at run time either.
COMPACT ?=
ifneq ($(COMPACT),)
CCFLAGS += -D COMPACT
endif

# Use 'make analyze' to run a static-analysis pass over the whole
# project without touching the normal object files ('gcc')
ANALYZE ?= 0
ifeq ($(ANALYZE), 1)
ifeq ($(CC), gcc)
CCFLAGS += -fanalyzer
endif
endif


## Makefile files & directories
.SUFFIXES:
.SUFFIXES: .h .c .o

# Binary file options and running arguments
TARGET = $(B_DIR)/$(PROJECT_NAME_PROG)
MSG_TARGET = $(B_DIR)/$(PROJECT_NAME_PROG)-msg
DOXIGEN_FILE = Doxyfile
ARGS ?=

# Sources, objects and auto-generated dependencies
#
# Found rather than listed one directory level at a time.  The four
# 'wildcard' patterns this replaces reached exactly as deep as the tree
# currently goes, so a source added one level below that would have been
# left out of the build with nothing said about it: the failure shows up
# at link time, as a missing symbol, naming neither the file nor the
# reason.  Sorted so the same tree always builds in the same order.
# This is what 'BSDmakefile' already does.
SRCS = $(shell find $(S_DIR) -name '*.c' | sort)
OBJS = $(patsubst $(S_DIR)/%.c, $(O_DIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

# 'icowm-msg' (see 'tools/icowm-msg.c') builds and links entirely
# separately from icowm itself.  Its single object never joins 'OBJS',
# and its binary never joins 'TARGET', so a change to one never forces
# a rebuild of the other.
MSG_SRCS = $(shell find $(T_DIR) -maxdepth 1 -name '*.c' | sort)
MSG_OBJS = $(patsubst $(T_DIR)/%.c, $(O_DIR)/tools/%.o, $(MSG_SRCS))
MSG_DEPS = $(MSG_OBJS:.o=.d)


## Options
.DEFAULT_GOAL := all

# Make all, create needed directories and build
all: mkdirs $(TARGET) $(MSG_TARGET) ctags
	@echo "Build $(BUILD_NUMBER)"

parallel: ctags
	$(MAKE) -j$(JOBS) all

mkdirs:
	@mkdir -p $(B_DIR) $(O_DIR) $(O_DIR)/tools
	@find $(S_DIR) -mindepth 1 -type d | \
	    sed 's|$(S_DIR)/||' | \
	    while read dir; do \
	        mkdir -p "$(O_DIR)/$$dir"; \
	    done

# Linkage
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
ifeq ($(SOURCE_DATE_EPOCH),)
	@echo "Increasing build number to $(BUILD_NUMBER)..."
	@echo $(BUILD_NUMBER) >$(BUILD_NUMBER_FILE)
else
	@echo "Reproducible build $(BUILD_NUMBER); build file left alone"
endif

$(MSG_TARGET): $(MSG_OBJS)
	$(CC) -o $@ $^ $(MSG_LDFLAGS)

# Compilation
#
# 'mkdirs' is an order-only prerequisite of both rules below, written
# with a pipe so the object files never carry a dependency on its
# timestamp.  Naming it in 'all' alone was not enough: GNU Make gives no
# order to the prerequisites of a target under '-j', so a parallel build
# from a clean tree could start compiling before the object directories
# existed, and fail on the dependency file it could not open.  Every
# object needing the directories says so itself now.
$(O_DIR)/%.o: $(S_DIR)/%.c | mkdirs
	$(CC) $(CCFLAGS) -c $< -o $@

$(O_DIR)/tools/%.o: $(T_DIR)/%.c | mkdirs
	$(CC) $(MSG_CCFLAGS) -c $< -o $@


## Tests
#
# See 'tests/Makefile.mk' for every test-related rule and variable; kept
# in its own file to keep this one focused on building IcoWM itself.
# Included, not sub-made, so it shares this Makefile's own variables
# ('CC', 'CCSTD', 'I_DIR', 'S_DIR', and so on) directly, with no need to
# re-export or duplicate any of them.
ifeq ($(wildcard $(TESTS_DIR)/Makefile.mk),)
    $(error Cannot find $(TESTS_DIR)/Makefile.mk)
else
    include $(TESTS_DIR)/Makefile.mk
endif


# Other options
ctags:
ifneq (,$(shell command -v ctags 2>/dev/null))
	@echo "Generating tags..."
	@ctags -R --exclude='doc' --exclude='obj' --exclude='tmp' .
else
	@echo "Skipping tags: 'ctags' not found"
endif

install:
	@test -x $(TARGET) || { \
	    echo "install: $(TARGET) is not built; run 'make' first" >&2; \
	    exit 1; }
	@test -x $(MSG_TARGET) || { \
	    echo "install: $(MSG_TARGET) is not built; run 'make' first" >&2; \
	    exit 1; }
	$(INSTALL_DIR) $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) $(TARGET) $(DESTDIR)$(BINDIR)
	$(INSTALL_PROGRAM) $(MSG_TARGET) $(DESTDIR)$(BINDIR)
	$(INSTALL_DIR) $(DESTDIR)$(MANDIR)/man1 $(DESTDIR)$(MANDIR)/man5
	$(INSTALL_DATA) doc/man/man1/*.1 $(DESTDIR)$(MANDIR)/man1
	$(INSTALL_DATA) doc/man/man5/*.5 $(DESTDIR)$(MANDIR)/man5
	$(INSTALL_DIR) $(DESTDIR)$(XSESSIONSDIR)
	$(INSTALL_DATA) doc/$(PROJECT_NAME_PROG).desktop \
	    $(DESTDIR)$(XSESSIONSDIR)
	$(INSTALL_DIR) $(DESTDIR)$(DOCDIR)
	$(INSTALL_DATA) README.md LICENSE COMPLIANCE.md $(DESTDIR)$(DOCDIR)
	$(INSTALL_DATA) doc/config.md doc/themes.md doc/icowm.md \
	    doc/icowm-msg.md $(DESTDIR)$(DOCDIR)
	$(INSTALL_DIR) $(DESTDIR)$(ICON_SCALABLE)
	$(INSTALL_DATA) doc/icon/$(PROJECT_NAME_PROG).svg \
	    $(DESTDIR)$(ICON_SCALABLE)
	$(INSTALL_DIR) $(DESTDIR)$(ICON_SYMBOLIC)
	$(INSTALL_DATA) doc/icon/$(PROJECT_NAME_PROG)-symbolic.svg \
	    $(DESTDIR)$(ICON_SYMBOLIC)
	@find locale -name '*.mo' | while read mo; do \
	    lang=$$(echo "$$mo" | cut -d/ -f2); \
	    $(INSTALL_DIR) \
	        "$(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES"; \
	    $(INSTALL_DATA) "$$mo" \
	        "$(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES"; \
	done
	@find doc/config.example -type d | \
	    sed 's|doc/config.example||' | \
	    while read dir; do \
	        $(INSTALL_DIR) "$(DESTDIR)$(EXAMPLEDIR)$$dir"; \
	    done
	@find doc/config.example -type f | \
	    sed 's|doc/config.example/||' | \
	    while read file; do \
	        $(INSTALL_DATA) "doc/config.example/$$file" \
	            "$(DESTDIR)$(EXAMPLEDIR)/$$file"; \
	    done
	@echo "Installed under $(DESTDIR)$(PREFIX)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(PROJECT_NAME_PROG)
	rm -f $(DESTDIR)$(BINDIR)/$(PROJECT_NAME_PROG)-msg
	@for page in doc/man/man1/*.1; do \
	    rm -f "$(DESTDIR)$(MANDIR)/man1/$$(basename $$page)"; \
	done
	@for page in doc/man/man5/*.5; do \
	    rm -f "$(DESTDIR)$(MANDIR)/man5/$$(basename $$page)"; \
	done
	rm -f $(DESTDIR)$(XSESSIONSDIR)/$(PROJECT_NAME_PROG).desktop
	rm -f $(DESTDIR)$(ICON_SCALABLE)/$(PROJECT_NAME_PROG).svg
	rm -f $(DESTDIR)$(ICON_SYMBOLIC)/$(PROJECT_NAME_PROG)-symbolic.svg
	@find locale -name '*.mo' | while read mo; do \
	    lang=$$(echo "$$mo" | cut -d/ -f2); \
	    rm -f \
	        "$(DESTDIR)$(LOCALEDIR)/$$lang/LC_MESSAGES/$$(basename $$mo)"; \
	done
	rm -rf $(DESTDIR)$(EXAMPLEDIR)
	rm -rf $(DESTDIR)$(DOCDIR)
	@echo "Removed from $(DESTDIR)$(PREFIX)"

ccflags:
	@echo $(CCFLAGS)

ldflags:
	@echo $(LDFLAGS)

clean-obj:
	@rm -f $(OBJS) $(DEPS) $(MSG_OBJS) $(MSG_DEPS)
	@rm -rf $(O_DIR)/* $(O_DIR)

clean-bin:
	@rm -f $(TARGET) $(MSG_TARGET)
	@rm -rf $(B_DIR)

clean-build:
	@-rm -f $(BUILD_NUMBER_FILE)

clean: clean-obj clean-bin

run:
	$(TARGET) $(ARGS)

hard: clean all

hard-run: hard run

doxygen:
ifneq (,$(shell command -v doxygen 2>/dev/null))
	@if [ -f '$(DOXIGEN_FILE)' ]; then \
	    doxygen; \
	else \
	    echo "Error: '$(DOXIGEN_FILE)' not found" >&2; \
	fi
else
	@echo "Skipping doxygen: 'doxygen' not found" >&2
endif

# Packages a source-only snapshot for distribution.  Excludes anything
# generated ('tmp', 'bin', 'obj', 'tags', 'Build', 'compile_commands.json')
# and every dotfile/dotdir ('.*'), plus 'doc/doxygen': the user can
# always regenerate that with 'make doxygen', so shipping it would only
# add dead weight to the archive.
dist: clean
	@ver=$$(echo '$(PROJECT_VERSION)' | tr -d '"'); \
	dirname=$$(basename "$(PWD)"); \
	outfile="../$(PROJECT_NAME_PROG)_$${ver}.tar.gz"; \
	tar --exclude='.*' --exclude='tmp' --exclude='bin' --exclude='obj' \
	    --exclude='tags' --exclude='Build' --exclude='compile_commands.json' \
	    --exclude='doc/doxygen' \
	    -czf "$$outfile" -C .. "$$dirname"; \
	echo "Created $$outfile"

headers:
	@fail=0; total=0; \
	for h in $$(cd $(I_DIR) && find . -name '*.h' | sed 's|^\./||'); do \
	    total=$$((total + 1)); \
	    printf '#include <%s>\nint main(void){return 0;}\n' "$$h" \
	        | $(CC) $(HDRFLAGS) -fsyntax-only -x c -  2>/dev/null \
	        || { echo "not self-contained: $$h" >&2; \
	             fail=$$((fail + 1)); }; \
	done; \
	echo "$$((total - fail))/$$total headers compile on their own"; \
	[ $$fail -eq 0 ]

analyze:
ifeq ($(CC), gcc)
	$(MAKE) ANALYZE=1 all
else
	@command -v scan-build >/dev/null 2>&1 || \
	    { echo "Error: 'scan-build' not found (part of the" \
	           "clang-tools/llvm package); required to analyze" \
	           "under clang" >&2; exit 1; }
	scan-build --use-cc=$(CC) $(MAKE) all
endif

help:
	@echo "Command:"
	@echo "  make all              Build project"
	@echo "  make parallel         Build with parallel jobs"
	@echo "  make clean-obj        Clean object files"
	@echo "  make clean            Clean binary and object files"
	@echo "  make ctags            Generate tag files for source"
	@echo "  make doxygen          Create Doxygen documentation"
	@echo "  make dist             Package a source-only tarball for release"
	@echo "  make analyze          Run a static-analysis pass (if 'gcc')"
	@echo "  make hard             Clean and build"
	@echo "  make run              Run binary (if exists)"
	@echo "  make run ARGS=<args>  Run with arguments (if binary exists)"
	@echo "  make hard-run         Clean, build and run (if binary exists)"
	@echo "  make test             Build and run every 'tests/*/test_*.c'"
	@echo "  make headers          Check every header compiles alone"
	@echo "  make install          Install under 'PREFIX'"
	@echo "  make uninstall        Remove what 'install' put there"
	@echo "  make help             Show this help"
	@echo
	@echo "Options:"
	@echo "  Use 'CC=<compiler>' to select a compiler ('gcc' or 'clang')"
	@echo "  Use 'JOBS=<n>' to compile with 'n' parallel jobs using 'parallel'"
	@echo "  Use 'DEBUG=1' to generate detailed debug information"
	@echo "  Use 'DEBUG=2' to also link with address sanitizer"
	@echo "  Use 'STRIP=0' to keep symbols (they are discarded by default)"
	@echo "  Use 'PREFIX=<dir>' to install elsewhere (default, '/usr/local')"
	@echo "  Use 'DESTDIR=<dir>' to stage an install for packaging"
	@echo "  Use 'COMPACT=1' to build using smaller arrays"
	@echo
	@echo "Binary will be placed in '$(TARGET)'"
	@echo "IPC client tool will be placed in '$(MSG_TARGET)'"


## Auto-generated header dependecies
-include $(DEPS)
-include $(MSG_DEPS)

## Phony targets
.PHONY: all mkdirs ctags clean clean-obj clean-bin clean-build \
    run hard hard-run test headers install uninstall \
    doxygen dist analyze ccflags ldflags parallel help
