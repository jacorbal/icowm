Hacking on IcoWM
================

Herein is the map for one who would alter the code, rather than build it
or send a patch thereunto.  [`INSTALL.md`](INSTALL.md) sets forth how it
is built;  [`CONTRIBUTING.md`](CONTRIBUTING.md) sets forth the manner in
which each file is written.  Neither explains what the several pieces
are, and that is what follows.

The shape of it
---------------

About ten score and sixteen source files and ten score and seventeen
headers, laid out so that a header mirrors its source, e.g.,
`src/policy/placement/icon.c` is declared in
`include/policy/placement/icon.h`.  One from twenty sources sit at the
top of `src/`, each the entry point of the directory beside it, to wit,
`client.c` and `client/`, `stage.c` and `stage/`, and suchlike.

| Directory  | What lives there |
|------------|---|
| `adt/`     | Data structures: circular doubly linked list, open hash table |
| `cctl/`    | Client control: adopting windows the manager did not map |
| `client/`  | What a client is, its properties, layout and predicates |
| `cmds/`    | The operations on a client, one file per family |
| `config/`  | Reading and validating the JSON files, and linting them |
| `desktop/` | A desktop and the clients it holds |
| `enact/`   | The public verbs, which wrap `cmds/` and announce over IPC |
| `handler/` | One file per X event type |
| `input/`   | Keyboard and pointer: bindings, drags, interception |
| `ipc/`     | The control socket, its commands and its events |
| `loop/`    | The main loop: polling, timers, signals, dispatch |
| `menu/`    | Menus and dialogs, including the shared message dialog |
| `policy/`  | Where things go and what gets focus: placement, stacking |
| `render/`  | Drawing: text, glyphs, icons, outlines, decorations |
| `rules/`   | Matching a window against `rules.json` and applying it |
| `stage/`   | An X screen, its monitors and its desktops |
| `systray/` | The tray, its protocol and what it shows |
| `utils/`   | Safe strings, XCB helpers, spawning, system memory |
| `wm/`      | The manager itself: startup, EWMH, actions, shutdown |

Words that mean particular things
---------------------------------

Much of what goes awry in this code does so because two of these were
mistaken for one.

**Stage** is an X screen.  A machine ordinarily has exactly one, and
`memguard` mode fixes the count at one.

**Monitor** is a physical display inside a stage.  Two monitors on one
card make one stage with two monitors, which is the common
arrangement, so a single stage says nothing about how many monitors
there are.  RandR reports them; `stage_monitor_for_point` resolves
which one a point falls on.

**Desktop** is a virtual workspace within a stage.  A client belongs
to exactly one, unless it is pinned to all of them, and even then it
keeps the one it belongs to.

**Viewport** is a desktop's pannable area, sized in whole screens by
`topology.screens.desktops[].viewport`.  At `1x1`, the default, it is
the screen and there is nothing to pan; larger, the screen becomes
a window onto a canvas wider or taller than itself.  Panning moves that
window, not the desktop: the current desktop never changes, and every
non-sticky client is translated the opposite way so it keeps its place
on the canvas.

**Page** is one screen-sized cell of that canvas, addressed `{column,
row}` from the top-left.  A client belongs to the page its own top-left
corner falls in, and to that one only, whatever it overlaps; a client
larger than a page, or straddling two, would otherwise belong to several
at once and every caller needs a single answer.  The origin, though, is
not confined to page boundaries: `viewport.pan` slides it by pixels
while `viewport.page` and `viewport.go-to` land on a page exactly, so
a view can straddle two pages and is reported as whichever one holds its
middle.

**Sticky and pinned** are the two flags that make a client belong
everywhere, one per axis, and neither implies the other.  Pinned is
about desktops: the client follows the user to every one of them.
Sticky is about pages: the client holds its position on the physical
screen while the canvas scrolls under it, so it is on every page.  The
EWMH state named `_NET_WM_STATE_STICKY` is the pinned one, a collision
this manager inherited rather than chose.

**Client** is a managed window, and it owns four X windows: the
application's own `window`, the `frame` wrapped around it, the
`titlebar` inside that frame, and the `icon_window` standing in for it
on the desktop while it is iconified.  Which of the four an event
arrived on decides what it means.

**Transient family** is a client and everything transient for it, at any
depth.  It moves as one: raising, lowering and iconifying resolve the
family's top-level member first, so a dialog never ends up behind the
window it belongs to.

**Layer** is `above`, `normal` or `below`, and outranks stacking order:
every client in `above` sits over every client in `normal`, whatever the
order within each.

How a change reaches the screen
-------------------------------

Four steps, and the direction is always the same:

```
handler/  ->  policy/  ->  cmds/  ->  enact/
```

**`handler/`** turns an X event into an intent.  One file per event
type: `map.c` decides what to do with a window asking to appear,
`configure.c` with one asking to move.

**`policy/`** answers the questions with more than one right answer:
where a window goes, what gets focus, how the stack is ordered.
A policy decides; it does not act.

**`cmds/`** performs one operation on one client, at the level of
X requests.  `ccmd_client_iconify` unmaps, marks, creates the icon
window.

**`enact/`** is the public face of `cmds/`, and the difference is that
it announces: `enact_client_iconify` calls the command and then
broadcasts over the control socket.  Anything a user or a script can ask
for goes through `enact/`, so that the socket never misses an event.

Reach for `cmds/` from inside the manager and `enact/` from anything
answering a request.

The loop that drives all of it is in `loop/`: `pollset.c` waits on the
X connection and the control socket, `timers.c` schedules what has to
happen later, `dispatch.c` hands each event to its handler.

Things worth knowing before changing them
-----------------------------------------

**Fonts have two backends.**  X core bitmap fonts through XCB, and
TrueType through fontconfig and FreeType.  Never count characters to
work out how wide something is: `menu_draw_measure` reports pixels and
handles both, and a proportional font makes character counts wrong.

**Only one message dialog exists.**  `menu/dialog/message.c` draws it,
and `fortune`, `shortcuts`, `inspect`, `info` and the rest are thin
wrappers that compose text and hand it over.  Scrolling, the monitor cap
and the button all live there, once.

**Menu and dialog coordinates differ.**  A click on a menu arrives in
root coordinates and is converted; motion arrives already relative to
the window.  Titlebar buttons and dialogs use the window-relative pair
throughout.  Getting this wrong puts a click on the row below the one
the pointer is highlighting.

**Struts are honoured by default.**  A panel reserving space shrinks
every desktop's work area, and placement measures against that work
area, not against the monitor.

**The compact build is not restricted-memory mode.**  `make COMPACT=1`
is a compile-time option that shrinks fixed arrays; `icowm -M <mib>` is
a runtime mode that reads `memguard.json` instead of `config.json`.
The two of them are unrelated and both exist, and also may coexist.

**An override-redirect window of icowm's own needs its own
`_NET_WM_WINDOW_TYPE`.**  A menu, a dialog, a tooltip, the tray bar, any
window icowm creates for itself rather than adopts from a client, is
invisible to a compositor or a pager unless it says what it is.  The
pattern is always the same, right after the `xcb_create_window` call
that makes it: guard on `xcb_ewmh_connection_get() != NULL`, take the
matching `_NET_WM_WINDOW_TYPE_*` atom off it, and hand both to
`xcb_ewmh_set_wm_window_type`.  Both `menu/context/ctxmenu.c` and
`menu/cycle.c` use `_MENU`; the three dialogs under `menu/dialog/` and
`menu/search.c` use `_DIALOG`; `menu/popup.c` and
`input/mouse/drag/overlay.c` use `_TOOLTIP`; `menu/notify.c` uses
`_NOTIFICATION`; `systray/protocol.c`'s tray bar uses `_DOCK`.  Adding
a new one of these windows without adding this block is the single most
recurring gap this tree has had, so treat it as part of creating the
window, not as a follow-up.  The one deliberate exception is
`render/outline.c`'s drag outline, which is a guide with no EWMH
semantic of its own, and `xsettings.c`'s selection-owner window, which
is never mapped at all.

**A restart is a fresh `execvp`, not a config reload.**  `restart_wm`
(over IPC, or `icowm-msg restart_wm` from a terminal) sets a flag `main`
checks once `wm_start` returns, then re-executes this very binary with
the same `argv`.  Every managed client is already back under bare X by
then, released the same way any other exit releases them, so the fresh
process's own startup scan just adopts each one again.  `reload_config`
is a different thing entirely, as it re-reads files into the process
already running, and never touches `argv` or `main` at all.

**A `_NET_WM_PID` may name a process on another host.**
`WM_CLIENT_MACHINE` says which one; `client.c` compares it against
`gethostname(2)` once at adoption time, and `cmds/client/focus.c` only
ever escalates a stuck kill to a real `SIGKILL` (`cctl_kill_register`)
when the two agree.  `xcb_kill_client` alone, being protocol-level
rather than PID-based, still reaches a remote client's connection just
fine.

Building for work rather than for use
-------------------------------------

```sh
make DEBUG=1    # symbols, no optimization
make DEBUG=2    # the above, plus AddressSanitizer
make analyze    # a static analysis pass, with gcc
make headers    # check every header compiles alone
```

`DEBUG=2` deliberately leaves `-fanalyzer` off: gcc 13 loses track of
its own instrumentation when given both and reports uninitialized values
that are not there.

Build with both compilers is recommended before believing anything is
finished.  gcc and clang each catch what the other does not: clang's
`-Wdocumentation` finds a comment naming a parameter that no longer
exists, and its `-Wswitch-enum` finds an enumeration value quietly
swallowed by a `default`.

At runtime, `-L <level>` sets how much is logged, from `0` for every
traced action to `8` for fatal errors alone, and `-l <log_file>` sends
it to a file.  Using `-L 0` on a busy session produces tens of thousands
of lines, which is what makes it useful when an event ordering is in
question.

Allowed values for log level:

| Value | Level |
|-------|---|
| 0     | Trace |
| 1     | Debug |
| 2     | Info |
| 3     | Notice |
| 4     | Warning |
| 5     | Error |
| 6     | Critical |
| 7     | Alert |
| 8     | Fatal |

Tests
-----

```sh
make test
```

A hundred threescore and sixteen suites under `tests/`, mirroring the
directories they exercise, plus thirteen at the top for the pieces that
have no directory of their own.  They run without an X server, as what
they test is the logic that can be reached without one, which is most of
the placement, the configuration parsing, the rules and the data
structures.

`tests/harness/` holds the TAP output and the assertions;
`tests/manual/` holds what has to be driven by hand.

Anything wherein state survives between calls is the weak spot, and
`tests/policy/test_placement.c` is the pattern to follow, for it stubs
what the module communicates with and drives the state machine directly.
The stateful modules with no suite as yet are the window list, the drag
tracker, the modal keyboard state and manual placement.

Methinks there ought yet to be more tests.

Documentation
-------------

The manual pages under `doc/man/` are `mdoc`, and `mandoc -T lint` does
check them.  But `man --warnings` does not, on a system whose manual
pages have been stripped out, so it will report nothing and mean
nothing.

`doc/*.md` are the long-form references, installed alongside the manual
pages.  When a command, an event or a configuration key is added, it
belongs in three places: the code, the manual page and the reference.
Comparing the three is a `grep` away, and they drift apart silently
otherwise.

A word at the end
-----------------

Read before writing.  Very little here is arbitrary, though a good deal
of it is unobvious, and the reason for a thing is more often set down in
the comment above it than anywhere else.  Where you find a rule that
seems to serve nothing, look once more before undoing it, as the guard
that appears to protect against the impossible is usually the one that
was put there the day the impossible happened.

And take the small care.  A window manager is not admired; it is lived
in, every hour of every day, and what its user notices is never the
clever part but the ragged edge.  The click that lands a row too low,
the dialog that will not close, the icon that comes back somewhere other
than where it was left.  Mend those, and the rest is quiet.
