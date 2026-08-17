# Makefile (for GNU Make)
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
PROJECT_VERSION = "1.0.1-rc.1"
PROJECT_VERSION_CODENAME = "'ovelya"
LICENSE = "ISC License"
COPYRIGHT = "Copyright (c) 2026"
AUTHOR = "J. A. Corbal"
RELEASE_DATE = "20261221 (intended)"


## Directories
PWD = $(CURDIR)
I_DIR = $(PWD)/include
S_DIR = $(PWD)/src
T_DIR = $(PWD)/tools
TESTS_DIR = $(PWD)/tests
L_DIR = $(PWD)/lib
O_DIR = $(PWD)/obj
B_DIR = $(PWD)/bin

SHELL=/bin/sh
JOBS ?= $(shell nproc)
PKGCONF ?= $(shell command -v pkgconf 2>/dev/null || \
           command -v pkg-config 2>/dev/null || echo pkgconf)


## Compiler & linker options
CCSTD = c99  # c89 | c90, c99, c11, c17, gnu11, gnu17,...
CCOPT = 3    # 0:debug; 1:optimize; 2:optimize more; 3:even more
CCOPTS = -pedantic -pedantic-errors
CCEXTRA = -fdiagnostics-color=always -fdiagnostics-show-location=once

CCWARN_POSIX = -D _POSIX_C_SOURCE=200112L  #-D __STRICT_ANSI__

CCWARN_TINY = $(CCWARN_POSIX) -Wpedantic -Wall -Wextra -Wshadow -Wundef \
              -Werror

CCWARN_MORE = -Wwrite-strings -Wconversion -Wdouble-promotion

CCWARN_MOST = -Wformat -Wuninitialized -Wfloat-equal \
              -Wcast-align -Wpointer-arith -Wstrict-overflow=5 \
              -Wunreachable-code -Wmissing-format-attribute \
              -Wdeprecated

CCWARN_GCC = -Wlogical-op -Wstrict-aliasing=3 -Wduplicated-branches \
             -Wformat-overflow -Wformat-signedness -Wstrict-aliasing=3 \
             -Wno-suggest-attribute=format   -fwrapv

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
# 'icowm-msg' (see 'tools/icowm-msg.c') is a small, deliberately
# self-contained IPC client: it never touches X11 at all, so it has
# no reason to pull in the XCB or font libraries the window manager
# itself needs, only JSON for the wire protocol it speaks.
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
OTHR_LFLAGS = -lpthread
LDFLAGS = -L $(L_DIR) $(XCB_LFLAGS) $(FONT_LFLAGS) $(JSON_LFLAGS) \
          $(OTHR_LFLAGS)
MSG_LDFLAGS = -L $(L_DIR) $(JSON_LFLAGS)


## Data & build information
BUILD_NUMBER_FILE = Build
ifneq (,$(wildcard $(BUILD_NUMBER_FILE)))
    LAST_BUILD_NUMBER := $(shell cat $(BUILD_NUMBER_FILE))
else
    LAST_BUILD_NUMBER := 0
endif
BUILD_NUMBER := $(shell echo $$(($(LAST_BUILD_NUMBER) + 1)))

CCFLAGS += -D BUILD_NUMBER=$(BUILD_NUMBER)
CCFLAGS += -D BUILD_TIMESTAMP=\"$(shell date -u +'%Y%m%dT%H%M')\"
CCFLAGS += -D PROJECT_NAME_LONG=\"$(PROJECT_NAME_LONG)\"
CCFLAGS += -D PROJECT_NAME_SHORT=\"$(PROJECT_NAME_SHORT)\"
CCFLAGS += -D PROJECT_NAME_PROG=\"$(PROJECT_NAME_PROG)\"
CCFLAGS += -D PROJECT_VERSION=\"$(PROJECT_VERSION)\"
CCFLAGS += -D PROJECT_VERSION_CODENAME=\"$(PROJECT_VERSION_CODENAME)\"
CCFLAGS += -D AUTHOR=\"$(AUTHOR)\"
CCFLAGS += -D COPYRIGHT=\"$(COPYRIGHT)\"
CCFLAGS += -D LICENSE=\"$(LICENSE)\"
CCFLAGS += -D RELEASE_DATE=\"$(RELEASE_DATE)\"
CCFLAGS += -D I18N_DOMAIN=\"default\"
CCFLAGS += -D I18N_LOCALE_DIR=\"$(CURDIR)/locale\"

# 'icowm-msg' only ever prints its own name, IcoWM's own short name,
# its version, its license, its copyright line, and its author (see
# 'tools/icowm-msg.c'); the rest of the metadata above is icowm's
# own '-v' output, not something a small IPC client has any reason
# to report about itself.
MSG_CCFLAGS += -D PROJECT_NAME_SHORT=\"$(PROJECT_NAME_SHORT)\"
MSG_CCFLAGS += -D PROJECT_NAME_PROG=\"$(PROJECT_NAME_PROG)\"
MSG_CCFLAGS += -D PROJECT_VERSION=\"$(PROJECT_VERSION)\"
MSG_CCFLAGS += -D PROJECT_VERSION_CODENAME=\"$(PROJECT_VERSION_CODENAME)\"
MSG_CCFLAGS += -D AUTHOR=\"$(AUTHOR)\"
MSG_CCFLAGS += -D COPYRIGHT=\"$(COPYRIGHT)\"
MSG_CCFLAGS += -D LICENSE=\"$(LICENSE)\"


## Options on 'make'
# Compiler: 'make clean && make CC=clang' or 'make clean && make CC=gcc'
CC = clang
ifeq ($(CC), clang)
    CCWARN += $(CCWARN_CLANG)
else ifeq ($(CC), gcc)
    CCWARN += $(CCWARN_GCC)
else
    $(error Unsupported compiler '$(CC)': CC only admits 'gcc' or 'clang')
endif

# Use 'make clean && make DEBUG=1' to add debugging information
# Use 'make clean && make DEBUG=2' to compile & link with address sanitizer
DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CCFLAGS += -DDEBUG -g3 -ggdb3 -O0
else ifeq ($(DEBUG), 2)
    CCFLAGS += -DDEBUG -g3 -ggdb3 -O0 \
               -fsanitize=address -fno-omit-frame-pointer
    LDFLAGS += -fsanitize=address -fPIE
    ifeq ($(CC), gcc)
        CCFLAGS += -fanalyzer
    endif
else
    CCFLAGS += -DNDEBUG -O$(CCOPT)
endif

# Use 'make clean && make STRIP=1' to discard symbols from object files
STRIP ?= 0
ifeq ($(STRIP), 1)
    LDFLAGS += -s
endif

# Use 'make COMPACT=1' to shrink several compile-time array capacities
# throughout the codebase, for building specifically for a severely
# memory-constrained target.
#
# Independent of restricted-memory mode ('icowm -M <mib>').
# It does not turn that mode on by itself, and it does not supply
# a default for '-M  <mib>' when that flag is left off at run time
# either.
#
# See 'defs/compact.h' for a broader explanation.
COMPACT ?=
ifneq ($(COMPACT),)
CCFLAGS += -D COMPACT
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
SRCS = $(wildcard $(S_DIR)/*.c) \
       $(wildcard $(S_DIR)/*/*.c) \
       $(wildcard $(S_DIR)/*/*/*.c) \
       $(wildcard $(S_DIR)/*/*/*/*.c)
OBJS = $(patsubst $(S_DIR)/%.c, $(O_DIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

# 'icowm-msg' (see 'tools/icowm-msg.c') builds and links entirely
# separately from icowm itself: its own single object never joins
# 'OBJS', and its own binary never joins 'TARGET', so a change to
# one never forces a rebuild of the other.
MSG_SRCS = $(wildcard $(T_DIR)/*.c)
MSG_OBJS = $(patsubst $(T_DIR)/%.c, $(O_DIR)/tools/%.o, $(MSG_SRCS))
MSG_DEPS = $(MSG_OBJS:.o=.d)


## Options
.DEFAULT_GOAL := all

# Make all, create needed directories and build
all: mkdirs $(TARGET) $(MSG_TARGET) ctags
	@echo "Build $(BUILD_NUMBER)"

parallel:
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
	@echo "Increasing build number to $(BUILD_NUMBER)..."
	@echo $(BUILD_NUMBER) >$(BUILD_NUMBER_FILE)

$(MSG_TARGET): $(MSG_OBJS)
	$(CC) -o $@ $^ $(MSG_LDFLAGS)

# Compilation
$(O_DIR)/%.o: $(S_DIR)/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

$(O_DIR)/tools/%.o: $(T_DIR)/%.c
	$(CC) $(MSG_CCFLAGS) -c $< -o $@


## Tests
#
# See 'tests/Makefile.mk' for every test-related rule and variable; kept
# in its own file to keep this one focused on building IcoWM itself.
# Included, not sub-made, so it shares this Makefile's own variables
# ('CC', 'CCSTD', 'I_DIR', 'S_DIR', and so on) directly, with no need to
# re-export or duplicate any of them.
include tests/Makefile.mk


# Other options
ctags:
ifneq (,$(shell command -v ctags 2>/dev/null))
	@echo "Generating tags..."
	@ctags -R --exclude='doc' --exclude='obj' --exclude='tmp' .
else
	@echo "Skipping tags: 'ctags' not found"
endif

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
	@[ -f '$(DOXIGEN_FILE)' ] && doxygen || \
		echo "Error: '$(DOXIGEN_FILE)' not found" >&2

help:
	@echo "Command:"
	@echo "  make all               Build project"
	@echo "  make parallel          Build with parallel jobs"
	@echo "  make clean-obj         Clean object files"
	@echo "  make clean             Clean binary and object files"
	@echo "  make ctags             Generate tag files for source"
	@echo "  make doxygen           Create Doxygen documentation"
	@echo "  make hard              Clean and build"
	@echo "  make run               Run binary (if exists)"
	@echo "  make run ARGS=<args>   Run with arguments (if binary exists)"
	@echo "  make hard-run          Clean, build and run (if binary exists)"
	@echo "  make test              Build and run every tests/*/test_*.c"
	@echo
	@echo "Options:"
	@echo "  Use 'CC=<compiler>' to select a compiler ('gcc' or 'clang')"
	@echo "  Use 'JOBS=<n>' to compile with 'n' parallel jobs using 'parallel'"
	@echo "  Use 'DEBUG=1' to generate detailed debug information"
	@echo "  Use 'DEBUG=2' to also link with address sanitizer"
	@echo "  Use 'STRIP=1' to build and discard symbols from object files"
	@echo "  Use 'COMPACT=1' to build using smaller arrays"
	@echo
	@echo "Binary will be placed in '$(TARGET)'"
	@echo "IPC client tool will be placed in '$(MSG_TARGET)'"


## Auto-generated header dependecies
-include $(DEPS)
-include $(MSG_DEPS)

## Phony targets
.PHONY: all mkdirs ctags clean clean-obj clean-bin clean-build run \
        hard hard-run doxygen ccflags ldflags parallel help
