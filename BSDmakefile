# BSDmakefile (for BSD Make / 'bmake')
#
# Project: IcoWM ('icowm'), Iconifying Window Manager
# Author: J. A. Corbal (<jacorbal@gmail.com>)

# Copyright (c) 2026, J. A. Corbal
# All rights reserved.
#
# This file is licensed under the 'ISC License'.
# Read the 'LICENSE' file in the root of this repository for details.

# This is a 'bmake' (FreeBSD/NetBSD/OpenBSD 'make') port of
# 'GNUmakefile'.  GNU Make has no reason to ever read this file (it
# always prefers 'GNUmakefile' when both exist), and this file has no
# '%' pattern rules, '$(shell ...)', 'ifeq'/'ifneq', or any other
# GNU-only construct: every one of those is replaced below with its
# bmake equivalent ('.for' loops instead of pattern rules, '!=' instead
# of '$(shell ...)', '.if'/'.elif'/'.endif' instead of 'ifeq'/'ifneq',
# and so on), verified against the FreeBSD/NetBSD/OpenBSD make(1)
# manuals rather than guessed from GNU Make's syntax.
#
# NOTE: 'tests/Makefile.mk', included by the 'Tests' section below, is
# written for GNU Make (it shares this file's GNU-Make-specific
# variables directly, per its comment in 'GNUmakefile').  It is NOT
# converted here: testing under 'bmake' is not this port's goal, only
# building the project itself is, so that section includes it only if
# present and skips it silently otherwise, rather than erroring out.
# Every other target in this file ('all', 'parallel', 'ctags', the
# 'clean-*' targets, and so on) does not depend on it at all, and is
# unaffected either way.

## Project metadata
PROJECT_NAME_PROG = icowm
PROJECT_NAME_SHORT = "IcoWM"
PROJECT_NAME_LONG = "Iconifying Window Manager"
PROJECT_VERSION = "1.0.1-rc.1"
PROJECT_VERSION_CODENAME = "'ovelya"
LICENSE = "ISC License"
COPYRIGHT = "Copyright (c) 2026"
AUTHOR = "J. A. Corbal"
RELEASE_DATE = "20260923"


## Directories
# '.CURDIR' is bmake's built-in for "the directory this makefile lives
# in / was invoked from", the same role '$(CURDIR)' plays in GNU Make;
# kept as its 'PWD' variable, rather than referencing '.CURDIR'
# everywhere directly, purely to keep every directory variable below an
# exact, line-by-line match for 'GNUmakefile''s own.
PWD = ${.CURDIR}
# bmake, unlike GNU Make, searches for a directory literally named 'obj'
# (among a few other candidates) in the launch directory and, if one
# exists, 'chdir's into it before doing anything else at all, including
# parsing the rest of this very file; confirmed as a built-in part of
# bmake itself, not something requiring any system makefile
# ('sys.mk'/'bsd.obj.mk') to be included first.
#
# Since 'O_DIR' below is that exact directory name, and every relative,
# non-'.CURDIR'-based bare filename further down ('BUILD_NUMBER_FILE',
# 'DOXIGEN_FILE') would then resolve inside it instead of the project
# root the moment 'mkdirs' has ever created it once, this pins bmake's
# notion of '.OBJDIR' to be '.CURDIR' outright, disabling that search
# entirely; every directory this file manages itself ('O_DIR' and the
# rest) is already tracked through its absolute, '.CURDIR'-derived
# variables regardless, so bmake's separate src/obj-splitting mechanism
# was never being relied on to begin with.
.OBJDIR: ${.CURDIR}
I_DIR = ${PWD}/include
S_DIR = ${PWD}/src
T_DIR = ${PWD}/tools
L_DIR = ${PWD}/lib
O_DIR = ${PWD}/obj
B_DIR = ${PWD}/bin
TESTS_DIR = ${PWD}/tests

# A plain 'SHELL' variable has no special meaning to bmake at all
# (unlike GNU Make, which recognizes it by name); '.SHELL: path=...'
# is bmake's mechanism for the same guarantee 'GNUmakefile''s
# 'SHELL=/bin/sh' line makes: every recipe below runs under a known,
# POSIX shell, regardless of whatever shell the invoking environment
# happens to default to.
.SHELL: path=/bin/sh

# '!=' hands its right-hand side to the shell immediately and captures
# stdout, the same role '$(shell ...)' plays in GNU Make, but it is its
# assignment operator, not a function that can be nested inside another
# assignment; a plain '?=' cannot be combined with it directly (there is
# no single operator for "run a shell command, but only if not already
# overridden"), so each one first captures into its '_..._DETECTED'
# helper, then '?=' picks that helper only if 'JOBS'/'PKGCONF' was not
# already set on the command line or in the environment, exactly
# preserving 'GNUmakefile''s override rules.
#
# 'nproc' is GNU-coreutils-only and does not exist on a stock BSD system
# at all; 'sysctl -n hw.ncpu' is the actual BSD-native way to ask for
# the same figure, tried first here since this file's whole reason to
# exist is running correctly on a BSD system, with 'nproc' only as
# a fallback for a bmake build running on Linux, and '1' as the final,
# always-safe fallback if neither tool is present.
_JOBS_DETECTED != sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 1
JOBS ?= ${_JOBS_DETECTED}


## Installation directories
# Every one of these is overridable, so a distribution may move a single
# directory without having to restate the rest, and 'DESTDIR' stages the
# whole tree somewhere else for a package build.
OS != uname -s

PREFIX ?= /usr/local
DESTDIR ?=
BINDIR ?= ${PREFIX}/bin
DATADIR ?= ${PREFIX}/share
LOCALEDIR ?= ${DATADIR}/locale
XSESSIONSDIR ?= ${DATADIR}/xsessions
DOCDIR ?= ${DATADIR}/doc/${PROJECT_NAME_PROG}
EXAMPLEDIR ?= ${DATADIR}/${PROJECT_NAME_PROG}
# Icon themes are searched by this exact layout, so the two files go
# where a theme expects to find them rather than under a directory of
# this project's choosing: the scalable one is what a session list
# shows, and the symbolic one is for the panels and trays that recolor
# what they draw.
ICONDIR ?= ${DATADIR}/icons/hicolor
# Named once rather than written out at each of the four places that
# reach for it, the path being too long to fit a line whole and neither
# make joining a split one the same way
ICON_SCALABLE = ${ICONDIR}/scalable/apps
ICON_SYMBOLIC = ${ICONDIR}/symbolic/apps

# Manual pages sit directly under the prefix on every BSD, and under
# 'share' on Linux and anything else, which is where the FHS puts them.
# Guessed from 'uname', and overridable like the rest for the systems
# that follow neither.
.if !empty(OS:M*BSD) || ${OS} == "DragonFly"
MANDIR ?= ${PREFIX}/man
.else
MANDIR ?= ${DATADIR}/man
.endif

# Neither '-D' nor an owner is asked for anywhere below: the first is
# a GNU extension BSD's 'install' does not have, and the second names
# a group that is 'root' on Linux and 'wheel' on the BSDs.
# The directories are made separately, and ownership is left to whoever
# runs this.
INSTALL ?= install
INSTALL_PROGRAM ?= ${INSTALL} -m 0755
INSTALL_DATA ?= ${INSTALL} -m 0644
INSTALL_DIR ?= ${INSTALL} -d -m 0755
_PKGCONF_DETECTED != command -v pkgconf 2>/dev/null || \
        command -v pkg-config 2>/dev/null || echo pkgconf
PKGCONF ?= ${_PKGCONF_DETECTED}


## Compiler & linker options
# c89 | c90, c99, c11, c17, gnu11, gnu17,...
CCSTD = c99
# 0:debug; 1:optimize; 2:optimize more; 3:even more
CCOPT = 3
CCOPTS = -pedantic -pedantic-errors
CCEXTRA = -fdiagnostics-color=always -fdiagnostics-show-location=once

# -D __STRICT_ANSI__
CCWARN_POSIX = -D _POSIX_C_SOURCE=200112L

CCWARN_TINY = ${CCWARN_POSIX} -Wpedantic -Wall -Wextra -Wshadow -Wundef \
              -Werror

CCWARN_MORE = -Wwrite-strings -Wconversion -Wdouble-promotion

CCWARN_MOST = -Wformat -Wuninitialized -Wfloat-equal \
              -Wcast-align -Wpointer-arith -Wstrict-overflow=2 \
              -Wunreachable-code -Wmissing-format-attribute \
              -Wdeprecated -fwrapv

CCWARN_GCC = -Wlogical-op -Wstrict-aliasing=3 -Wduplicated-branches \
             -Wformat-overflow -Wformat-signedness -Wstrict-aliasing=3 \
             -Wno-suggest-attribute=format

CCWARN_CLANG = -Wbad-function-cast -Wextra-semi-stmt -Wmissing-prototypes \
               -Wswitch-enum -Wcovered-switch-default -Wreserved-identifier \
               -Wdeclaration-after-statement -Wsometimes-uninitialized \
               -Wno-fortify-source -Wno-cast-align -Wno-cast-qual \
               -Wdocumentation

CCWARN = ${CCWARN_TINY} ${CCWARN_MORE} ${CCWARN_MOST}

CCDEPS = -MMD -MP

XCB_CFLAGS != ${PKGCONF} --cflags \
        xcb xcb-keysyms xcb-util xcb-icccm xcb-ewmh xcb-randr xcb-sync \
        xcb-cursor xcb-render xcb-renderutil 2>/dev/null
FONT_CFLAGS != ${PKGCONF} --cflags freetype2 fontconfig 2>/dev/null | \
        sed 's/-I/-isystem /g'
JSON_CFLAGS != ${PKGCONF} --cflags libcjson 2>/dev/null || \
        ${PKGCONF} --cflags cjson 2>/dev/null
CCFLAGS_BASE = ${CCOPTS} ${CCWARN} -std=${CCSTD} ${CCEXTRA} -I ${I_DIR} \
               ${CCDEPS}
CCFLAGS = ${CCFLAGS_BASE} ${XCB_CFLAGS} ${FONT_CFLAGS} ${JSON_CFLAGS}

# The same flags without the dependency-file ones, for the 'headers'
# pass: those would write a '.d' beside the makefile for every header
# checked, and outlive the throwaway source describing it
HDRFLAGS = ${CCOPTS} ${CCWARN} -std=${CCSTD} ${CCEXTRA} -I ${I_DIR} \
           ${XCB_CFLAGS} ${FONT_CFLAGS} ${JSON_CFLAGS}

# 'icowm-msg' (see 'tools/icowm-msg.c') is a small, deliberately
# self-contained IPC client: it never touches X11 at all, so it has no
# reason to pull in the XCB or font libraries the window manager itself
# needs, only JSON for the wire protocol it speaks.
MSG_CCFLAGS = ${CCFLAGS_BASE} ${JSON_CFLAGS}

XCB_LFLAGS != ${PKGCONF} --libs \
        xcb xcb-keysyms xcb-util xcb-icccm xcb-ewmh xcb-randr xcb-sync \
        xcb-cursor xcb-render xcb-renderutil 2>/dev/null || \
        printf '%s ' '-lxcb' '-lxcb-keysyms' '-lxcb-util' '-lxcb-icccm' \
        '-lxcb-ewmh' '-lxcb-randr' '-lxcb-sync' \
        '-lxcb-cursor' '-lxcb-render' '-lxcb-render-util'
FONT_LFLAGS != ${PKGCONF} --libs freetype2 fontconfig 2>/dev/null || \
        printf '%s' '-lfreetype -lfontconfig'
JSON_LFLAGS != ${PKGCONF} --libs libcjson 2>/dev/null || \
        ${PKGCONF} --libs cjson 2>/dev/null || printf '%s' '-lcjson'
# 'gettext' sits inside the C library on Linux, glibc and musl alike,
# and in a library of its own on every BSD, where it comes from
# 'gettext-runtime' or pkgsrc's 'gettext-lib'.  Probed rather than
# assumed, since either make may run on either system: pkg-config
# first, for those shipping a '.pc' file for it, then a link test for
# those that do not, and nothing at all where the C library answers
# already.  Without this the link fails outright on a BSD, every call
# to 'gettext' going unresolved.
INTL_LFLAGS != ${PKGCONF} --libs intl 2>/dev/null || \
        { printf 'int main(void){return 0;}' | \
          ${CC} -x c - -o /dev/null -lintl 2>/dev/null && \
          printf '%s' '-lintl'; } || printf '%s' ''
OTHR_LFLAGS = -lpthread ${INTL_LFLAGS}
LDFLAGS = -L ${L_DIR} ${XCB_LFLAGS} ${FONT_LFLAGS} ${JSON_LFLAGS} \
          ${OTHR_LFLAGS}
MSG_LDFLAGS = -L ${L_DIR} ${JSON_LFLAGS}


## Data & build information
# 'SOURCE_DATE_EPOCH', when the environment sets it, is the agreed way
# for a distribution to ask for a reproducible build: the same sources
# have to give the same binary, whenever they are compiled.  Two things
# here stand in the way of that, and both step aside when it is set.
#
# The timestamp below is taken from that epoch instead of from the
# clock, spelled for BSD 'date' first and for GNU 'date' second, since
# the two disagree about how an epoch is given.
#
# The build number stops counting and the file stops being written.
# It is a counter of this author's builds, which is useful here and
# meaningless in a package, where it would only record how many times
# somebody else's machine had compiled the sources and leave a tracked
# file dirty for having done so.
SOURCE_DATE_EPOCH ?=

BUILD_NUMBER_FILE = Build
.if exists(${BUILD_NUMBER_FILE})
LAST_BUILD_NUMBER != cat ${BUILD_NUMBER_FILE}
.else
LAST_BUILD_NUMBER = 0
.endif
.if empty(SOURCE_DATE_EPOCH)
BUILD_NUMBER != echo $$((${LAST_BUILD_NUMBER} + 1))
_BUILD_TIMESTAMP != date -u +'%Y%m%dT%H%M'
.else
BUILD_NUMBER = ${LAST_BUILD_NUMBER}
_BUILD_TIMESTAMP != date -u -r ${SOURCE_DATE_EPOCH} +'%Y%m%dT%H%M' \
    2>/dev/null || date -u -d @${SOURCE_DATE_EPOCH} +'%Y%m%dT%H%M' \
    2>/dev/null || echo 19700101T0000
.endif

CCFLAGS += -D BUILD_NUMBER=${BUILD_NUMBER}
CCFLAGS += -D BUILD_TIMESTAMP=\"${_BUILD_TIMESTAMP}\"
CCFLAGS += -D PROJECT_NAME_LONG=\"${PROJECT_NAME_LONG}\"
CCFLAGS += -D PROJECT_NAME_SHORT=\"${PROJECT_NAME_SHORT}\"
CCFLAGS += -D PROJECT_NAME_PROG=\"${PROJECT_NAME_PROG}\"
CCFLAGS += -D PROJECT_VERSION=\"${PROJECT_VERSION}\"
CCFLAGS += -D PROJECT_VERSION_CODENAME=\"${PROJECT_VERSION_CODENAME}\"
CCFLAGS += -D AUTHOR=\"${AUTHOR}\"
CCFLAGS += -D COPYRIGHT=\"${COPYRIGHT}\"
CCFLAGS += -D LICENSE=\"${LICENSE}\"
CCFLAGS += -D RELEASE_DATE=\"${RELEASE_DATE}\"
CCFLAGS += -D I18N_DOMAIN=\"icowm\"
CCFLAGS += -D I18N_LOCALE_DIR=\"${LOCALEDIR}\"

# 'icowm-msg' only ever prints its name, IcoWM's short name, its
# version, its license, its copyright line, and its author (see
# 'tools/icowm-msg.c'); the rest of the metadata above is icowm's '-v'
# output, not something a small IPC client has any reason to report
# about itself.
MSG_CCFLAGS += -D PROJECT_NAME_SHORT=\"${PROJECT_NAME_SHORT}\"
MSG_CCFLAGS += -D PROJECT_NAME_PROG=\"${PROJECT_NAME_PROG}\"
MSG_CCFLAGS += -D PROJECT_VERSION=\"${PROJECT_VERSION}\"
MSG_CCFLAGS += -D PROJECT_VERSION_CODENAME=\"${PROJECT_VERSION_CODENAME}\"
MSG_CCFLAGS += -D AUTHOR=\"${AUTHOR}\"
MSG_CCFLAGS += -D COPYRIGHT=\"${COPYRIGHT}\"
MSG_CCFLAGS += -D LICENSE=\"${LICENSE}\"


## Options on 'make'
# Compiler: 'make clean && make CC=clang' or 'make clean && make CC=gcc'
CC = clang
.if ${CC} == "clang"
CCWARN += ${CCWARN_CLANG}
.elif ${CC} == "gcc"
CCWARN += ${CCWARN_GCC}
.else
.error Unsupported compiler '${CC}': CC only admits 'gcc' or 'clang'
.endif

# 'clang' has no '=auto' value for '-flto' (only 'thin'/'full', or
# nothing at all); only 'gcc' knows to parallelize its LTRANS pass
# across every core this way.
.if ${CC} == "gcc"
LTO_FLAG = -flto=auto
.else
LTO_FLAG = -flto
.endif

# Use 'make clean && make DEBUG=1' to add debugging information
# Use 'make clean && make DEBUG=2' to compile & link with address sanitizer
DEBUG ?= 0
.if ${DEBUG} == "1"
CCFLAGS += -DDEBUG -g3 -ggdb3 -O0
.elif ${DEBUG} == "2"
# No '-fanalyzer' alongside the sanitizer.  The two answer different
# questions, one before the program runs and one while it does, and
# gcc 13 loses track of its own instrumentation when asked for both:
# a loop whose condition calls another translation unit, holding
# a variable that unit fills through a pointer, is reported as a use of
# an uninitialized value that no initialization silences.  'make
# analyze' is where the analyzer belongs, and it still runs there.
CCFLAGS += -DDEBUG -g3 -ggdb3 -O0 \
           -fsanitize=address -fno-omit-frame-pointer -fPIE
LDFLAGS += -fsanitize=address -pie
.else
CCFLAGS += -DNDEBUG -O${CCOPT} ${LTO_FLAG} \
           -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE
LDFLAGS += ${LTO_FLAG} -Wl,-z,relro,-z,now -Wl,-z,noexecstack -pie
.endif

# Symbols are discarded by default; 'make STRIP=0' keeps them.  Any
# debug build keeps them whatever this says, a stripped binary being of
# no use to a debugger or a sanitizer.
STRIP ?= 1
.if ${DEBUG} == "0" && ${STRIP} != "0"
LDFLAGS += -s
MSG_LDFLAGS += -s
.endif

# Use 'make COMPACT=1' to shrink several compile-time array capacities
# throughout the codebase, for building specifically for a severely
# memory-constrained target.  Independent of restricted-memory mode
# ('icowm -M <mib>').  It does not turn that mode on by itself, and it
# does not supply a default for '-M <mib>' when that flag is left off at
# run time either.
COMPACT ?=
.if !empty(COMPACT)
CCFLAGS += -D COMPACT
.endif

# Use 'make analyze' to run a static-analysis pass over the whole
# project without touching the normal object files ('gcc')
ANALYZE ?= 0
.if ${ANALYZE} == "1"
.if ${CC} == "gcc"
CCFLAGS += -fanalyzer
.endif
.endif


## Makefile files & directories
.SUFFIXES:
.SUFFIXES: .h .c .o

# Binary file options and running arguments
TARGET = ${B_DIR}/${PROJECT_NAME_PROG}
MSG_TARGET = ${B_DIR}/${PROJECT_NAME_PROG}-msg
DOXIGEN_FILE = Doxyfile
ARGS ?=

# Sources, objects and auto-generated dependencies
SRCS != find ${S_DIR} -name '*.c' | sort
OBJS = ${SRCS:S,${S_DIR}/,${O_DIR}/,:.c=.o}
DEPS = ${OBJS:.o=.d}

# 'icowm-msg' (see 'tools/icowm-msg.c') builds and links entirely
# separately from icowm itself.  Its single object never joins 'OBJS',
# and its binary never joins 'TARGET', so a change to one never forces
# a rebuild of the other.
MSG_SRCS != find ${T_DIR} -maxdepth 1 -name '*.c' 2>/dev/null | sort
MSG_OBJS = ${MSG_SRCS:S,${T_DIR}/,${O_DIR}/tools/,:.c=.o}
MSG_DEPS = ${MSG_OBJS:.o=.d}


## Options
# bmake has no ".DEFAULT_GOAL" directive; '.MAIN:' is its own, equally
# explicit way to name the target built when none is given on the
# command line, rather than relying on 'all' merely happening to be the
# first target defined below.
.MAIN: all

# Make all, create needed directories and build
all: mkdirs ${TARGET} ${MSG_TARGET} ctags
	@echo "Build ${BUILD_NUMBER}"

parallel:
	${MAKE} -j${JOBS} all

mkdirs:
	@mkdir -p ${B_DIR} ${O_DIR} ${O_DIR}/tools
	@find ${S_DIR} -mindepth 1 -type d | \
		sed 's|${S_DIR}/||' | \
		while read dir; do \
			mkdir -p "${O_DIR}/$$dir"; \
		done

# 'mkdirs' has to finish before any object is compiled.  Naming it in
# 'all' alone was not enough: neither make orders the prerequisites of
# a target under '-j', so a parallel build from a clean tree could start
# compiling before the object directories existed, and fail on the
# dependency file it could not open.  bmake has no order-only
# prerequisite of GNU Make's kind, so the order is stated outright.
.ORDER: mkdirs ${TARGET}
.ORDER: mkdirs ${MSG_TARGET}


# Linkage
${TARGET}: ${OBJS}
	${CC} -o ${.TARGET} ${.ALLSRC} ${LDFLAGS}
.if empty(SOURCE_DATE_EPOCH)
	@echo "Increasing build number to ${BUILD_NUMBER}..."
	@echo ${BUILD_NUMBER} >${BUILD_NUMBER_FILE}
.else
	@echo "Reproducible build ${BUILD_NUMBER}; build file left alone"
.endif

${MSG_TARGET}: ${MSG_OBJS}
	${CC} -o ${.TARGET} ${.ALLSRC} ${MSG_LDFLAGS}

# Compilation
#
# One explicit rule per source file, generated by '.for' rather than by
# the two '%'-pattern rules 'GNUmakefile' uses for this same job;
# '${src}' is the loop variable itself, substituted textually at parse
# time, not a local/dynamic variable, so it names the exact, single
# source file each generated rule compiles.
.for src in ${SRCS}
${src:S,${S_DIR}/,${O_DIR}/,:.c=.o}: ${src}
	${CC} ${CCFLAGS} -c ${src} -o ${.TARGET}
.endfor

.for src in ${MSG_SRCS}
${src:S,${T_DIR}/,${O_DIR}/tools/,:.c=.o}: ${src}
	${CC} ${MSG_CCFLAGS} -c ${src} -o ${.TARGET}
.endfor


## Tests
#
# See 'tests/Makefile.mk' for every test-related rule and variable.
# UNLIKE THE REST OF THIS FILE, that one is NOT ported to 'bmake' here.
# It is written for GNU Make throughout (its comment in 'GNUmakefile'
# says it shares this file's variables directly), and testing under
# bmake is not this port's goal, only building the project itself is.
# Included only if present, silently skipped otherwise, so its absence
# (or its GNU-only syntax, if it is ever actually read by a stray 'make
# test') never blocks 'all' or any other real target below from
# building.
.if exists(${TESTS_DIR}/Makefile.mk)
.include "${TESTS_DIR}/Makefile.mk"
.endif


# Other options
ctags:
	@if command -v ctags >/dev/null 2>&1; then \
		echo "Generating tags..."; \
		ctags -R --exclude='doc' --exclude='obj' --exclude='tmp' .; \
	else \
		echo "Skipping tags: 'ctags' not found"; \
	fi

install:
	@test -x ${TARGET} || { \
		echo "install: ${TARGET} is not built; run 'make' first" >&2; \
		exit 1; }
	@test -x ${MSG_TARGET} || { \
		echo "install: ${MSG_TARGET} is not built; run 'make' first" >&2; \
		exit 1; }
	${INSTALL_DIR} ${DESTDIR}${BINDIR}
	${INSTALL_PROGRAM} ${TARGET} ${DESTDIR}${BINDIR}
	${INSTALL_PROGRAM} ${MSG_TARGET} ${DESTDIR}${BINDIR}
	${INSTALL_DIR} ${DESTDIR}${MANDIR}/man1 ${DESTDIR}${MANDIR}/man5
	${INSTALL_DATA} ${PWD}/doc/man/man1/*.1 ${DESTDIR}${MANDIR}/man1
	${INSTALL_DATA} ${PWD}/doc/man/man5/*.5 ${DESTDIR}${MANDIR}/man5
	${INSTALL_DIR} ${DESTDIR}${XSESSIONSDIR}
	${INSTALL_DATA} ${PWD}/doc/${PROJECT_NAME_PROG}.desktop \
		${DESTDIR}${XSESSIONSDIR}
	${INSTALL_DIR} ${DESTDIR}${DOCDIR}
	${INSTALL_DATA} ${PWD}/README.md ${PWD}/LICENSE ${PWD}/COMPLIANCE.md \
		${DESTDIR}${DOCDIR}
	${INSTALL_DATA} ${PWD}/doc/config.md ${PWD}/doc/themes.md \
		${PWD}/doc/icowm.md ${PWD}/doc/icowm-msg.md ${DESTDIR}${DOCDIR}
	${INSTALL_DIR} ${DESTDIR}${ICON_SCALABLE}
	${INSTALL_DATA} ${PWD}/doc/icon/${PROJECT_NAME_PROG}.svg \
		${DESTDIR}${ICON_SCALABLE}
	${INSTALL_DIR} ${DESTDIR}${ICON_SYMBOLIC}
	${INSTALL_DATA} ${PWD}/doc/icon/${PROJECT_NAME_PROG}-symbolic.svg \
		${DESTDIR}${ICON_SYMBOLIC}
	@cd ${PWD} && find locale -name '*.mo' | while read mo; do \
		lang=$$(echo "$$mo" | cut -d/ -f2); \
		${INSTALL_DIR} \
			"${DESTDIR}${LOCALEDIR}/$$lang/LC_MESSAGES"; \
		${INSTALL_DATA} "$$mo" \
			"${DESTDIR}${LOCALEDIR}/$$lang/LC_MESSAGES"; \
	done
	@cd ${PWD} && find doc/config.example -type d | \
		sed 's|doc/config.example||' | \
		while read dir; do \
			${INSTALL_DIR} "${DESTDIR}${EXAMPLEDIR}$$dir"; \
		done
	@cd ${PWD} && find doc/config.example -type f | \
		sed 's|doc/config.example/||' | \
		while read file; do \
			${INSTALL_DATA} "doc/config.example/$$file" \
				"${DESTDIR}${EXAMPLEDIR}/$$file"; \
		done
	@echo "Installed under ${DESTDIR}${PREFIX}"

uninstall:
	rm -f ${DESTDIR}${BINDIR}/${PROJECT_NAME_PROG}
	rm -f ${DESTDIR}${BINDIR}/${PROJECT_NAME_PROG}-msg
	@cd ${PWD} && for page in doc/man/man1/*.1; do \
		rm -f "${DESTDIR}${MANDIR}/man1/$$(basename $$page)"; \
	done
	@cd ${PWD} && for page in doc/man/man5/*.5; do \
		rm -f "${DESTDIR}${MANDIR}/man5/$$(basename $$page)"; \
	done
	rm -f ${DESTDIR}${XSESSIONSDIR}/${PROJECT_NAME_PROG}.desktop
	rm -f ${DESTDIR}${ICON_SCALABLE}/${PROJECT_NAME_PROG}.svg
	rm -f ${DESTDIR}${ICON_SYMBOLIC}/${PROJECT_NAME_PROG}-symbolic.svg
	@cd ${PWD} && find locale -name '*.mo' | while read mo; do \
		lang=$$(echo "$$mo" | cut -d/ -f2); \
		rm -f "${DESTDIR}${LOCALEDIR}/$$lang/LC_MESSAGES/$$(basename \
			$$mo)"; \
	done
	rm -rf ${DESTDIR}${EXAMPLEDIR}
	rm -rf ${DESTDIR}${DOCDIR}
	@echo "Removed from ${DESTDIR}${PREFIX}"

ccflags:
	@echo ${CCFLAGS}

ldflags:
	@echo ${LDFLAGS}

clean-obj:
	@rm -f ${OBJS} ${DEPS} ${MSG_OBJS} ${MSG_DEPS}
	@rm -rf ${O_DIR}/* ${O_DIR}

clean-bin:
	@rm -f ${TARGET} ${MSG_TARGET}
	@rm -rf ${B_DIR}

clean-build:
	@-rm -f ${BUILD_NUMBER_FILE}

clean: clean-obj clean-bin

run:
	${TARGET} ${ARGS}

hard: clean all

hard-run: hard run

doxygen:
	@[ -f '${DOXIGEN_FILE}' ] && doxygen || \
		echo "Error: '${DOXIGEN_FILE}' not found" >&2

# 'CCDEPS' is cleared for this pass: the dependency files it asks for
# would land beside the makefile, one per header, and outlive the
# throwaway source they describe.  Nothing is written to disk at all
# here, the source going in on the standard input instead.
headers:
	@fail=0; total=0; \
	for h in `cd ${I_DIR} && find . -name '*.h' | sed 's|^\./||'`; do \
		total=`expr $$total + 1`; \
		printf '#include <%s>\nint main(void){return 0;}\n' "$$h" \
			| ${CC} ${HDRFLAGS} -fsyntax-only -x c - \
			  2>/dev/null \
			|| { echo "not self-contained: $$h" >&2; \
			     fail=`expr $$fail + 1`; }; \
	done; \
	echo "`expr $$total - $$fail`/$$total headers compile on their own"; \
	[ $$fail -eq 0 ]

analyze:
.if ${CC} == "gcc"
	${MAKE} ANALYZE=1 all
.else
	@command -v scan-build >/dev/null 2>&1 || \
		{ echo "Error: 'scan-build' not found (part of the" \
		       "clang-tools/llvm package); required to analyze" \
		       "under clang" >&2; exit 1; }
	scan-build --use-cc=${CC} ${MAKE} all
.endif

help:
	@echo "Command:"
	@echo "  make all              Build project"
	@echo "  make parallel         Build with parallel jobs"
	@echo "  make clean-obj        Clean object files"
	@echo "  make clean            Clean binary and object files"
	@echo "  make ctags            Generate tag files for source"
	@echo "  make doxygen          Create Doxygen documentation"
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
	@echo "  Use 'PREFIX=<dir>' to install elsewhere (def. '/usr/local')"
	@echo "  Use 'DESTDIR=<dir>' to stage an install for packaging"
	@echo "  Use 'COMPACT=1' to build using smaller arrays"
	@echo
	@echo "Binary will be placed in '${TARGET}'"
	@echo "IPC client tool will be placed in '${MSG_TARGET}'"


## Auto-generated header dependencies
#
# GNU Make's '-include' silently skips a missing file; bmake's
# '.include' has no such silent form of its own that could be confirmed
# portable across every BSD make variant, so the same "skip whichever
# '.d' files do not exist yet" behavior (true on a clean build, before
# any object has ever been compiled) is spelled out explicitly here
# instead, using only 'exists()' and '.include', both already confirmed
# above.
.for dep in ${DEPS}
.if exists(${dep})
.include "${dep}"
.endif
.endfor
.for dep in ${MSG_DEPS}
.if exists(${dep})
.include "${dep}"
.endif
.endfor

## Phony targets
.PHONY: all mkdirs ctags clean clean-obj clean-bin clean-build \
    run hard hard-run test headers install uninstall \
    doxygen analyze ccflags ldflags parallel help
