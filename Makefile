# vim: set filetype=make nowrap noexpandtab tabstop=4:
# Makefile
#
# PROJECT: 'ICOWM' (`icowm`)
# AUTHOR: J. A. Corbal (<jacorbal@gmail.com>)

## Directories
PWD   = $(CURDIR)
I_DIR = ${PWD}/include
S_DIR = ${PWD}/src
L_DIR = ${PWD}/lib
O_DIR = ${PWD}/obj
B_DIR = ${PWD}/bin

SHELL=/bin/bash

## Compiler & linker options
CC          = gcc  # gcc, clang
CCSTD       = c11  # c89 | c90, c99, c11, c17, gnu11, gnu17
CCOPT       = 2	   # 0:debug; 1:optimize; 2:optimize more; 3:even more
CCOPTS      = -pedantic -pedantic-errors
CCEXTRA     = -fdiagnostics-color=always -fdiagnostics-show-location=once
CCWARN_TINY = -Wpedantic -Wall -Wextra -Wshadow -Wundef #-Werror
CCWARN_MORE = -Wwrite-strings -Wconversion -Wdouble-promotion
CCWARN_MOST = -Wformat -Wuninitialized -Wfloat-equal \
			  -Wcast-align -Wpointer-arith -Wstrict-overflow=5 \
			  -Wunreachable-code -Wmissing-format-attribute
CCWARN_GCC = -Wlogical-op -Wstrict-aliasing=3 -Wduplicated-branches \
			 -Wformat-overflow -Wformat-signedness -Wstrict-aliasing=3 \
			 -Wno-suggest-attribute=format -Wno-unused-parameter
CCWARN		= ${CCWARN_TINY} ${CCWARN_MORE} ${CCWARN_MOST} ${CCWARN_GCC}
#SQL_LFLAGS  = -lsqlite3
CCFLAGS     = ${CCOPTS} ${CCWARN} -std=${CCSTD} ${CCEXTRA} -I ${I_DIR}
LDFLAGS     = -L ${L_DIR} -lcjson -lX11 -lXpm

# Use `make DEBUG=1` to add debugging information, symbol table, etc.
# Use `make DEBUG=2` to link with the address sanitizer 
DEBUG ?= 0
ifeq ($(DEBUG), 1)
	CCFLAGS += -DDEBUG -g3 -ggdb3 -O0
else ifeq ($(DEBUG), 2)
	CCFLAGS += -DDEBUG -g3 -ggdb3 -O0
	LDFLAGS += -fsanitize=address -fno-omit-frame-pointer -fPIC
else
	CCFLAGS += -DNDEBUG -O${CCOPT}
endif


## Makefile options
SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .h .c .o


## Files options
TARGET = ${B_DIR}/main
SRCS = $(wildcard ${S_DIR}/*.c) $(wildcard ${S_DIR}/*/*.c)
OBJS = $(patsubst ${S_DIR}/%.c, ${O_DIR}/%.o, $(SRCS))
RUN_ARGS =


## Linkage
${TARGET}: ${OBJS}
	${CC} -o $@ $^ ${LDFLAGS} 


## Compilation
${O_DIR}/%.o: ${S_DIR}/%.c
	${CC} -o $@ -c $< ${CCFLAGS}


## Make options
.PHONY: ctags clean clean-obj clean-all run hard hard-run doxygen

all:
	make ${TARGET}
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
	@echo "  'make doxygen'..................... Create Doxygen documentation"
	@echo "  'make hard'..................................... Clean and build"
	@echo "  'make run'............................... Run binary (if exists)"
	@echo "  'make hard-run'......... Clean, build and run binary (if exists)"
	@echo ""
	@echo "  Binary will be placed in '${TARGET}'"
