# Makefile
# vim: set ft=make tw=72 nowrap:

## Directories
PWD   = $(CURDIR)
I_DIR = ${PWD}/include
S_DIR = ${PWD}/src
L_DIR = ${PWD}/lib
O_DIR = ${PWD}/obj
B_DIR = ${PWD}/bin

SHELL=/bin/bash

## Compiler & linker opts.
CC         = cc # gcc, clang
CCSTD      = c11 # c89 | c90, c99, c11, c17, c20, c2x
CCOPT      = 3
CCOPTS     = -pedantic -pedantic-errors
CCEXTRA    = -fdiagnostics-color=always -fdiagnostics-show-location=once 
CCWARN     = -Wpedantic -Wall -Wshadow -Wextra -Wwrite-strings -Wconversion -Werror
CCFLAGS    = ${CCOPTS} ${CCWARN} -std=${CCSTD} ${CCEXTRA} -I ${I_DIR}
LDFLAGS    = -lX11 -lXpm -L ${L_DIR}

# Use `make DEBUG=1` to add debugging information, symbol table, etc.
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CCFLAGS += -DDEBUG -g -ggdb -O0
else ifeq ($(DEBUG), 2)
	CCFLAGS += -DDEBUG -g -ggdb -O0
	LDFLAGS += -fsanitize=address -fno-omit-frame-pointer -fPIC
else
	CCFLAGS += -DNDEBUG -O${CCOPT}
endif


## Makefile opts.
SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .h .c .o


## Files options
TARGET = ${B_DIR}/main
OBJS = $(patsubst ${S_DIR}/%.c, ${O_DIR}/%.o, $(wildcard ${S_DIR}/*.c))
RUN_ARGS =

## Linkage
${TARGET}: ${OBJS}
	${CC} -o $@ $^ ${LDFLAGS}


## Compilation
${O_DIR}/%.o: ${S_DIR}/%.c
	${CC} -o $@ -c $< ${CCFLAGS}


## Make options
.PHONY: clean clean-obj clean-all hard run hard-run help

all:
	make ${TARGET}

clean-obj:
	rm --force ${OBJS}

clean-bin:
	rm --force ${TARGET}

clean:
	@make clean-obj
	@make clean-bin

clean-all:
	@make clean

run:
	${TARGET} ${RUN_ARGS}

hard:
	@make clean
	@make all

hard-run:
	@make clean
	@make all
	@make run

help:
	@echo "Type:"
	@echo "  'make all'......................... Build project"
	@echo "  'make run'................ Run binary (if exists)"
	@echo "  'make clean-obj'.............. Clean object files"
	@echo "  'make clean'....... Clean binary and object files"
	@echo "  'make hard'...................... Clean and build"
	@echo ""
	@echo " Binary will be placed in '${TARGET}'"
