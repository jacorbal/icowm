# IcoWM Manual

What IcoWM is, how to start it, every command-line option it accepts,
and restricted-memory mode's own run-time behavior.  For the JSON
configuration files themselves, namely what each one controls, every
field each accepts, its type, and its default, see
[`config.md`](config.md) instead.  For `icowm-msg`, the command-line
client for the IPC control socket section 5 below documents, see
[`icowm-msg.md`](icowm-msg.md) instead.

---

## Table of Contents

1. [What IcoWM is](#1-what-icowm-is)
2. [Starting IcoWM](#2-starting-icowm)
   - [2.1. Exiting IcoWM](#21-exiting-icowm)
3. [Command-line options](#3-command-line-options)
   - [3.1. Main options](#31-main-options)
   - [3.2. Logging](#32-logging)
   - [3.3. Other options](#33-other-options)
4. [Restricted-memory mode (`icowm -M <mib>`)](#4-restricted-memory-mode-icowm--m-mib)
   - [4.1. What this mode changes, and what it leaves alone](#41-what-this-mode-changes-and-what-it-leaves-alone)
   - [4.2. Refusing to start, and warning while running](#42-refusing-to-start-and-warning-while-running)
   - [4.3. How many windows it will manage at once](#43-how-many-windows-it-will-manage-at-once)
   - [4.4. Warning and error dialogs cannot be dismissed by accident](#44-warning-and-error-dialogs-cannot-be-dismissed-by-accident)
   - [4.5. Building an even lighter version](#45-building-an-even-lighter-version)
   - [4.6. Default values compared](#46-default-values-compared)
5. [IPC control socket](#5-ipc-control-socket)
   - [5.1. Connecting](#51-connecting)
   - [5.2. Wire protocol](#52-wire-protocol)
   - [5.3. Commands](#53-commands)
     - [5.3.1. Queries](#531-queries)
     - [5.3.2. Client actions taking only `client_id`](#532-client-actions-taking-only-client_id)
     - [5.3.3. Client actions taking their own extra arguments](#533-client-actions-taking-their-own-extra-arguments)
     - [5.3.4. Desktop-scoped actions](#534-desktop-scoped-actions)
     - [5.3.5. Surface actions](#535-surface-actions)
     - [5.3.6. Whole window manager](#536-whole-window-manager)
   - [5.4. The `icowm-msg` tool](#54-the-icowm-msg-tool)
   - [5.5. Talking to the socket directly](#55-talking-to-the-socket-directly)
   - [5.6. Subscribing to events](#56-subscribing-to-events)

---

## 1. What IcoWM is

IcoWM is a minimalist stacking window manager for the X Window System.
It manages ordinary, freely overlapping windows: there is no tiling or
automatic layout of application windows, and it represents an iconified
(minimized) window as an actual icon placed on the desktop, in the style
of classic window managers such as TWM, rather than as an entry in
a taskbar.

Configuration lives in a set of JSON files, described in full in
`config.md`; none of them is required, and IcoWM falls back to
a built-in default for any file that is absent.

IcoWM also exposes a local IPC control socket, so an external script can
query its state or drive it directly, without going through X11 client
messages; see section 5 below, and `icowm-msg.md` for its own small
command-line client, built alongside IcoWM itself for exactly that.

## 2. Starting IcoWM

IcoWM is started from an `.xinitrc`, a display manager session entry, or
any other place an X session's window manager is normally launched:

```sh
exec icowm
```

With no arguments, IcoWM connects to the display named by the `DISPLAY`
environment variable, reads its configuration from the directory
described in `config.md` section 1, and runs an ordinary, unrestricted
session.

### 2.1. Exiting IcoWM

There are two distinct ways to shut IcoWM down, and they behave very
differently on purpose.

The normal quit action (the "Quit" keybinding, its own confirmation
dialog, and the root menu's "Quit" entry) asks every currently open
application to close itself first, the same request closing one window
individually already sends, so an application with unsaved changes gets
the same chance to warn about it that it already gets any other time.
IcoWM waits for all of them (see `config.md` section
2.6's own `shutdown.timeout-seconds`) before actually exiting.

The emergency exit shortcut, `Ctrl+Mod1+Backspace` (off by default, see
`config.md` section 2.6's own `shutdown.enable-emergency-shortcut`),
does none of that.  It runs with no dialog, no confirmation, and no wait
of any kind: the moment it is pressed, IcoWM terminates immediately,
skipping even the exit session hooks (`config.md` section 8) the normal
quit action always runs.  This is deliberate, not an oversight: this
shortcut exists for situations where IcoWM itself might be unresponsive
or in some broken state, so it is kept to the smallest, most direct
action possible.  A confirmation dialog, or any other step that waits on
IcoWM's own event loop or rendering to keep working, would only be as
reliable as whatever it is that might be the very reason someone reaches
for this shortcut in the first place.

## 3. Command-line options

### 3.1. Main options

| Option            | Description |
|-------------------|-------------|
| `-d <display>`    | Set the X server display to connect to (e.g., `:0`). If not given, the `DISPLAY` environment variable is used. |
| `-c <config_dir>` | Set the configuration directory, overriding the lookup order described in `config.md` section 1. |
| `-C`              | Check every configuration file under `<config_dir>` for JSON syntax errors, print the result, and exit without starting a session. |
| `-M <mib>`        | Enable restricted-memory mode, with `<mib>` as the ceiling in mebibytes; see section 4. Must be at least 10. |
| `-s`              | Disable the IPC control socket entirely for this run: `ipc_init` is never called at all, rather than being attempted and possibly failing. See section 5 for what the socket does. |

### 3.2. Logging

| Option           | Description |
|------------------|-------------|
| `-L <log_level>` | Set the minimum log verbosity level (0-8; see the level list IcoWM itself prints with `-h`, from `trace` through `fatal`). |
| `-l <log_file>`  | Set the log destination: a file path, or one of the keywords `DEFAULT` (errors to `stderr`, informational messages to `stdout`), `STDOUT`, `STDERR`, or `NULL` (disable logging). |
| `-q`             | Quiet mode: only fatal errors are logged; identical to the highest `-L` level. |
| `-t`             | Enable function-level tracing for every logged message, not only at the `trace` level. |

### 3.3. Other options

| Option | Description                                     |
|--------|-------------------------------------------------|
| `-h`   | Show usage information and exit.                |
| `-v`   | Show version and license information, and exit. |

## 4. Restricted-memory mode (`icowm -M <mib>`)

`icowm -M <mib>` runs IcoWM in a mode aimed at genuinely memory-
constrained systems: an old machine, a low-power single-board computer,
a virtual machine given only a small amount of RAM.  `<mib>` is a number
of mebibytes, and must be at least 10; IcoWM refuses a smaller value
outright, since it could not realistically run in less than that
regardless of anything else this mode does.

### 4.1. What this mode changes, and what it leaves alone

This mode always runs with a single screen and a single desktop on it,
with no way to configure more of either: `memguard.json` (see
`config.md` section 10) has no field for a screen or desktop count at
all.  If the system actually has more than one X screen, only the first
is managed; the rest are left alone entirely.

Beyond that, several things are always turned off, regardless of what
`memguard.json` or the active theme say:

- **Icon pictures and hint characters.** A minimized window still shows
  a small icon you can click to restore it, but that icon is plain,
  without the application's own picture drawn on it or any overlay
  character hinting at its state.
- **Menu pixmaps.** Menu entries show their text only, without any icon
  alongside it.
- **Modern font rendering.** Every font the active theme specifies is
  replaced with IcoWM's own fixed built-in font, unless it already names
  some variant of that same font itself (matched case-sensitively):
  plain, traditional X core font rendering throughout, instead of the
  sharper, more flexible rendering IcoWM normally uses, which carries
  a real, ongoing memory cost of its own.
- **XSettings propagation.** Theme and font settings are not published
  to other applications over the XSettings protocol, regardless of the
  active theme's own `xsettings.is-enabled`.
- **Systray icon embedding.** The systray itself (clock, battery, and
  its own frame) still shows when enabled, but never accepts a docked
  application icon.

Everything else about how IcoWM looks and behaves comes from
`memguard.json` (see `config.md` section 10 for exactly which fields it
accepts) the same way `config.json` does for an ordinary session, which
is not read at all while `-M <mib>` is in effect.  `bindings.json`,
`a11y.json`, and a theme file under `themes/` are read exactly as in an
ordinary session: restricted-memory mode is never a reason to also give
up basic accessibility accommodations.

With a single desktop, the window-list menu and the "send window to..."
option skip straight to that desktop's own windows instead of first
asking you to pick a desktop you do not have a choice about anyway; this
is the same behavior an ordinary session gets with only one desktop
configured, restricted-memory mode or not.

### 4.2. Refusing to start, and warning while running

Before doing anything else, IcoWM checks how much memory the system
actually has free right now.  If that is less than the `<mib>` you gave
`-M <mib>`, IcoWM will not start at all, and says why in its log:
promising to stay under a ceiling is not meaningful if the system cannot
even spare that much to begin with.

Once running, IcoWM keeps an eye on its own memory use, checking every
few seconds.  If it ever reaches the ceiling you set, a dialog appears
telling you how much it is using and what the ceiling is, and suggesting
you close a window or two before opening anything else.  This is
a warning, not a hard wall enforced by the operating system: IcoWM does
not forcibly cut itself off at that number, since doing so reliably
would mean guaranteeing every single thing it might ever try to allocate
handles running out of memory gracefully, and getting that wrong would
mean a window manager that crashes instead of one that merely warns you
in time to act.

### 4.3. How many windows it will manage at once

Restricted-memory mode also limits how many application windows it will
actually manage at the same time, since each window IcoWM manages
carries its own real, ongoing cost regardless of anything else.  This
limit is worked out from the ceiling you chose with `-M <mib>`, roughly
like this:

1. A small slice of the ceiling (6 MiB) is set aside for IcoWM itself,
   before counting any windows at all.
2. Whatever is left over is divided up, generously, at a quarter of
   a mebibyte per window.
3. The result is never fewer than one window, and never more than 64.

In practice, because of how generous that per-window allowance is, this
reaches the 64-window ceiling by `-M 22`, and stays there for anything
more generous than that too; it only drops below 64 for a smaller value
than that, down to 16 windows at `-M <mib>`'s own smallest accepted
value (10 MiB).  Once you are at that limit, opening another application
shows a warning dialog explaining that a window has to be closed first;
the new window's own application is left waiting rather than being
handed something broken to work with.

### 4.4. Warning and error dialogs cannot be dismissed by accident

The two dialogs this mode shows (the memory-ceiling warning in section
4.2, the too-many-windows warning in section 4.3) behave a little
differently from IcoWM's other dialogs, on purpose: pressing Escape does
nothing at all, and pressing Enter or Space does nothing either until
you have actually selected the "OK" button first, either by clicking it
directly or by pressing Tab to select it and then Enter or Space.
A message serious enough to use one of these two dialogs is not meant to
be dismissed by the same reflexive key press that closes whatever else
happened to have focus a moment before.

### 4.5. Building an even lighter version

Everything above is a choice you make each time you start IcoWM, with
the `-M <mib>` flag.  It cannot make a few things smaller that are fixed
once IcoWM itself is built (how many screens, desktops, or monitors an
ordinary, unrestricted session can ever track at once, mainly), since
those are not something any flag can change afterward, only how many of
them you actually use at once.

If you know you are always going to run on a severely memory-
constrained machine, you can build IcoWM itself with that in mind:

```sh
make COMPACT=1
```

This produces a separate build (you would need to rebuild without it to
go back to the ordinary one) that starts with smaller allowances for
several things throughout: fewer screens, desktops, and monitors than an
ordinary, unrestricted session could otherwise ever track at once;
a smaller starting allowance for how many windows a desktop is initially
prepared for; and a smaller allowance for how long a message dialog's
own text can be.

**`COMPACT` and `-M <mib>` are entirely independent of each other.**
`COMPACT` only changes those fixed, compiled-in ceilings; it does not
turn restricted-memory mode on by itself, and it does not choose
a `<mib>` value for `-M <mib>` on its own either.  A `COMPACT` build
launched without `-M <mib>` at all runs a perfectly ordinary,
unrestricted session: no memory-ceiling warning, no window-count limit,
icon pictures and modern font rendering both still on, exactly as an
ordinary build would behave without `-M <mib>`, just one with smaller
compiled-in ceilings on screens, desktops, and monitors.
Restricted-memory mode's own behavior (sections 4.1 through 4.4) only
ever happens when you actually pass `-M <mib>` at the time you start
IcoWM, in either kind of build.  The two are meant to complement each
other for a build genuinely sized for a memory-constrained target from
the ground up, but each also works perfectly well entirely without the
other.

### 4.6. Default values compared

The table below assumes no configuration file changes any of these; if
yours does, your own configuration always wins over the defaults shown
here, in every row, in either kind of build.

| Setting | Ordinary build | `COMPACT` build |
|---------|---------------:|----------------:|
| Desktops per screen, ordinary session, no `config.json` at all | 4 | 4 |
| Screens, or desktops on that single screen, `-M <mib>` given (see 4.1) | 1 | 1 |
| Most screens an ordinary, unrestricted session can ever track at once | 6 | 1 |
| Most desktops per screen an ordinary, unrestricted session can ever track at once | 10 | 4 |
| Most physical monitors an ordinary, unrestricted session can ever track at once | 16 | 2 |
| Most XRandR output profiles you can configure at once, ordinary session | 16 | 2 |

`-M <mib>`'s own smallest accepted value (10 MiB), the memory set aside
for IcoWM itself before dividing up the rest among windows (6 MiB), the
rough cost assumed per window (a quarter of a mebibyte), and the hard
ceiling on how many windows it will ever manage regardless of a very
generous `-M <mib>` value (64) do not change between the two kinds of
build; see section 4.3 for how those combine.  RandR output-profile
management (`randr.json`) is never consulted at all in restricted-memory
mode, in either kind of build, since it always runs with a single, fixed
screen and desktop.  Accessibility (`a11y.json`, section 6) is the one
exception to that pattern: it is consulted exactly as in an ordinary
session, in either kind of build, since restricted-memory mode is never
a reason to also give up basic accessibility accommodations.

## 5. IPC control socket

IcoWM listens on a local Unix domain socket external tools can connect
to, to query its current state or ask it to do something, without going
through X11 client messages directly.  Every command already reachable
from a key binding is reachable here too, since the socket calls the
exact same underlying actions; it is a third way into that one catalog,
not a separate one of its own.

### 5.1. Connecting

The socket lives at `icowm/socket` under `$XDG_RUNTIME_DIR` (falling
back to `/tmp/icowm-<uid>` when that variable is unset), the same
resolution order every other IcoWM-owned XDG path follows.  On a typical
`systemd`-managed Linux system this resolves to
`/run/user/<uid>/icowm/socket`.  The containing directory is created
with mode `0700` if it does not already exist, so nothing else on the
system can even see the socket file, let alone connect to it.

### 5.2. Wire protocol

Each message, in either direction, is one line of JSON terminated by
`\n`.  A request is a JSON object with at least a string `"cmd"` field;
any other fields are that command's own arguments.  A response is always
a JSON object with at least a boolean `"ok"` field: `true`, with that
command's own result fields alongside it, on success; `false`, with
a string `"error"` field explaining what went wrong, on failure.
A malformed request (not valid JSON, valid JSON with no `"cmd"`, an
unrecognized command name, a missing or invalid argument) is reported
the same way, never left unanswered.

Every command that accepts a `"surface_id"` treats it as optional: when
left out, IcoWM falls back to the first surface in its own list, the
only reasonable choice on a single-monitor setup and still a usable one
on a multi-monitor one.

### 5.3. Commands

Every command below, except the four read-only queries, is a thin
wrapper around exactly one `enact_*` function: the same catalog of
actions the keyboard and mouse already reach through key bindings and
menus (see `enact.h` in the source tree), so the socket is a third way
into that one catalog, not a separate one of its own. Each command does
exactly the one thing its own `enact_*` function does, nothing more:
none of them chain multiple actions together, even where a keyboard
shortcut's own behavior might. `focus_client`, for instance, only moves
input focus; it does not also raise the client the way clicking on
a partially covered window normally would.  Combine two commands from
a script when the combined behavior is what is actually wanted:
`focus_client` followed by `raise_client` reproduces "focus and raise"
in full.

Every `id` (a desktop's, a client's, a surface's) is the same numeric
identifier IcoWM already uses for it internally: a client's `id` is its
X window ID, a desktop's `id` is its index on its own surface,
a surface's `id` is its own screen index. Every command that accepts
a `surface_id` treats it as optional in the way section
5.2 already describes. A `client_id` that does not currently belong to
  any managed client, or a `desktop_id` out of range for the resolved
  surface, is reported as a normal `"ok": false` error, never
  a connection drop.

Two things `enact.h` itself can do are deliberately left out of this
catalog:

- Adding or removing a client from a desktop's own internal tracking (as
  opposed to visibly moving it to another desktop, which
  `send_client_to_desktop` below does do) is bookkeeping tied to mapping
  and unmapping a window, not a user-facing action on its own; calling
  it by itself over IPC, detached from the window (re)parenting it is
  normally paired with, could leave IcoWM's own internal state
  inconsistent with what is actually on screen.
- Opening the interactive window-cycling menu is left out too, since it
  expects further keyboard or mouse input afterward to actually pick
  something from it; that is not something a fire-and-forget socket
  command can usefully drive.

#### 5.3.1. Queries

Read-only; take no arguments beyond what is noted.

| Command         | Response fields |
|-----------------|-----------------|
| `get_version`   | `protocol_version` (an integer identifying the shape of this wire protocol itself, not an IcoWM release number; only bumped if a command's own argument or response shape ever changes in a way an existing client could not already handle) |
| `list_desktops` | `desktops`: an array of `{id, name, surface_id, current}`, one entry per desktop on every managed surface |
| `list_clients`  | `clients`: an array of `{id, name, desktop_id, surface_id, x, y, w, h, iconified, urgent, sticky}`, one entry per focusable, non-skip-taskbar client on every desktop of every managed surface (`x`, `y`, `w`, `h` are that client's own current position and size, in pixels, the same geometry `move_client`, `move_resize_client`, and `resize_client` below change) |
| `get_focused`   | `focused`: an array of `{surface_id, client_id}`, one entry per managed surface (`client_id` is `null` when that surface currently has no active client) |

#### 5.3.2. Client actions taking only `client_id`

Every one of these takes exactly one argument, `client_id`, and responds
with a bare `{"ok": true}` on success.

| Command                    | What it does |
|----------------------------|--------------|
| `close_client`             | Politely asks the client to close (`WM_DELETE_WINDOW`, the same as its own close button), or destroys its window directly if it does not support that |
| `kill_client`              | Forcibly terminates the client's own X connection; a last resort for a client `close_client` cannot reach at all |
| `focus_client`             | Moves input focus to the client, without raising it (see this section's own introduction) |
| `unfocus_client`           | Takes input focus away from the client, if it currently had it |
| `iconify_client`           | Iconifies (minimizes) the client |
| `deiconify_client`         | Restores the client if it was iconified |
| `hide_client`              | Hides the client without iconifying it |
| `unhide_client`            | Undoes `hide_client` |
| `pin_client`               | Makes the client visible on every desktop of its own surface |
| `unpin_client`             | Undoes `pin_client` |
| `toggle_pin_client`        | Toggles between `pin_client` and `unpin_client` |
| `urge_client`              | Marks the client urgent (see the urgency-blinking behavior in its own theme documentation) |
| `unurge_client`            | Undoes `urge_client` |
| `center_client`            | Centers the client on its own current screen |
| `move_client_to_next_monitor` | Moves the client to the next physical monitor, keeping its position relative to that monitor's own top-left corner |
| `maximize_client_horz`     | Maximizes the client horizontally only |
| `maximize_client_vert`     | Maximizes the client vertically only |
| `maximize_client`          | Maximizes the client both horizontally and vertically |
| `raise_client`             | Raises the client to the front of its own current layer |
| `lower_client`             | Lowers the client to the back of its own current layer |
| `set_layer_above_client`   | Moves the client to the "always on top" layer |
| `set_layer_normal_client`  | Moves the client back to the ordinary layer |
| `set_layer_below_client`   | Moves the client to the "always below" layer |
| `cycle_layer_client`       | Cycles the client through above, normal, and below, in that order |

A fullscreen client's own stacking is always forced above every other
client while it holds focus, including every other above-layer one,
the same way a fullscreen application covers a taskbar or panel in
most desktop environments; see the `fullscreen`/`unfullscreen`
commands below.  Its real layer (whatever the four commands above
last set it to) is untouched the whole time, and takes effect again,
with no command of its own needed, as soon as it loses focus.  While
it holds focus and is fullscreen, `set_layer_above_client`,
`set_layer_normal_client`, `set_layer_below_client`, and
`cycle_layer_client` are all silently no-ops for it, and the window
context menu's own "Layer" submenu is disabled the same way, for the
same reason: choosing a layer here would have no visible effect until
it later leaves fullscreen, which would read as broken rather than
merely deferred.

| `shade_client`             | Rolls the client up into just its own titlebar |
| `unshade_client`           | Undoes `shade_client` |
| `toggle_shade_client`      | Toggles between `shade_client` and `unshade_client` |
| `fullscreen_client`        | Makes the client fill its own screen, without any decoration |
| `unfullscreen_client`      | Undoes `fullscreen_client` |
| `toggle_fullscreen_client` | Toggles between `fullscreen_client` and `unfullscreen_client` |
| `toggle_decorate_client`   | Shows or hides the client's own titlebar and border |
| `send_client_to_front`     | Raises the client to the front of its own desktop's window stack, independent of its layer |
| `send_client_to_back`      | Sends the client to the back of its own desktop's window stack, independent of its layer |

#### 5.3.3. Client actions taking their own extra arguments

| Command                  | Arguments | What it does |
|--------------------------|-----------|--------------|
| `move_client`            | `client_id`, `x`, `y` (both signed) | Moves the client so its own top-left corner is at that position |
| `move_resize_client`      | `client_id`, `x`, `y` (both signed), `w`, `h` (both unsigned) | Moves and resizes the client in one step, to that top-left corner and that size; see `resize_client` below to resize only, leaving position alone |
| `resize_client`          | `client_id`, `w`, `h` (both unsigned) | Resizes the client only, from wherever its own top-left corner already is; see `move_resize_client` above to move and resize together in one step |
| `move_client_to_monitor` | `client_id`, `monitor_index` | Moves the client to that physical monitor, the same as `move_client_to_next_monitor` but to a specific one rather than the next one |
| `rename_client`          | `client_id`, `name` | Overrides the client's own window title as IcoWM displays it |
| `reclass_client`         | `client_id`, `class_name`, `instance_name` | Overrides the client's own ICCCM `WM_CLASS` (both its class and instance name), which theme rules and other IcoWM behavior that matches on window class use |
| `rerole_client`          | `client_id`, `role` | Overrides the client's own window role |
| `set_client_icon`        | `client_id`, `icon_name` | Overrides which icon IcoWM shows for the client when iconified |

#### 5.3.4. Desktop-scoped actions

| Command                  | Arguments | What it does |
|--------------------------|-----------|--------------|
| `set_desktop_background` | `desktop_id`, `surface_id` (optional), `color` (a packed `0xRRGGBB` value) | Sets that desktop's own solid background color |
| `show_desktop`           | `desktop_id`, `surface_id` (optional), `show` (boolean) | Shows or hides every client on that desktop at once, the same as a "show desktop" shortcut |
| `send_client_to_desktop` | `client_id`, `target_desktop_id` (on the client's own current surface) | Moves the client to another desktop on the same surface |
| `iconify_all`            | `desktop_id` (optional; the resolved surface's own current desktop otherwise), `surface_id` (optional) | Iconifies every client on that desktop at once |
| `deiconify_all`          | Same arguments as `iconify_all` | Restores every iconified client on that desktop at once |
| `rearrange_desktop`      | `desktop_id` (optional; the resolved surface's own current desktop otherwise), `surface_id` (optional) | Re-applies the configured placement policy to every client on that desktop; see `config.md`'s own `windows.placement-policy` for which policy that is |
| `toggle_scratchpad`      | `desktop_id` (optional; the resolved surface's own current desktop otherwise), `surface_id` (optional) | Launches the scratchpad (see `config.md`'s own `scratchpad`), or shows/hides it on that desktop if it is already running |

#### 5.3.5. Surface actions

| Command        | Arguments                                        | What it does |
|----------------|--------------------------------------------------|--------------|
| `goto_desktop` | `desktop_id` (required), `surface_id` (optional) | Switches the resolved surface to that desktop |
| `goto_next_desktop` | `surface_id` (optional)                     | Switches the resolved surface to its own next desktop, wrapping around after the last one |
| `goto_prev_desktop` | `surface_id` (optional)                     | Switches the resolved surface to its own previous desktop, wrapping around before the first one |

#### 5.3.6. Whole window manager

| Command         | Arguments | What it does |
|-----------------|-----------|--------------|
| `exit_wm`       | none      | Requests that IcoWM stop and exit, the same as its own quit shortcut |
| `reload_config` | none      | Reloads every configuration file, the same as sending IcoWM `SIGHUP` |

### 5.4. The `icowm-msg` tool

`icowm-msg` is a small, standalone command-line client for the
socket, built and installed alongside IcoWM itself as a separate
binary (see `make help`):

```sh
$ icowm-msg get_version
{"ok":true,"protocol_version":1}

$ icowm-msg goto_desktop desktop_id=1
{"ok":true}
```

Everything specific to `icowm-msg` itself, its own usage, how
a `key=value` argument becomes a request field, its exit status, and its
own options, is documented in full in [`icowm-msg.md`](icowm-msg.md),
not repeated here; the commands it sends and their own arguments remain
the ones section 5.3 above documents.

### 5.5. Talking to the socket directly

Since the protocol is plain, newline-delimited JSON (section 5.2), it
can also be exercised directly from a shell, without `icowm-msg`, using
a tool like `socat`; useful for a system without `icowm-msg` installed,
or for watching the raw traffic while debugging something:

```sh
$ echo '{"cmd": "list_desktops"}' | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/icowm/socket
{"ok":true,"desktops":[{"id":0,"name":"Work","surface_id":0,"current":true}]}

$ echo '{"cmd": "goto_desktop", "desktop_id": 1}' | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/icowm/socket
{"ok":true}
```

### 5.6. Subscribing to events

Every command in section 5.3 follows the same request/response shape:
one line in, one line back, connection otherwise idle in
between. `subscribe` and `unsubscribe` are the two exceptions: once
subscribed, that same connection starts receiving extra lines on its
own, one per matching event, for as long as it stays open, without
sending anything further itself. Each event line carries its own
`"event"` field naming which one it is, alongside that event's own
fields; there is no `"ok"` field on an event line the way there is on
every ordinary response, since nothing was asked for it to answer.

```sh
$ echo '{"cmd": "subscribe", "events": ["window_mapped"]}' | socat - UNIX-CONNECT:$XDG_RUNTIME_DIR/icowm/socket
{"ok":true}
{"client_id":23068673,"desktop_id":0,"surface_id":0,"event":"window_mapped"}
```

(`socat` above exits once the connection closes or is interrupted; in
practice a real subscriber keeps the connection open and keeps reading
for as long as it wants more events, the same way `icowm-msg -w` does;
see `icowm-msg.md` section 9 for that.)

`subscribe` takes one argument, a non-empty array `"events"` of
recognized event names; an unrecognized name anywhere in the array
rejects the request as a whole (`"ok": false`, nothing is
subscribed). `unsubscribe` takes the same, optional this time: given, it
unsubscribes from only those (an unrecognized or never-subscribed name
among them is not an error, since there is nothing to undo either way);
left out entirely, it unsubscribes from everything at once. Both
otherwise respond the same bare `{"ok": true}` every other command that
takes no result fields of its own does.

Every event type:

| Event              | Fields |
|--------------------|--------|
| `window_mapped`    | `client_id`, `desktop_id`, `surface_id`: a client was just mapped onto that desktop |
| `window_closed`    | `client_id`, `desktop_id`, `surface_id`: a client was just destroyed |
| `desktop_switched` | `surface_id`, `desktop_id`: that surface's own current desktop just changed to `desktop_id` |
| `focus_changed`    | `surface_id`, `client_id`: that client just became the active one on its own surface |
| `urgency_set`      | `client_id`, `desktop_id`, `surface_id`: that client's urgency hint was just set |
| `urgency_cleared`  | `client_id`, `desktop_id`, `surface_id`: that client's urgency hint was just cleared |
| `window_moved`     | `client_id`, `desktop_id`, `surface_id`: that client's own position just changed (see the `list_clients` command for its current `x`/`y`) |
| `window_resized`   | `client_id`, `desktop_id`, `surface_id`: that client's own size just changed (see the `list_clients` command for its current `w`/`h`) |
| `rule_applied`     | `client_id`, `desktop_id`, `surface_id`: a loaded rule just changed one or more of that client's own properties |
| `pin_set`          | `client_id`, `desktop_id`, `surface_id`: that client was just pinned (visible on every desktop) |
| `pin_cleared`      | `client_id`, `desktop_id`, `surface_id`: that client was just unpinned |
| `fullscreen_set`   | `client_id`, `desktop_id`, `surface_id`: that client just entered full screen |
| `fullscreen_cleared` | `client_id`, `desktop_id`, `surface_id`: that client just left full screen |
| `shade_set`        | `client_id`, `desktop_id`, `surface_id`: that client was just shaded (rolled up into its titlebar) |
| `shade_cleared`    | `client_id`, `desktop_id`, `surface_id`: that client was just unshaded |
| `hide_set`         | `client_id`, `desktop_id`, `surface_id`: that client was just hidden |
| `hide_cleared`     | `client_id`, `desktop_id`, `surface_id`: that client was just unhidden |
| `decoration_set`   | `client_id`, `desktop_id`, `surface_id`: that client's own titlebar and border were just shown |
| `decoration_cleared` | `client_id`, `desktop_id`, `surface_id`: that client's own titlebar and border were just hidden |
| `client_iconified` | `client_id`, `desktop_id`, `surface_id`: that client was just iconified |
| `client_deiconified` | `client_id`, `desktop_id`, `surface_id`: that client was just restored from being iconified |
| `layer_changed`    | `client_id`, `desktop_id`, `surface_id`: that client's own stacking layer just changed (see `list_clients` for its current layer) |
| `client_desktop_changed` | `client_id`, `desktop_id`, `surface_id`: that client just moved to a different desktop (`desktop_id` is the new one) |
| `client_renamed`   | `client_id`, `desktop_id`, `surface_id`, `name`: that client's own displayed title was just overridden |
| `client_reclassed` | `client_id`, `desktop_id`, `surface_id`, `class_name`, `instance_name`: that client's own `WM_CLASS` was just overridden |
| `client_reroled`   | `client_id`, `desktop_id`, `surface_id`, `role`: that client's own window role was just overridden |
| `client_icon_changed` | `client_id`, `desktop_id`, `surface_id`, `icon_name`: that client's own displayed icon was just overridden |
| `desktop_background_changed` | `desktop_id`, `surface_id`: that desktop's own solid background color was just set |
| `desktop_shown`    | `desktop_id`, `surface_id`: every client on that desktop was just shown at once |
| `desktop_hidden`   | `desktop_id`, `surface_id`: every client on that desktop was just hidden at once |
| `config_reloaded`  | none: every configuration file was just reloaded |
| `stacking_changed` | `client_id`, `desktop_id`, `surface_id`: that client's own position within its layer's stacking order just changed |

A subscription lasts only as long as the connection itself: closing the
connection (or losing it) drops every subscription made on it, with no
separate `unsubscribe` needed first. A single connection can freely mix
subscribing and ordinary commands: nothing about sending `subscribe`
changes how the rest of the protocol on that same connection behaves,
beyond the extra, unsolicited lines that start arriving afterward.
