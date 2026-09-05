# IcoWM Configuration Reference

Description of every configuration file used by IcoWM, the purpose of
each file, and every supported option together with its type, accepted
values, and built-in default value.

---

## Table of Contents

1. [Directory structure](#1-directory-structure)
2. [`config.json`: Base configuration](#2-configjson-base-configuration)
   - [2.1. `theme`](#21-theme)
   - [2.2. `topology`](#22-topology)
   - [2.3. `programs`](#23-programs)
   - [2.4. `windows`](#24-windows)
   - [2.5. `icons`](#25-icons)
   - [2.6. `shutdown` / `fortune`](#26-shutdown--fortune)
   - [2.7. `startup-notification`](#27-startup-notification)
   - [2.8. `menu`](#28-menu)
   - [2.9. `systray`](#29-systray)
   - [2.10. `desktops`](#210-desktops)
   - [2.11. `scratchpad`](#211-scratchpad)
   - [2.12. `prompt`](#212-prompt)
   - [2.13. Reload behavior](#213-reload-behavior)
3. [`bindings.json`: Keyboard and mouse bindings](#3-bindingsjson-keyboard-and-mouse-bindings)
   - [3.1. Binding syntax](#31-binding-syntax)
   - [3.2. `modifiers`](#32-modifiers)
   - [3.3. `keyboard.launch`](#33-keyboardlaunch)
   - [3.4. `keyboard.window`](#34-keyboardwindow)
   - [3.5. `keyboard.wm`](#35-keyboardwm)
   - [3.6. `keyboard.desktop`](#36-keyboarddesktop)
   - [3.7. `keyboard.cycle`](#37-keyboardcycle)
   - [3.8. `keyboard.viewport`](#38-keyboardviewport)
   - [3.9. `mouse.window`](#39-mousewindow)
   - [3.10. `mouse.cycle`](#310-mousecycle)
   - [3.11. Fixed titlebar behavior](#311-fixed-titlebar-behavior)
   - [3.12. What a modal dialog is not allowed to do](#312-what-a-modal-dialog-is-not-allowed-to-do)
4. [`themes/<name>.json`: Theme configuration](#4-themesnamejson-theme-configuration)
   - See [`themes.md`](themes.md) for the full reference
5. [`randr.json`: XRandR output profiles](#5-randrjson-xrandr-output-profiles)
   - [5.1. Top-level fields](#51-top-level-fields)
   - [5.2. `outputs[]` entries](#52-outputs-entries)
   - [5.3. Scope: per-X-screen, not per-`outputs[]`-entry](#53-scope-per-x-screen-not-per-outputs-entry)
   - [5.4. Reload behavior](#54-reload-behavior)
6. [`a11y.json`: Timing and visual-feedback overrides](#6-a11yjson-timing-and-visual-feedback-overrides)
   - [6.1. Fields](#61-fields)
   - [6.2. Reload behavior](#62-reload-behavior)
7. [`rules.json`: Per-window rules](#7-rulesjson-per-window-rules)
   - [7.1. Rule file shape](#71-rule-file-shape)
   - [7.2. Rule entry fields](#72-rule-entry-fields)
   - [7.3. Match fields](#73-match-fields)
   - [7.4. Apply fields](#74-apply-fields)
8. [`session.json`: Session lifecycle hooks](#8-sessionjson-session-lifecycle-hooks)
   - [8.1. Hook arrays](#81-hook-arrays)
9. [`menu.json`: Root desktop menu](#9-menujson-root-desktop-menu)
   - [9.1. Top-level structure](#91-top-level-structure)
   - [9.2. Entry types](#92-entry-types)
   - [9.3. Entry fields reference](#93-entry-fields-reference)
10. [`memguard.json`: Restricted-memory mode configuration](#10-memguardjson-restricted-memory-mode-configuration)
    - [10.1. Configurable fields](#101-configurable-fields)
    - [10.2. Fields this mode never lets `memguard.json` change](#102-fields-this-mode-never-lets-memguardjson-change)
    - [10.3. Restrictions on the active theme](#103-restrictions-on-the-active-theme)
11. [Full examples](#11-full-examples)

For everything that is not a configuration file, namely what IcoWM is,
every command-line option, and restricted-memory mode's run-time
behavior, see [`icowm.md`](icowm.md) instead.

---

## 1. Directory structure

IcoWM looks for its configuration files in the following directory,
evaluated in order:

| Condition                 | Config directory                    |
|---------------------------|-------------------------------------|
| Command-line prefix given | The path passed on the command line |
| `$XDG_CONFIG_HOME` is set | `$XDG_CONFIG_HOME/icowm/`           |
| `$HOME` is set            | `$HOME/.icowm/`                     |
| Neither is set            | `./.icowm/` (current directory)     |

Inside that directory the expected file tree is:

```
~/.icowm/
├── config.json       Base configuration
├── bindings.json     Keyboard & mouse bindings
├── menu.json         Root desktop menu entries
├── randr.json        XRandR output profiles
├── a11y.json         Timing and visual-feedback overrides
├── rules.json        Optional matching rules per-window
├── session.json      Command lists run at start, end, or on config. reload
├── memguard.json     Restricted-memory mode ('-M <mib>') configuration
└── themes/
    └── default.json  Theme file referenced by 'config.json'
```

- **All files are optional**; they fall back to built-in defaults when
  absent.  A file that does exist but cannot actually be read as valid
  JSON is a different matter: IcoWM still falls back to defaults for it,
  the same as if it were absent, but also shows a single warning dialog
  listing every such file from the whole startup (or reload, or the
  moment `menu.json` is actually read, which only happens the first time
  the root menu is opened) together, rather than one dialog per file.
  A missing file is an ordinary, silent choice to use the defaults;
  a broken one is worth knowing about.  A theme file named by
  a correctly-parsed `config.json` is treated the same way as any other
  file for this purpose (a syntax error in it joins that same combined
  dialog); one that is simply not found at all follows the ordinary
  missing-file rule and says nothing by itself, but if a dialog is
  already being shown for some other file's syntax error regardless, it
  adds one further line naming the missing theme file and confirming the
  built-in default is being used instead.
- Theme files are loaded from the `themes/` sub-directory.  The theme
  name field in `config.json` must match the filename without the
  `.json` extension.
- `randr.json`; XRandR hot-plug event handling is always active
  regardless of this file existence.
- None of the files above configure IcoWM's IPC control socket: it has
  no options to set, and is either brought up or, with `-s`,
  deliberately skipped for that run (`icowm.md`, §3.1).  See `icowm.md`
  §5 for where it lives and its full wire protocol.

## 2. `config.json`: Base configuration

Controls the fundamental behavior of the window manager: screens,
virtual desktops, default programs, window management policies, and icon
placement.

### 2.1. `theme`

| Key     | Type   | Default               |
|---------|--------|-----------------------|
| `theme` | string | `""` (built-in theme) |

Name of the theme to load, without the `.json` extension.  The file
`<config_dir>/themes/<theme_name>.json` is looked up inside the
configuration directory.

```json
"theme": "default"
```

As long as it's on the `<config_dir>/themes` directory, it can be
specified a directory tree (always without the `.json` extension).
For example, to use the theme `<config_dir>/modus/operandi/tinted.json`:

```json
"theme": "modus/operandi/tinted",

```

An empty string or an omitted key causes the built-in default theme to
be used.  If not theme key is set, or the theme does not exist, it will
also revert to the built-in default theme.

### 2.2. `topology`

Configures the number of physical screens and the virtual desktops
assigned to each.  **Takes effect at startup only**: changing anything
under `topology` and reloading the configuration has no effect on an
already-running window manager.  For desktop behavior that
*does* reload, namely warp, cycle, and reserved margins, see §2.10
(`desktops`) instead, a deliberately separate, sibling section for
exactly that reason.

#### `topology.screens.count`

| Key                      | Type    | Default |
|--------------------------|---------|---------|
| `topology.screens.count` | integer | `1`     |

Number of physical screens (monitors) to manage.  Maximum is `6`.

#### `topology.screens.desktops[]`

The `desktops` array sits directly under `topology.screens`; there is no
intervening `settings` object.  It accepts two shapes:

**Simple shape** (one screen, desktops listed directly):

```json
"topology": {
    "screens": {
        "count": 1,
        "desktops": [
            { "name": "Desktop 0", "background-color": "#1a1a2e" },
            { "name": "Desktop 1", "background-color": "#16213e" }
        ]
    }
}
```

**Per-screen shape** (each array entry represents one screen):

```json
"topology": {
    "screens": {
        "count": 2,
        "desktops": [
            {
                "count": 4,
                "inaugural": 0,
                "settings": [
                    { "name": "Work",  "background-color": "#1a1a2e" },
                    { "name": "Web",   "background-color": "#16213e" },
                    { "name": "Media", "background-color": "#0f3460" },
                    { "name": "Misc",  "background-color": "#533483" }
                ]
            },
            {
                "count": 1,
                "inaugural": 0,
                "settings": [
                    { "name": "Code", "background-color": "#c0c0c0" }
                ]
            }
        ]
    }
}
```

Which shape is in use is detected from the first array entry alone
(whether it carries `settings`/`count`/`inaugural` fields).
Note that the per-screen shape's per-desktop `settings[]` array (holding
`name`/`background-color`) is a different, unrelated thing from the
`topology.screens.settings` object this schema no longer has: that inner
`settings[]` was never removed, only the outer one that used to wrap
`desktops` was.

Per-screen shape fields:

| Key                           | Type    | Default      | Description |
|-------------------------------|---------|--------------|-------------|
| `count`                       | integer | `4`          | Number of virtual desktops for this screen (or `CONFIG_MAX_DESKTOPS` if that is smaller than `4`).  Maximum is `16`.  Always `1`, regardless of this value, under restricted-memory mode (`-M`); see that mode's section. |
| `inaugural`                   | integer | `0`          | Zero-based index of the desktop shown at startup.  Values out of range fall back to `0`. |
| `layout`                      | object  | see below    | This screen's desktop-grid arrangement; see `topology.screens.desktops[].layout` below. |
| `settings[].name`             | string  | `"Desktop N" | Display name of desktop N. |
| `settings[].background-color` | string  | none         | Root
background color as a hex color `"#RRGGBB"` or `"RRGGBB"`.  Left unset,
a desktop falls back to `theme.desktop.color.background` (`themes.md` §5). |

#### `topology.screens.desktops[].layout`

Optional, and only meaningful in the per-screen shape above (the simple,
one-screen shape has no place to put it).  Interprets the same flat,
zero-based desktop list every desktop already lives in as a grid, so
navigation and the search box can move and label desktops by
row/column, not only by ID.  Nothing about the desktop list itself, or
a desktop's `settings`, changes depending on whether this is present at
all.

**The two linear special cases.**  With `rows` equal to `1` (the
default, and so also what a screen with no `layout` configured at all
effectively has), the desktop grid is exactly one row wide: there is no
"north" or "south" to move to at all, and east/west behave the exact
same way switching desktops always did before `layout` existed.
Symmetrically, with `columns` equal to `1`, the grid is exactly one
column tall: purely north/south, no east or west.  Every screen that
does not deliberately configure more than one row and more than one
column stays in one of these two familiar, linear cases.

```json
"topology": {
    "screens": {
        "count": 1,
        "desktops": [
            {
                "count": 6,
                "inaugural": 0,
                "layout": {
                    "orientation": "horizontal",
                    "corner": "top-left",
                    "rows": 2,
                    "columns": 3
                },
                "settings": [
                    { "name": "Main" },
                    { "name": "Web" },
                    { "name": "Chat" },
                    { "name": "Media" },
                    { "name": "Files" },
                    { "name": "Extra" }
                ]
            }
        ]
    }
}
```

This lays desktops `0`-`5` out as:

```
[0][1][2]
[3][4][5]
```

| Key           | Type    | Default        | Description |
|---------------|---------|----------------|--------------------|
| `orientation` | string  | `"horizontal"` | `"horizontal"` fills one whole row before moving to the next; `"vertical"` fills one whole column before moving to the next. |
| `corner`      | string  | `"top-left"`   | Which corner desktop `0` itself starts at, and so which direction IDs advance from there: `"top-left"`, `"top-right"`, `"bottom-left"`, `"bottom-right"`. |
| `rows`        | integer | `1`            | Number of rows.    |
| `columns`     | integer | `1`            | Number of columns. |

**More examples.**  The same `6` desktops, `orientation: "vertical"`
instead (each column fills before moving to the next one):

```
[0][2][4]
[1][3][5]
```

`orientation: "horizontal"`, `corner: "top-right"` (desktop `0` starts
at the top right instead, IDs advancing leftward across each row):

```
[2][1][0]
[5][4][3]
```

Only `5` of the `6` cells filled (`count: 5`, everything else the
same as the first example): the last cell is a desktop-less gap,
which navigation steps past on its own rather than landing on:

```
[0][1][2]
[3][4][ ]
```

`orientation` and `corner` each default independently the moment
`layout` is present but does not itself name that key.  `rows` and
`columns` default differently depending on how many of the two are
actually named:

- **Neither named**: falls back to the exact same single row as if
  `layout` were absent entirely (`rows: 1`, `columns` equal to `count`).
- **Exactly one named**: the other is computed by ceiling division
  against `count`, not simply defaulted to `1`.  Naming only `columns:
  5` with `4` desktops, say, still means one row of `5` (`4` fits in
  a single row that wide already), but naming only `columns: 1` with
  those same `4` desktops means as many rows as needed to hold every one
  of them in that single column, `4` rows:

  ```
  [0]
  [1]
  [2]
  [3]
  ```

  A `1x1` grid, rejected the moment more than one desktop exists, would
  otherwise be the result of simply defaulting the unnamed key to `1`
  too, the same way `orientation`/`corner` do; ceiling division is what
  makes naming only one axis alone a genuinely useful, "strictly
  vertical" or "strictly horizontal" request instead.
- **Both named**: used exactly as read, validated as a pair the same way
  as always (see below).

`rows * columns` is allowed to exceed `count`, as the gap example above
shows: a legitimate choice, room to add one more desktop later without
reshaping the grid.  Only a `rows`/`columns` combination too small to
ever hold `count` desktops at all, no matter how arranged, or either
value explicitly `0`, negative, non-numeric, or above
`CONFIG_MAX_DESKTOPS`, is rejected outright; logged as a warning,
falling back to the same single-row default as if `layout` were absent.

**Adding and removing desktops at runtime** (the window list's "Add new
desktop"/"Remove last desktop" entries, their keyboard shortcuts, and
the equivalent IPC actions) always appends at the end or removes the
last one, exactly as before `layout` existed, purely a "create it" or
"take it away" action, never switching which desktop is currently being
viewed.  The grid stays consistent with the current count automatically,
growing or shrinking by exactly one row or column at a time as needed:

Starting from the first example above (`2` rows, `3` columns, all `6`
filled):

```
[0][1][2]
[3][4][5]
```

Adding a `7`th:

```
[0][1][2]
[3][4][5]
[6][ ][ ]
```

A brand new row appears below the last one, never a new column, since
`orientation: "horizontal"` already fills columns first, and growing
that same axis would reshuffle where every existing desktop sits
(desktop `3`, at row `1` column `0` today, would otherwise have to jump
to row `0` column `3` to make room).  Growing the untouched axis instead
never moves anything already there; it only ever opens up an entirely
new row (or, for `orientation: "vertical"`, a new column) beyond the
last one.  The same reasoning holds with `columns` set to `2` instead of
`3`:

```
[0][1]
[2][3]
[4][5]
```

Adding a `7`th here also opens a new row, since `columns` is still what
`orientation: "horizontal"` fills first:

```
[0][1]
[2][3]
[4][5]
[6][ ]
```

Adding when a gap cell already exists (the `5`-desktop example earlier,
say) simply fills it; the grid does not grow at all.

Removing always takes the highest-numbered desktop, which fill order
always places in the last row (or column) with any member at all.
Starting from a full `3x3` grid (`9` desktops):

```
[0][1][2]
[3][4][5]
[6][7][8]
```

Removing once (`8` remains) leaves row `2` with two other members still
in it, so the grid's shape is unaffected:

```
[0][1][2]
[3][4][5]
[6][7][ ]
```

Removing once more (`7` remains) empties row `2` down to a single
member, still no shrink:

```
[0][1][2]
[3][4][5]
[6][ ][ ]
```

Removing a third time (`6` remains) takes that last member of row `2`,
and only now, with row `2` genuinely empty, does the grid shrink back
down to `2x3`:

```
[0][1][2]
[3][4][5]
```

Only once that last row (or column) loses its last member does removing
it also shrink the grid back down by one row or column, undoing the
growth above; while it still has another desktop left in it, removing
one leaves the grid's shape unaffected.  Never shrinks below `1` on
either axis, so a screen configured down to a single remaining desktop
always keeps a well-formed, if entirely empty-of-gaps, `1x1` grid.

Ignored entirely under restricted-memory mode (`-M`), which is always
locked to a single desktop regardless of what `layout` (or `count`)
says.

#### `topology.screens.desktops[].viewport`

Optional, and only meaningful in the per-screen shape above.  Gives
every desktop on this screen a pannable area larger than the physical
screen itself, `columns` × `rows` screens wide, scrolled through by
resting the pointer against a screen edge (`desktops.pan-on-edge-hover`,
§2.10) or a dedicated set of keyboard shortcuts (`bindings.json`), with
`_NET_DESKTOP_VIEWPORT` kept in sync for any EWMH-aware pager as the
origin moves.  That origin is remembered independently per desktop, so
switching to another desktop and back leaves the first one exactly
where panning last left it.  Absent entirely, or present with both
`columns` and `rows` left at `1`, a desktop's viewport is exactly the
size of the physical screen: nothing to pan to, the same as every
desktop already behaved before `viewport` existed.

```json
"topology": {
    "screens": {
        "count": 1,
        "desktops": [
            {
                "count": 2,
                "inaugural": 0,
                "viewport": {
                    "columns": 2,
                    "rows": 2
                },
                "settings": [
                    { "name": "Main" },
                    { "name": "Wide" }
                ]
            }
        ]
    }
}
```

Both of this screen's desktops get a `2x2` pannable area, four screens'
worth of space apiece with only one quarter visible at once, navigated
the same way regardless of which one is currently active.

| Key       | Type    | Default | Description |
|-----------|---------|---------|-------------|
| `columns` | integer | `1`     | Width of the pannable area, in whole screens.  Missing on its own defaults to `1` independently of `rows`. |
| `rows`    | integer | `1`     | Height of the pannable area, in whole screens.  Missing on its own defaults to `1` independently of `columns`. |

Either value explicitly `0`, negative, non-numeric, or above `16`
rejects the whole `viewport` object outright, logged as a warning, and
falls back to `1x1` (panning disabled) entirely, never a partial
fallback of just the one bad axis.  Ignored entirely under
restricted-memory mode (`-M`), which always keeps a `1x1` viewport
regardless of what `viewport` says, the same way it already locks
`count` down to a single desktop.

#### Notes on configuration reload

The exception is on `topology` parameters themselves (screen count, and
how many desktops each screen has, along with each desktop's
`name`/`background-color`).  Changing any of these and reloading has no
effect on an already-running window manager.  This does not extend to
the separate, sibling `desktops` section (`show-overlay`,
`warp-on-edge-drag`, `wrap-at-bounds`, `margins`) despite the similar
name.  That one describes navigation behavior and reserved space, not
topology, and does take effect on reload, same as everything else.
Every other field in `config.json`, and every other configuration file
(`bindings.json`, `menus.json`, `randr.json`, `rules.json`,
`session.json`, and the active theme), does take effect on reload, as
documented throughout this file.  Growing or shrinking the number of
screens or desktops at runtime would mean deciding what happens to
whatever clients, focus, and EWMH state already live on a desktop being
removed, none of which reload does today; restart the window manager to
pick up a `topology.*` change instead.

### 2.3. `programs`

Associates application categories with the executables IcoWM will launch
for the corresponding keyboard shortcuts.

| Key                     | Type   | Default     | Description          |
|-------------------------|--------|-------------|----------------------|
| `programs.terminal`     | string | `"xterm"`   | Terminal emulator.   |
| `programs.launcher`     | string | `"gmrun"`   | Application launcher; see `prompt` (§2.12) for the built-in alternative that can replace spawning this entirely. |
| `programs.file-manager` | string | `"pcmanfm"` | File manager.        |
| `programs.editor`       | string | `"gvim"`    | Text editor.         |
| `programs.web-browser`  | string | `"firefox"` | Web browser.         |

```json
"programs": {
    "terminal":     "xterm",
    "launcher":     "gmrun",
    "file-manager": "pcmanfm",
    "editor":       "gvim",
    "web-browser":  "firefox"
}
```

### 2.4. `windows`

Controls window behavior: gravity, edge snapping, resize grips, focus
policy, and placement policy.

#### `windows.gravity`

| Key               | Type   | Default        |
|-------------------|--------|----------------|
| `windows.gravity` | string | `"north-west"` |

Fallback window gravity, used only for a client that never declares one
via `WM_NORMAL_HINTS`'s `win_gravity` field.  Gravity governs which
corner (or edge, or the center) of a window's frame stays visually fixed
when its decoration is toggled on or off, and when it is resized through
`icowm-msg`'s `resize_client` action without an accompanying move.

A client that declares a `win_gravity` always takes precedence over this
value, at any point in that client's life, not only when it is first
mapped: several common toolkits (e.g., `xterm`'s `Xt` shell,
LibreOffice's `VCL`) only finalize their real size hints a moment after
their first map, once fonts and chrome are ready, and that later update
commonly carries an explicit `win_gravity`, typically `NorthWestGravity`
or `StaticGravity`, i.e., "do nothing special", that this field can
never override.  This is deliberate, ICCCM-mandated behavior, not
a limitation specific to IcoWM: this field's only practical effect is on
the comparatively rare client that never declares a `win_gravity` at
all, ever.  This matches how other ICCCM-compliant window managers treat
this same field: none of them offer a way to force a client's gravity to
something the client itself did not request.  Per-application placement
rules (`rules.json`) are the tool for overriding where a specific
application ends up on screen, not this field.

Accepted `windows.gravity` values: `"north-west"`, `"north"`,
`"north-east"`, `"east"`, `"south-east"`, `"south"`, `"south-west"`,
`"west"`, `"center"`, `"static"`.

#### `windows.edges`

| Key                  | Type    | Default | Description |
|----------------------|---------|---------|-------------|
| `edges.snap.window`  | integer | `6`     | Attraction distance in pixels toward another window's edge.  When a window being dragged comes within this many pixels of another window's border, it snaps into alignment with it.  Set to `0` to disable this specifically. |
| `edges.snap.screen`  | integer | `6`     | Attraction distance in pixels toward the screen's edge.  Set to `0` to disable this specifically. |
| `edges.resistance`   | integer | `20`    | How many pixels of deliberate extra drag it takes for a horizontally or vertically maximized window's locked axis to actually start changing while being interactively resized with the mouse.  Dragging back under this same threshold before releasing restores the maximized axis, reversibly, for the whole drag.  Set to `0` to remove the axis lock entirely, letting the maximized axis change on the very first pixel of drag. |

```json
"windows": {
    "edges": {
        "snap": {
            "window": 6,
            "screen": 6
        },
        "resistance": 20
    }
}
```

#### `windows.move-step`

| Key                 | Type    | Default |
|---------------------|---------|---------|
| `windows.move-step` | integer | `10`    |

Keyboard movement step in pixels.  Each key press that moves the focused
window changes its position by this amount.  Values lower than `1` are
treated as `1`.

#### `windows.resize-step`

| Key                   | Type    | Default |
|-----------------------|---------|---------|
| `windows.resize-step` | integer | `20`    |

Keyboard resize step in pixels.  Each key press that resizes the focused
window changes the size of the axis being resized by this amount.
Values lower than `1` are treated as `1`.

This applies only to a window that publishes no usable size hints.  A
window that does publish them, a terminal emulator sizing itself in
whole character cells being the usual case, is resized by whole
increments of its own instead, snapped from its base (or minimum) size,
so that it never lands on a size it cannot actually render.

#### `windows.show-geom`

| Key                 | Type    | Default |
|---------------------|---------|---------|
| `windows.show-geom` | boolean | `true`  |

When `true`, geometry when moving (mouse drag) or position when resizing
(with mouse drag) is shown in the center of the window.

#### `windows.solid-drag`

| Key                  | Type    | Default |
|----------------------|---------|---------|
| `windows.solid-drag` | boolean | `true`  |

When `true`, the real window itself is moved or resized live, redrawn on
every pointer motion for the whole duration of a mouse drag.  When
`false`, a plain outline stand-in is drawn instead, and the real window
is only ever moved or resized once, the moment the mouse button is
released; this avoids repainting whatever the client itself draws on
every single motion event, which can matter on genuinely slow hardware
if the client redraws something expensive on each resize.  Icon drags
are always solid regardless of this setting, moving just the small icon
window being cheap enough on its own that the distinction would add
nothing.  Restricted-memory mode (`memguard.json`) always runs with this
`false`, and does not expose the key for the person to override; see
§10.2 below.

#### `windows.focus`

| Key                        | Type    | Default   | Description |
|----------------------------|---------|-----------|-------------|
| `focus.policy`             | string  | `"click"` | Focus policy. `"click"` requires a click to focus; `"sloppy"` focuses whichever window is under the pointer. |
| `focus.focus-new`          | boolean | `true`    | When `true`, newly mapped windows receive focus automatically. |
| `focus.raise`              | boolean | `false`   | When `true`, a window is also raised when it gains focus by pointer or wheel. |
| `focus.delay-ms`           | integer | `250`     | Milliseconds the pointer must sit still over a window before it is focused. Only takes effect under `"sloppy"`; has no effect under `"click"`. |

```json
"windows": {
    "focus": {
        "policy": "click",
        "focus-new": true,
        "raise": false,
        "delay-ms": 250
    }
}
```

`raise` only ever adds raising; it never takes it away.  Most ways of
focusing a window raise it regardless, because raising is part of what
was asked for: clicking a window, choosing it from the window list or
the search box, cycling to it with the keyboard, activating it from
another application, and a per-window rule that focuses it all bring it
to the front whatever this is set to.

What it governs is the handful of ways a window can gain focus without
anyone asking for it to come forward: the pointer entering it under
`"sloppy"` focus, and the wheel over the desktop shading or unshading
it.  With `false` those move the keyboard without disturbing the
stacking order, so a window can be typed into while staying under
whatever the person had deliberately placed above it.

It therefore makes no observable difference under `"click"` focus, where
none of those paths is reached.

`delay-ms` likewise only has an effect under `"sloppy"` focus.  With it
left at the default of `250`, the pointer entering a window focuses it
immediately, same as always.  Set above `0`, the pointer has to sit
still over the window for that many milliseconds before it actually
gains focus; leaving early cancels it, so passing through a window on
the way to another one never steals focus along the way.  Under
`"click"` focus this key is simply never consulted at all.

#### `windows.placement`

| Key                       | Type              | Default     | Description |
|---------------------------|-------------------|-------------|-------------|
| `placement.policy`        | string            | `"smart"`   | How newly mapped windows are placed. |
| `placement.monitor`       | string or integer | `"pointer"` | Which physical monitor a placement decision targets, on a surface with more than one. |
| `placement.group-related` | boolean           | `false`     | Cluster windows of the same application together. |

Accepted placement policy values:

| Value           | Behavior |
|-----------------|----------|
| `"cascade"`     | Places windows in a stepped diagonal sequence. |
| `"centered"`    | Centers the window on the screen. |
| `"under-mouse"` | Places the window under the current pointer position. |
| `"smart"`       | Finds the position that minimizes overlap with existing windows. |
| `"manual"`      | Shows an outline following the pointer and waits for a click to say where the window goes. |

Under `"manual"` a newly mapped window is held back rather than shown:
an outline of it follows the pointer, and the window appears where that
outline stands the moment a button is pressed.  Windows opening
together are asked about one at a time, in the order they opened, each
held back until the one before it is settled.

The keyboard answers too, with the same keys that move a window already
on screen: the arrow keys move the outline by `windows.move_step` and
carry the pointer along with it, `Return` settles the window where it
stands, and `Escape` gives up and leaves it where `"smart"` had already
chosen.  Every other key does nothing while the question is open.

Five seconds of silence do the same thing as `Escape`, so a window
opened by something running in the background never holds the pointer
and the keyboard for good.  The five seconds are counted from the last
answer rather than from when the question opened, so aiming slowly, by
either device, is never mistaken for ignoring it.

The pointer and the keyboard are both held while the question stands,
so nothing else answers to either until it is settled.  A window that
requests a position itself, a dialog centered over its parent, a dock,
a scratchpad window, and a window asking to start iconified are all
placed the way they always are and never asked about.

Accepted placement monitor values (only meaningful on a surface made up
of more than one physical monitor sharing the same combined X screen;
has no effect otherwise):

| Value       | Behavior |
|-------------|----------|
| `"active"`  | Targets whichever monitor holds the current desktop's active window, falling back to `"pointer"` when there is none. |
| `"pointer"` | Targets whichever monitor the pointer is currently on: not necessarily where on that monitor the pointer actually is, only which one it is on, so the window can still land far from the cursor within it depending on `placement.policy`. |
| `"primary"` | Always targets the monitor RandR reports as primary. |

`placement.monitor` also accepts a plain number instead of one of the
strings above, a zero-based index into the surface's monitor list, e.g.,
`"monitor": 1` targets that specific monitor directly.  Falls back to
monitor 0 if the surface does not have that many, logging a warning, the
same as `systray.monitor.index` and `rules.json`'s `apply.monitor`.

Transient (dialog) windows are always centered over their parent window,
regardless of `placement.policy`, and windows clustered by
`group-related` (below) are always placed next to the sibling they are
grouped with; both also always target whichever monitor that parent or
sibling is actually on, regardless of `placement.monitor`, since neither
case is about picking a monitor for a window with no better signal to go
on: they already have one.

Set `group-related` to `true` and a newly mapped window whose
`WM_CLIENT_LEADER` (or, failing that, its `WM_HINTS` window group)
matches another currently visible window's is placed offset from that
group instead of running the policy above for it, i.e., a second, third,
fourth, ... window opened by the same application lands next to the
others instead of wherever `policy` would otherwise put it.  It is
`false` by default, so every window always uses `policy`, with no
special-casing for related ones.

This is named after what it actually groups by (an application's stated
client/window group), not by `WM_CLASS`, since not every application
that opens several related windows gives them all the exact same class
name.

```json
"windows": {
    "placement": {
        "policy": "smart",
        "monitor": "pointer",
        "group-related": false
    }
}
```

### 2.5. `icons`

#### `icons.show-geom`

| Key               | Type    | Default |
|-------------------|---------|---------|
| `icons.show-geom` | boolean | `false` |

When `true`, geometry when moving (mouse drag) is shown in the center of
the icon.

#### `icons.placement`

Controls how iconified windows are laid out on the desktop.

| Key                      | Type   | Default   | Description |
|--------------------------|--------|-----------|-------------|
| `icons.placement.policy` | string | `"smart"` | How new icons are placed. |

Accepted icon placement values:

| Value        | Behavior                                                    |
|--------------|-------------------------------------------------------------|
| `"bottom"`   | Icons fill the bottom row of the screen from left to right. |
| `"top"`      | Icons fill the top row from left to right.                  |
| `"left"`     | Icons fill the left column from top to bottom.              |
| `"right"`    | Icons fill the right column from top to bottom.             |
| `"smart"`    | Icons are placed in the first available free slot.          |
| `"in-place"` | Each icon appears over its window's top-left corner.        |

Under `"in-place"` an icon takes the spot the window itself occupied, so
iconifying looks like the window turning into its icon rather than the
icon appearing somewhere else.  When another icon already sits there,
spots are tried outward from that corner a grid step at a time, nearest
first, so a taken corner costs the icon as little distance from its
window as the desktop allows.  On a desktop crowded enough that nothing
near the corner is free, `"smart"` answers instead.

The four edge policies count their slots from a screen edge; this one
counts from wherever the window happened to be, so its icons do not line
up with theirs.

```json
"icons": {
    "placement": {
        "policy": "smart"
    }
}
```

### 2.6. `shutdown` / `fortune`

| Key                                     | Type    | Default     |
|-----------------------------------------|---------|-------------|
| `shutdown.enable-emergency-shortcut`    | boolean | `false`     |
| `shutdown.timeout-seconds`              | integer | `15`        |
| `fortune.is-enabled`                    | boolean | `true`      |
| `fortune.command`                       | string  | `"fortune"` |

`shutdown` groups every setting about how the window manager itself
shuts down: the hardcoded emergency exit shortcut, and the wait the
normal quit action performs.  `fortune` gates a normal, configurable
shortcut instead (see `keyboard.wm.fortune` in §3.5): unlike the
emergency exit, there is no risk in triggering it by accident, so it has
no reason to be fixed in place the same way the emergency exit is.

`shutdown.enable-emergency-shortcut`, when `true`, activates
`Ctrl+Mod1+Backspace`.  Not configurable via `bindings.json` like
a normal keybinding, and while enabled, that exact key combination
cannot be reused by any `bindings.json` entry, whether that would happen
intentionally or by accident: any such binding is ignored (with
a warning logged) so the emergency exit always keeps
`Ctrl+Mod1+Backspace` to itself.  Set to `false` (the default) to
disable the shortcut entirely, for example on systems where the key
combination might be triggered accidentally.

This shortcut terminates the window manager immediately: no confirmation
dialog, no menu, none of the coordinated wait `shutdown.timeout-seconds`
below governs for the normal quit action, and not even the exit session
hooks a normal quit or an external `SIGTERM` otherwise runs.  This is
deliberate, not an oversight.  The emergency exit exists specifically
for situations where the window manager itself might be unresponsive or
in some broken state, so it is kept to the smallest, most direct action
possible: a signal sent to its process, detected the very next time its
main loop gets to check for one.  Every one of the things this shortcut
skips (a dialog, a menu, the coordinated client-closing wait) depends on
that same main loop and its rendering still working; adding any of them
back in as a required step, even one that can itself be canceled, would
make the emergency exit only as reliable as whatever it is that might be
the very reason someone is reaching for it in the first place.  For the
same reason, this shortcut is detected ahead of every other keyboard
handling in the window manager, including whatever any currently open
dialog or menu would otherwise do with that same key combination, so it
keeps working even while one of those has the keyboard grabbed.

`shutdown.timeout-seconds` applies only to the normal quit action (the
"Quit" keybinding and its confirmation dialog): once confirmed, every
managed client is first asked to close by itself (the same
`WM_DELETE_WINDOW` request closing one window individually already
sends, so an application with unsaved changes gets the same chance to
warn the user), and the window manager waits up to this many seconds for
all of them to actually close before forcing whichever ones are still
open closed regardless and exiting anyway.  A value of `0` skips the
wait entirely and force-closes every remaining client right away.  Never
consulted by `shutdown.enable-emergency-shortcut` above, for the reasons
already covered.

When `fortune.is-enabled` is `true`, its keyboard shortcut (see
`keyboard.wm.fortune`, §3.5) opens a small dialog running
`fortune.command` through a shell and showing its output, or, if that
command produces none (not installed, an empty database, and so on), an
in-joke message suggesting it should be.  `fortune.command` is run
literally, exactly as configured, so it may be any shell command line,
not just a bare executable name; e.g., `"fortune -s"` for short-only
fortunes, `"fortune -o"` for offensive ones, or a specific fortune
database or language, whatever a person's installed `fortune` supports.
Purely for fun; harmless to leave off, and harmless to turn on.

```json
"shutdown": {
    "enable-emergency-shortcut": true,
    "timeout-seconds": 15
},
"fortune": {
    "is-enabled": true,
    "command": "fortune"
}
```

### 2.7. `startup-notification`

| Key                                    | Type    | Default |
|----------------------------------------|---------|---------|
| `startup-notification.timeout-seconds` | integer | `15`    |

How long a startup-notification sequence (the busy cursor shown while
a launched application is starting up, see the freedesktop.org Startup
Notification specification) waits before being expired automatically.
Not every application is startup-notification aware, so this is what
keeps the busy cursor from staying on indefinitely when a launched
process never signals that it is ready.  Raise it for applications that
are slow to show their first window (some office suites, for example);
lower it if 15 seconds feels like it lingers too long for the
applications actually launched day to day.

```json
"startup-notification": {
    "timeout-seconds": 20
}
```

### 2.8. `menu`

| Key                      | Type   | Default         |
|--------------------------|--------|-----------------|
| `menus.root.position`    | string | `"under-mouse"` |
| `menus.windows.position` | string | `"under-mouse"` |

Controls where a menu appears when it is opened by a means with no
screen position, such as a keyboard shortcut, one setting per menu type:
`root` is the desktop context menu (`menu.json`, opened by
`keyboard.wm.menus.root`, see §3.5); `windows` is the menu listing every
window on every desktop (opened by `keyboard.wm.menus.windows`).

Accepted values:

| Value              | Behavior |
|--------------------|----------|
| `"center"`         | Always opens the menu in the center of the screen. |
| `"under-mouse"`    | Opens the menu under the current mouse pointer position instead, matching the naming of `windows.placement.policy`. |
| `"top-left"`       | Pins the menu to the top-left corner of the current desktop's work area. |
| `"top-right"`      | Pins the menu to the top-right corner of the current desktop's work area. |
| `"bottom-left"`    | Pins the menu to the bottom-left corner of the current desktop's work area. |
| `"bottom-right"`   | Pins the menu to the bottom-right corner of the current desktop's work area. |

This setting has no effect when a menu is opened with the mouse (e.g.,
right-click on the desktop for the root menu), since it already opens
under the pointer in that case.

```json
"menu": {
    "root": {
        "position": "under-mouse"
    },
    "windows": {
        "position": "under-mouse"
    }
}
```

### 2.9. `systray`

| Key                     | Type    | Default           |
|-------------------------|---------|-------------------|
| `systray.is-enabled`    | boolean | `true`            |
| `systray.reserve-space` | boolean | `false`           |
| `systray.avoid-overlap` | boolean | `true`            |
| `systray.margins`       | object  | see below         |
| `systray.position`      | string  | `"top-left"`      |
| `systray.monitor`       | object  | see below         |
| `systray.order`         | string  | `"left-to-right"` |
| `systray.layer`         | string  | `"below"`         |

Built-in systray dock.  `is-enabled` turns it on, and `position` (one of
`"top-left"`, `"top-right"`, `"bottom-left"`, or `"bottom-right"`)
selects which corner it docks in; which area that corner is measured
against is `monitor`'s job, described next.

`reserve-space` controls whether the tray publishes its
`_NET_WM_STRUT_PARTIAL`/`_NET_WM_STRUT`, reserving its on-screen area
the same way an external panel or dock does, so maximized windows and
this window manager's placement logic both leave it alone, per the
specification's recommendation for a docking area, a taskbar, or
a panel.  `false` by default: an explicit `{0, 0, 0, 0}` strut,
reserving nothing, the same as if the tray were not there at all for
placement purposes.  Set to `true` for the tray to reserve its space
instead, e.g., for a `layer` other than `"above"` or `"overlay"`, where
nothing else already keeps windows off the tray visually.

`avoid-overlap` controls whether `windows.placement`'s `"smart"` mode
avoids landing a newly mapped window on top of the tray, if possible.
`true` by default.  Has no effect while `reserve-space` is `true`: the
tray's on-screen area is already excluded from the region smart
placement searches in that case, so no candidate position could ever
land on it regardless of this setting.  Only meaningful, then, for
a tray configured strutless (`reserve-space` `false`), for every
candidate position smart placement scores is checked against the tray's
current on-screen rectangle the same way it already checks every other
visible client, so a new window still tends to avoid sitting on top of
the tray even though the tray itself reserves no space for that to be
guaranteed.  This affects placement scoring only; the tray is not a real
client, so it still cannot be moved, iconified, or otherwise acted on
the way an actual window can.

`margins` (an object with `top`/`right`/`bottom`/`left` integers, all
`0` by default) adds extra reserved space on top of whatever the tray's
actual size and position already reserve, mirroring `desktops.margins`
(§2.10) exactly, including that it is not restricted to whichever edge
the tray currently docks at: a `left` or `right` value still reserves
space on that side even while the tray itself sits at the top or bottom.
Has no effect while `reserve-space` is `false`.

```json
"systray": {
    "reserve-space": false,
    "avoid-overlap": true,
    "margins": { "top": 0, "right": 0, "bottom": 0, "left": 0 }
}
```

`monitor` selects which physical monitor `position`'s corner is measured
against, on a surface made up of more than one sharing the same combined
X screen; it has no effect otherwise.  It is an object with an `anchor`
field and, only when `anchor` is `"index"`, an `index` field:

| Value       | Behavior |
|-------------|----------|
| `"surface"` | Measures `position` against the whole combined surface, exactly as if there were only one monitor (default). |
| `"primary"` | Measures `position` against whichever monitor RandR reports as primary. |
| `"index"`   | Measures `position` against `monitor.index` specifically, a zero-based index into that surface's monitor list.  Falls back to monitor `0` if it does not exist, logging a warning. |

```json
"systray": {
    "is-enabled": true,
    "position": "top-right",
    "monitor": { "anchor": "primary" }
}
```

Only one tray dock ever exists at a time, regardless of `monitor`: the
`_NET_SYSTEM_TRAY_Sn` manager selection this implements is one per
screen by its specification (see below), so a genuinely independent tray
dock per monitor, each accepting its icons, is not something any
implementation of this protocol can offer, IcoWM included.  `monitor`
only changes which single monitor the one dock IcoWM does provide sits
on.

The key `order` controls where a newly docked icon is placed relative to
the ones already there: `"left-to-right"` appends it after the last
icon, `"right-to-left"` inserts it before the first, and `"ascending"`
/ `"descending"` instead keep the whole row continuously sorted
alphabetically ('A-Z' or 'Z-A') by each icon's window class name,
ignoring insertion order entirely.

The key `layer` controls where the dock sits in the stacking order:
`"below"` (the default) keeps it behind every normal client window,
`"above"` keeps it above normal windows but still lets a fullscreen
window cover it while that window holds focus, the same way a fullscreen
window covers a taskbar or panel in most desktop environments, and
`"overlay"` keeps it above absolutely everything, including fullscreen
windows.

When enabled, IcoWM acquires the `_NET_SYSTEM_TRAY_Sn` manager selection
on startup and embeds icon windows that request docking via the
freedesktop.org System Tray Protocol together with the XEMBED handshake;
the dock window stays hidden while no icons are docked.  Toggling
`is-enabled` off and back on via a configuration reload releases and
re-acquires the selection immediately without losing already-docked
icons: the dock window and its icons persist in the background while
disabled, just hidden and not accepting new dock requests, so they
reappear as soon as it is re-enabled instead of only newly-launched tray
icons showing up.

If another tray manager (e.g., `tint2`'s built-in tray) already owns the
selection, IcoWM's built-in tray steps aside and stays disabled for that
session.  Only one tray manager can be active at a time, same as with
any other implementation of this protocol.

Icons are embedded with the window manager's default visual rather than
a negotiated 32-bit ARGB one, so icons relying on real alpha
transparency may show a solid background instead of blending into the
tray.

#### `systray.clock`

| Key                        | Type    | Default   |
|----------------------------|---------|-----------|
| `systray.clock.is-enabled` | boolean | `true`    |
| `systray.clock.format`     | string  | `"%a %R"` |

An optional clock drawn inside the systray dock.  `is-enabled` turns it
on; when it is the only reason the tray would otherwise stay hidden (no
icons docked), the tray still shows with just the clock.  Where it is
positioned and aligned is shared with `systray.battery` below; see
`systray.text`.

`format` is a `strftime(3)` format string, interpreted in the system's
local time zone.  A few common examples:

| `format`     | Looks like         |
|--------------|--------------------|
| `"%a %R"`    | `Fri 14:07`        |
| `"%H:%M"`    | `14:07`            |
| `"%H:%M:%S"` | `14:07:32`         |
| `"%F %R"`    | `2026-08-07 14:07` |
| `"%a %d %b"` | `Fri 07 Aug`       |

The clock redraws itself once per second while enabled; a `format`
string without `%S` or other sub-minute fields simply redraws the same
text every second, which is harmless.

#### `systray.battery`

| Key                                  | Type    | Default  |
|--------------------------------------|---------|----------|
| `systray.battery.is-enabled`         | boolean | `false`  |
| `systray.battery.threshold.charged`  | integer | `100`    |
| `systray.battery.threshold.low`      | integer | `20`     |
| `systray.battery.threshold.critical` | integer | `5`      |
| `systray.battery.backend.type`       | string  | `"acpi"` |
| `systray.battery.backend.number`     | integer | `0`      |
| `systray.battery.poll-seconds`       | integer | `30`     |

An optional battery/AC status drawn inside the systray dock, read
directly from the kernel rather than through any external daemon.
`backend.type` selects which kernel interface to read: `"acpi"` reads
`/sys/class/power_supply` (the modern, near-universal interface on
Linux), and `"apm"` reads the older `/proc/apm` for hardware or kernels
without ACPI.  `backend.number` selects which battery to read when
a system has more than one (0-indexed, e.g., `1` for `BAT1`); it is
ignored under `"apm"`, which only ever exposes one aggregate battery
regardless of how many cells the system actually has.  `poll-seconds` is
how often the status is re-read; a percentage does not need per-second
freshness the way a clock does, so raise it to poll less often (saving
the handful of file reads each poll costs) or lower it for a battery
that drains quickly enough that 30 seconds feels stale.

The status text's exact shape depends on both AC power and how the
battery's charge compares to `threshold`:

| State                                          | Text         |
|------------------------------------------------|--------------|
| On battery, above `low`                        | `"X%"`       |
| On battery, at/below `low`                     | `"X%!"`      |
| On battery, at/below `critical`                | `"X%!!"`     |
| On AC, not fully charged                       | `"X% AC"`    |
| Fully charged (`charged` or above), on battery | `"Full"`     |
| Fully charged, on AC                           | `"Full, AC"` |
| No battery found for `backend`                 | `"N/A"`      |

A battery counts as "fully charged" once its percentage reaches
`threshold.charged`, regardless of what the kernel itself reports as its
charging state: some hardware never reports "full" even sitting at 100%
on AC power, so going by the percentage alone reads correctly across
more machines than trusting the kernel's status string would.  The
status is re-read every 30 seconds; a percentage does not need
per-second freshness the way a clock does.  Where it is positioned and
aligned is shared with `systray.clock` above; see `systray.text`.

#### `systray.text`

| Key                     | Type            | Default                  |
|-------------------------|-----------------|--------------------------|
| `systray.text.order`    | array of string | `[ "clock", "battery" ]` |
| `systray.text.position` | string          | `"left"`                 |

Shared placement for the clock and battery status text: which of the two
show, in what left-to-right order.  `order` lists the enabled items to
show, by name (`"clock"` and/or `"battery"`); an item absent from this
list never shows even if `is-enabled` is `true`, and one with
`is-enabled` set to `false` is skipped even when listed here.  Both
entries are optional; an empty list shows neither, regardless of their
individual `is-enabled` settings.

`position` (`"left"` or `"right"`) controls whether the whole text block
sits before or after the icons, in dock order; it does not affect which
corner of the screen the tray itself sits in, which is still
`systray.position` above.  How the text looks once shown (the gap
between the two items, and their vertical alignment within the tray) is
a theme setting rather than a behavior one; see `systray.text` in the
theme documentation (`themes.md` §4).

```json
"systray": {
    "is-enabled": false,
    "position": "top-right",
    "order": "left-to-right",
    "layer": "below",
    "clock": {
        "is-enabled": true,
        "format": "%a %R"
    },
    "battery": {
        "is-enabled": true,
        "threshold": {
            "charged": 100,
            "low": 20,
            "critical": 5
        },
        "backend": {
            "type": "acpi",
            "number": 0
        }
    },
    "text": {
        "order": [ "battery", "clock" ],
        "position": "right"
    }
}
```

### 2.10. `desktops`

Desktop-navigation and reserved-space behavior: whether the active
desktop's name briefly overlays the screen on switch, whether switching
between desktops behaves cyclically at the two ends, whether dragging
a window past a screen edge switches desktops with it, and how much of
every desktop's area stays reserved regardless of what any client itself
publishes.  A sibling of `topology` (§2.2) at the root of `config.json`,
not nested inside it: deliberately so, since unlike `topology`,
everything here **does** take effect on a configuration reload.

| Key                 | Type    | Default | Description |
|---------------------|---------|---------|-------------|
| `show-overlay`      | boolean | `true`  | Whether a small notification popup is displayed in the center of the screen for approximately 400 ms whenever the active virtual desktop changes.  The popup shows the desktop index and name in the format `[index] -- Name`, or just `[index]` when the desktop has no name; with a `topology.screens.desktops[].layout` genuinely more than one row configured, `(row,column)` is appended after the index the same way it is in the search box and window lists. |
| `notify-activity`   | boolean | `true`  | Whether a client becoming urgent on a desktop other than the one currently visible on its surface shows an informational dialog naming that desktop (`Detected activity on desktop [index] -- Name`, with a surface disambiguator appended when more than one surface is managed).  A client urgent on the currently visible desktop already gets its titlebar blink instead (see `urgency.*` in `a11y.json`, §6), which this never duplicates. |
| `warp-on-edge-drag` | boolean | `true`  | While dragging a window or icon to move it, holding the pointer against a screen edge switches to the adjacent desktop in that direction (left/right always; top/bottom too, once a `layout` with more than one row is configured), cursor and dragged window or icon both carried across, after a short delay.  Meaningless with only one desktop. |
| `pan-on-edge-hover`  | boolean | `true`  | With no drag in progress, merely resting the pointer against a screen edge pans the current desktop's viewport toward that edge instead, after a short delay, repeating for as long as the pointer stays held there.  Meaningless on a screen whose `topology.screens.desktops[].viewport` is `1x1` (panning not configured); an edge held during a drag is `warp-on-edge-drag` above's to answer instead, never this one's. |
| `wrap-at-bounds`    | boolean | `true`  | Whether switching past the edge of the desktop grid, in any of the four compass directions, however triggered (keyboard binding, mouse scroll, an edge drag, or otherwise), wraps around to the other end of that same row or column, rather than stopping there.  Meaningless with only one desktop. |
| `margins.top`       | integer | `0`     | Extra space reserved at the top of every desktop's workarea, in pixels, on every screen. |
| `margins.right`     | integer | `0`     | Extra space reserved on the right, in pixels. |
| `margins.bottom`    | integer | `0`     | Extra space reserved at the bottom, in pixels. |
| `margins.left`      | integer | `0`     | Extra space reserved on the left, in pixels. |

`margins` adds on top of whatever space a client already reserves for
itself via `_NET_WM_STRUT`/`_NET_WM_STRUT_PARTIAL` (a panel or dock,
say) rather than overriding it: the two are meant to coexist, not
compete.  It exists for a program that reserves screen space without
publishing either property itself (a desktop widget like Conky is the
classic example): configuring a margin here reserves that space for it,
the same way maximizing a window or its initial placement already
respects a panel's published strut.  `margins` applies identically to
every desktop on every screen; there is no per-desktop or per-screen
override.

```json
"desktops": {
    "show-overlay": true,
    "notify-activity": true,
    "warp-on-edge-drag": true,
    "pan-on-edge-hover": true,
    "wrap-at-bounds": true,
    "margins": {
        "top": 0,
        "right": 0,
        "bottom": 0,
        "left": 0
    }
}
```

### 2.11. `scratchpad`

A single dedicated client, launched on demand and toggled visible/hidden
instead of iconified/restored, the same way a dropdown terminal works in
other window managers.  Hiding it never terminates the underlying
process: the same client is shown again next time, with whatever state
it was left in (a shell's scrollback, say), until it exits by itself, at
which point the next toggle launches a fresh one.  Never appears in
`list_clients` (IPC), the window cycle, or the window-list menu; cannot
be decorated, un-pinned, moved to a different layer, iconified, shaded,
moved, or resized, by any means (keybind, mouse, menu, or IPC);
`Alt+Space` does nothing on it; loses input focus by hiding itself
automatically; and is skipped by `rearrange_desktop`.  `toggle
fullscreen` is deliberately left alone: nothing above prevents it.

**Reloading configuration never affects a scratchpad client already
alive.** Every field below (`command`, `edge`, `width`/`height`,
`ignore-margins`) is only ever read the moment a fresh scratchpad is
actually launched, never while the current one is still around, hidden
or shown.  Changing `command` and reloading, say, has no effect on an
already-running scratchpad session at all; it only takes effect the next
time one gets launched, which means the current client's process (the
shell inside it, typically) has to exit first, since hiding it is not
enough to release it.

`command` is not limited to a terminal: any graphical program works, as
long as it actually opens a window at all rather than running as a pure
command-line tool with no display, e.g., `"gvim ~/docs/my_notes.txt"` to
always have the same notes file one toggle away, exactly as much as
a shell would be.

| Key                     | Type                | Default                    | Description |
|--------------------------|--------------------|----------------------------|-------------|
| `is-enabled`             | boolean            | `true`                     | Enables the toggle action; a `toggle_scratchpad` command or its keybind does nothing at all while this is `false`. |
| `command`                | string             | `"xterm -fg black -bg ivory -cr black"` | Launched the first time the toggle runs with no scratchpad client yet.  Whatever this launches is forced to the `WM_CLASS` `"Scratchpad"` once it maps, regardless of what it sets (or fails to set) itself, so any command works here, not only ones able to pass their `-class`; see the note above on what kind of command this can be. |
| `edge`                   | string             | `"top"`                    | Screen edge it slides out from: `"top"`, `"bottom"`, `"left"`, or `"right"`. |
| `width`                  | integer or `"max"` | `"max"`                    | Always-applied width, in pixels, or `"max"` for however much of that axis is actually available, so a fixed resolution never has to be hard-coded. |
| `height`                 | integer or `"max"` | `200`                      | Always-applied height, in pixels, or `"max"` (see `width` above). |
| `ignore-margins`         | boolean            | `false`                    | `false` places it the same way an ordinary client already respects `desktops.margins` and the systray's reserved space; `true` lets it use the full edge regardless, e.g., a top-edge scratchpad sliding out from underneath an external panel that already reserves that same space rather than starting just below it. |

Its border is themed separately from every other window, since it never
has any other decoration; see `themes.md` §10.

```json
"scratchpad": {
    "is-enabled": true,
    "command": "xterm -fg black -bg ivory -cr black",
    "edge": "top",
    "width": "max",
    "height": 200,
    "ignore-margins": false
}
```

### 2.12. `prompt`

A single, always-centered text field for typing and launching a command
directly, with no application listing, no fuzzy matching, and no cache
of any kind, unlike `programs.launcher` (§2.3): a mistyped or missing
command shows an informational dialog (never a blocking warning or
error) rather than doing nothing silently or interrupting further than
necessary, and a successful one closes the box right away.

| Key          | Type    | Default | Description |
|--------------|---------|---------|-------------|
| `is-enabled` | boolean | `false` in normal mode; `true` in restricted-memory mode (`-M`) | When `true`, the `launcher` keyboard shortcut opens this built-in box instead of spawning `programs.launcher`.  Restricted-memory mode defaults this to `true` specifically to avoid that extra process, even a minimal one such as this same mode's default for `programs.launcher`. |

```json
"prompt": {
    "is-enabled": false
}
```

### 2.13. Reload behavior

Reloading the configuration (`SIGHUP`, the reload keybinding, or the
root menu action) re-reads whichever theme file `config.json` names and
applies the new colors, font, and titlebar button lists to every open
window immediately, since those are read live from the theme on every
repaint.  Every already-decorated window's frame is also resized to
match a changed `border.width` or `titlebar.height`, the same way it
resizes on a focus change (see `themes.md` §1).

What reload does **not** do is force a window's decorated/undecorated
state to follow a changed `window.is-decorated` or `titlebar.height` in
the theme file.  A window that was decorated when it was mapped stays
decorated after a reload even if the reloaded theme now says
`"is-decorated": false` (and vice versa).  Only newly mapped windows,
and windows whose decoration is toggled by hand, pick up that setting.
This is deliberate: undoing a decoration choice a person made for
a specific window just because the theme file changed would be
a surprising, unrequested side effect.

## 3. `bindings.json`: Keyboard and mouse bindings

Defines all keyboard shortcuts and mouse button bindings.  This file is
optional; if absent, the built-in defaults listed in the tables below
are used.

### 3.1. Binding syntax

A binding is a `+`-separated chain of modifier aliases and a final key
or button name:

```
modifier1+[modifier2+[modifier3+]]KeyName
```

- **Modifier aliases** are the symbolic names defined in the `modifiers`
  section (e.g., `modc`, `mod1`).  Using aliases instead of literal key
  names (e.g., `Control`, `Alt`) makes it easy to remap all shortcuts by
  changing only the `modifiers` section.
- **Key names** are standard X11 keysym names (e.g., `Return`, `Tab`,
  `Left`, `a`, `F1`).
- **Mouse button names** are `button1` through `button5`.
- An empty string `""` means *unbound* (the action has no shortcut).

Example:  `"modc+mod1+Return"` with the default modifiers resolves to
`Control+Alt+Return`.

### 3.2. `modifiers`

Symbolic names for modifier keys.  Every binding that references
a modifier uses one of these aliases.

| Alias  | Default X11 key | Description                     |
|--------|-----------------|---------------------------------|
| `modc` | `Control`       | Control key                     |
| `mods` | `Shift`         | Shift key                       |
| `modl` | `Caps_Lock`     | Caps Lock                       |
| `mod1` | `Alt`           | Alt / Meta key                  |
| `mod2` | `Num_Lock`      | Num Lock                        |
| `mod3` | `""`            | *Unassigned* (empty by default) |
| `mod4` | `Super`         | Super / Windows key             |
| `mod5` | `Hyper`         | Hyper key                       |

```json
"modifiers": {
    "modc": "Control",
    "mods": "Shift",
    "modl": "Caps_Lock",
    "mod1": "Alt",
    "mod2": "Num_Lock",
    "mod3": "",
    "mod4": "Super",
    "mod5": "Hyper"
}
```

### 3.3. `keyboard.launch`

Shortcuts for launching external applications.

| Key            | Default binding    | Action                           |
|----------------|--------------------|----------------------------------|
| `terminal`     | `modc+mod1+Return` | Launch the terminal emulator.    |
| `launcher`     | `modc+mod1+r`      | Launch the application launcher. |
| `file-manager` | `modc+mod1+q`      | Launch the file manager.         |
| `web-browser`  | `modc+mod1+w`      | Launch the web browser.          |
| `editor`       | `modc+mod1+e`      | Launch the text editor.          |

### 3.4. `keyboard.window`

Actions performed on the currently focused window.

#### Direct window actions

| Key             | Default binding         | Action |
|-----------------|-------------------------|--------|
| `close`         | `modc+mod1+c`           | Send `WM_DELETE_WINDOW` to politely close the window. |
| `kill`          | `modc+mod1+mods+Escape` | Forcibly terminate the client process. |
| `iconify`       | `modc+mod1+i`           | Iconify the window (TWM-style desktop icon). |
| `iconify-all`   | `modc+mod1+mods+i`      | Iconify (minimize) every client on the current desktop. |
| `deiconify-all` | `modc+mod1+mods+d`      | Restore every iconified client on the current desktop. |
| `arrange`       | `modc+mod1+mods+a`      | Re-apply the configured placement policy to every client on the current desktop, spreading them back out.  A transient dialog among them is re-centered over its parent instead (ICCCM §4.1.2.6). |
| `hide`          | `modc+mod1+mods+u`      | Hide the window without iconifying it. |
| `maximize`      | `modc+mod1+m`           | Toggle maximize (full work area). |
| `fullscreen`    | `modc+mod1+f`           | Toggle true fullscreen mode. |
| `shade`         | `modc+mod1+s`           | Roll-up / roll-down the window (shade). |
| `pin`           | `modc+mod1+p`           | Toggle pinned mode (window appears on all desktops). |
| `decorate`      | `modc+mod1+d`           | Toggle window decorations (title bar). |
| `layer`         | `modc+mod1+mods+y`      | Cycle the window stacking layer: *normal* > *above* > *below*. |
| `info`          | `modc+mod4+mods+i`      | Show a popup with window information. |
| `inspect`       | `modc+mod4+i`           | Open a dialog listing everything the manager holds about the focused window: its identity, where it sits, its state spelled out rather than as a number, its size hints, and what it is transient for.  Also on the window menu. |

#### `keyboard.window.move.relative`

Move the focused window by a fixed step in the given direction.

| Key     | Default binding |
|---------|-----------------|
| `right` | `modc+mod1+l`   |
| `left`  | `modc+mod1+h`   |
| `up`    | `modc+mod1+k`   |
| `down`  | `modc+mod1+j`   |

#### `keyboard.window.move.absolute`

Teleport the focused window to a named screen position.

| Key            | Default binding | Destination           |
|----------------|-----------------|-----------------------|
| `center`       | `modc+mod1+g`   | Center of the screen. |
| `top-left`     | `modc+mod1+y`   | Top-left corner.      |
| `top-right`    | `modc+mod1+u`   | Top-right corner.     |
| `bottom-left`  | `modc+mod1+b`   | Bottom-left corner.   |
| `bottom-right` | `modc+mod1+n`   | Bottom-right corner.  |

#### `keyboard.window.resize`

Resize the focused window by a fixed step in the given direction.

| Key     | Default binding    |
|---------|--------------------|
| `right` | `modc+mod1+mods+l` |
| `left`  | `modc+mod1+mods+h` |
| `up`    | `modc+mod1+mods+k` |
| `down`  | `modc+mod1+mods+j` |

#### `keyboard.window.send-to.desktop`

Carry the focused window to the desktop north/south/east/west of the
current one, following it there.  Parallels `keyboard.cycle.desktop`
(§3.7), which only switches the view itself without moving any window
along.  A silent no-op when there is no different desktop to move to in
that direction at all: only one exists (always the case in
restricted-memory mode), wrapping is disabled
(`desktops.wrap-at-bounds`, §2.10) and this is already the edgemost
desktop that way, or (north/south only, on a screen with no
`topology.screens.desktops[].layout` configured, or one with a single
row) there is no second row to move to in the first place.

| Key     | Default binding      |
|---------|-----------------------|
| `north` | `modc+mod1+mods+Up`   |
| `south` | `modc+mod1+mods+Down` |
| `east`  | `modc+mod1+mods+Right`|
| `west`  | `modc+mod1+mods+Left` |

#### `keyboard.window.send-to.monitor`

Move the focused window to the monitor north/south/east/west of the
current one on its surface, resolved by real physical position (from
RandR) rather than detection order.  Unlike `send-to.desktop` just
above, never wraps around at all, and has no equivalent of
`desktops.wrap-at-bounds` to make that configurable: wrapping
a definite, ordered list (a desktop's) has one obviously correct
meaning, but wrapping a genuinely 2-D physical arrangement does not
(does "east, wrapped" mean the westmost monitor overall, or only the
westmost one still on the same row?), so no attempt is made to invent
one.  A no-op on a surface with one monitor or none, or when no monitor
lies in that direction at all.

| Key     | Default binding           |
|---------|----------------------------|
| `north` | `modc+mod1+mod4+mods+Up`   |
| `south` | `modc+mod1+mod4+mods+Down` |
| `east`  | `modc+mod1+mod4+mods+Right`|
| `west`  | `modc+mod1+mod4+mods+Left` |

```json
"window": {
    "send-to": {
        "desktop": {
            "north": "modc+mod1+mods+Up",
            "south": "modc+mod1+mods+Down",
            "east": "modc+mod1+mods+Right",
            "west": "modc+mod1+mods+Left"
        },
        "monitor": {
            "north": "modc+mod1+mod4+mods+Up",
            "south": "modc+mod1+mod4+mods+Down",
            "east": "modc+mod1+mod4+mods+Right",
            "west": "modc+mod1+mod4+mods+Left"
        }
    }
}
```

### 3.5. `keyboard.wm`

Window manager control shortcuts.

| Key                             | Default binding        | Action |
|---------------------------------|------------------------|--------|
| `search`                        | `modc+mod4+mods+s`     | Open the fuzzy window-search widget. |
| `scratchpad`                    | `modc+mod1+mods+F12`   | Launch the scratchpad, or show/hide it if already running; see `scratchpad` (§2.11). |
| `toggle-strutless-maximization` | *(unbound)*            | Toggle whether panel/tray struts are set aside when computing work areas on this surface (strutless maximization); also reachable via IPC (`toggle_strutless_maximize`) and its entry in the root menu. |
| `redraw`                        | `modc+mod1+mods+r`     | Force a full redraw of all windows. |
| `reload`                        | `modc+mod1+mods+c`     | Reload the configuration files (equivalent to `SIGHUP`). |
| `quit`                          | `modc+mod1+mods+x`     | Exit IcoWM. |
| `shortcuts`                     | `modc+mod4+F1`         | Show a dialog listing every currently active keyboard shortcut. |
| `fortune`                       | `modc+mod4+Backspace`  | Open the `fortune` easter-egg dialog; only active when `fortune.is-enabled` is also true (§2.6). |

`search` opens a centered, live-filtered list of every window across
every desktop.  Typing narrows the list by fuzzy subsequence match
against each window's name (the typed characters must appear in order,
but not necessarily contiguous); `Up`/`Down` or the mouse select a row,
`Return` or a click confirms it, and `Escape` cancels.  Confirming
switches to the window's desktop, restores it first if it was iconified,
hidden, or shaded, then focuses and raises it.  Each row shows the
window's icon (when `theme.menu.show-pixmaps` is enabled), its name, its
desktop's name (when the surface has more than one desktop) alongside
its index and, once `topology.screens.desktops[].layout` configures
genuinely more than one row, its `(row,column)` position too, and any
bracketed state hints that apply (`f`/`m`/`h`/`v` for fullscreen or one
of the maximized variants, `s` for shaded, `p` for pinned, `!`
for urgent).

`shortcuts` opens a dialog listing every active keyboard binding
described in this section, grouped by category and read directly from
the configuration actually in effect, so it always matches what is
really bound rather than a separately maintained description of the
defaults.  `F1` is used here since it conventionally means "help" on
most keyboards, and `modc+mod4` is unlikely to already be claimed by
another running application.

#### `keyboard.wm.menus`

Keyboard shortcuts for the two menus that have no inherent screen
position.


| Key       | Default binding    | Action |
|-----------|--------------------|--------|
| `root`    | `modc+mod1+mods+m` | Open the desktop (root) context menu, `menu.json`. |
| `windows` | `modc+mod1+mods+w` | Open the menu listing every window on every desktop. |

```json
"wm": {
    "menus": {
        "root": "modc+mod1+mods+m",
        "windows": "modc+mod1+mods+w"
    }
}
```

### 3.6. `keyboard.desktop`

Desktop-level actions: switching, adding/removing, and the show-desktop
toggle.  Its top-level section, a sibling of `keyboard.window` rather
than nested under `keyboard.wm`: none of these act on any one particular
client the way everything under `keyboard.window` does, but they are
just as much a coherent, frequently reached-for group as that one is,
not really a good fit for `keyboard.wm`'s remaining, much more disparate
set of window-manager-lifecycle actions (`quit`, `reload`, `redraw`, and
the like) either.

| Key      | Default binding        | Action |
|----------|------------------------|--------|
| `add`    | `modc+mod4+mods+Right` | Add a new, empty desktop to the end of the list. |
| `remove` | `modc+mod4+mods+Left`  | Remove the last desktop, moving any client still on it to the new last one first; refused while only one desktop remains. |
| `show`   | `modc+mod4+mods+d`     | Hide all windows and show the empty desktop. |

`add`/`remove` always act on the surface's last desktop.  A new one is
always appended at the end; removing one always takes the last one,
moving any client still on it to the new last desktop first (its EWMH
`_NET_WM_DESKTOP` is updated to match, unless it is pinned, whose
property already holds the EWMH "all desktops" sentinel).  Removing
a specific desktop by index is not offered: with removal always
affecting the last one, every existing index below it stays exactly
where it was, so no other binding (`go-to` just below, a rule's
`desktop` match, and so on) is ever silently invalidated by a removal
elsewhere in the list.

```json
"desktop": {
    "add": "modc+mod4+mods+Right",
    "remove": "modc+mod4+mods+Left",
    "show": "modc+mod4+mods+d"
}
```

#### `keyboard.desktop.go-to`

Jump directly to a virtual desktop by index (0-9).  Desktops beyond
index 9 are not reachable by these shortcuts.

| Key        | Default binding | Destination |
|------------|-----------------|-------------|
| `desktop0` | `modc+mod1+0`   | Desktop 0.  |
| `desktop1` | `modc+mod1+1`   | Desktop 1.  |
| `desktop2` | `modc+mod1+2`   | Desktop 2.  |
| `desktop3` | `modc+mod1+3`   | Desktop 3.  |
| `desktop4` | `modc+mod1+4`   | Desktop 4.  |
| `desktop5` | `modc+mod1+5`   | Desktop 5.  |
| `desktop6` | `modc+mod1+6`   | Desktop 6.  |
| `desktop7` | `modc+mod1+7`   | Desktop 7.  |
| `desktop8` | `modc+mod1+8`   | Desktop 8.  |
| `desktop9` | `modc+mod1+9`   | Desktop 9.  |

If the interest is to use a 1-based indexing system, a trick could be
setting `inaugural` to `1`.  Another is to change every single
`keyboard.desktop.go-to` binding.

### 3.7. `keyboard.cycle`

Shortcuts for cycling through desktops, iconified windows, and open
windows.

#### `keyboard.cycle.desktop`

Switches the view to the desktop north/south/east/west of the current
one, without moving any window along; see
`keyboard.window.send-to.desktop` above for the one that does.

| Key     | Default binding    | Action                            |
|---------|---------------------|-----------------------------------|
| `north` | `modc+mod1+Up`      | Switch to the desktop north of the current one. |
| `south` | `modc+mod1+Down`    | Switch to the desktop south of the current one. |
| `east`  | `modc+mod1+Right`   | Switch to the desktop east of the current one.  |
| `west`  | `modc+mod1+Left`    | Switch to the desktop west of the current one.  |

#### `keyboard.cycle.window`

Keyboard `Alt+Tab`-style navigation through open (non-iconified)
windows.

| Key    | Default binding | Action                                       |
|--------|-----------------|----------------------------------------------|
| `prev` | `mod1+mods+Tab` | Focus the previous window in the cycle list. |
| `next` | `mod1+Tab`      | Focus the next window in the cycle list.     |

#### `keyboard.cycle.icon`

Cycle through iconified (minimized) windows only.

| Key    | Default binding      | Action                   |
|--------|----------------------|--------------------------|
| `prev` | `modc+mod1+mods+Tab` | Focus the previous icon. |
| `next` | `modc+mod1+Tab`      | Focus the next icon.     |

### 3.8. `keyboard.viewport`

Shortcuts for panning a desktop's viewport, when its
`topology.screens.desktops[].viewport` grid (§2.2) is wider or taller
than one screen; see that section for how the grid itself is sized.

#### `keyboard.viewport.pan`

Pans the current desktop's viewport by one whole screen in the given
direction, translating every non-sticky client the opposite way so
their on-screen position stays put relative to the desktop's virtual
canvas; unlike `keyboard.cycle.desktop` above, the active desktop never
changes, only where within it the physical screen is looking.

| Key     | Default binding      | Action                              |
|---------|-----------------------|-------------------------------------|
| `north` | `modc+mod4+mods+Up`   | Pan the viewport one screen north.  |
| `south` | `modc+mod4+mods+Down` | Pan the viewport one screen south.  |
| `east`  | `modc+mod4+mod5+Right`| Pan the viewport one screen east.   |
| `west`  | `modc+mod4+mod5+Left` | Pan the viewport one screen west.   |

```json
"viewport": {
    "pan": {
        "north": "modc+mod4+mods+Up",
        "south": "modc+mod4+mods+Down",
        "east": "modc+mod4+mod5+Right",
        "west": "modc+mod4+mod5+Left"
    }
}
```

### 3.9. `mouse.window`

Mouse button bindings for window management.

| Key      | Default binding | Action |
|----------|-----------------|--------|
| `move`   | `mod1+button1`  | Click and drag to move the window. |
| `lower`  | `mod1+button2`  | Lower the window to the bottom of the stack. |
| `resize` | `mod1+button3`  | Click and drag to resize the window. |

### 3.10. `mouse.cycle`

Mouse wheel bindings for switching virtual desktops, or (over a window's
titlebar) shading/unshading or maximizing/restoring it instead:

- `west`/`east` (plain scroll, matching what `prev`/`next` always meant
  before desktops gained compass directions): over a titlebar, shade or
  unshade the window there; elsewhere, switch desktop.
- `north`/`south` (scroll with a held modifier): over a titlebar,
  maximize (only if not already) or restore from maximized (only if
  currently maximized); elsewhere, switch desktop.  Never moves focus,
  unlike shade/unshade, since a maximized or restored window stays
  exactly as interactable either side of the change.

| Key     | Default binding   |
|---------|-------------------|
| `north` | `mods+button4`    |
| `south` | `mods+button5`    |
| `east`  | `button5`         |
| `west`  | `button4`         |

```json
"mouse": {
    "cycle": {
        "desktop": {
            "north": "mods+button4",
            "south": "mods+button5",
            "east": "button5",
            "west": "button4"
        }
    }
}
```

### 3.11. Fixed titlebar behavior

Beside the decoration buttons, a titlebar answers each mouse button on
its own, without any binding configured for it:

| Button | Action                |
|--------|-----------------------|
| Left   | Click and drag to move the window; double-click to shade or unshade it (see `interaction.double-click-ms` in §6). |
| Middle | Send the window to the back.  An ordinary click on whatever it went behind brings it forward again. |
| Right  | Open the window menu. |

None of these can be reconfigured, being part of what a decorated window
is rather than a binding.  A configured binding always wins over them:
with `mouse.window.lower` at its default of `mod1+button2`, a middle
click with `mod1` held lowers the window from anywhere on it, and
a plain middle click does so only over the titlebar.

### 3.12. What a modal dialog is not allowed to do

A window declaring `_NET_WM_STATE_MODAL` can neither be maximized nor
put into fullscreen, whichever way it is asked: the titlebar button, the
window menu entry, a keyboard binding, or the application's own EWMH
request.  The maximize button is drawn disabled, the same as it already
is for a window that has fixed its size.

A modal exists to be answered while the window it belongs to stays in
view, and filling the screen puts that window out of sight with no way
back to it that does not go through the modal first.

Resizing is untouched.  A client says it will not be resized by pinning
both axes in `WM_NORMAL_HINTS`, and a modal that resizes freely, which
a rename prompt with a growable text field does, keeps doing so.

## 4. `themes/<name>.json`: Theme configuration

Controls the visual appearance of windows, desktop icons, and the
systray.  Moved to its document, [`themes.md`](themes.md), since this
file had grown too large to navigate comfortably alongside every other
configuration file it also covers.

## 5. `randr.json`: XRandR output profiles

Defines per-output settings applied by IcoWM through the XRandR
extension.  This file is **optional**.  If absent, XRandR hot-plug event
handling still works (screen-change notifications are processed), but no
output profiles are configured.

Up to **16** output entries are supported (**2** in a low-memory build
(as per built-in default values).

Applied at startup, and again whenever an output connects or disconnects
afterward (e.g., plugging in an external monitor).

### 5.1. Top-level fields

| Key          | Type    | Default | Description |
|--------------|---------|---------|-------------|
| `is-enabled` | boolean | `false` | Master switch.  Set to `true` to activate output profile management. |

### 5.2. `outputs[]` entries

Each entry in the `outputs` array describes one physical display output.
A profile whose `name` does not match any currently-connected output is
simply skipped until one by that name appears.

| Key            | Type    | Default    | Description |
|----------------|---------|------------|-------------|
| `name`         | string  | `""`       | Output connector name as reported by the X server (e.g., `"HDMI-1"`, `"eDP-1"`, `"DP-2"`).  Run `xrandr` in a terminal to list available names. |
| `is-enabled`   | boolean | `false`    | Whether this output is used at all.  `true` applies `resolution`, `position`, and `rotation` below to the output, and lets IcoWM manage windows on it.  `false` instead turns the output off (blanking it, the same as unplugging it) and excludes it from window placement entirely, useful for a permanently-connected output (a projector for mirroring, say) that should never receive windows. |
| `is-primary`   | boolean | `false`    | Mark this output as the primary display.  Only applied when `is-enabled` is `true`; applied as a separate step right after the rest of this profile. |
| `resolution.w` | integer | `0`        | Preferred horizontal resolution in pixels.  Matched against the modes the screen currently reports; if `0`, `0`, or no exact match exists, the output's already-active mode is kept instead (or its first preferred mode, if it had none), and, since nothing was actually requested in that case, its resolution plays no part in deciding whether this profile changed anything on a later reload, or in what a `[ Revert ]` on the confirm dialog restores (see `position`/`rotation`/`is-primary` above and below, which always do).  Only applied when `is-enabled` is `true`. |
| `resolution.h` | integer | `0`        | Preferred vertical resolution in pixels.  See `resolution.w` above. |
| `position.x`   | integer | `0`        | Horizontal position of this output in the virtual screen.  Only applied when `is-enabled` is `true`. |
| `position.y`   | integer | `0`        | Vertical position of this output in the virtual screen.  Only applied when `is-enabled` is `true`. |
| `rotation`     | string  | `"normal"` | Screen rotation.  Only applied when `is-enabled` is `true`. |

Accepted `rotation` values are: `"normal"`, `"left"` (90°), `"right"`
(270°), `"inverted"` (180°).

### 5.3. Scope: per-X-screen, not per-`outputs[]`-entry

RandR is scoped to a single X screen: CRTCs, outputs, and modes are
queried and configured against one screen's root window, with no
cross-screen notion at the protocol level.  RandR profiles are applied
once per surface (X screen managed), querying and applying RandR state
independently for each.  `outputs[]` itself, however, is a single list
shared by every surface, with no per-screen field.  On a multi-GPU setup
where two X screens each expose an output of the same name, the matching
profile is applied identically to both, with no way to scope it to one
screen only.  Not applicable to the common case of one X screen managing
several outputs via RandR 1.5 monitors, nor in practice to most
multi-screen setups either, since output names are driver/GPU- assigned
and are not normally reused across independent GPUs.

### 5.4. Reload behavior

`randr.json` is re-read on every configuration reload and applied
immediately, before rules, keyboard/mouse bindings, or any other reload
step, so a resync of clients or desktops elsewhere in the same reload
already reflects the new screen geometry if RandR itself just changed
it.

If that application actually changed anything, a confirm dialog appears,
centered on the affected screen: "The 'randr.json' configuration has
been applied.  Keep it, or revert to the previous one?", with
`[ Revert]` selected by default and a live countdown underneath.
Pressing `[ Keep ]` (or `Tab`/arrow then `Enter`/`Space`) keeps the
just-applied profile; pressing `[ Revert ]`, Escape, or letting the
countdown reach zero undoes it, restoring every changed output's prior
mode, position, rotation, and primary status exactly.  The countdown
defaults to 10 seconds (as per default values).  No dialog appears at
all when nothing actually changed (see the comparison below), nor at
startup or on a hotplug event; only a reload, the one moment a person is
at the keyboard to have triggered it, offers this.

With more than one screen, every screen still gets its profiles applied
on the same reload even when more than one changes, but only the first
screen to actually change is offered the dialog: only one confirm dialog
can be open at a time, and only the single most recent change is
remembered well enough to revert.

Every reload compares each configured profile against the matching
output's actual current state first (resolution, position, rotation,
primary status) and issues an XRandR write for it only when at least one
of them genuinely differs.  A `randr.json` whose profiles already match
reality therefore issues no XRandR requests at all, including on
a reload triggered by an unrelated file (e.g., `config.json`), and on
a hotplug event for an output some other profile targets.

```json
{
    "is-enabled": true,
    "outputs": [
        {
            "name": "eDP-1",
            "is-enabled": true,
            "is-primary": true,
            "resolution": { "w": 1920, "h": 1080 },
            "position": { "x": 0, "y": 0 },
            "rotation": "normal"
        },
        {
            "name":  "HDMI-1",
            "is-enabled": true,
            "is-primary": false,
            "resolution": { "w": 2560, "h": 1440 },
            "position": { "x": 1920, "y": 0 },
            "rotation": "normal"
        }
    ]
}
```

## 6. `a11y.json`: Timing and visual-feedback overrides

An entirely optional file: everything here has a built-in default
already in effect before this file exists at all, so nobody who never
creates it sees any behavior change.  A reload resets every field back
to its built-in default first, then applies only what the file actually
specifies, the same way the active theme (§4) already does, rather than
merging on top of whatever an earlier load left in place.

`is-enabled` (default `false`) gates every field below at once, the same
opt-in-only posture `randr.json`'s `is-enabled` (§5) already has: a file
that exists but never turns this on is read without error, same as ever,
but has no effect at all, the same as if it were absent.  A person can
keep an `a11y.json` around, ready to reference or hand off, without it
applying until they explicitly turn this on.

### 6.1. Fields

| Key                                | Type    | Default |
|------------------------------------|---------|---------|
| `is-enabled`                       | boolean | `false` |
| `interaction.double-click-ms`      | integer | `400`   |
| `focus-indicator.min-border-width` | integer | `0`     |
| `urgency.sound-bell`               | boolean | `false` |
| `urgency.blink-interval-ms`        | integer | `600`   |

`interaction.double-click-ms` is how long, in milliseconds, between two
clicks on a titlebar for them to count as a double-click (which toggles
shade) rather than two independent single clicks.  Raise it for more
forgiving timing.

`focus-indicator.min-border-width` enforces a minimum border width, in
pixels, on every window regardless of what the active theme's
`window.active.border.width` / `window.inactive.border.width`
(`themes/<name>.json`, `themes.md` §1) specify.  A theme that already
sets a wider border than this is left untouched; this only ever raises
a border that would otherwise be thinner than it, keeping the focus
indicator visible even for a theme that sets an unusually thin one.  The
default of `0` never raises anything, deferring entirely to whatever the
active theme already specifies.

`urgency.sound-bell`, when `true`, sounds the X server's bell
(`xcb_bell`) the moment a client first becomes urgent, once per
transition into urgency rather than repeatedly while it stays that way,
alongside the visual blink every urgent client's titlebar (and icon, if
iconified) already gets regardless of this setting.

`urgency.blink-interval-ms` is how many milliseconds pass between one
blink phase and the next for that same visual indicator.  Lower it for
a faster, more attention-grabbing blink; raise it for a slower one.

```json
{
    "is-enabled": true,
    "interaction": {
        "double-click-ms": 500
    },
    "focus-indicator": {
        "min-border-width": 3
    },
    "urgency": {
        "sound-bell": true,
        "blink-interval-ms": 400
    }
}
```

### 6.2. Reload behavior

Unlike `topology` in `config.json` (§2.2), every field here does take
effect on a configuration reload (the reload keybind, `SIGHUP`, or the
root menu's "Reload configuration" entry): none of them describe screen
or desktop topology, so none of the concerns that keep `topology`
reload-only-at-startup apply here.

## 7. `rules.json`: Per-window rules

Defines optional matching rules that are evaluated when a window is
first mapped and, optionally, again when relevant ICCCM/EWMH properties
change.  If the file is absent or malformed, IcoWM continues without any
rules.

Rules are evaluated in declaration order.  When multiple entries match
the same window, later entries override earlier ones on a field-by-field
basis.

The file may be either:

- a top-level JSON array of rule objects, or
- an object with a `rules` array.

### 7.1. Rule file shape

```json
[
    {
        "when": "map",
        "match": {
            "class": "XTerm"
        },
        "apply": {
            "desktop": 1,
            "focus": true
        }
    }
]
```

Equivalent wrapper form:

```json
{
    "rules": [
        {
            "when": "map",
            "match": {
                "class": "XTerm"
            },
            "apply": {
                "desktop": 1,
                "focus": true
            }
        }
    ]
}
```

### 7.2. Rule entry fields

Each rule entry is a JSON object with the following keys:

| Key     | Type   | Default | Description |
|---------|--------|---------|-------------|
| `when`  | string | `"map"` | When the rule is eligible to run.  Accepted values: `"map"`, `"property"`, `"both"`. |
| `match` | object | `{}`    | Set of window-property predicates.  Omitted or empty means the rule matches every window. |
| `apply` | object | none    | Actions to apply when the rule matches.  If absent or not an object, the entry is ignored. |

The `when` field controls *when* IcoWM is allowed to evaluate the rule.
Use `"map"` for actions that should be decided only once, when the
window first appears.  Use `"property"` for rules that should react only
after a later change to a tracked window property such as title, role,
or type.  Use `"both"` when the same rule should be considered in both
situations: at initial map time and again after relevant property
updates.

### 7.3. Match fields

All match fields are optional.  A rule matches only when all specified
fields match the current window.

| Key               | Type            | Default | Description |
|-------------------|-----------------|---------|-------------|
| `match.instance`  | string or array | unset   | Match the first string in `WM_CLASS` (instance name). |
| `match.class`     | string or array | unset   | Match the second string in `WM_CLASS` (class name). |
| `match.role`      | string or array | unset   | Match `WM_WINDOW_ROLE`. |
| `match.title`     | string or array | unset   | Match the current window title. |
| `match.type`      | string or array | unset   | Match `_NET_WM_WINDOW_TYPE`. |
| `match.transient` | boolean         | unset   | Match whether the window is transient for another window. |

String matches use shell-style glob patterns, so `\*` matches any
sequence of characters and `?` matches any single character.  `[...]` is
also special, the same way it is in a shell glob: it defines a character
class matching any *one* of the characters inside the brackets, not
a literal pair of brackets, so `"[Application]"` does not match a title
that literally reads `[Application]`; it matches a title that is the
single character `A`, `p`, `l`, `i`, `c`, `t`, `o`, or `n`.  To match
a literal `[` or `]`, escape it with a backslash, doubled as usual for
a JSON string: `"\\[Application\\]"` in `rules.json` reaches the matcher
as `\[Application\]`, matching the literal text `[Application]`.
A plain `"*Application*"` is usually simpler when the brackets
themselves do not need to be part of the match.

Every field above except `transient` accepts either a single string or
a JSON array of strings.  When it is an array, the window matches that
field if it matches *any one* of the values in the array (an "or" within
the field), up to 6 values; the rest of the fields still all have to
match too (the "and" across fields still applies).  This is useful for
grouping several related applications under one rule instead of
repeating the same `apply` block for each of them:

```json
{
    "when": "map",
    "match": {
        "title": ["*Sonata", "*mpv", "mplayer"],
        "class": "MEDIA",
        "type": "normal"
    },
    "apply": {
        "pinned": true,
        "decorated": false,
        "layer": "below"
    }
}
```

This matches any window whose title matches one of the three patterns
*and* whose class is `"MEDIA"`; a window matching only one of those two
conditions does not match the rule.  A single string, as in `"class"`
above, still works exactly as before.  There is no need to wrap a single
value in an array.

Accepted `match.type` values are: `"normal"`, `"desktop"`, `"dock"`,
`"toolbar"`, `"menu"`, `"utility"`, `"splash"`, `"dialog"`.

As with every other match field, omitting `match.type` entirely does not
default to `"normal"`.  It means that criterion is not checked at all,
so the rule matches windows of any type.  In the example above,
`"type"`: "normal" is an explicit, active condition; removing that line
and the rule would also match e.g., a dialog with a matching title and
class.

### 7.4. Apply fields

All apply fields are optional.  Only the fields present in the last
matching rule for each property are applied.

| Key                      | Type                 | Default | Description |
|--------------------------|----------------------|---------|-------------|
| `apply.desktop`          | integer              | unset   | Zero-based desktop index to move the window to.  Falls back to desktop `0` if it does not exist, logging a warning. |
| `apply.monitor`          | integer              | unset   | Zero-based monitor index, within the window's surface, to place the window on.  Falls back to monitor `0` if it does not exist, logging a warning. |
| `apply.layer`            | string               | unset   | Stacking layer.  Accepted values: `"below"`, `"normal"`, `"above"`.  Falls back to `"normal"` if unrecognized, logging a warning. |
| `apply.focus`            | boolean              | unset   | Whether the matched window should receive focus. |
| `apply.pinned`           | boolean              | unset   | Whether the window should be visible on all desktops. |
| `apply.decorated`        | boolean              | unset   | Whether the window should keep its decorations. |
| `apply.iconified`        | boolean              | unset   | Whether the window should be iconified, turned into an actual icon on the desktop instead of a taskbar entry, the same as pressing `iconify` by hand.  Overrides the window's own `WM_HINTS` initial-state request. |
| `apply.fullscreen`       | boolean              | unset   | Whether the window should be fullscreen.  Overrides the window's own EWMH initial-state hint, and takes precedence over `apply.maximized` when a rule asks for both, the same precedence entering fullscreen by hand already has over a maximized window. |
| `apply.maximized`        | boolean              | unset   | Whether the window should be maximized, both horizontally and vertically together; there is no field here for maximizing only one axis.  Overrides the window's own EWMH initial-state hint. |
| `apply.shaded`           | boolean              | unset   | Whether the window should be shaded, rolled up to just its titlebar.  Ignored, with a warning logged, if the window is not decorated, if the same rule also asks for `apply.fullscreen`, or if the same rule also asks for `apply.iconified`. |
| `apply.hidden`           | boolean              | unset   | Whether the window should be hidden: withdrawn from the desktop and any pager or taskbar alike, without being iconified. |
| `apply.opacity`          | integer or object    | unset   | Desired opacity, 0 to 100, published on the window through `_NET_WM_WINDOW_OPACITY`, overriding the theme's `window.active.opacity`/`window.inactive.opacity` (`themes.md` §1) for this one window; see below. |
| `apply.opacity.active`   | integer              | unset   | Opacity while the window is focused (when `opacity` is an object). |
| `apply.opacity.inactive` | integer              | unset   | Opacity while the window is not focused (when `opacity` is an object). |
| `apply.position`         | object or `"center"` | unset   | Where to place the window; see below. |
| `apply.position.x`       | integer              | unset   | X position in pixels (when `position` is an object), relative to `apply.monitor`'s top-left corner if set, or to the surface's otherwise. |
| `apply.position.y`       | integer              | unset   | Y position in pixels (when `position` is an object), relative to `apply.monitor`'s top-left corner if set, or to the surface's otherwise. |
| `apply.size.width`       | integer              | unset   | Window width in pixels; must be greater than `0`. |
| `apply.size.height`      | integer              | unset   | Window height in pixels; must be greater than `0`. |

Position and size are applied independently.  Specifying only `position`
moves the window without resizing it; specifying only `size` resizes it
without moving it; both may be present to set position and size at once.
Both `size.width` and `size.height` are only accepted when strictly
postive, greater than `0`.

Key `opacity` is either a single integer applying to both the active and
inactive state alike, or an object naming one, the other, or both
separately:

```json
"apply": {
    "opacity": 90
}
```

```json
"apply": {
    "opacity": {
        "active": 100,
        "inactive": 75
    }
}
```

A `"opacity": {"active": ...}` object with only one of the two keys
overrides that one state only, leaving the other to keep following the
theme's value.  As with every other apply field, no compositing manager
reading `_NET_WM_WINDOW_OPACITY` back means this has no visible effect
at all, whether it comes from here or from the theme.

Key `position` is either an `{"x": ..., "y": ...}` object with an
absolute pixel position, or the string `"center"`, which centers the
window on its screen at the moment the rule is applied instead of using
a fixed point:

```json
"apply": {
    "position": "center"
}
```

Combining `"position": "center"` with a `size` object centers the window
at that new size, not whatever size it happened to already have.

`monitor` selects one physical monitor within the window's surface (only
meaningful on a surface made up of more than one monitor sharing the
same combined X screen; not named `screen`, since this project's
`screens[]`/`screen_id` terminology refers to a whole X screen, and this
codebase has no notion of moving a window to a *different* surface at
all, so a field with that name would misleadingly suggest a capability
that does not exist).  It changes what `position` is relative to rather
than being a distinct placement action.  Explicit `x`/`y` become offsets
from that monitor's top-left corner instead of the whole surface's, and
`"center"` centers on that monitor instead of the whole surface.  A rule
that sets `monitor` without also setting `position` still centers the
window on that monitor by default, since otherwise `monitor` alone would
have no visible effect at all:

```json
"apply": {
    "monitor": 1
}
```

```json
"apply": {
    "monitor": 1,
    "position": { "x": 20, "y": 20 }
}
```

The second example places the window 20 pixels from the top-left corner
of monitor `1`, not of the whole surface.

## 8. `session.json`: Session lifecycle hooks

Defines optional command lists that IcoWM launches asynchronously at key
lifecycle points.  If the file is absent or malformed, no hooks run.

Each command is expanded with POSIX `wordexp()` semantics before
execution, then started via `fork()` and `execvp()`.  Hook commands run
independently from the window manager; IcoWM only logs their start and
eventual termination status.

Command substitution is disabled.  Environment-variable expansion may
also be disabled on platforms that provide `WRDE_NOENV`.

If the "emergency shortcut" is used to exit, all pending session hooks
will be ignored.

### 8.1. Hook arrays

| Key         | Type             | Default | Description |
|-------------|------------------|---------|-------------|
| `on-start`  | array of strings | `[]`    | Commands launched after IcoWM startup initialization. |
| `on-reload` | array of strings | `[]`    | Commands launched after a configuration reload (`SIGHUP` or the reload action). |
| `on-exit`   | array of strings | `[]`    | Commands launched when IcoWM is exiting. |

Only non-empty string entries are used; all other array items are
ignored.

## 9. `menu.json`: Root desktop menu

`menu.json` defines the user-configurable entries that appear when the
user right-clicks on the empty desktop (root window).  The file is
**optional**; when it is absent or cannot be parsed, the built-in footer
(`Reload configuration`, `Redraw all windows`, `Exit`) is still shown
without any preceding separator.

### 9.1. Top-level structure

The file must contain a single JSON object with one key: `"menu"`, whose
value is a JSON array of entry objects.

```json
{
    "menu": [
        { ... },
        { ... }
    ]
}
```

### 9.2. Entry types

Each entry object must have a `"type"` string field.  Four types are
supported:

| `"type"`      | Description                                       |
|---------------|---------------------------------------------------|
| `"command"`   | Clickable item that launches an application       |
| `"separator"` | Horizontal dividing line (no other fields needed) |
| `"label"`     | Non-clickable section heading                     |
| `"submenu"`   | Nested sub-menu revealed on hover/click           |

### 9.3. Entry fields reference

#### `"command"` entry

| Field       | Type   | Required | Description                         |
|-------------|--------|----------|-------------------------------------|
| `"type"`    | string | yes      | Must be `"command"`                 |
| `"name"`    | string | yes      | Label text shown in the menu        |
| `"command"` | string | yes      | Shell command or program to execute |

The `"command"` value is passed through `wordexp(3)` so environment
variables and simple shell expansions (`~`, `$HOME`, ...) are supported.
Command injection via sub-shells is **disabled** (`WRDE_NOCMD`).

If the command cannot be executed, IcoWM logs a warning and shows an
informational dialog so the user is notified immediately.

#### `"separator"` entry

| Field    | Type   | Required | Description           |
|----------|--------|----------|-----------------------|
| `"type"` | string | yes      | Must be `"separator"` |

At this moment there's only one kind of `"type"`, which is
`"separator"`.

#### `"label"` entry

| Field    | Type   | Required | Description                     |
|----------|--------|----------|---------------------------------|
| `"type"` | string | yes      | Must be `"label"`               |
| `"name"` | string | yes      | Section heading text to display |

At this moment there's only one kind of `"type"`, which is `"label"`.

#### `"submenu"` entry

| Field     | Type   | Required | Description                              |
|-----------|--------|----------|------------------------------------------|
| `"type"`  | string | yes      | Must be `"submenu"`                      |
| `"name"`  | string | yes      | Label text shown in the parent menu      |
| `"items"` | array  | yes      | Nested array of entry objects (any type) |

Sub-menus can be nested up to 4 levels deep.

## 10. `memguard.json`: Restricted-memory mode configuration

Read only when IcoWM is launched with `-M <mib>` (see `icowm.md`'s
"Restricted-memory mode" section for what that flag does and why it
exists); an ordinary session never reads this file, and this file has no
effect at all without `-M <mib>`.  It fully replaces `config.json` for
that one session: `config.json` itself is not consulted at all while `-M
<mib>` is in effect, but `bindings.json` and a theme file under
`themes/` are still read exactly as in an ordinary session (see §10.3
for the one exception).  The file is **optional**; a missing or
unreadable one falls back to a fixed built-in profile.

Only the fields below are ever read from it; anything else present in
the file is silently ignored, and every field this mode's screen and
desktop counts, RandR handling, and startup-notification setting are
fixed and cannot be configured here at all.  The single desktop this
mode always starts with stays that way for the whole session too: the
window list's "Add new desktop" and "Remove last desktop" entries, and
their keyboard shortcuts, do not even appear while this mode is active,
not merely refuse to act.

### 10.1. Configurable fields

| Key                                      | Type              | Default        | Description |
|------------------------------------------|-------------------|----------------|-------------|
| `theme`                                  | string            | `""` (built-in default theme) | Same as `config.json`'s `theme`: the filename (without `.json`) of a theme under `themes/`. |
| `programs.editor`                        | string            | `"gvim"`       | Same as `config.json`'s `programs.editor`. |
| `programs.file-manager`                  | string            | `"pcmanfm"`    | Same as `config.json`'s `programs.file-manager`. |
| `programs.launcher`                      | string            | `"gmrun"`      | Same as `config.json`'s `programs.launcher`. |
| `programs.terminal`                      | string            | `"xterm"`      | Same as `config.json`'s `programs.terminal`. |
| `programs.web-browser`                   | string            | `"firefox"`    | Same as `config.json`'s `programs.web-browser`. |
| `prompt.is-enabled`                      | boolean           | `true`         | Same as `config.json`'s `prompt.is-enabled` (§2.12), except restricted-memory mode defaults this to `true` rather than `false`, to avoid spawning `programs.launcher` as a separate process. |
| `desktops.margins.top/right/bottom/left` | integer           | `0`            | Same as `config.json`'s `desktops.margins`; this mode always runs with a single screen and a single desktop, so this is the only per-desktop setting still worth having. |
| `windows.move-step`                      | integer           | `10`           | Same as `config.json`'s `windows.move-step`. |
| `windows.show-geom`                      | boolean           | `true`         | Same as `config.json`'s `windows.show-geom`: shows a small overlay with the exact position (moving) or size (resizing) while dragging. |
| `windows.edges.snap.window`              | integer           | `6`            | Same as `config.json`'s `windows.edges.snap.window`: attraction distance in pixels toward another window's edge. |
| `windows.edges.snap.screen`              | integer           | `6`            | Same as `config.json`'s `windows.edges.snap.screen`: attraction distance in pixels toward the screen's edge. |
| `windows.edges.resistance`               | integer           | `20`           | Same as `config.json`'s `windows.edges.resistance`: pixels of deliberate extra drag before a maximized axis starts changing while interactively resizing. |
| `windows.gravity`                        | string            | `"north-west"` | Same as `config.json`'s `windows.gravity`: a fallback only, for a client that never declares its ; see §2.4 for the accepted values and why this is fallback-only. |
| `windows.focus.focus-new`                | boolean           | `true`         | Same as `config.json`'s `focus.focus-new`: when `true`, newly mapped windows receive focus automatically. |
| `windows.focus.raise`                    | boolean           | `false`        | Same as `config.json`'s `focus.raise`: when `true`, a window is also raised when it gains focus by pointer or wheel. |
| `windows.focus.policy`                   | string            | `"click"`      | Same as `config.json`'s `focus.policy`: `"click"` requires a click to focus; `"sloppy"` focuses whichever window is under the pointer. |
| `windows.focus.delay-ms`                 | integer           | `250`          | Same as `config.json`'s `focus.delay-ms`: milliseconds the pointer must sit still over a window before it is focused; only takes effect under `"sloppy"`. |
| `windows.placement.policy`               | string            | `"smart"`      | Same as `config.json`'s `windows.placement.policy`: `smart`, `cascade`, `centered`, or `under-mouse`. |
| `windows.placement.monitor`              | string or integer | `"pointer"`    | Same as `config.json`'s `windows.placement.monitor`: which physical monitor a placement decision targets, on a surface with more than one. |
| `windows.placement.group-related`        | boolean           | `false`        | Same as `config.json`'s `windows.placement.group-related`: cluster windows of the same application together. |
| `icons.show-geom`                        | boolean           | `false`        | Same as `config.json`'s `icons.show-geom`: shows the exact size in the center of the icon while resizing. |
| `icons.placement.policy`                 | string            | `"smart"`      | Same as `config.json`'s `icons.placement.policy`: `top`, `bottom`, `left`, `right`, or `smart`. |
| `systray`                                | object            | see §10.2      | The entire `systray` object, in the same shape as `config.json`'s §2.9, with the two exceptions in §10.2. |
| `shutdown.enable-emergency-shortcut`     | boolean           | `false`        | Same as `config.json`'s `shutdown.enable-emergency-shortcut`. |
| `shutdown.timeout-seconds`               | integer           | `15`           | Same as `config.json`'s `shutdown.timeout-seconds`. |

### 10.2. Fields this mode never lets `memguard.json` change

A handful of fields are read the same way as `config.json`'s identical
`systray` object, but immediately forced back to a fixed value
afterward, since restricted-memory mode never docks any icon at all
(embedding is always off) and so has no use for them:

- **`systray.text.position`** and **`systray.order`** only ever affect
  docked pixmap icons (where their text sits relative to them, and the
  order newly docked ones are placed in); both are always reset to their
  ordinary default (`left` and `left-to-right`, respectively) regardless
  of what the file specifies.
- **Embedding itself** cannot be turned on at all in this mode; the
  systray still shows (clock, battery, and its frame) when
  `systray.is-enabled` is `true`, just never accepts a docked
  application icon.
- **`windows.solid-drag`**, unlike the two fields just above, is not
  even accepted in the file at all (the schema this mode validates
  against does not list it; see §10.1), rather than being read and then
  overridden: this mode always runs with it forced to `false`, no
  exception.

### 10.3. Restrictions on the active theme

Whichever theme ends up active, named in `memguard.json`, or the
built-in default if none is, gets further restricted after loading, on
top of whatever `memguard.json` itself configured:

- Every font the theme specifies (window titles, icon labels, menu
  entries, dialog text, the systray's clock/battery text, and the
  desktop-name overlay) is replaced with IcoWM's fixed built-in font,
  **unless** it already names some variant of that same font (matched
  case-sensitively): a theme is free to specify that font directly
  instead of leaving every field to fall back to it, if it wants any of
  the styling (size, weight) that comes with naming it explicitly rather
  than implicitly.
- `XSettings` propagation (`themes.md` §9) is always off, regardless of
  `theme.xsettings.is-enabled`.
- Icon pixmaps, icon hint characters, and menu pixmaps are always off,
  regardless of what the theme itself specifies for `icon.show-pixmaps`,
  `icon.show-hints`, and `menu.show-pixmaps`.

None of this is configurable through `memguard.json` itself; it applies
to whatever theme loads, unconditionally.

## 11. Full examples

### `config.json`

```json
{
    "theme": "default",

    "topology": {
        "screens": {
            "count": 2,
            "desktops": [
                {
                    "count": 4,
                    "inaugural": 0,
                    "layout": {
                        "orientation": "horizontal",
                        "corner": "top-left",
                        "rows": 2,
                        "columns": 2
                    },
                    "viewport": {
                        "columns": 2,
                        "rows": 1
                    },
                    "settings": [
                        { "name": "Desktop 0", "background-color": "#4c5b6b" },
                        { "name": "Desktop 1", "background-color": "#8a8f94" },
                        { "name": "Desktop 2", "background-color": "#6a5470" },
                        { "name": "Desktop 3", "background-color": "#5a7d6f" }
                    ]
                },
                {
                    "count": 2,
                    "inaugural": 0,
                    "settings": [
                        { "name": "Desktop A", "background-color": "#4c5b6b" },
                        { "name": "Desktop B", "background-color": "#8a8f94" }
                    ]
                }
            ]
        }
    },

    "desktops": {
        "show-overlay": true,
        "notify-activity": true,
        "warp-on-edge-drag": true,
        "pan-on-edge-hover": true,
        "wrap-at-bounds": true,
        "margins": { "top": 0, "right": 0, "bottom": 0, "left": 0 }
    },

    "programs": {
        "terminal": "xterm",
        "launcher": "gmrun",
        "editor": "gvim",
        "file-manager": "pcmanfm",
        "web-browser": "firefox"
    },

    "prompt": {
        "is-enabled": true
    },

    "windows": {
        "gravity": "north-west",
        "move-step": 10,
        "snap": 4,
        "show-geom": true,
        "solid-drag": true,
        "focus": {
            "policy": "click",
            "focus-new": true,
            "raise": false,
            "delay-ms": 250
        },
        "placement": {
            "policy": "smart",
            "monitor": "pointer",
            "group-related": true
        }
    },

    "icons": {
        "show-geom": false,
        "placement": {
            "policy": "smart"
        }
    },

    "menus": {
        "root": {
            "position": "under-mouse"
        },
        "windows": {
            "position": "under-mouse"
        }
    },

    "systray": {
        "is-enabled": true,
        "reserve-space": false,
        "avoid-overlap": true,
        "margins": { "top": 0, "right": 0, "bottom": 0, "left": 0 },
        "position": "top-right",
        "monitor": {
            "anchor": "surface",
            "index": 0
        },
        "order": "left-to-right",
        "layer": "below",
        "clock": {
            "is-enabled": true,
            "format": "%a %R"
        },
        "battery": {
            "is-enabled": true,
            "threshold": {
                "charged": 100,
                "low": 20,
                "critical": 5
            },
            "backend": {
                "type": "acpi",
                "number": 0
            },
            "poll-seconds": 30
        },
        "text": {
            "order": [ "battery", "clock" ],
            "position": "right"
        }
    },

    "startup-notification": {
        "timeout-seconds": 20
    },

    "scratchpad": {
        "is-enabled": true,
        "command": "xterm -fg black -bg ivory -cr black",
        "edge": "top",
        "width": "max",
        "height": 200,
        "ignore-margins": false
    },

    "prompt": {
        "is-enabled": false
    },

    "shutdown": {
        "enable-emergency-shortcut": false,
        "timeout-seconds": 15
    },

    "fortune": {
        "is-enabled": false,
        "command": "fortune"
    }
}
```

### `bindings.json`

```json
{
    "modifiers": {
        "modc": "Control",
        "mods": "Shift",
        "modl": "Caps_Lock",
        "mod1": "Alt",
        "mod2": "Num_Lock",
        "mod3": "",
        "mod4": "Super",
        "mod5": "Hyper"
    },

    "keyboard": {
        "wm": {
            "search": "modc+mod1+mods+s",
            "scratchpad": "modc+mod1+mods+F12",
            "redraw": "modc+mod1+mods+r",
            "reload": "modc+mod1+mods+c",
            "fortune": "modc+mod4+Backspace",
            "shortcuts": "modc+mod4+F1",
            "quit": "modc+mod1+mods+x",
            "menus": {
                "root": "modc+mod1+mods+m",
                "windows": "modc+mod1+mods+w"
            },
            "toggle-strutless-maximization": ""
        },

        "desktop": {
            "add": "modc+mod4+mods+Right",
            "remove": "modc+mod4+mods+Left",
            "show": "modc+mod4+mods+d",
            "go-to": {
                "desktop0": "modc+mod1+0",
                "desktop1": "modc+mod1+1",
                "desktop2": "modc+mod1+2",
                "desktop3": "modc+mod1+3",
                "desktop4": "modc+mod1+4",
                "desktop5": "modc+mod1+5",
                "desktop6": "modc+mod1+6",
                "desktop7": "modc+mod1+7",
                "desktop8": "modc+mod1+8",
                "desktop9": "modc+mod1+9"
            }
        },

        "launch": {
            "terminal": "modc+mod1+Enter",
            "launcher": "modc+mod1+r",
            "file-manager": "modc+mod1+q",
            "web-browser": "modc+mod1+w",
            "editor": "modc+mod1+e"
        },

        "window": {
            "arrange": "modc+mod1+mods+a",
            "close": "mod1+modc+c",
            "decorate": "modc+mod1+d",
            "hide": "modc+mod1+mods+u",
            "iconify": "modc+mod1+i",
            "iconify-all": "modc+mod1+mods+i",
            "deiconify-all": "modc+mod1+mods+d",
            "info": "modc+mod4+mods+i",
            "layer": "modc+mod1+mods+y",
            "maximize": "modc+mod1+m",
            "fullscreen": "modc+mod1+f",
            "pin": "modc+mod1+p",
            "shade": "modc+mod1+s",
            "kill": "modc+mod1+mods+Escape",
            "send-to": {
                "desktop": {
                    "north": "modc+mod1+mods+Up",
                    "east": "modc+mod1+mods+Right",
                    "south": "modc+mod1+mods+Down",
                    "west": "modc+mod1+mods+Left"
                },
                "monitor": {
                    "north": "modc+mod1+mod4+mods+Up",
                    "east": "modc+mod1+mod4+mods+Right",
                    "south": "modc+mod1+mod4+mods+Down",
                    "west": "modc+mod1+mod4+mods+Left"
                }
            },
            "move": {
                "relative": {
                    "left": "modc+mod1+h",
                    "down": "modc+mod1+j",
                    "up": "modc+mod1+k",
                    "right": "modc+mod1+l"
                },
                "absolute": {
                    "center": "modc+mod1+g",
                    "top-left": "modc+mod1+y",
                    "top-right": "modc+mod1+u",
                    "bottom-left": "modc+mod1+b",
                    "bottom-right": "modc+mod1+n"
                }
            },
            "resize": {
                "left": "modc+mod1+mods+h",
                "down": "modc+mod1+mods+j",
                "up": "modc+mod1+mods+k",
                "right": "modc+mod1+mods+l"
            }
        },

        "cycle": {
            "desktop": {
                "north": "modc+mod1+Up",
                "west": "modc+mod1+Left",
                "south": "modc+mod1+Down",
                "east": "modc+mod1+Right"
            },
            "icon": {
                "prev": "modc+mod1+mods+Tab",
                "next": "modc+mod1+Tab"
            },
            "window": {
                "prev": "mod1+mods+Tab",
                "next": "mod1+Tab"
            }
        }
    },

    "mouse": {
        "window": {
            "move": "mod1+button1",
            "lower": "mod1+button2",
            "resize": "mod1+button3"
        },

        "cycle": {
            "desktop": {
                "north": "mods+button4",
                "east": "button5",
                "south": "mods+button5",
                "west": "button4"
            }
        }
    }
}
```

### `menu.json`

```json
{
    "menu": [
        { "type": "label",   "name": "Applications" },
        { "type": "command", "name": "Terminal",  "command": "xterm" },
        { "type": "command", "name": "Browser",   "command": "firefox" },
        { "type": "command", "name": "Mail",      "command": "thunderbird" },
        { "type": "separator" },
        { "type": "label",   "name": "Editors" },
        {
            "type": "submenu",
            "name": "Text editors",
            "items": [
                { "type": "command", "name": "Vim",    "command": "gvim" },
                { "type": "command", "name": "Emacs",  "command": "emacs" },
                { "type": "command", "name": "Gedit",  "command": "gedit" }
            ]
        },
        { "type": "submenu",
            "name": "Games",
            "items": [
                { "type": "label", "name": "Roguelike" },
                { "type": "command", "name": "NetHack", "command": "xterm -e nethack" },
                { "type": "separator" },
                { "type": "label", "name": "FPS" },
                { "type": "command", "name": "Nexuiz",  "command": "nexuiz" }
            ]
        },

        { "type": "separator" },
        { "type": "command", "name": "Lock screen", "command": "xsecurelock" }
    ]
}
```

The fixed footer entries (`Reload configuration`, `Redraw all windows`,
`Exit`) are always appended after the user-defined entries and cannot be
overridden.  The `Exit` entry opens the same quit-confirmation dialog as
the keyboard `exit` binding.

### `rules.json`

```json
{
    "rules": [
    {
        "when": "property",
        "match": {
            "title": "Journal console"
        },
        "apply": {
            "desktop": 2,
            "focus": true,
            "layer": "above"
        }
    },
    {
        "when": "map",
        "match": {
            "class": [ "Sonata", "Deadbeef" ],
                "type": "normal"
        },
        "apply": {
            "pinned": true,
            "decorated": false,
            "layer": "below",
            "position": {
                "x": 1460,
                "y": 10
            }
        }
    },
    {
        "when": "map",
        "match": {
            "instance": "gmrun",
            "type": "normal"
        },
        "apply": {
            "decorated": false,
            "layer": "above",
            "position": "center"
        }
    }
    ]
}
```

This example shows three complete rules:

- a `"property"` rule that waits until a window title becomes `"Journal
  console"`, then moves it to desktop of index `2`, focuses it, and
  raises it to the `"above"` layer.
- a `"map"` rule for the program classes `"Sonata"` and "`Deadbeef`"
  that applies once when the window is first managed, keeping it pinned,
  undecorated, in the `"below"` layer, and positioned at the top-right
  corner using a fixed geometry.
- a `"map"` rule for the program instance `"gmrun"` that applies once
  when the window is first managed, keeping it undecorated, in the
  `"above"` layer.

### `session.json`

```json
{
    "on-start": [
        "xsetbg -fullscreen '$HOME/images/background.png'",
        "tint2",
        "picom --config $HOME/.config/picom/picom.conf",
        "nm-applet",
        "volumeicon"
    ],
    "on-reload": [
        "pkill -HUP picom",
        "notify-send -e -i 'configuration' 'IcoWM' 'Configuration reloaded'"
    ],
    "on-exit": [
        "notify-send -e -i 'exit' 'IcoWM' 'Shutting down session hooks'"
    ]
}
```

This example starts a compositor and tray applets when IcoWM launches,
reloads or notifies companion processes after configuration changes, and
emits a final notification on exit.

### `memguard.json`

```json
{
    "theme": "compact",
    "programs": {
        "terminal": "xterm",
        "launcher": "gmrun"
    },
    "prompt": {
        "is-enabled": true
    },
    "desktops": {
        "margins": { "top": 0, "right": 0, "bottom": 0, "left": 0 }
    },
    "windows": {
        "move-step": 10,
        "placement": { "policy": "smart" }
    },
    "icons": {
        "placement": { "policy": "bottom" }
    },
    "systray": {
        "is-enabled": true,
        "clock": { "is-enabled": true, "format": "%a %R" },
        "battery": { "is-enabled": true }
    },
    "shutdown": {
        "enable-emergency-shortcut": true,
        "timeout-seconds": 15
    }
}
```

This example is only ever read when IcoWM is launched with `-M <mib>`;
see `icowm.md`'s "Restricted-memory mode" section for what that flag
does.  It names a theme of its (`themes/compact.json`, not shown here),
keeps the systray's clock and battery on, turns on the emergency
shortcut, since a severely memory-constrained session is exactly the
kind of place where a hung window is more likely and a guaranteed way
out is worth having, and turns on the built-in `prompt` (§2.12) instead
of `programs.launcher` (`gmrun` here) to avoid that extra process
altogether.

### `a11y.json`

```json
{
    "is-enabled": true,
    "interaction": {
        "double-click-ms": 500
    },
    "focus-indicator": {
        "min-border-width": 3
    },
    "urgency": {
        "sound-bell": true,
        "blink-interval-ms": 400
    }
}
```
