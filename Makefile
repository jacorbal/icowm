# Makefile
#
# Project: IcoWM (`icowm`) -- Iconifying Window Manager
# Author: J. A. Corbal (<jacorbal@gmail.com>)

## Project metadata
__PROJECT_NAME_PROG = "icowm"
__PROJECT_NAME_SHORT = "IcoWM"
__PROJECT_NAME_LONG = "Iconifying Window Manager"
__PROJECT_VERSION = "0.1.0-alpha"
__PROJECT_VERSION_CODENAME = "'ovelya"
__LICENSE = "ISC License"
__COPYRIGHT = "Copyright (c) 2025"
__AUTHOR = "J. A. Corbal"
__RELEASE_DATE = "20250621 (intended)"

## Directories
PWD   = $(CURDIR)
I_DIR = ${PWD}/include
S_DIR = ${PWD}/src
L_DIR = ${PWD}/lib
O_DIR = ${PWD}/obj
B_DIR = ${PWD}/bin

SHELL=/bin/sh

## Compiler & linker options
CCSTD       = c99 # c89 | c90, c99, c11, c17, gnu11, gnu17
CCOPT       = 2   # 0:debug; 1:optimize; 2:optimize more; 3:even more
CCOPTS      = -pedantic -pedantic-errors
CCEXTRA     = -fdiagnostics-color=always -fdiagnostics-show-location=once

CCWARN_POSIX = -D_POSIX_C_SOURCE=200112L #-D__STRICT_ANSI__

CCWARN_TINY = ${CCWARN_POSIX} -Wpedantic -Wall -Wextra -Wshadow -Wundef #-Werror

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
               -Wdocumentation -Weverything \
               -Wno-fortify-source -Wno-cast-align -Wno-cast-qual

CCWARN      = ${CCWARN_TINY} ${CCWARN_MORE} ${CCWARN_MOST}
CCFLAGS     = ${CCOPTS} ${CCWARN} -std=${CCSTD} ${CCEXTRA} -I ${I_DIR}

JSON_LFLAGS = -lcjson
XCB_LFLAGS  = $(shell pkgconf --libs xcb xcb-keysyms xcb-util)
OTHR_LFLAGS = -lpthread
LDFLAGS     = -L ${L_DIR} ${JSON_LFLAGS} ${XCB_LFLAGS} ${OTHR_LFLAGS}


## Data & build information
BUILD_NUMBER_FILE = Build
ifneq (,$(wildcard $(BUILD_NUMBER_FILE)))
    LAST_BUILD_NUMBER := $(shell cat $(BUILD_NUMBER_FILE))
    __BUILD_NUMBER := $(shell echo $$(($(LAST_BUILD_NUMBER) + 1)))
else
    __BUILD_NUMBER := 1
endif
CCFLAGS += -D__BUILD_NUMBER=$(__BUILD_NUMBER)
CCFLAGS += -D__BUILD_TIMESTAMP=\"$(shell date -u +'%Y%m%dT%H%M')\"
CCFLAGS += -D__PROJECT_NAME_LONG=\"$(__PROJECT_NAME_LONG)\"
CCFLAGS += -D__PROJECT_NAME_SHORT=\"$(__PROJECT_NAME_SHORT)\"
CCFLAGS += -D__PROJECT_NAME_PROG=\"$(__PROJECT_NAME_PROG)\"
CCFLAGS += -D__PROJECT_VERSION=\"$(__PROJECT_VERSION)\"
CCFLAGS += -D__PROJECT_VERSION_CODENAME=\"$(__PROJECT_VERSION_CODENAME)\"
CCFLAGS += -D__AUTHOR=\"$(__AUTHOR)\"
CCFLAGS += -D__COPYRIGHT=\"$(__COPYRIGHT)\"
CCFLAGS += -D__LICENSE=\"$(__LICENSE)\"
CCFLAGS += -D__RELEASE_DATE=\"$(__RELEASE_DATE)\"

## Options on `make`
# Compiler: `make clean && make CC=clang` or `make clean && make CC=gcc`
CC = clang
ifeq ($(CC), clang)
    CCWARN += ${CCWARN_CLANG}
else ifeq ($(CC), gcc)
    CCWARN += ${CCWARN_GCC}
else
    $(error Unsupported compiler '$(CC)'. CC only admits 'gcc' or 'clang')
endif

# Use `make clean && make DEBUG=1` to add debugging information
# Use `make clean && make DEBUG=2` to also link with the address sanitizer 
DEBUG ?= 0
ifeq ($(DEBUG), 1)
    CCFLAGS += -DDEBUG -g3 -ggdb3 -O0
else ifeq ($(DEBUG), 2)
    CCFLAGS += -DDEBUG -g3 -ggdb3 -O0
    LDFLAGS += -fsanitize=address -fno-omit-frame-pointer -fPIC
else
    CCFLAGS += -DNDEBUG -O${CCOPT}
endif

# Use `make clean && make STRIP=1` to discard symbols from object files
STRIP ?= 0
ifeq ($(STRIP), 1)
    LDFLAGS += -s
endif


## Makefile files & directories
SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .h .c .o

# File options
TARGET = ${B_DIR}/main
RUN_ARGS =

# Sources and objects
SRCS = $(wildcard ${S_DIR}/*.c) \
       $(wildcard ${S_DIR}/*/*.c) \
       $(wildcard ${S_DIR}/*/*/*.c)
OBJS = $(patsubst ${S_DIR}/%.c, ${O_DIR}/%.o, $(SRCS))

# Linkage
${TARGET}: ${OBJS}
	${CC} -o $@ $^ ${LDFLAGS}

# Compilation
${O_DIR}/%.o: ${S_DIR}/%.c
	${CC} -o $@ -c $< ${CCFLAGS}


## Make options
.PHONY: ctags clean clean-obj clean-all run hard hard-run doxygen \
        $(BUILD_NUMBER_FILE)

all:
	make ${TARGET} ${BUILD_NUMBER_FILE}
	@make ctags

ctags:
ifeq (,$(wildcard "/usr/bin/ctags"))
	@echo "Generating tags..."
	@ctags -R --exclude='doc' --exclude='obj' .
endif

clean-obj:
	rm --force ${OBJS}

clean-bin:
	rm --force ${TARGET}

clean-build:
	rm --force ${BUILD_NUMBER_FILE}

clean:
	@make clean-obj
	@make clean-bin

run:
	${TARGET} ${RUN_ARGS}

hard:
	@make clean
	@make all

hard-run:
	@make hard
	@make run

doxygen:
	@[ -f 'Doxyfile' ] && doxygen || echo "'Doxyfile' not found" >&2

help:
	@echo "Type:"
	@echo "  'make all'........................................ Build project"
	@echo "  'make clean-obj'............................. Clean object files"
	@echo "  'make clean'...................... Clean binary and object files"
	@echo "  'make ctags'...................... Generate tag files for source"
	@echo "  'make doxygen'..................... Create Doxygen documentation"
	@echo "  'make hard'..................................... Clean and build"
	@echo "  'make run'............................... Run binary (if exists)"
	@echo "  'make hard-run'......... Clean, build and run binary (if exists)"
	@echo ""
	@echo "  Binary will be placed in '${TARGET}'"

$(BUILD_NUMBER_FILE):
	@echo "Increasing build number to $(__BUILD_NUMBER)..."
	@echo $(__BUILD_NUMBER) > $(BUILD_NUMBER_FILE)
