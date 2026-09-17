Installing IcoWM
================

Requirements
------------

A C99 compiler, either GNU Make or BSD make, and the libraries listed
under "[Dependencies](README.md#dependencies)" in `README.md`.  Nothing
else: there is no `configure` step, and no build system beyond the two
makefiles in this directory.

GNU Make reads `GNUmakefile`, BSD make reads `BSDmakefile`, and each
picks its own file without being told.  Everything below works the same
with either, so `make` throughout means whichever one is installed.

Building
--------

```sh
make
```

That builds two programs into `./bin`: `icowm` itself, and `icowm-msg`,
the command-line client for its control socket.  Neither is installed
yet.

To build with several jobs at once:

```sh
make parallel
```

Installing
----------

```sh
sudo make install
```

This puts, under `/usr/local` by default:

| Path                               | What it holds |
|------------------------------------|---|
| `bin/icowm`, `bin/icowm-msg`       | the two programs |
| `share/man/man1`, `share/man/man5` | the manual pages |
| `share/xsessions/icowm.desktop`    | the session entry a display manager reads to offer IcoWM at login |
| `share/applications/icowm.desktop` | lets GNOME-style tooling discover IcoWM as an available window manager; never shown in an application launcher itself |
| `share/locale/<lang>`              | the compiled message catalogues |
| `share/icons/hicolor`              | the application icon, scalable and symbolic |
| `share/icowm`                      | a configuration to copy from |
| `share/doc/icowm`                  | `README.md`, `LICENSE`, `COMPLIANCE.md` & the four guides under `doc/` |

To install somewhere other than `/usr/local`:

```sh
make PREFIX=/usr
sudo make PREFIX=/usr install
```

`PREFIX` has to be given to **both**, since the path the program looks
in for its message catalogues is compiled into it.  Building with one
prefix and installing with another leaves the program unable to find its
translations.

To undo it:

```sh
sudo make uninstall
```

That removes exactly what `install` put there, and nothing else.
A locale directory shared with other packages is left alone.

Running it
----------

If a display manager is in use, IcoWM appears in its session list once
installed, from the `xsessions` entry above.

To start it from a plain X session instead, put this in `~/.xinitrc`:

```sh
exec icowm
```

IcoWM starts with no configuration at all, falling back to a built-in
default for anything absent.  To begin from the example instead:

```sh
mkdir -p ~/.config/icowm
cp -r /usr/local/share/icowm/* ~/.config/icowm/
```

See `icowm(1)` for where it looks, and `icowm(5)` and its companion
pages for what each file holds.

Build options
-------------

| Option         | Effect |
|----------------|---|
| `CC=clang`     | build with a different compiler |
| `DEBUG=1`      | debugging symbols, no optimization |
| `DEBUG=2`      | the above, plus AddressSanitizer |
| `STRIP=0`      | keep symbols in a release build |
| `COMPACT=1`    | smaller fixed arrays, for a memory-tight machine |
| `make analyze` | a static analysis pass, with `gcc` |
| `make test`    | build and run the test suites |
| `make help`    | every target and option |

Symbols are discarded by default, and a debug build keeps them
regardless of `STRIP`.

Packaging
---------

```sh
make DESTDIR=/path/to/staging PREFIX=/usr install
```

`DESTDIR` stages the whole tree somewhere else without touching the
running system, and is composable with `PREFIX`.  `BINDIR`, `MANDIR`,
`DATADIR`, `LOCALEDIR`, `XSESSIONSDIR`, `DOCDIR` and `EXAMPLEDIR` may
each be overridden on their own where a distribution puts one of them
somewhere unusual.

Nothing here runs `gtk-update-icon-cache`.  A staged install has no
cache to refresh, and refreshing the running system's own is the package
manager's business rather than this makefile's.

Setting `SOURCE_DATE_EPOCH` makes the build reproducible: the timestamp
compiled into the program comes from that epoch rather than the clock,
and the build counter stops advancing, so the same sources give the same
binary every time.

On the BSDs
-----------

Manual pages go under `${PREFIX}/man` rather than `${PREFIX}/share/man`,
which the makefiles work out from `uname` and which `MANDIR` overrides.

`gettext` lives in a library of its own rather than inside the
C library, from `gettext-runtime` on FreeBSD and OpenBSD or
`gettext-lib` from pkgsrc on NetBSD.  The makefiles look for it and link
it when it is there.

Two things IcoWM reads on Linux have no counterpart yet: the memory
figures under `/proc`, which the restricted-memory warning uses, and the
battery state under `/proc/apm` and `/sys/class/power_supply`, which the
system tray shows.  Neither is required.  Both simply report nothing
when the files are absent, and everything else runs unchanged.

**IMPORTANT NOTICE.**  IcoWM has not yet been built or run on a BSD.
Reports are welcome.
