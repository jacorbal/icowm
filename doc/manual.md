# IcoWM Manual

What IcoWM is, how to start it, every command-line option it accepts,
and restricted-memory mode's own run-time behavior.  For the JSON
configuration files themselves, namely what each one controls, every field
each accepts, its type, and its default, see
[`config.md`](config.md) instead.

---

## Table of Contents

1. [What IcoWM is](#1-what-icowm-is)
2. [Starting IcoWM](#2-starting-icowm)
3. [Command-line options](#3-command-line-options)
   - [3.1 Main options](#31-main-options)
   - [3.2 Logging](#32-logging)
   - [3.3 Other options](#33-other-options)
4. [Restricted-memory mode (`icowm -M <mib>`)](#4-restricted-memory-mode-icowm--m-mib)
   - [4.1 What this mode changes, and what it leaves alone](#41-what-this-mode-changes-and-what-it-leaves-alone)
   - [4.2 Refusing to start, and warning while running](#42-refusing-to-start-and-warning-while-running)
   - [4.3 How many windows it will manage at once](#43-how-many-windows-it-will-manage-at-once)
   - [4.4 Warning and error dialogs cannot be dismissed by accident](#44-warning-and-error-dialogs-cannot-be-dismissed-by-accident)
   - [4.5 Building an even lighter version](#45-building-an-even-lighter-version)
   - [4.6 Default values compared](#46-default-values-compared)

---

## 1. What IcoWM is

IcoWM is a minimalist stacking window manager for the X Window System.
It manages ordinary, freely overlapping windows: there is no tiling
or automatic layout of application windows, and it represents an
iconified (minimized) window as an actual icon placed on the desktop,
in the style of classic window managers such as TWM, rather than as an
entry in a taskbar.

Configuration lives in a set of JSON files, described in full in
`config.md`; none of them is required, and IcoWM falls
back to a built-in default for any file that is absent.

## 2. Starting IcoWM

IcoWM is started from an `.xinitrc`, a display manager session entry,
or any other place an X session's window manager is normally launched:

```sh
exec icowm
```

With no arguments, IcoWM connects to the display named by the
`DISPLAY` environment variable, reads its configuration from the
directory described in `config.md` section 1, and runs an ordinary,
unrestricted session.

## 3. Command-line options

### 3.1 Main options

| Option | Description |
|--------|-------------|
| `-d <display>` | Set the X server display to connect to (e.g., `:0`). If not given, the `DISPLAY` environment variable is used. |
| `-c <config_dir>` | Set the configuration directory, overriding the lookup order described in `config.md` section 1. |
| `-C` | Check every configuration file under `<config_dir>` for JSON syntax errors, print the result, and exit without starting a session. |
| `-M <mib>` | Enable restricted-memory mode, with `<mib>` as the ceiling in mebibytes; see section 4. Must be at least 10. |

### 3.2 Logging

| Option | Description |
|--------|-------------|
| `-L <log_level>` | Set the minimum log verbosity level (0-8; see the level list IcoWM itself prints with `-h`, from `trace` through `fatal`). |
| `-l <log_file>` | Set the log destination: a file path, or one of the keywords `DEFAULT` (errors to `stderr`, informational messages to `stdout`), `STDOUT`, `STDERR`, or `NULL` (disable logging). |
| `-q` | Quiet mode: only fatal errors are logged; identical to the highest `-L` level. |
| `-t` | Enable function-level tracing for every logged message, not only at the `trace` level. |

### 3.3 Other options

| Option | Description |
|--------|-------------|
| `-h` | Show usage information and exit. |
| `-v` | Show version and license information, and exit. |

## 4. Restricted-memory mode (`icowm -M <mib>`)

`icowm -M <mib>` runs IcoWM in a mode aimed at genuinely memory-
constrained systems: an old machine, a low-power single-board
computer, a virtual machine given only a small amount of RAM.  `<mib>`
is a number of mebibytes, and must be at least 10; IcoWM refuses a
smaller value outright, since it could not realistically run in less
than that regardless of anything else this mode does.

### 4.1 What this mode changes, and what it leaves alone

This mode always runs with a single screen and a single desktop on
it, with no way to configure more of either: `memguard.json` (see
`config.md` section 9) has no field for a screen or desktop count at
all.  If the system actually has more than one X screen, only the
first is managed; the rest are left alone entirely.

Beyond that, several things are always turned off, regardless of what
`memguard.json` or the active theme say:

- **Icon pictures and hint characters.** A minimized window still
  shows a small icon you can click to restore it, but that icon is
  plain, without the application's own picture drawn on it or any
  overlay character hinting at its state.
- **Menu pixmaps.** Menu entries show their text only, without any
  icon alongside it.
- **Modern font rendering.** Every font the active theme specifies is
  replaced with IcoWM's own fixed built-in font, unless it already
  names some variant of that same font itself (matched
  case-sensitively): plain, traditional X core font rendering
  throughout, instead of the sharper, more flexible rendering IcoWM
  normally uses, which carries a real, ongoing memory cost of its own.
- **XSettings propagation.** Theme and font settings are not published
  to other applications over the XSettings protocol, regardless of the
  active theme's own `xsettings.is-enabled`.
- **Systray icon embedding.** The systray itself (clock, battery, and
  its own frame) still shows when enabled, but never accepts a docked
  application icon.

Everything else about how IcoWM looks and behaves comes from
`memguard.json` (see `config.md` section 9 for exactly which fields it
accepts) the same way `config.json` does for an ordinary session,
which is not read at all while `-M <mib>` is in effect.  `bindings.json`
and a theme file under `themes/` are read exactly as in an ordinary
session.

With a single desktop, the window-list menu and the "send window
to..." option skip straight to that desktop's own windows instead of
first asking you to pick a desktop you do not have a choice about
anyway; this is the same behavior an ordinary session gets with only one
desktop configured, restricted-memory mode or not.

### 4.2 Refusing to start, and warning while running

Before doing anything else, IcoWM checks how much memory the system
actually has free right now.  If that is less than the `<mib>` you
gave `-M <mib>`, IcoWM will not start at all, and says why in its log:
promising to stay under a ceiling is not meaningful if the system
cannot even spare that much to begin with.

Once running, IcoWM keeps an eye on its own memory use, checking every
few seconds.  If it ever reaches the ceiling you set, a dialog appears
telling you how much it is using and what the ceiling is, and
suggesting you close a window or two before opening anything else.
This is a warning, not a hard wall enforced by the operating system:
IcoWM does not forcibly cut itself off at that number, since doing so
reliably would mean guaranteeing every single thing it might ever try
to allocate handles running out of memory gracefully, and getting that
wrong would mean a window manager that crashes instead of one that
merely warns you in time to act.

### 4.3 How many windows it will manage at once

Restricted-memory mode also limits how many application windows it
will actually manage at the same time, since each window IcoWM manages
carries its own real, ongoing cost regardless of anything else.  This
limit is worked out from the ceiling you chose with `-M <mib>`,
roughly like this:

1. A small slice of the ceiling (6 MiB) is set aside for IcoWM itself,
   before counting any windows at all.
2. Whatever is left over is divided up, generously, at a quarter of a
   mebibyte per window.
3. The result is never fewer than one window, and never more than 64.

In practice, because of how generous that per-window allowance is,
this reaches the 64-window ceiling by `-M 22`, and stays there for
anything more generous than that too; it only drops below 64 for a
smaller value than that, down to 16 windows at `-M <mib>`'s own
smallest accepted value (10 MiB).  Once you are at that limit, opening
another application shows a warning dialog explaining that a window
has to be closed first; the new window's own application is left
waiting rather than being handed something broken to work with.

### 4.4 Warning and error dialogs cannot be dismissed by accident

The two dialogs this mode shows (the memory-ceiling warning in section
4.2, the too-many-windows warning in section 4.3) behave a little
differently from IcoWM's other dialogs, on purpose: pressing Escape
does nothing at all, and pressing Enter or Space does nothing either
until you have actually selected the "OK" button first, either by
clicking it directly or by pressing Tab to select it and then Enter or
Space.  A message serious enough to use one of these two dialogs is
not meant to be dismissed by the same reflexive key press that closes
whatever else happened to have focus a moment before.

### 4.5 Building an even lighter version

Everything above is a choice you make each time you start IcoWM, with
the `-M <mib>` flag.  It cannot make a few things smaller that are
fixed once IcoWM itself is built (how many screens, desktops, or
monitors an ordinary, unrestricted session can ever track at once,
mainly), since those are not something any flag can change afterward,
only how many of them you actually use at once.

If you know you are always going to run on a severely memory-
constrained machine, you can build IcoWM itself with that in mind:

```sh
make COMPACT=1
```

This produces a separate build (you would need to rebuild without it
to go back to the ordinary one) that starts with smaller allowances
for several things throughout: fewer screens, desktops, and monitors
than an ordinary, unrestricted session could otherwise ever track at
once; a smaller starting allowance for how many windows a desktop is
initially prepared for; and a smaller allowance for how long a message
dialog's own text can be.

**`COMPACT` and `-M <mib>` are entirely independent of each other.**
`COMPACT` only changes those fixed, compiled-in ceilings; it does not
turn restricted-memory mode on by itself, and it does not choose a
`<mib>` value for `-M <mib>` on its own either.  A `COMPACT` build
launched without `-M <mib>` at all runs a perfectly ordinary,
unrestricted session: no memory-ceiling warning, no window-count
limit, icon pictures and modern font rendering both still on, exactly
as an ordinary build would behave without `-M <mib>`, just one with
smaller compiled-in ceilings on screens, desktops, and monitors.
Restricted-memory mode's own behavior (sections 4.1 through 4.4) only
ever happens when you actually pass `-M <mib>` at the time you start
IcoWM, in either kind of build.  The two are meant to complement each
other for a build genuinely sized for a memory-constrained target from
the ground up, but each also works perfectly well entirely without the
other.

### 4.6 Default values compared

The table below assumes no configuration file changes any of these;
if yours does, your own configuration always wins over the defaults
shown here, in every row, in either kind of build.

| Setting | Ordinary build | `COMPACT` build |
|---------|----------------:|----------------:|
| Desktops per screen, ordinary session, no `config.json` at all | 4 | 4 |
| Screens, or desktops on that single screen, `-M <mib>` given (see 4.1) | 1 | 1 |
| Most screens an ordinary, unrestricted session can ever track at once | 6 | 1 |
| Most desktops per screen an ordinary, unrestricted session can ever track at once | 10 | 4 |
| Most physical monitors an ordinary, unrestricted session can ever track at once | 16 | 2 |
| Most XRandR output profiles you can configure at once, ordinary session | 16 | 2 |

`-M <mib>`'s own smallest accepted value (10 MiB), the memory set
aside for IcoWM itself before dividing up the rest among windows
(6 MiB), the rough cost assumed per window (a quarter of a mebibyte),
and the hard ceiling on how many windows it will ever manage
regardless of a very generous `-M <mib>` value (64) do not change
between the two kinds of build; see section 4.3 for how those combine.
RandR output-profile management (`randr.json`) is never consulted at
all in restricted-memory mode, in either kind of build, since it
always runs with a single, fixed screen and desktop.
