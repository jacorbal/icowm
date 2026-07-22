# Makefile (for GNU Make)
#
# Project: IcoWM (`icowm`) -- Iconifying Window Manager
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
PROJECT_VERSION = "0.1.0-alpha"
PROJECT_VERSION_CODENAME = "'ovelya"
LICENSE = "ISC License"
COPYRIGHT = "Copyright (c) 2026"
AUTHOR = "J. A. Corbal"
RELEASE_DATE = "20261221 (intended)"


## Directories
PWD   = $(CURDIR)
I_DIR = $(PWD)/include
S_DIR = $(PWD)/src
L_DIR = $(PWD)/lib
O_DIR = $(PWD)/obj
B_DIR = $(PWD)/bin

SHELL=/bin/sh
JOBS ?= $(shell nproc)


## Compiler & linker options
CCSTD       = c99  # c89 | c90, c99, c11, c17, gnu11, gnu17,...
CCOPT       = 3    # 0:debug; 1:optimize; 2:optimize more; 3:even more
CCOPTS      = -pedantic -pedantic-errors
CCEXTRA     = -fdiagnostics-color=always -fdiagnostics-show-location=once

CCWARN_POSIX = -D _POSIX_C_SOURCE=200112L  #-D __STRICT_ANSI__

CCWARN_TINY = $(CCWARN_POSIX) -Wpedantic -Wall -Wextra -Wshadow -Wundef -Werror

CCWARN_MORE = -Wwrite-strings -Wconversion -Wdouble-promotion

CCWARN_MOST = -Wformat -Wuninitialized -Wfloat-equal \
				-Wcast-align -Wpointer-arith -Wstrict-overflow=5 \
				-Wunreachable-code -Wmissing-format-attribute \
				-Wdeprecated \
				-Wno-padded -Wno-unused-parameter -Wno-format-nonliteral
CCWARN_GCC  = -Wlogical-op -Wstrict-aliasing=3 -Wduplicated-branches \
				-Wformat-overflow -Wformat-signedness -Wstrict-aliasing=3 \
				-Wno-suggest-attribute=format

CCWARN_CLANG = -Wbad-function-cast -Wextra-semi-stmt -Wmissing-prototypes \
				-Wswitch-enum -Wcovered-switch-default -Wreserved-identifier \
				-Wdeclaration-after-statement -Wsometimes-uninitialized \
				-Wno-fortify-source -Wno-cast-align -Wno-cast-qual \
				-Wdocumentation

CCWARN      = $(CCWARN_TINY) $(CCWARN_MORE) $(CCWARN_MOST)

CCDEPS      = -MMD -MP

CCFLAGS     = $(CCOPTS) $(CCWARN) -std=$(CCSTD) $(CCEXTRA) -I $(I_DIR) ${CCDEPS}

XCB_LFLAGS  = $(shell pkgconf --libs xcb xcb-keysyms xcb-util xcb-icccm xcb-ewmh)
JSON_LFLAGS = -lcjson
OTHR_LFLAGS = -lpthread
LDFLAGS     = -L $(L_DIR) $(XCB_LFLAGS) $(JSON_LFLAGS) $(OTHR_LFLAGS)


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


## Options on `make`
# Compiler: `make clean && make CC=clang` or `make clean && make CC=gcc`
CC = clang
ifeq ($(CC), clang)
	CCWARN += $(CCWARN_CLANG)
else ifeq ($(CC), gcc)
	CCWARN += $(CCWARN_GCC)
else
	$(error Unsupported compiler '$(CC)': CC only admits 'gcc' or 'clang')
endif

# Use `make clean && make DEBUG=1` to add debugging information
# Use `make clean && make DEBUG=2` to also link with the address sanitizer
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CCFLAGS += -DDEBUG -g3 -ggdb3 -O0
else ifeq ($(DEBUG), 2)
	CCFLAGS += -DDEBUG -g3 -ggdb3 -O0
	LDFLAGS += -fsanitize=address -fno-omit-frame-pointer -fPIC
	ifeq ($(CC), gcc)
	    CCGFLAGS += -fanalyzer
	endif
else
	CCFLAGS += -DNDEBUG -O$(CCOPT)
endif

# Use `make clean && make STRIP=1` to discard symbols from object files
STRIP ?= 0
ifeq ($(STRIP), 1)
	LDFLAGS += -s
endif


## Makefile files & directories
.SUFFIXES:
.SUFFIXES: .h .c .o

# Binary file options and running arguments
TARGET = $(B_DIR)/$(PROJECT_NAME_PROG)
DOXIGEN_FILE = Doxyfile
ARGS ?=

# Sources and objects
SRCS = $(wildcard $(S_DIR)/*.c) \
		$(wildcard $(S_DIR)/*/*.c) \
		$(wildcard $(S_DIR)/*/*/*.c)
OBJS = $(patsubst $(S_DIR)/%.c, $(O_DIR)/%.o, $(SRCS))
DEPS = $(OBJS:.o=.d)

-include $(DEPS)


## Options
# Make all, create needed directories and build
all: mkdirs $(TARGET) ctags
	@echo "Build $(BUILD_NUMBER) complete"

parallel:
	$(MAKE) -j$(JOBS) all

mkdirs:
	@mkdir -p $(B_DIR) $(O_DIR)
	@for dir in $$(find $(S_DIR) -mindepth 1 -maxdepth 1 -type d | \
    	sed 's|$(S_DIR)/||'); do \
			mkdir -p "$(O_DIR)/$$dir"; \
		done

# Linkage
$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
	@echo "Increasing build number to $(BUILD_NUMBER)..."
	@echo $(BUILD_NUMBER) >$(BUILD_NUMBER_FILE)

# Compilation
$(O_DIR)/%.o: $(S_DIR)/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

# Other options
ctags:
ifeq (,$(wildcard "/usr/bin/ctags"))
	@echo "Generating tags..."
	@ctags -R --exclude='doc' --exclude='obj' --exclude='tmp' .
else
	$(error Cannot find '/usr/bin/ctags')
endif

ccflags:
	@echo $(CCFLAGS)

ldflags:
	@echo $(LDFLAGS)

clean-obj:
	@rm -f $(OBJS) $(DEPS)
	@rm -rf $(O_DIR)/* $(O_DIR)

clean-bin:
	@rm -f $(TARGET)
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
	@echo
	@echo "Options:"
	@echo "  Use 'CC=<compiler>' to select a compiler ('gcc' or 'clang')"
	@echo "  Use 'JOBS=<n>' to compile with 'n' parallel jobs using 'parallel'"
	@echo "  Use 'DEBUG=1' to generate detailed debug information"
	@echo "  Use 'DEBUG=2' to also link with address sanitizer"
	@echo
	@echo "Binary will be placed in '$(TARGET)'"


## Phony targets
.PHONY: all mkdirs ctags clean clean-obj clean-bin clean-build run \
		hard hard-run doxygen ccflags ldflags parallel help
