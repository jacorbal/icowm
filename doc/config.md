# IcoWM Configuration Reference

Description of every configuration file used by IcoWM, the purpose of
each file, and every supported option together with its type, accepted
values, and built-in default value.

---

## Table of Contents

1. [Directory layout](#1-directory-layout)
2. [`config.json`: Base configuration](#2-configjson-base-configuration)
   - [2.1 `theme`](#21-theme)
   - [2.2 `topology`](#22-topology)
   - [2.3 `programs`](#23-programs)
   - [2.4 `windows`](#24-windows)
   - [2.5 `icons`](#25-icons)
   - [2.6 `show-desktop-overlay`](#26-show-desktop-overlay)
   - [2.7 `enable-emergency-shortcut` / `enable-fortune-shortcut`](#27-enable-emergency-shortcut--enable-fortune-shortcut)
   - [2.8 `startup-notification`](#28-startup-notification)
   - [2.9 `menu`](#29-menu)
   - [2.10 `systray`](#210-systray)
   - [2.11 `desktops`](#211-desktops)
3. [`bindings.json`: Keyboard and mouse bindings](#3-bindingsjson-keyboard-and-mouse-bindings)
   - [3.1 Binding syntax](#31-binding-syntax)
   - [3.2 `modifiers`](#32-modifiers)
   - [3.3 `keyboard.launch`](#33-keyboardlaunch)
   - [3.4 `keyboard.window`](#34-keyboardwindow)
   - [3.5 `keyboard.wm`](#35-keyboardwm)
   - [3.6 `keyboard.cycle`](#36-keyboardcycle)
   - [3.7 `mouse.window`](#37-mousewindow)
   - [3.8 `mouse.cycle`](#38-mousecycle)
4. [`themes/<name>.json`: Theme configuration](#4-themesnamejson-theme-configuration)
   - [4.1 `window`](#41-window)
   - [4.2 `icon`](#42-icon)
   - [4.3 `systray`](#43-systray)
   - [4.4 `desktop`](#44-desktop)
   - [4.5 `menu`](#45-menu)
   - [4.6 `dialog`](#46-dialog)
   - [4.7 `overlay`](#47-overlay)
   - [4.8 `xsettings`](#48-xsettings)
   - [4.9 Configuration reload and already-open windows](#49-configuration-reload-and-already-open-windows)
5. [`randr.json`: XRandR output profiles](#5-randrjson-xrandr-output-profiles)
   - [5.1 Top-level fields](#51-top-level-fields)
   - [5.2 `outputs[]` entries](#52-outputs-entries)
   - [5.3 Scope: per-X-screen, not per-`outputs[]`-entry](#53-scope-per-x-screen-not-per-outputs-entry)
   - [5.4 Reload behavior](#54-reload-behavior)
6. [`rules.json`: Per-window rules](#6-rulesjson-per-window-rules)
   - [6.1 Rule file shape](#61-rule-file-shape)
   - [6.2 Rule entry fields](#62-rule-entry-fields)
   - [6.3 Match fields](#63-match-fields)
   - [6.4 Apply fields](#64-apply-fields)
7. [`session.json`: Session lifecycle hooks](#7-sessionjson-session-lifecycle-hooks)
   - [7.1 Hook arrays](#71-hook-arrays)
8. [`menu.json`: Root desktop menu](#8-menujson-root-desktop-menu)
   - [8.1 Top-level structure](#81-top-level-structure)
   - [8.2 Entry types](#82-entry-types)
   - [8.3 Entry fields reference](#83-entry-fields-reference)
9. [`memguard.json`: Restricted-memory mode configuration](#9-memguardjson-restricted-memory-mode-configuration)
   - [9.1 Configurable fields](#91-configurable-fields)
   - [9.2 Fields this mode never lets `memguard.json` change](#92-fields-this-mode-never-lets-memguardjson-change)
   - [9.3 The active theme's own restrictions](#93-the-active-themes-own-restrictions)
10. [Full examples](#10-full-examples)

For everything that is not a configuration file, namely what IcoWM is,
every command-line option, and restricted-memory mode's own run-time
behavior, see [`manual.md`](manual.md) instead.

---

## 1. Directory layout

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
├── rules.json        Optional matching rules per-window
├── session.json      Command lists run at start, end, or on config. reload
├── memguard.json     Restricted-memory mode ('-M <mib>') configuration
└── themes/
    └── default.json  Theme file referenced by 'config.json'
```

- **All files are optional**; they fall back to built-in defaults when
  absent.  A file that does exist but cannot actually be read as valid
  JSON is a different matter: IcoWM still falls back to defaults for
  it, the same as if it were absent, but also shows a single warning
  dialog listing every such file from the whole startup (or reload,
  or the moment `menu.json` is actually read, which only happens the
  first time the root menu is opened) together, rather than one
  dialog per file.  A missing file is an ordinary, silent choice to
  use the defaults; a broken one is worth knowing about.  A theme file
  named by a correctly-parsed `config.json` is treated the same way as
  any other file for this purpose (a syntax error in it joins that
  same combined dialog); one that is simply not found at all follows
  the ordinary missing-file rule and says nothing on its own, but if a
  dialog is already being shown for some other file's syntax error
  regardless, it adds one further line naming the missing theme file
  and confirming the built-in default is being used instead.
- Theme files are loaded from the `themes/` sub-directory.  The theme
  name field in `config.json` must match the filename without the
  `.json` extension.
- `randr.json`; XRandR hot-plug event handling is always active
  regardless of this file existence.
- None of the files above configure IcoWM's own IPC control socket:
  it has no options of its own to set, and is either brought up or,
  with `-s`, deliberately skipped for that run (`manual.md` section
  3.1). See `manual.md` section 5 for where it lives and its full
  wire protocol.

## 2. `config.json`: Base configuration

Controls the fundamental behavior of the window manager: screens,
virtual desktops, default programs, window management policies, and
icon placement.

### 2.1 `theme`

| Key     | Type   | Default               |
|---------|--------|-----------------------|
| `theme` | string | `""` (built-in theme) |

Name of the theme to load, without the `.json` extension.  The file
`themes/<value>.json` is looked up inside the configuration directory.
An empty string or an omitted key causes the built-in default theme to
be used.

```json
"theme": "default"
```

### 2.2 `topology`

Configures the number of physical screens and the virtual desktops
assigned to each.  **Takes effect at startup only**: changing anything
under `topology` and reloading the configuration has no effect on an
already-running window manager (see section 4.9).  For desktop
behavior that *does* reload, namely warp, cycle, and reserved margins,
see section 2.11 (`desktops`) instead, a deliberately separate,
sibling section for exactly that reason.

#### `topology.screens.count`

| Key                      | Type    | Default |
|--------------------------|---------|---------|
| `topology.screens.count` | integer | `1`     |

Number of physical screens (monitors) to manage.  Maximum is `6`.

#### `topology.screens.desktops[]`

The `desktops` array sits directly under `topology.screens`; there
is no intervening `settings` object.  It accepts two layouts:

**Simple layout** (one screen, desktops listed directly):

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

**Per-screen layout** (each array entry represents one screen):

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
(whether it carries its own `settings`/`count`/`inaugural` fields).
Note that the per-screen layout's own per-desktop `settings[]` array
(holding `name`/`background-color`) is a different, unrelated thing
from the `topology.screens.settings` object this schema no longer
has: that inner `settings[]` was never removed, only the outer one
that used to wrap `desktops` was.

Per-screen layout fields:

| Key                           | Type    | Default      | Description |
|-------------------------------|---------|--------------|-------------|
| `count`                       | integer | `4`          | Number of virtual desktops for this screen (or `CONFIG_MAX_DESKTOPS` if that is smaller than `4`). Maximum is `10`. |
| `inaugural`                   | integer | `0`          | Zero-based index of the desktop shown at startup.  Values out of range fall back to `0`. |
| `settings[].name`             | string  | `"Desktop N" | Display name of desktop N. |
| `settings[].background-color` | string  | none         | Root background color as a hex color `"#RRGGBB"` or `"RRGGBB"`.  Left unset, a desktop falls back to `theme.desktop.color.background` (section 4.4). |

### 2.3 `programs`

Associates application categories with the executables IcoWM will
launch for the corresponding keyboard shortcuts.

| Key                     | Type   | Default     | Description          |
|-------------------------|--------|-------------|----------------------|
| `programs.terminal`     | string | `"xterm"`   | Terminal emulator.   |
| `programs.launcher`     | string | `"gmrun"`   | Application launcher |
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

### 2.4 `windows`

Controls window behavior: gravity, edge snapping, resize grips, focus
policy, and placement policy.

#### `windows.gravity`

| Key               | Type   | Default        |
|-------------------|--------|----------------|
| `windows.gravity` | string | `"north-west"` |

Default window gravity, i.e., the corner used as the reference point
for window coordinates.

Accepted `windows.gravity` values: `"north-west"`, `"north"`,
`"north-east"`, `"east"`, `"south-east"`, `"south"`, `"south-west"`,
`"west"`, `"center"`, `"static"`.

#### `windows.snap`

| Key            | Type    | Default |
|----------------|---------|---------|
| `windows.snap` | integer | `4`     |

Snap threshold in pixels.  When a window being dragged comes within
this many pixels of a screen edge or another window border, it snaps
into alignment.  Set to `0` to disable snapping.

#### `windows.move-step`

| Key                 | Type    | Default |
|---------------------|---------|---------|
| `windows.move-step` | integer | `10`    |

Keyboard movement step in pixels.  Each key press that moves the focused
window changes its position by this amount.  Values lower than `1` are
treated as `1`.

#### `windows.show-geom`

| Key                 | Type    | Default |
|---------------------|---------|---------|
| `windows.show-geom` | boolean | `true`  |

When `true`, geometry when moving (mouse drag) or position when resizing
(with mouse drag) is shown in the center of the window.

#### `windows.focus`

| Key                        | Type    | Default   | Description |
|----------------------------|---------|-----------|-------------|
| `focus.policy`             | string  | `"click"` | Focus policy. `"click"` requires a click to focus; `"follow-mouse"` focuses whichever window is under the pointer. |
| `focus.is-new-focused`     | boolean | `true`    | When `true`, newly mapped windows receive focus automatically. |
| `focus.is-raised-on-focus` | boolean | `false`   | When `true`, a window is raised to the top of the stack when it receives focus. |

```json
"windows": {
    "focus": {
        "policy": "click",
        "is-new-focused": true,
        "is-raised-on-focus": false
    }
}
```

#### `windows.placement`

| Key                       | Type    | Default     | Description |
|---------------------------|---------|-------------|-------------|
| `placement.policy`        | string  | `"smart"`   | How newly mapped windows are placed. |
| `placement.monitor`       | string  | `"pointer"` | Which physical monitor a placement decision targets, on a surface with more than one. |
| `placement.group-related` | boolean | `true`      | Cluster windows of the same application together. |

Accepted placement policy values:

| Value           | Behavior |
|-----------------|----------|
| `"cascade"`     | Places windows in a stepped diagonal sequence. |
| `"centered"`    | Centers the window on the screen. |
| `"under-mouse"` | Places the window under the current pointer position. |
| `"smart"`       | Finds the position that minimizes overlap with existing windows. |

Accepted placement monitor values (only meaningful on a surface made up
of more than one physical monitor sharing the same combined X screen;
has no effect otherwise):

| Value       | Behavior |
|-------------|----------|
| `"pointer"` | Targets whichever monitor the pointer is currently on: not necessarily where on that monitor the pointer actually is, only which one it is on, so the window can still land far from the cursor within it depending on `placement.policy`. |
| `"primary"` | Always targets the monitor RandR reports as primary. |

Transient (dialog) windows are always centered over their parent window,
regardless of `placement.policy`, and windows clustered by
`group-related` (below) are always placed next to the sibling they are
grouped with; both also always target whichever monitor that parent or
sibling is actually on, regardless of `placement.monitor`, since neither
case is about picking a monitor for a window with no better signal to
go on: they already have one.

When `group-related` is `true` (the default), a newly mapped window
whose `WM_CLIENT_LEADER` (or, failing that, its `WM_HINTS` window group)
matches another currently visible window's is placed offset from that
group instead of running the policy above for it, i.e., a second,
third,... window opened by the same application lands next to the others
instead of wherever `policy` would otherwise put it.  Set it to `false`
to always use `policy` for every window, with no special-casing for
related ones.

This is named after what it actually groups by (an application's stated
client/window group), not by `WM_CLASS`, since not every application
that opens several related windows gives them all the exact same class
name.

```json
"windows": {
    "placement": {
        "policy": "smart",
        "monitor": "pointer",
        "group-related": true
    }
}
```

### 2.5 `icons`

#### `icons.show-geom`

| Key               | Type    | Default |
|-------------------|---------|---------|
| `icons.show-geom` | boolean | `true`  |

When `true`, geometry when moving (mouse drag) is shown in the center of
the icon.

#### `icons.placement`

Controls how iconified windows are laid out on the desktop.

| Key                      | Type   | Default   | Description |
|--------------------------|--------|-----------|-------------|
| `icons.placement.policy` | string | `"smart"` | How new icons are placed. |

Accepted icon placement values:

| Value      | Behavior                                                    |
|------------|-------------------------------------------------------------|
| `"bottom"` | Icons fill the bottom row of the screen from left to right. |
| `"top"`    | Icons fill the top row from left to right.                  |
| `"left"`   | Icons fill the left column from top to bottom.              |
| `"right"`  | Icons fill the right column from top to bottom.             |
| `"smart"`  | Icons are placed in the first available free slot.          |

```json
"icons": {
    "placement": {
        "policy": "smart"
    }
}
```

### 2.6 `show-desktop-overlay`

| Key                    | Type    | Default |
|------------------------|---------|---------|
| `show-desktop-overlay` | boolean | `true`  |

When `true`, a small notification popup is displayed in the center of
the screen for approximately 400 ms whenever the active virtual desktop
changes.  The popup shows the desktop index and name in the format
`[index] -- Name`, or just `[index]` in the case when the desktop has no
name.  Set to `false` to suppress the popup entirely.

```json
"show-desktop-overlay": true
```

### 2.7 `enable-emergency-shortcut` / `enable-fortune-shortcut`

| Key                         | Type    | Default |
|-----------------------------|---------|---------|
| `enable-emergency-shortcut` | boolean | `false` |
| `enable-fortune-shortcut`   | boolean | `false` |

Two hardcoded shortcuts, each off by default and each independently
gated by its own boolean here rather than being configurable via
`bindings.json` like a normal keybinding.

When `true`, `enable-emergency-shortcut` activates
`Ctrl+Mod1+BackSpace`, which immediately terminates the window manager
ignoring pending session hooks.  Set to `false` (the default) to
disable that shortcut, for example on systems where the key
combination might be triggered accidentally.  While enabled, that
exact key combination cannot be reused by any binding in
`bindings.json`, whether that would happen intentionally or by
accident: any such binding is ignored (with a warning logged) so the
emergency exit always keeps `Ctrl+Mod1+BackSpace` to itself.

When `true`, `enable-fortune-shortcut` activates `Ctrl+Mod4+BackSpace`
(mirroring the emergency exit combination above but with `Mod4` in
place of `Mod1`, keeping the two visually and mnemonically distinct),
which opens a small dialog showing the output of the `fortune`
command, or, if `fortune` is not installed, an in-joke message
suggesting it should be.  Purely for fun; harmless to leave off, and
harmless to turn on.

```json
"enable-emergency-shortcut": true,
"enable-fortune-shortcut": true
```

### 2.8 `startup-notification`

| Key                                    | Type    | Default |
|----------------------------------------|---------|---------|
| `startup-notification.timeout-seconds` | integer | `20`    |

How long a startup-notification sequence (the busy cursor shown while
a launched application is starting up, see the freedesktop.org
Startup Notification specification) waits before being expired
automatically.  Not every application is startup-notification aware,
so this is what keeps the busy cursor from staying on indefinitely
when a launched process never signals that it is ready.  Raise it for
applications that are slow to show their first window (some office
suites, for example); lower it if 20 seconds feels like it lingers
too long for the applications actually launched day to day.

```json
"startup-notification": {
    "timeout-seconds": 20
}
```

### 2.9 `menu`

| Key                      | Type   | Default         |
|--------------------------|--------|-----------------|
| `menus.root.position`    | string | `"under-mouse"` |
| `menus.windows.position` | string | `"under-mouse"` |

Controls where a menu appears when it is opened by a means with no
screen position of its own, such as a keyboard shortcut, one setting per
menu type: `root` is the desktop context menu (`menu.json`, opened by
`keyboard.wm.menus.root`, see [3.5 `keyboard.wm`](#35-keyboardwm));
`windows` is the menu listing every window on every desktop (opened by
`keyboard.wm.menus.windows`). Supported values are `"center"`, which
always opens the menu in the center of the screen, and `"under-mouse"`,
which opens it under the current mouse pointer position instead,
matching the naming of `windows.placement.policy`.

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

### 2.10 `systray`

| Key                     | Type    | Default           |
|-------------------------|---------|-------------------|
| `systray.is-enabled`    | boolean | `true`            |
| `systray.reserve-space` | boolean | `false`           |
| `systray.margins`       | object  | see below         |
| `systray.position`      | string  | `"top-right"`     |
| `systray.monitor`       | object  | see below         |
| `systray.order`         | string  | `"left-to-right"` |
| `systray.layer`         | string  | `"below"`         |

Built-in systray dock.  `is-enabled` turns it on, and `position` (one of
`"top-left"`, `"top-right"`, `"bottom-left"`, or `"bottom-right"`)
selects which corner it docks in; which area that corner is measured
against is `monitor`'s job, described next.

`reserve-space` controls whether the tray publishes its own
`_NET_WM_STRUT_PARTIAL`/`_NET_WM_STRUT`, reserving its own on-screen
area the same way an external panel or dock does, so maximized windows
and this window manager's own placement logic both leave it alone,
per the specification's own recommendation for a docking area, a
taskbar, or a panel.  `false` by default: an explicit `{0, 0, 0, 0}`
strut, reserving nothing, the same as if the tray were not there at
all for placement purposes.  Set to `true` for the tray to reserve its
own space instead, e.g., for a `layer` other than `"above"` or
`"overlay"`, where nothing else already keeps windows off the tray
visually.

`margins` (an object with `top`/`right`/`bottom`/`left` integers, all
`0` by default) adds extra reserved space on top of whatever the
tray's own actual size and position already reserve, mirroring
`desktops.margins` (section 2.11) exactly, including that it is not
restricted to whichever edge the tray currently docks at: a `left` or
`right` value still reserves space on that side even while the tray
itself sits at the top or bottom.  Has no effect while `reserve-space`
is `false`.

```json
"systray": {
    "reserve-space": false,
    "margins": { "top": 0, "right": 0, "bottom": 0, "left": 0 }
}
```

`monitor` selects which physical monitor `position`'s corner is
measured against, on a surface made up of more than one sharing the
same combined X screen; it has no effect otherwise.  It is an object
with an `anchor` field and, only when `anchor` is `"index"`, an
`index` field:

| Value       | Behavior |
|-------------|----------|
| `"surface"` | Measures `position` against the whole combined surface, exactly as if there were only one monitor (default). |
| `"primary"` | Measures `position` against whichever monitor RandR reports as primary. |
| `"index"`   | Measures `position` against `monitor.index` specifically, a zero-based index into that surface's own monitor list.  Falls back to monitor `0` if it does not exist, logging a warning. |

```json
"systray": {
    "is-enabled": true,
    "position": "top-right",
    "monitor": { "anchor": "primary" }
}
```

Only one tray dock ever exists at a time, regardless of `monitor`: the
`_NET_SYSTEM_TRAY_Sn` manager selection this implements is one per
screen by its own specification (see below), so a genuinely
independent tray dock per monitor, each accepting its own icons, is
not something any implementation of this protocol can offer, IcoWM
included.  `monitor` only changes which single monitor the one dock
IcoWM does provide sits on.

The key `order` controls where a newly docked icon is placed relative to
the ones already there: `"left-to-right"` appends it after the last
icon, `"right-to-left"` inserts it before the first, and `"ascending"`
/ `"descending"` instead keep the whole row continuously sorted
alphabetically ('A-Z' or 'Z-A') by each icon's window class name,
ignoring insertion order entirely.

The key `layer` controls where the dock sits in the stacking order:
`"below"` (the default) keeps it behind every normal client window,
`"above"` keeps it above normal windows but still lets a fullscreen
window cover it, the same way a fullscreen window covers a taskbar or
panel in most desktop environments, and `"overlay"` keeps it above
absolutely everything, including fullscreen windows.

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
| `systray.clock.format`     | string  | `"%H:%M"` |

An optional clock drawn inside the systray dock.  `is-enabled` turns
it on; when it is the only reason the tray would otherwise stay
hidden (no icons docked), the tray still shows with just the clock.
Where it is positioned and aligned is shared with `systray.battery`
below; see `systray.text`.

`format` is a `strftime(3)` format string, interpreted in the system's
local time zone.  A few common examples:

| `format`     | Looks like         |
|--------------|--------------------|
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
Linux), and `"apm"` reads the older `/proc/apm` for hardware or
kernels without ACPI.  `backend.number` selects which battery to read
when a system has more than one (0-indexed, e.g., `1` for `BAT1`);
it is ignored under `"apm"`, which only ever exposes one aggregate
battery regardless of how many cells the system actually has.
`poll-seconds` is how often the status is re-read; a percentage does
not need per-second freshness the way a clock does, so raise it to
poll less often (saving the handful of file reads each poll costs) or
lower it for a battery that drains quickly enough that 30 seconds
feels stale.

The status text's exact shape depends on both AC power and how the
battery's charge compares to `threshold`:

| State                                          | Text        |
|------------------------------------------------|-------------|
| On battery, above `low`                        | `"X%"`      |
| On battery, at/below `low`                     | `"X%!"`     |
| On battery, at/below `critical`                | `"X%!!"`    |
| On AC, not fully charged                       | `"X% AC"`   |
| Fully charged (`charged` or above), on battery | `"Full"`    |
| Fully charged, on AC                           | `"Full AC"` |
| No battery found for `backend`                 | `"N/A"`     |

A battery counts as "fully charged" once its percentage reaches
`threshold.charged`, regardless of what the kernel itself reports as
its charging state: some hardware never reports "full" even sitting
at 100% on AC power, so going by the percentage alone reads correctly
across more machines than trusting the kernel's own status string
would.  The status is re-read every 30 seconds; a percentage does not
need per-second freshness the way a clock does.  Where it is
positioned and aligned is shared with `systray.clock` above; see
`systray.text`.

#### `systray.text`

| Key                     | Type            | Default                  |
|-------------------------|-----------------|--------------------------|
| `systray.text.order`    | array of string | `[ "battery", "clock" ]` |
| `systray.text.position` | string          | `"right"`                |

Shared placement for the clock and battery status text: which of the
two show, in what left-to-right order.  `order` lists the enabled
items to show, by name (`"clock"` and/or `"battery"`); an item absent
from this list never shows even if its own `is-enabled` is `true`,
and one with `is-enabled` set to `false` is skipped even when listed
here.  Both entries are optional; an empty list shows neither,
regardless of their individual `is-enabled` settings.

`position` (`"left"` or `"right"`) controls whether the whole text
block sits before or after the icons, in dock order; it does not
affect which corner of the screen the tray itself sits in, which is
still `systray.position` above.  How the text looks once shown (the
gap between the two items, and their vertical alignment within the
tray) is a theme setting rather than a behavior one; see
`systray.text` in the theme documentation (section 4.3).

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

### 2.11 `desktops`

Desktop-navigation and reserved-space behavior: how switching between
desktops behaves at the two ends, whether dragging a window past a
screen edge switches desktops with it, and how much of every
desktop's own area stays reserved regardless of what any client
itself publishes.  A sibling of `topology` (section 2.2) at the root
of `config.json`, not nested inside it: deliberately so, since unlike
`topology`, everything here **does** take effect on a configuration
reload (see section 4.9).

| Key              | Type    | Default | Description |
|------------------|---------|---------|-------------|
| `warp`           | boolean | `true`  | While dragging a window or icon to move it, holding the pointer against the left or right screen edge switches to the adjacent desktop, cursor and dragged window or icon both carried across, after a short delay. Meaningless with only one desktop. |
| `cycle`          | boolean | `true`  | Whether switching past the first or last desktop, however it is triggered (keyboard binding, mouse scroll, or otherwise), wraps around to the other end, rather than stopping there. Meaningless with only one desktop. |
| `margins.top`    | integer | `0`     | Extra space reserved at the top of every desktop's own workarea, in pixels, on every screen. |
| `margins.right`  | integer | `0`     | Extra space reserved on the right, in pixels. |
| `margins.bottom` | integer | `0`     | Extra space reserved at the bottom, in pixels. |
| `margins.left`   | integer | `0`     | Extra space reserved on the left, in pixels. |

`margins` adds on top of whatever space a client already reserves for
itself via `_NET_WM_STRUT`/`_NET_WM_STRUT_PARTIAL` (a panel or dock,
say) rather than overriding it: the two are meant to coexist, not
compete. It exists for a program that reserves screen space without
publishing either property itself (a desktop widget like Conky is the
classic example): configuring a margin here reserves that space for
it, the same way maximizing a window or its initial placement already
respects a panel's own published strut. `margins` applies identically
to every desktop on every screen; there is no per-desktop or
per-screen override.

```json
"desktops": {
    "warp": true,
    "cycle": true,
    "margins": {
        "top": 0,
        "right": 0,
        "bottom": 0,
        "left": 0
    }
}
```

## 3. `bindings.json`: Keyboard and mouse bindings

Defines all keyboard shortcuts and mouse button bindings.  This file is
optional; if absent, the built-in defaults listed in the tables below
are used.

### 3.1 Binding syntax

A binding is a `+`-separated chain of modifier aliases and a final key
or button name:

```
modifier1+[modifier2+[modifier3+]]KeyName
```

- **Modifier aliases** are the symbolic names defined in the
  `modifiers` section (e.g., `modc`, `mod1`).  Using aliases instead
  of literal key names (e.g., `Control`, `Alt`) makes it easy to
  remap all shortcuts by changing only the `modifiers` section.
- **Key names** are standard X11 keysym names (e.g., `Return`, `Tab`,
  `Left`, `a`, `F1`).
- **Mouse button names** are `button1` through `button5`.
- An empty string `""` means *unbound* (the action has no shortcut).

Example:  `"modc+mod1+Return"` with the default modifiers resolves to
`Control+Alt+Return`.

### 3.2 `modifiers`

Symbolic names for modifier keys.  Every binding that references a
modifier uses one of these aliases.

| Alias  | Default X11 key | Description                   |
|--------|-----------------|-------------------------------|
| `modc` | `Control`       | Control key                   |
| `mods` | `Shift`         | Shift key                     |
| `modl` | `Caps_Lock`     | Caps Lock                     |
| `mod1` | `Alt`           | Alt / Meta key                |
| `mod2` | `Num_Lock`      | Num Lock                      |
| `mod3` | `""`            | Unassigned (empty by default) |
| `mod4` | `Super`         | Super / Windows key           |
| `mod5` | `Hyper`         | Hyper key                     |

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

### 3.3 `keyboard.launch`

Shortcuts for launching external applications.  The executables are
taken from the `programs` section of `config.json`.

| Key            | Default binding    | Action                           |
|----------------|--------------------|----------------------------------|
| `terminal`     | `modc+mod1+Return` | Launch the terminal emulator.    |
| `launcher`     | `modc+mod1+r`      | Launch the application launcher. |
| `file-manager` | `modc+mod1+q`      | Launch the file manager.         |
| `web-browser`  | `modc+mod1+w`      | Launch the web browser.          |
| `editor`       | `modc+mod1+e`      | Launch the text editor.          |

### 3.4 `keyboard.window`

Actions performed on the currently focused window.

#### Direct window actions

| Key             | Default binding         | Action |
|-----------------|-------------------------|--------|
| `close`         | `modc+mod1+c`           | Send `WM_DELETE_WINDOW` to politely close the window. |
| `kill`          | `modc+mod1+mods+Escape` | Forcibly terminate the client process. |
| `iconify`       | `modc+mod1+i`           | Iconify the window (TWM-style desktop icon). |
| `iconify-all`   | `modc+mod4+mods+i`      | Iconify (minimize) every client on the current desktop. |
| `deiconify-all` | `modc+mod4+mods+d`      | Restore every iconified client on the current desktop. |
| `arrange`       | `modc+mod1+mods+a`      | Re-apply the configured placement policy to every client on the current desktop, spreading them back out. A transient dialog among them is re-centered over its own parent instead (ICCCM §4.1.2.6). |
| `hide`          | `modc+mod1+mods+u`      | Hide the window without iconifying it. |
| `maximize`      | `modc+mod1+m`           | Toggle maximize (full work area). |
| `next-monitor`  | `modc+mod1+mods+n`      | Move the window to the next monitor, on a surface with more than one; no effect otherwise. |
| `fullscreen`    | `modc+mod1+f`           | Toggle true fullscreen mode. |
| `shade`         | `modc+mod1+s`           | Roll-up / roll-down the window (shade). |
| `pin`           | `modc+mod1+p`           | Toggle sticky mode (window appears on all desktops). |
| `decorate`      | `modc+mod1+d`           | Toggle window decorations (title bar). |
| `layer`         | `modc+mod1+mods+y`      | Cycle the window stacking layer: *normal* > *above* > *below*. |
| `info`          | `modc+mod1+mods+i`      | Show a popup with window information. |
| `show-desktop`  | `modc+mod1+mods+d`      | Toggle show-desktop mode: hide all windows; press again to restore them. |

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

### 3.5 `keyboard.wm`

Window manager control shortcuts.

| Key            | Default binding    | Action |
|----------------|--------------------|--------|
| `search`       | `modc+mod4+mods+s` | Open the fuzzy window-search widget. |
| `show-desktop` | `modc+mod1+mods+d` | Hide all windows and show the empty desktop. |
| `redraw`       | `modc+mod1+mods+r` | Force a full redraw of all windows. |
| `reload`       | `modc+mod1+mods+c` | Reload the configuration files (equivalent to `SIGHUP`). |
| `quit`         | `modc+mod1+mods+x` | Exit IcoWM. |
| `shortcuts`    | `modc+mod4+F1`     | Show a dialog listing every currently active keyboard shortcut. |

`search` opens a centered, live-filtered list of every window across
every desktop.  Typing narrows the list by fuzzy subsequence match
against each window's name (the typed characters must appear in
order, but not necessarily contiguous); `Up`/`Down` or the mouse
select a row, `Return` or a click confirms it, and `Escape` cancels.
Confirming switches to the window's desktop, restores it first if it
was iconified, hidden, or shaded, then focuses and raises it.  Each
row shows the window's icon (when `theme.menu.show-pixmaps` is
enabled), its name, its desktop's name (when the surface has more
than one desktop), and any bracketed state hints that apply
(`f`/`m`/`h`/`v` for fullscreen or one of the maximized variants,
`s` for shaded, `p` for pinned/sticky, `!` for urgent).

`shortcuts` opens a dialog listing every active keyboard binding
described in this section, grouped by category and read directly from
the configuration actually in effect, so it always matches what is
really bound rather than a separately maintained description of the
defaults.  `F1` is used here since it conventionally means "help" on
most keyboards, and `modc+mod4` is unlikely to already be claimed by
another running application.

#### `keyboard.wm.menus`

Keyboard shortcuts for the two menus that have no inherent screen
position of their own (see `config.menus.*` in `config.json` for
where each one appears when opened this way).


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

#### `keyboard.wm.go-to`

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
`keyboard.wm.go-to` binding.

### 3.6 `keyboard.cycle`

Shortcuts for cycling through desktops, iconified windows, and
open windows.

#### `keyboard.cycle.desktop`

| Key    | Default binding   | Action                                  |
|--------|-------------------|-----------------------------------------|
| `prev` | `modc+mod1+Left`  | Switch to the previous virtual desktop. |
| `next` | `modc+mod1+Right` | Switch to the next virtual desktop.     |

#### `keyboard.cycle.window`

Keyboard Alt+Tab-style navigation through open (non-iconified) windows.

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

### 3.7 `mouse.window`

Mouse button bindings for window management.

| Key      | Default binding | Action |
|----------|-----------------|--------|
| `move`   | `mod1+button1`  | Click and drag to move the window. |
| `lower`  | `mod1+button2`  | Lower the window to the bottom of the stack. |
| `resize` | `mod1+button3`  | Click and drag to resize the window. |

### 3.8 `mouse.cycle`

Mouse button bindings for switching virtual desktops.

| Key                  | Default binding | Action |
|----------------------|-----------------|--------|
| `cycle.desktop.prev` | `button4`       | Scroll up to go to the previous desktop. |
| `cycle.desktop.next` | `button5`       | Scroll down to go to the next desktop. |

## 4. `themes/<name>.json`: Theme configuration

Controls the visual appearance of windows, desktop icons, and the
systray.  Theme files live in the `themes/` subdirectory of the
configuration directory.  The name in `config.json`'s `"theme"` field
selects which file is loaded.

All color values are hex strings in the form `"#RRGGBB"` or `"RRGGBB"`.

`name` is the only top-level field IcoWM actually reads.  `author`,
`creation-date`, and `modified-date` may also appear at the top level,
but they are purely comments for whoever maintains the file; IcoWM
never parses or acts on them.

```json
{
    "name": "Default theme",
    "author": "Jane Doe",
    "creation-date": "Sat Aug  1 03:57:54 UTC 2026",
    "modified-date": "Sat Aug  7 15:08:21 UTC 2026",

    "window": { "...": "..." },
    "icon": { "...": "..." },
    "systray": { "...": "..." }
}
```

### 4.1 `window`

Appearance settings for managed windows.

| Key            | Type    | Default | Description |
|----------------|---------|---------|-------------|
| `is-decorated` | boolean | `true`  | When `false`, windows start without any decoration (no title bar, no themed border).  Equivalent to setting `titlebar.height` to `0`; see below. |

#### `window.titlebar`

| Key                  | Type             | Default | Description |
|----------------------|------------------|---------|-------------|
| `height`             | integer          | `19` | Title bar height in pixels.  A value of `0` is equivalent to `window.is-decorated: false`: with nothing to draw and nowhere to put buttons, the window is treated as undecorated regardless of `is-decorated`'s own value. |
| `alignment`          | string           | `"left"` | Where the title text sits within the space its buttons leave available.  One of `"left"`, `"center"`, `"right"`. |
| `padding.horizontal` | integer          | `2` | Horizontal inset, in pixels, between the frame's edge and its outermost buttons on each side, and between a button group and the title text. |
| `padding.vertical`   | integer          | `2` | Vertical inset, in pixels, buttons are kept from the titlebar's top and bottom edge before being centered in whatever room that leaves.  If the titlebar is too short for the padding to fit a full button, this is ignored in favor of plain centering. |
| `buttons.left`       | array of strings | `["pin", "layer"]` | Buttons drawn left-to-right starting at the frame's left edge. |
| `buttons.right`      | array of strings | `["iconize", "hide", "shade", "maximize", "fullscreen", "close"]` | Buttons drawn right-to-left starting at the frame's right edge. |
| `buttons.color.on`   | string           | `"#253040"` | Color for a button whose own state is currently engaged: pinned, a non-normal layer, or simply the window being focused for every other button. |
| `buttons.color.off`  | string           | `"#4A5566"` | Color for a button otherwise, i.e., not engaged. |

Accepted button names, for both `buttons.left` and `buttons.right`,
are: `"pin"`, `"layer"`, `"iconize"`, `"hide"`, `"shade"`,
`"maximize"`, `"fullscreen"`, `"close"`.  A button omitted from both
lists is simply never drawn and never clickable; there is no separate
setting to hide a button.  The same name can only usefully appear
once across both lists (whichever list is processed for it first
wins its slot; putting it in both does not draw it twice).

`buttons.color` is independent of `window.active`/`window.inactive`'s
own `color.foreground` below, so a theme can restyle button glyphs
without the title text changing color to match, or the other way
around.  There is deliberately no third color for a button that
cannot currently do anything (e.g., maximize on a non-resizable
client): that button is not drawn at all rather than needing a color
of its own for that case.

#### `window.active` / `window.inactive`

Appearance of the focused window (`active`) and of windows that do not
have focus (`inactive`).  Both share the same shape:

| Key                | Type    | Default (active) | Default (inactive) | Description |
|--------------------|---------|------------------|--------------------|-------------|
| `font`             | string  | `"fixed bold"`   | `"fixed"`          | Title bar font (see note below). |
| `color.background` | string  | `"#9AAEC8"`      | `"#D0D9E5"`        | Title bar background color. |
| `color.foreground` | string  | `"#253040"`      | `"#4A5566"`        | Title bar text color. |
| `border.color`     | string  | `"#4A5566"`      | `"#7F9AB6"`        | Border color. |
| `border.width`     | integer | `2`              | `2`                | Border thickness in pixels. |

`border.width` need not match between `active` and `inactive`.  When
they differ, a decorated window's frame actually grows or shrinks by
the difference every time it gains or loses focus, so its content
never has to resize; see the note on configuration reload below for
the one case this resizing does not happen automatically.

### 4.2 `icon`

Appearance settings for iconified windows.

| Key            | Type    | Default | Description |
|----------------|---------|---------|-------------|
| `is-captioned` | boolean | `true`  | When `true`, the icon displays the window title below the icon graphic. |
| `show-pixmaps` | boolean | `true`  | When `true`, draws the client's own `_NET_WM_ICON` image, centered in and clipped to the icon's own square graphic area, above the caption (the two never overlap). Not every application publishes this property; one that does not simply shows no icon graphic, same as when this is `false`. Scaled to a consistent size regardless of whichever size the application published, since these vary widely from one application to another; see `WM_ICON_PIXMAP_SCALE` in `defs/icon.h` for that fraction of the icon square the image is scaled to fill (not currently configurable from a JSON file, only at compile time). The built image is cached per client and only rebuilt when the application actually changes its `_NET_WM_ICON` property; every other redraw (an unrelated window on the same desktop moving, an `Expose` after a virtual terminal switch, cycling selection past it) reuses the cached one instead of re-fetching and re-processing the same image again.  Forced to `false` automatically in restricted-memory mode (see `-M`), regardless of what this file says. |
| `show-hints`   | boolean | `true`  | When `true`, draws small state-hint indicators in the icon's own top corners: a filled square in the top-left when the client is sticky/pinned, and a single letter in the top-right for whichever state it was in right before being iconified (`f`: fullscreen; `m`: maximized; `h`: maximized horizontally; `v`: maximized vertically; none for plain normal). |

#### `icon.active` / `icon.inactive`

Same shape as `window.active` / `window.inactive` above (`font`,
`color.background`, `color.foreground`, `border.color`,
`border.width`), applied to the icon selected in the icon-cycle menu
(`active`) versus every other icon (`inactive`).

| Key                | Type    | Default (active) | Default (inactive) |
|--------------------|---------|------------------|--------------------|
| `font`             | string  | `"fixed bold"`   | `"fixed"`          |
| `color.background` | string  | `"#9AAEC8"`      | `"#D0D9E5"`        |
| `color.foreground` | string  | `"#253040"`      | `"#4A5566"`        |
| `border.color`     | string  | `"#4A5566"`      | `"#7F9AB6"`        |
| `border.width`     | integer | `1`              | `1`                |

### 4.3 `systray`

A `font` / `color` / `border` block, the same shape as `window.active`
above, applied to the systray dock itself, plus its own height, each
docked icon's own size and padding, and the placement of the
clock/battery text within it.

| Key                | Type    | Default     |
|--------------------|---------|-------------|
| `font`             | string  | `"fixed"`   |
| `color.background` | string  | `"#D0D9E5"` |
| `color.foreground` | string  | `"#4A5566"` |
| `border.color`     | string  | `"#7F9AB6"` |
| `border.width`     | integer | `1`         |
| `height`           | integer | `22`        |
| `pixmap.size`      | integer | `24`        |
| `pixmap.padding`   | integer | `4`         |
| `text.gap`         | integer | `12`        |
| `text.valign`      | string  | `"center"`  |

`pixmap.size` is the side length, in pixels, every docked icon's own
embed window is forced to regardless of whatever size it originally
requested; `pixmap.padding` is the space, in pixels, kept around and
between icons.

`height` is the tray dock's own height in pixels; icons and the clock
and/or battery status text (when either is enabled, see
`systray.clock`/`systray.battery` in `config.json`) are positioned
within it according to `text.valign`, and centered for icons.  It is
clamped up to at least `pixmap.size` if set any smaller, so a single
icon never gets clipped; with the default `height` of `22` actually
sitting below the default `pixmap.size` of `24`, that clamp is exactly
what applies in practice, leaving `text.valign` no visible room to
work with until `height` is raised past `pixmap.size`.

`text.gap` is the horizontal space, in pixels, between the clock and
battery text when both are shown (see `systray.text.order` in
`config.json`); without it the two would run together as if they
were one string, e.g., "N/A Fri 23:39" instead of "N/A   Fri 23:39".
It has no effect on the inset between the text block as a whole and
the tray's own edges, which is fixed to `pixmap.padding` above.
Which side of the icons the text sits on (`systray.text.position` in
`config.json`) stays a behavior setting rather than an appearance
one, since it changes where among the icons the text counts as being
docked; only its internal spacing and vertical alignment are theme
concerns.

```json
"systray": {
    "font": "fixed",
    "color": { "background": "#D0D9E5", "foreground": "#4A5566" },
    "border": { "color": "#7F9AB6", "width": 1 },
    "height": 22,
    "pixmap": {
        "size": 24,
        "padding": 4
    },
    "text": {
        "gap": 12,
        "valign": "center"
    }
}
```

### 4.4 `desktop`

The desktop's own default background color, used only as a fallback:
see the explanation right after the table below for exactly when it
applies.

| Key                | Type   | Default     |
|--------------------|--------|-------------|
| `color.background` | string | `"#5F7187"` |

This is deliberately not the same tone as `systray.color.background`
or the other UI-chrome colors above (`"#D0D9E5"`-family): a desktop
background is a large, full-screen area rather than a small UI
element, so it wants a more neutral, less attention-grabbing tone,
and a darker one gives windows placed on top of it more contrast to
stand out against than a light background would.  It still reads as
the same overall blue-gray palette as the rest of the default theme,
close to `window.active.color.foreground`'s own `"#4A5566"`, rather
than an unrelated new hue.

This value is used only when a desktop's own entry in
`topology.screens.desktops` (`config.json`, section 2.2) does not
set its own `background-color`; a desktop that does set one always
keeps it, regardless of this.  It is also only ever used when no
external tool (`xsetbg`, `feh`, `nitrogen`, `hsetroot`, and so on)
has painted the root window with its own wallpaper image, exactly
the same way an explicit per-desktop `background-color` is: icowm
never overwrites an externally set wallpaper with either one.

```json
"desktop": {
    "color": { "background": "#5F7187" }
}
```

### 4.5 `menu`

Applies to every context menu (root menu, per-window menu, the
all-desktops window list, and their submenus) and to the Alt+Tab-style
cycle menu's own window chrome (its per-row entries in list mode use
this too).  The cycle menu's individual icon cells keep using
`icon.active` / `icon.inactive` (section 4.2) instead of this, since
that already themes "the icon currently selected while cycling"
specifically; likewise, the border drawn around the actual window or
icon being previewed while cycling uses `window.active` /
`window.inactive` (section 4.1), since that is a highlight on the real
window, not on the menu.

`unselected.border`, `selected.border`, and `label.border` each style
one row's own outline; the menu window's outer frame is a separate
field, `border` (below the per-entry styles in the table), so raising
or lowering an entry's own border never changes whether the window
itself has a frame, and vice versa.  The cycle menu's own window
shares this same `border` for its outer frame, so a context menu and
the cycle menu always present the same outer border, regardless of
whatever an entry's own border happens to be set to.

| Key                           | Type    | Default        |
|-------------------------------|---------|----------------|
| `unselected.font`             | string  | `"fixed"`      |
| `unselected.color.background` | string  | `"#D0D9E5"`    |
| `unselected.color.foreground` | string  | `"#4A5566"`    |
| `unselected.border.color`     | string  | `"#7F9AB6"`    |
| `unselected.border.width`     | integer | `1`            |
| `selected.font`               | string  | `"fixed bold"` |
| `selected.color.background`   | string  | `"#9AAEC8"`    |
| `selected.color.foreground`   | string  | `"#253040"`    |
| `selected.border.color`       | string  | `"#4A5566"`    |
| `selected.border.width`       | integer | `1`            |
| `label.font`                  | string  | `"fixed"`      |
| `label.color.background`      | string  | `"#D0D9E5"`    |
| `label.color.foreground`      | string  | `"#7F9AB6"`    |
| `label.border.color`          | string  | `"#7F9AB6"`    |
| `label.border.width`          | integer | `0`            |
| `disabled.color.foreground`   | string  | `"#A0A8B0"`    |
| `separator.color`             | string  | `"#7F9AB6"`    |
| `border.color`                | string  | `"#7F9AB6"`    |
| `border.width`                | integer | `1`            |
| `padding.horizontal`          | integer | `12`           |
| `padding.vertical`            | integer | `4`            |
| `show-pixmaps`                | boolean | `true`         |

`unselected` styles an entry that is neither hovered nor the
keyboard-navigated selection; `selected` styles the entry that is.
`label` styles a non-interactive heading row: it never borrows
`unselected` or `selected` even though it can look similar by default,
so it can be told apart (e.g., dimmer text, no border) without also
having to look like a normal, hoverable entry.  Any of the three
styles' `border.width` can be raised above `0` to draw an outline
around that kind of row; `unselected`/`selected` default to a subtle
`1`, `label` to `0` so heading rows stay plain.
`disabled.color.foreground` colors the text of an entry that cannot
currently be activated (e.g., "maximize" on a client that cannot be
resized); its background still comes from `unselected` or `selected`
depending on whether it happens to also be the current selection.
`separator.color` is the line color for a separator between groups of
entries.
`border.color` and `border.width` are the menu window's own outer
frame, entries aside; see the paragraph above the table for how this
differs from any entry's own `border`.

`padding.horizontal` and `padding.vertical` are the inset in pixels
between a menu window's own edges and its content: row text (and, for
a submenu, its arrow indicator) for `padding.horizontal`, and the
space above the first row and below the last for `padding.vertical`.
Both apply to every context menu and to the Alt+Tab-style cycle menu
alike, and equally to `unselected`, `selected`, and `label` rows.

`show-pixmaps` controls whether an entry that represents a client
window (the per-window context menu, and the cycle menu's own list
mode) draws that client's own `_NET_WM_ICON` image beside its label,
the same `icon.show-pixmaps` (section 4.2) controls for iconified
windows; entries that do not represent a specific client (labels,
separators, submenu headers) are unaffected either way.

```json
"menu": {
    "unselected": {
        "font": "fixed",
        "color": { "background": "#D0D9E5", "foreground": "#4A5566" },
        "border": { "color": "#7F9AB6", "width": 1 }
    },
    "selected": {
        "font": "fixed bold",
        "color": { "background": "#9AAEC8", "foreground": "#253040" },
        "border": { "color": "#4A5566", "width": 1 }
    },
    "label": {
        "font": "fixed",
        "color": { "background": "#D0D9E5", "foreground": "#7F9AB6" },
        "border": { "color": "#7F9AB6", "width": 0 }
    },
    "disabled": {
        "color": { "foreground": "#A0A8B0" }
    },
    "separator": {
        "color": "#7F9AB6"
    },
    "padding": {
        "horizontal": 12,
        "vertical": 4
    },
    "border": { "color": "#7F9AB6", "width": 1 },
    "show-pixmaps": true
}
```

### 4.6 `dialog`

Applies to the quit-confirmation dialog and the generic message
dialog.

| Key                                  | Type    | Default        |
|--------------------------------------|---------|----------------|
| `color.background`                   | string  | `"#D0D9E5"`    |
| `border.color`                       | string  | `"#7F9AB6"`    |
| `border.width`                       | integer | `2`            |
| `label.font`                         | string  | `"fixed"`      |
| `label.color.foreground`             | string  | `"#4A5566"`    |
| `label.padding.horizontal`           | integer | `12`           |
| `label.padding.vertical`             | integer | `12`           |
| `button.unselected.font`             | string  | `"fixed"`      |
| `button.unselected.color.background` | string  | `"#D0D9E5"`    |
| `button.unselected.color.foreground` | string  | `"#4A5566"`    |
| `button.unselected.border.color`     | string  | `"#7F9AB6"`    |
| `button.unselected.border.width`     | integer | `1`            |
| `button.selected.font`               | string  | `"fixed bold"` |
| `button.selected.color.background`   | string  | `"#9AAEC8"`    |
| `button.selected.color.foreground`   | string  | `"#253040"`    |
| `button.selected.border.color`       | string  | `"#4A5566"`    |
| `button.selected.border.width`       | integer | `1`            |
| `button.gap`                         | integer | `12`           |
| `button.padding.horizontal`          | integer | `12`           |
| `button.padding.vertical`            | integer | `6`            |

`color.background` and `border` are the dialog window's own background
and frame.  `label` styles the prompt or message text (e.g., "Are you
sure you want to exit IcoWM?"); `label.padding` is the inset between
the dialog window's own edges and that text.  `button.unselected` and
`button.selected` style the dialog's buttons (e.g., "Cancel" / "Exit"),
the same not-selected/keyboard-navigated-choice distinction as `menu`
above; the message dialog's single "OK" button always uses
`button.selected`, since there is nothing else it could be navigated
away from.  `button.gap` is the horizontal space between adjacent
buttons.  `button.padding` is the inset between a button's own edges
and its label, shared by both `unselected` and `selected` so a button
does not change size (and shove its neighbor sideways) as the
highlight moves onto or off of it; each button is still sized wide and
tall enough for whichever of the two fonts is larger, and its label
stays centered within that fixed size regardless of which font ends
up drawn, so switching to a wider `selected` font (bold by default)
never looks off-center.

```json
"dialog": {
    "color": { "background": "#D0D9E5" },
    "border": { "color": "#7F9AB6", "width": 2 },
    "label": {
        "font": "fixed",
        "color": { "foreground": "#4A5566" },
        "padding": { "horizontal": 12, "vertical": 12 }
    },
    "button": {
        "unselected": {
            "font": "fixed",
            "color": { "background": "#D0D9E5", "foreground": "#4A5566" },
            "border": { "color": "#7F9AB6", "width": 1 }
        },
        "selected": {
            "font": "fixed bold",
            "color": { "background": "#9AAEC8", "foreground": "#253040" },
            "border": { "color": "#4A5566", "width": 1 }
        },
        "gap": 12,
        "padding": { "horizontal": 12, "vertical": 6 }
    }
}
```

### 4.7 `overlay`

A single `font` / `color` / `border` block, the same shape as
`window.active` (section 4.1), applied to transient informational
overlays that are not menus or dialogs: the client-info popup and the
desktop-switch notification.  Both are single-style, non-interactive
overlays with no selected/unselected state to distinguish.

| Key                | Type    | Default     |
|--------------------|---------|-------------|
| `font`             | string  | `"fixed"`   |
| `color.background` | string  | `"#D0D9E5"` |
| `color.foreground` | string  | `"#4A5566"` |
| `border.color`     | string  | `"#7F9AB6"` |
| `border.width`     | integer | `1`         |

```json
"overlay": {
    "font": "fixed",
    "color": { "background": "#D0D9E5", "foreground": "#4A5566" },
    "border": { "color": "#7F9AB6", "width": 1 }
}
```

### 4.8 `xsettings`

| Key                                 | Type    | Default     |
|-------------------------------------|---------|-------------|
| `xsettings.is-enabled`              | boolean | `false`     |
| `xsettings.dpi`                     | integer | `96`        |
| `xsettings.theme.gtk-theme-name`    | string  | `"Adwaita"` |
| `xsettings.theme.icon-theme-name`   | string  | `"Adwaita"` |
| `xsettings.theme.cursor-theme-name` | string  | `"Adwaita"` |
| `xsettings.theme.cursor-theme-size` | integer | `24`        |

Built-in XSETTINGS manager, implementing the freedesktop.org XSETTINGS
specification.  Lives in the theme rather than in `config.json`: every
value it publishes (a theme name, an icon theme, a cursor theme and
size, a display DPI) is an appearance choice, not a behavior one, even
though `is-enabled` still controls whether the manager runs at all.

Many GTK and Qt applications have a "use theme colors" or "use system
settings" option that only takes effect if some XSETTINGS manager is
running to tell them what the theme, icon theme, cursor theme, and
display DPI actually are; without one, those applications silently
fall back to their own built-in defaults regardless of what this
option is set to.  When enabled, IcoWM acquires the `_XSETTINGS_Sn`
manager selection on the first managed screen and publishes
`Net/ThemeName`, `Net/IconThemeName`, `Gtk/CursorThemeName`,
`Gtk/CursorThemeSize`, and `Xft/DPI` (as `dpi * 1024`, per the
specification) from the values below.

Changing any of these values and reloading the configuration updates the
published settings immediately for every application watching them,
without needing to restart them.  As with the systray, if another
settings manager (e.g., `xsettingsd`, or a desktop environment's own)
already owns the selection, IcoWM's built-in one steps aside rather than
fighting over ownership, for only one settings manager can be active at
a time.  This does not give applications a full theme (GTK/Qt themes are
CSS-like stylesheets, not something conveyed over XSETTINGS); it gives
them the *name* of a theme they already have installed to switch to,
exactly as a dedicated XSETTINGS daemon would.

```json
"xsettings": {
    "is-enabled": false,
    "dpi": 96,
    "theme": {
        "gtk-theme-name": "Adwaita",
        "icon-theme-name": "Adwaita",
        "cursor-theme-name": "Adwaita",
        "cursor-theme-size": 24
    }
}
```

### 4.9 Configuration reload and already-open windows

Reloading the configuration (`SIGHUP`, the reload keybinding, or the
root menu action) re-reads whichever theme file `config.json` names
and applies the new colors, font, and titlebar button lists to every
open window immediately, since those are read live from the theme on
every repaint.  Every already-decorated window's frame is also resized
to match a changed `border.width` or `titlebar.height`, the same way
it resizes on a focus change (see 4.1 above).

What reload does **not** do is force a window's decorated/undecorated
state to follow a changed `window.is-decorated` or `titlebar.height`
in the theme file.  A window that was decorated when it was mapped
stays decorated after a reload even if the reloaded theme now says
`"is-decorated": false` (and vice versa): only newly mapped windows,
and windows whose decoration is toggled by hand, pick up that setting.
This is deliberate: undoing a decoration choice a person made for a
specific window just because the theme file changed would be a
surprising, unrequested side effect.

The other exception is `config.json`'s own `topology.*` section
(screen count, and how many desktops each screen has, along with each
desktop's own `name`/`background-color`; see section 2.2): changing
any of these and reloading has no effect on an already-running window
manager.  This does not extend to the separate, sibling `desktops`
section (section 2.11: `warp`, `cycle`, `margins`) despite the similar
name: that one describes navigation behavior and reserved space, not
topology, and does take effect on reload, same as everything else.
Every other field in `config.json`, and every other configuration
file (`bindings.json`, `menus.json`, `randr.json`, `rules.json`,
`session.json`, and the active theme), does take effect on reload,
as documented throughout this file.  Growing or shrinking the number
of screens or desktops at runtime would mean deciding what happens to
whatever clients, focus, and EWMH state already live on a desktop
being removed, none of which reload does today; restart the window
manager to pick up a `topology.*` change instead.

---

> **Font format note:**  Every `font` field in a theme accepts the
> same string, tried through two backends in order:
>
> 1. **X core fonts** (accessed via XCB), IcoWM's original text
>    rendering path.  Only **X11 bitmap fonts** (BDF/PCF) are
>    available through this backend; it is tried first because it has
>    no per-glyph rasterization cost and every X server ships the
>    `fixed` family it falls back to below.
>
> 2. **TrueType/OpenType**, via fontconfig (font matching), FreeType2
>    (rasterization), and the X RENDER extension (compositing), used
>    automatically whenever a `font` string does not resolve to an X
>    core font, e.g., a family name such as `"DejaVu Sans"` that most
>    systems only have as a scalable font, not as a legacy X bitmap
>    one.  This is what gives window titles, menus, and the systray
>    clock/battery text real anti-aliasing and full UTF-8 support
>    (accented characters, non-Latin scripts, and so on), neither of
>    which the X core font backend can provide.
>
> If a `font` string resolves through neither backend, IcoWM falls
> back to `"fixed"`, so text rendering is never left completely
> broken by a single bad theme value.
>
> No separate field or prefix selects which backend is used: it is
> decided purely by whether the string resolves as an X core font
> first.  A short description like `"fixed bold 13"` almost always
> takes the X core font path, since `fixed` is an X bitmap family;
> a TrueType/OpenType family name takes the fontconfig path instead,
> using fontconfig's own pattern syntax rather than the short
> description syntax below.
>
> #### X core font syntax
>
> The `font` field accepts two X core font formats:
>
> 1. **Short description:**
>   `"[family] [bold] [italic|oblique] [size] [registry-encoding]"`
>
>    IcoWM parses this and constructs the appropriate XLFD wildcard
>    pattern internally. All fields after `family` are optional and can
>    appear in any order, except that `registry-encoding` (if given)
>    must come last.  `registry-encoding` is any token that contains
>    a hyphen, e.g., `iso8859-15` or `iso10646-1`; it maps to the last
>    two XLFD fields (`charset_registry` and `charset_encoding`).
>
>   Examples:
>    - `"fixed"`: the `fixed` alias (available on every X server)
>    - `"fixed 13"`: `fixed` family at 13 pixels
>    - `"fixed bold 13"`: `fixed` family, bold weight, 13 pixels
>    - `"fixed bold 13 iso8859-15"`: `fixed`, bold, 13 pixels,
>       ISO 8859-15 charset
>    - `"courier bold italic 17"`: Courier, bold italic, 17 pixels
>
> 2. **Full XLFD:** a string starting with "`-`", e.g.,
>    `"-*-fixed-bold-r-*-*-13-*-*-*-*-*-iso8859-15"`, is passed verbatim
>    to the X server.
>
> To list all X core fonts available on your system, run:
>
> ```
> xlsfonts
> ```
>
> or query a specific pattern:
>
> ```
> xlsfonts -fn '-*-fixed-*-*-*-*-*-*-*-*-*-*-*-*'
> ```
>
> #### TrueType/OpenType syntax
>
> A `font` string that reaches the fontconfig fallback is parsed with
> fontconfig's own pattern syntax, the same one used by tools such as
> `fc-match`:
>
> ```
> <family>[-<size>][:<name1>=<value1>[:<name2>=<value2>...]]
> ```
>
> Examples:
>  - `"DejaVu Sans Mono"`: family name alone, fontconfig's default size
>  - `"DejaVu Sans Mono-11"`: family and pixel size
>  - `"Noto Sans:bold"`: family and weight
>  - `"Noto Sans:bold:size=11"`: family, weight, and size
>
> To check what font a given pattern resolves to (and confirm it is
> actually installed) before putting it in a theme file, run:
>
> ```
> fc-match "Noto Sans:bold:size=11"
> ```
---

## 5. `randr.json`: XRandR output profiles

Defines per-output settings applied by IcoWM through the XRandR
extension.  This file is **optional**.  If absent, XRandR hot-plug
event handling still works (screen-change notifications are processed),
but no output profiles are configured.

Up to **16** output entries are supported (**2** in a
low-memory build; see `CONFIG_RANDR_MAX_OUTPUTS` in `defs/config.h`).

Applied at startup, and again whenever an output connects or
disconnects afterward (e.g., plugging in an external monitor).

### 5.1 Top-level fields

| Key          | Type    | Default | Description |
|--------------|---------|---------|-------------|
| `is-enabled` | boolean | `false` | Master switch.  Set to `true` to activate output profile management. |

### 5.2 `outputs[]` entries

Each entry in the `outputs` array describes one physical display output.
A profile whose `name` does not match any currently-connected output
is simply skipped until one by that name appears.

| Key            | Type    | Default    | Description |
|----------------|---------|------------|-------------|
| `name`         | string  | `""`       | Output connector name as reported by the X server (e.g., `"HDMI-1"`, `"eDP-1"`, `"DP-2"`).  Run `xrandr` in a terminal to list available names. |
| `is-enabled`   | boolean | `false`    | Whether this output is used at all.  `true` applies `resolution`, `position`, and `rotation` below to the output, and lets IcoWM manage windows on it.  `false` instead turns the output off (blanking it, the same as unplugging it) and excludes it from window placement entirely, useful for a permanently-connected output (a projector for mirroring, say) that should never receive windows. |
| `is-primary`   | boolean | `false`    | Mark this output as the primary display.  Only applied when `is-enabled` is `true`; applied as a separate step right after the rest of this profile. |
| `resolution.w` | integer | `0`        | Preferred horizontal resolution in pixels.  Matched against the modes the screen currently reports; if `0`, `0`, or no exact match exists, the output's own already-active mode is kept instead (or its first preferred mode, if it had none), and, since nothing was actually requested in that case, its resolution plays no part in deciding whether this profile changed anything on a later reload, or in what a `[ Revert ]` on the confirm dialog restores (see `position`/`rotation`/`is-primary` above and below, which always do). Only applied when `is-enabled` is `true`. |
| `resolution.h` | integer | `0`        | Preferred vertical resolution in pixels.  See `resolution.w` above. |
| `position.x`   | integer | `0`        | Horizontal position of this output in the virtual screen.  Only applied when `is-enabled` is `true`. |
| `position.y`   | integer | `0`        | Vertical position of this output in the virtual screen.  Only applied when `is-enabled` is `true`. |
| `rotation`     | string  | `"normal"` | Screen rotation.  Only applied when `is-enabled` is `true`. |

Accepted `rotation` values are: `"normal"`, `"left"` (90°),
`"right"` (270°), `"inverted"` (180°).

### 5.3 Scope: per-X-screen, not per-`outputs[]`-entry

RandR is scoped to a single X screen: CRTCs, outputs, and modes are
queried and configured against one screen's root window, with no
cross-screen notion at the protocol level. `surface_action_
apply_randr_profiles` is called once per surface (X screen managed),
querying and applying RandR state independently for each. `outputs[]`
itself, however, is a single list in `config_randr_s`, shared by
every surface, with no per-screen field. On a multi-GPU setup where
two X screens each expose an output of the same name, the matching
profile is applied identically to both, with no way to scope it to
one screen only. Not applicable to the common case of one X screen
managing several outputs via RandR 1.5 monitors, nor in practice to
most multi-screen setups either, since output names are driver/GPU-
assigned and are not normally reused across independent GPUs.

### 5.4 Reload behavior

`randr.json` is re-read on configuration reload (`KEYBIND_WM_RELOAD` /
`ACTION_WM_RELOAD`), updating `config->randr` in memory, and
`wm_action_config_reload` immediately applies it (`surface_action_
apply_randr_profiles`), before rules, keyboard/mouse bindings, or any
other reload step, so a resync of clients or desktops elsewhere in
the same reload already reflects the new screen geometry if RandR
itself just changed it.

If that application actually changed anything, a confirm dialog
appears, centered on the affected screen: "The 'randr.json'
configuration has been applied. Keep it, or revert to the previous
one?", with `[ Revert ]` selected by default and a live countdown
underneath. Pressing `[ Keep ]` (or Tab/arrow then Enter/Space) keeps
the just-applied profile; pressing `[ Revert ]`, Escape, or letting
the countdown reach zero undoes it, restoring every changed output's
prior mode, position, rotation, and primary status exactly. The
countdown defaults to 10 seconds (`DIALOG_RANDR_CONFIRM_TIMEOUT_
SECONDS` in `defs/dialog.h`). No dialog appears at all when nothing
actually changed (see the comparison below), nor at startup or on a
hotplug event; only a reload, the one moment a person is at the
keyboard to have triggered it, offers this.

With more than one screen, every screen still gets its own profiles
applied on the same reload even when more than one changes, but only
the first screen to actually change is offered the dialog: only one
confirm dialog can be open at a time, and only the single most recent
change is remembered well enough to revert.

Every call to `surface_action_apply_randr_profiles` compares each
configured profile against the matching output's actual current
state first (resolution, position, rotation, primary status) and
issues an XRandR write for it only when at least one of them
genuinely differs. A `randr.json` whose profiles already match reality
therefore issues no XRandR requests at all, including on a reload
triggered by an unrelated file (e.g., `config.json`), and on a hotplug
event for an output some other profile targets.

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

## 6. `rules.json`: Per-window rules

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

### 6.1 Rule file shape

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

### 6.2 Rule entry fields

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

### 6.3 Match fields

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
sequence of characters and `?` matches any single character.

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
        "sticky": true,
        "decorated": false,
        "layer": "below"
    }
}
```

This matches any window whose title matches one of the three patterns
*and* whose class is `"MEDIA"`; a window matching only one of those two
conditions does not match the rule.  A single string, as in `"class"`
above, still works exactly as before.  There is no need to wrap
a single value in an array.

Accepted `match.type` values are: `"normal"`, `"desktop"`, `"dock"`,
`"toolbar"`, `"menu"`, `"utility"`, `"splash"`, `"dialog"`.

As with every other match field, omitting `match.type` entirely does not
default to `"normal"`.  It means that criterion is not checked at all,
so the rule matches windows of any type. In the example above, `"type"`:
"normal" is an explicit, active condition; removing that line and the
rule would also match e.g., a dialog with a matching title and class.

### 6.4 Apply fields

All apply fields are optional.  Only the fields present in the last
matching rule for each property are applied.

| Key                 | Type                 | Default | Description |
|---------------------|----------------------|---------|-------------|
| `apply.desktop`     | integer              | unset   | Zero-based desktop index to move the window to.  Falls back to desktop `0` if it does not exist, logging a warning. |
| `apply.monitor`     | integer              | unset   | Zero-based monitor index, within the window's own surface, to place the window on.  Falls back to monitor `0` if it does not exist, logging a warning. |
| `apply.layer`       | string               | unset   | Stacking layer.  Accepted values: `"below"`, `"normal"`, `"above"`.  Falls back to `"normal"` if unrecognized, logging a warning. |
| `apply.focus`       | boolean              | unset   | Whether the matched window should receive focus. |
| `apply.sticky`      | boolean              | unset   | Whether the window should be visible on all desktops. |
| `apply.decorated`   | boolean              | unset   | Whether the window should keep its decorations. |
| `apply.position`    | object or `"center"` | unset   | Where to place the window; see below. |
| `apply.position.x`  | integer              | unset   | X position in pixels (when `position` is an object), relative to `apply.monitor`'s own top-left corner if set, or to the surface's otherwise. |
| `apply.position.y`  | integer              | unset   | Y position in pixels (when `position` is an object), relative to `apply.monitor`'s own top-left corner if set, or to the surface's otherwise. |
| `apply.size.width`  | integer              | unset   | Window width in pixels; must be greater than `0`. |
| `apply.size.height` | integer              | unset   | Window height in pixels; must be greater than `0`. |

Position and size are applied independently.  Specifying only `position`
moves the window without resizing it; specifying only `size` resizes it
without moving it; both may be present to set position and size at once.
Both `size.width` and `size.height` are only accepted when greater than
`0`.

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

`monitor` selects one physical monitor within the window's own surface
(only meaningful on a surface made up of more than one monitor sharing
the same combined X screen; not named `screen`, since this project's
own `screens[]`/`screen_id` terminology refers to a whole X screen, and
this codebase has no notion of moving a window to a *different* surface
at all, so a field with that name would misleadingly suggest a
capability that does not exist).  It changes what `position` is
relative to rather than being a placement action of its own: explicit
`x`/`y` become offsets from that monitor's own top-left corner instead
of the whole surface's, and `"center"` centers on that monitor instead
of the whole surface.  A rule that sets `monitor` without also setting
`position` still centers the window on that monitor by default, since
otherwise `monitor` alone would have no visible effect at all:

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

## 7. `session.json`: Session lifecycle hooks

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

### 7.1 Hook arrays

| Key         | Type             | Default | Description |
|-------------|------------------|---------|-------------|
| `on-start`  | array of strings | `[]`    | Commands launched after IcoWM startup initialization. |
| `on-reload` | array of strings | `[]`    | Commands launched after a configuration reload (`SIGHUP` or the reload action). |
| `on-exit`   | array of strings | `[]`    | Commands launched when IcoWM is exiting. |

Only non-empty string entries are used; all other array items are
ignored.

## 8. `menu.json`: Root desktop menu

`menu.json` defines the user-configurable entries that appear when the
user right-clicks on the empty desktop (root window).  The file is
**optional**; when it is absent or cannot be parsed, the built-in footer
(`Reload configuration`, `Redraw all windows`, `Exit`) is still shown
without any preceding separator.

### 8.1 Top-level structure

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

### 8.2 Entry types

Each entry object must have a `"type"` string field.  Four types are
supported:

| `"type"`      | Description                                       |
|---------------|---------------------------------------------------|
| `"command"`   | Clickable item that launches an application       |
| `"separator"` | Horizontal dividing line (no other fields needed) |
| `"label"`     | Non-clickable section heading                     |
| `"submenu"`   | Nested sub-menu revealed on hover/click           |

### 8.3 Entry fields reference

#### `"command"` entry

| Field       | Type   | Required | Description                         |
|-------------|--------|----------|-------------------------------------|
| `"type"`    | string | yes      | Must be `"command"`                 |
| `"name"`    | string | yes      | Label text shown in the menu        |
| `"command"` | string | yes      | Shell command or program to execute |

The `"command"` value is passed through `wordexp(3)` so environment
variables and simple shell expansions (`~`, `$HOME`, …) are supported.
Command injection via sub-shells is **disabled** (`WRDE_NOCMD`).

If the command cannot be executed, IcoWM logs a warning and shows an
informational dialog so the user is notified immediately.

#### `"separator"` entry

| Field    | Type   | Required | Description           |
|----------|--------|----------|-----------------------|
| `"type"` | string | yes      | Must be `"separator"` |

At this moment there's only one kind of `"type"`, which is `"separator"`.

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

Sub-menus can be nested to the depth limit defined by
`WM_CTXMENU_MAX_DEPTH` (default: 4).

---

## 9. `memguard.json`: Restricted-memory mode configuration

Read only when IcoWM is launched with `-M <mib>` (see `manual.md`'s own
"Restricted-memory mode" section for what that flag does and why it
exists); an ordinary session never reads this file, and this file has
no effect at all without `-M <mib>`.  It fully replaces `config.json`
for that one session: `config.json` itself is not consulted at all
while `-M <mib>` is in effect, but `bindings.json` and a theme file
under `themes/` are still read exactly as in an ordinary session
(see 9.2 for the one exception).  The file is **optional**; a missing
or unreadable one falls back to a fixed built-in profile.

Only the fields below are ever read from it; anything else present in
the file is silently ignored, and every field this mode's own screen
and desktop counts, RandR handling, and startup-notification setting
are fixed and cannot be configured here at all.

### 9.1 Configurable fields

| Key                                      | Type    | Default | Description |
|------------------------------------------|---------|---------|-------------|
| `theme`                                  | string  | `""` (built-in default theme) | Same as `config.json`'s own `theme`: the filename (without `.json`) of a theme under `themes/`. |
| `programs.editor`                        | string  | `"gvim"` | Same as `config.json`'s own `programs.editor`. |
| `programs.file-manager`                  | string  | `"pcmanfm"` | Same as `config.json`'s own `programs.file-manager`. |
| `programs.launcher`                      | string  | `"gmrun"` | Same as `config.json`'s own `programs.launcher`. |
| `programs.terminal`                      | string  | `"xterm"` | Same as `config.json`'s own `programs.terminal`. |
| `programs.web-browser`                   | string  | `"firefox"` | Same as `config.json`'s own `programs.web-browser`. |
| `desktops.margins.top/right/bottom/left` | integer | `0` | Same as `config.json`'s own `desktops.margins`; this mode always runs with a single screen and a single desktop, so this is the only per-desktop setting still worth having. |
| `windows.move-step`                      | integer | `10` | Same as `config.json`'s own `windows.move-step`. |
| `windows.placement.policy`               | string  | `"smart"` | Same as `config.json`'s own `windows.placement.policy`: `smart`, `cascade`, `centered`, or `under-mouse`. |
| `icons.placement.policy`                 | string  | `"smart"` | Same as `config.json`'s own `icons.placement.policy`: `top`, `bottom`, `left`, `right`, or `smart`. |
| `systray`                                | object  | see 9.2 | The entire `systray` object, in the same shape as `config.json`'s own (section 2.10), with the two exceptions in 9.2. |
| `enable-emergency-shortcut`              | boolean | `false` | Same as `config.json`'s own `enable-emergency-shortcut`. |

### 9.2 Fields this mode never lets `memguard.json` change

A handful of fields are read the same way as `config.json`'s own
identical `systray` object, but immediately forced back to a fixed
value afterward, since restricted-memory mode never docks any icon at
all (embedding is always off) and so has no use for them:

- **`systray.text.position`** and **`systray.order`** only ever affect
  docked pixmap icons (where their own text sits relative to them, and
  the order newly docked ones are placed in); both are always reset to
  their own ordinary default (`left` and `left-to-right`, respectively)
  regardless of what the file specifies.
- **Embedding itself** cannot be turned on at all in this mode; the
  systray still shows (clock, battery, and its own frame) when
  `systray.is-enabled` is `true`, just never accepts a docked
  application icon.

### 9.3 The active theme's own restrictions

Whichever theme ends up active, named in `memguard.json`, or the
built-in default if none is, gets further restricted after loading, on
top of whatever `memguard.json` itself configured:

- Every font the theme specifies (window titles, icon labels, menu
  entries, dialog text, the systray's own clock/battery text, and the
  desktop-name overlay) is replaced with IcoWM's own fixed built-in
  font, **unless** it already names some variant of that same font
  (matched case-sensitively): a theme is free to specify that font
  directly instead of leaving every field to fall back to it, if it
  wants any of the styling (size, weight) that comes with naming it
  explicitly rather than implicitly.
- XSettings propagation (section 4.8) is always off, regardless of
  `theme.xsettings.is-enabled`.
- Icon pixmaps, icon hint characters, and menu pixmaps are always off,
  regardless of what the theme itself specifies for `icon.show-pixmaps`,
  `icon.show-hints`, and `menu.show-pixmaps`.

None of this is configurable through `memguard.json` itself; it applies
to whatever theme loads, unconditionally.

---

## 10. Full examples

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
        "warp": true,
        "cycle": true,
        "margins": {
            "top": 0,
            "right": 0,
            "bottom": 0,
            "left": 0
        }
    },

    "programs": {
        "terminal": "xterm",
        "launcher": "gmrun",
        "editor": "gvim",
        "file-manager": "pcmanfm",
        "web-browser": "firefox"
    },

    "windows": {
        "gravity": "north-west",
        "move-step": 10,
        "snap": 4,
        "show-geom": true,
        "focus": {
            "policy": "click",
            "is-new-focused": true,
            "is-raised-on-focus": false
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
        "margins": {
            "top": 0,
            "right": 0,
            "bottom": 0,
            "left": 0
        },
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

    "show-desktop-overlay": true,
    "enable-emergency-shortcut": false,
    "enable-fortune-shortcut": false
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
        "launch": {
            "terminal": "modc+mod1+Return",
            "launcher": "modc+mod1+r",
            "file-manager": "modc+mod1+q",
            "web-browser": "modc+mod1+w",
            "editor": "modc+mod1+e"
        },

        "window": {
            "close": "modc+mod1+c",
            "kill": "modc+mod1+mods+Escape",
            "iconify": "modc+mod1+i",
            "iconify-all": "modc+mod4+mods+i",
            "deiconify-all": "modc+mod4+mods+d",
            "arrange": "modc+mod1+mods+a",
            "hide": "modc+mod1+mods+h",
            "maximize": "modc+mod1+m",
            "fullscreen": "modc+mod1+f",
            "shade": "modc+mod1+s",
            "pin": "modc+mod1+p",
            "decorate": "modc+mod1+d",
            "layer": "modc+mod1+mods+y",
            "info": "modc+mod1+mods+i",
            "move": {
                "relative": {
                    "right": "modc+mod1+l",
                    "left": "modc+mod1+h",
                    "up": "modc+mod1+k",
                    "down": "modc+mod1+j"
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
                "right": "modc+mod1+mods+l",
                "left": "modc+mod1+mods+h",
                "up": "modc+mod1+mods+k",
                "down": "modc+mod1+mods+j"
            }
        },

        "wm": {
            "menus": {
                "root": "modc+mod1+mods+m",
                "windows": "modc+mod1+mods+w"
            },
            "search": "modc+mod4+mods+s",
            "show-desktop": "modc+mod1+mods+d",
            "redraw": "modc+mod1+mods+r",
            "reload": "modc+mod1+mods+c",
            "quit": "modc+mod1+mods+x",
            "shortcuts": "modc+mod4+F1"
        },

        "cycle": {
            "desktop": {
                "prev": "modc+mod1+Left",
                "next": "modc+mod1+Right"
            },
            "window": {
                "prev": "mod1+mods+Tab",
                "next": "mod1+Tab"
            },
            "icon": {
                "prev": "modc+mod1+mods+Tab",
                "next": "modc+mod1+Tab"
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
                "prev": "button4",
                "next": "button5"
            }
        }
    }
}
```

### `themes/default.json`

```json
{
    "name": "Default theme",

    "window": {
        "is-decorated": true,
        "titlebar": {
            "height": 22,
            "alignment": "center",
            "padding": { "horizontal": 2, "vertical": 2 },
            "buttons": {
                "left": ["pin", "layer"],
                "right": ["close", "maximize", "shade", "iconize"],
                "color": { "on": "#253f60", "off": "#7086a0" }
            }
        },
        "active": {
            "font": "fixed bold",
            "color": { "background": "#9aaec8", "foreground": "#253040" },
            "border": { "color": "#4a5566", "width": 2 }
        },
        "inactive": {
            "font": "fixed",
            "color": { "background": "#d0d9e5", "foreground": "#4a5566" },
            "border": { "color": "#7f9ab6", "width": 2 }
        }
    },

    "icon": {
        "is-captioned": true,
        "show-pixmaps": true,
        "show-hints": true,
        "active": {
            "font": "fixed bold",
            "color": { "background": "#9aaec8", "foreground": "#253040" },
            "border": { "color": "#4a5566", "width": 1 }
        },
        "inactive": {
            "font": "fixed",
            "color": { "background": "#d0d9e5", "foreground": "#4a5566" },
            "border": { "color": "#7f9ab6", "width": 1 }
        }
    },

    "systray": {
        "font": "fixed bold",
        "color": { "background": "#d0d9e5", "foreground": "#4a5566" },
        "border": { "color": "#7f9ab6", "width": 1 },
        "height": 22,
        "pixmap": {
            "size": 24,
            "padding": 4
        },
        "text": {
            "gap": 12,
            "valign": "center"
        }
    },

    "desktop": {
        "color": { "background": "#5f7187" }
    },

    "menu": {
        "unselected": {
            "font": "fixed",
            "color": { "background": "#d0d9e5", "foreground": "#4a5566" },
            "border": { "color": "#7f9ab6", "width": 0 }
        },
        "selected": {
            "font": "fixed",
            "color": { "background": "#9aaec8", "foreground": "#253040" },
            "border": { "color": "#4a5566", "width": 0 }
        },
        "label": {
            "font": "fixed",
            "color": { "background": "#48607f", "foreground": "#d0d9e5" },
            "border": { "color": "#7f9ab6", "width": 0 }
        },
        "disabled": {
            "color": { "foreground": "#717b88" }
        },
        "separator": {
            "color": "#7f9ab6"
        },
        "border": { "color": "#7f9ab6", "width": 2 },
        "padding": {
            "horizontal": 12,
            "vertical": 4
        },
        "show-pixmaps": true
    },

    "dialog": {
        "color": { "background": "#d0d9e5" },
        "border": { "color": "#7f9ab6", "width": 2 },
        "label": {
            "font": "fixed bold",
            "color": { "foreground": "#4a5566" },
            "padding": { "horizontal": 12, "vertical": 12 }
        },
        "button": {
            "unselected": {
                "font": "fixed",
                "color": { "background": "#d0d9e5", "foreground": "#4a5566" },
                "border": { "color": "#7f9ab6", "width": 1 }
            },
            "selected": {
                "font": "fixed bold",
                "color": { "background": "#9aaec8", "foreground": "#253040" },
                "border": { "color": "#4a5566", "width": 1 }
            },
            "gap": 24,
            "padding": { "horizontal": 12, "vertical": 6 }
        }
    },

    "overlay": {
        "font": "fixed",
        "color": { "background": "#d0d9e5", "foreground": "#4a5566" },
        "border": { "color": "#7f9ab6", "width": 1 }
    },

    "xsettings": {
        "is-enabled": false,
        "dpi": 96,
        "theme": {
            "gtk-theme-name": "Adwaita",
            "icon-theme-name": "Adwaita",
            "cursor-theme-name": "Adwaita",
            "cursor-theme-size": 24
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
            "title": [ "Journal console" ]
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
            "title": [ "*Sonata" ],
                "type": "normal"
        },
        "apply": {
            "sticky": true,
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
            "title": [ "gmrun" ],
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
- a `"map"` rule for the program name `"\*Sonata"` that applies once
  when the window is first managed, keeping it sticky, undecorated, in
  the `"below"` layer, and positioned at the top-right corner using
  a fixed geometry.
- a `"map"` rule for the program name `"gmrun"` that applies once when
  the window is first managed, keeping it undecorated, in the `"above"`
  layer.

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
    "enable-emergency-shortcut": true
}
```

This example is only ever read when IcoWM is launched with `-M <mib>`;
see `manual.md`'s "Restricted-memory mode" section for what that flag
does.  It names a theme of its own (`themes/compact.json`, not shown
here), keeps the systray's clock and battery on, and turns on the
emergency shortcut, since a severely memory-constrained session is
exactly the kind of place where a hung window is more likely and a
guaranteed way out is worth having.

