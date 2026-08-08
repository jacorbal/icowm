# IcoWM Configuration Reference

Description of every configuration file used by IcoWM, the purpose of
each file, and every supported option together with its type, accepted
values, and built-in default value.

---

## Table of Contents

1. [Directory layout](#1-directory-layout)
2. [`config.json` -- Base configuration](#2-configjson----base-configuration)
   - [2.1 `theme`](#21-theme)
   - [2.2 `screens`](#22-screens)
   - [2.3 `programs`](#23-programs)
   - [2.4 `windows`](#24-windows)
   - [2.5 `icons`](#25-icons)
   - [2.6 `show-desktop-overlay`](#26-show-desktop-overlay)
   - [2.7 `enable-emergency-shortcut`](#27-enable-emergency-shortcut)
   - [2.8 `menus`](#28-menus)
   - [2.9 `systray`](#29-systray)
   - [2.10 `xsettings`](#210-xsettings)
3. [`bindings.json` -- Keyboard and mouse bindings](#3-bindingsjson----keyboard-and-mouse-bindings)
   - [3.1 Binding syntax](#31-binding-syntax)
   - [3.2 `modifiers`](#32-modifiers)
   - [3.3 `keyboard.launch`](#33-keyboardlaunch)
   - [3.4 `keyboard.window`](#34-keyboardwindow)
   - [3.5 `keyboard.wm`](#35-keyboardwm)
   - [3.6 `keyboard.cycle`](#36-keyboardcycle)
   - [3.7 `mouse.window`](#37-mousewindow)
   - [3.8 `mouse.cycle`](#38-mousecycle)
4. [`themes/<name>.json` -- Theme configuration](#4-themesnamejson----theme-configuration)
   - [4.1 `window`](#41-window)
   - [4.2 `icon`](#42-icon)
5. [`randr.json` -- XRandR output profiles](#5-randrjson----xrandr-output-profiles)
   - [5.1 Top-level fields](#51-top-level-fields)
   - [5.2 `outputs[]` entries](#52-outputs-entries)
6. [`rules.json` -- Per-window rules](#6-rulesjson----per-window-rules)
   - [6.1 Rule file shape](#61-rule-file-shape)
   - [6.2 Rule entry fields](#62-rule-entry-fields)
   - [6.3 Match fields](#63-match-fields)
   - [6.4 Apply fields](#64-apply-fields)
7. [`session.json` -- Session lifecycle hooks](#7-sessionjson--session-lifecycle-hooks)
   - [7.1 Hook arrays](#71-hook-arrays)
8. [`menu.json` -- Root desktop menu](#8-menujson----root-desktop-menu)
   - [8.1 Top-level structure](#81-top-level-structure)
   - [8.2 Entry types](#82-entry-types)
   - [8.3 Entry fields reference](#83-entry-fields-reference)
9. [Full examples](#9-full-examples)

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
└── themes/
    └── default.json  Theme file referenced by 'config.json'
```

- **All files are optional**; they fall back to built-in defaults when
  absent.
- Theme files are loaded from the `themes/` sub-directory.  The theme
  name field in `config.json` must match the filename without the
  `.json` extension.
- `randr.json`; XRandR hot-plug event handling is always active
  regardless of this file existence.

## 2. `config.json` -- Base configuration

Controls the fundamental behavior of the window manager: screens,
virtual desktops, default programs, window management policies, and
icon placement.

### 2.1 `theme`

| Key    | Type    | Default               |
|--------|---------|-----------------------|
| `theme` | string | `""` (built-in theme) |

Name of the theme to load, without the `.json` extension.  The file
`themes/<value>.json` is looked up inside the configuration directory.
An empty string or an omitted key causes the built-in default theme to
be used.

```json
"theme": "default"
```

### 2.2 `screens`

Configures the number of physical screens and the virtual desktops
assigned to each.

#### `screens.count`

| Key             | Type    | Default |
|-----------------|---------|---------|
| `screens.count` | integer | `1`     |

Number of physical screens (monitors) to manage.  Maximum is `6`.

#### `screens.settings.desktops[]`

The `desktops` array inside `screens.settings` accepts two layouts:

**Simple layout** (one screen, desktops listed directly):

```json
"screens": {
    "count": 1,
    "settings": {
        "desktops": [
            { "name": "Desktop 0", "background-color": "#1a1a2e" },
            { "name": "Desktop 1", "background-color": "#16213e" }
        ]
    }
}
```

**Per-screen layout** (each array entry represents one screen):

```json
"screens": {
    "count": 2,
    "settings": {
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

Per-screen layout fields:

| Key                           | Type    | Default      | Description |
|-------------------------------|---------|--------------|-------------|
| `count`                       | integer | `10`         | Number of virtual desktops for this screen. Maximum is `10`. |
| `inaugural`                   | integer | `0`          | Zero-based index of the desktop shown at startup.  Values out of range fall back to `0`. |
| `settings[].name`             | string  | `"Desktop N" | Display name of desktop N. |
| `settings[].background-color` | string  | `"#C0CCD8"`  | Root background color as a hex color `"#RRGGBB"` or `"RRGGBB"`. |

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

| Key                         | Type    | Default   | Description |
|-----------------------------|---------|-----------|-------------|
| `placement.policy`          | string  | `"smart"` | How newly mapped windows are placed. |
| `placement.group-related`   | boolean | `true`    | Cluster windows of the same application together. |

Accepted placement policy values:

| Value           | Behavior |
|-----------------|----------|
| `"cascade"`     | Places windows in a stepped diagonal sequence. |
| `"centered"`    | Centers the window on the screen. |
| `"under-mouse"` | Places the window under the current pointer position. |
| `"smart"`       | Finds the position that minimizes overlap with existing windows. |

Transient (dialog) windows are always centered over their parent window,
regardless of this setting.

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

| Key                      | Type   | Default   | Description               |
|--------------------------|--------|-----------|---------------------------|
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
| `show-desktop-overlay`  | boolean | `true`  |

When `true`, a small notification popup is displayed in the center of
the screen for approximately 400 ms whenever the active virtual desktop
changes.  The popup shows the desktop index and name in the format
`[index] -- Name`, or just `[index]` in the case when the desktop has no
name.  Set to `false` to suppress the popup entirely.

```json
"show-desktop-overlay": true
```

### 2.7 `enable-emergency-shortcut`

| Key                          | Type    | Default  |
|------------------------------|---------|----------|
| `enable-emergency-shortcut`  | boolean | `false`  |

When `true`, the hardcoded emergency exit shortcut `Ctrl+Mod1+BackSpace`
is active and immediately terminates the window manager ignoring peding
session hooks.  Set to `false` to disable that shortcut, for example on
systems where the key combination might be triggered accidentally.

```json

"enable-emergency-shortcut": true
```

### 2.8 `menu`

| Key                        | Type   | Default         |
|----------------------------|--------|-----------------|
| `menus.root.position`      | string | `"under-mouse"` |
| `menus.windows.position`   | string | `"under-mouse"` |

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

### 2.9 `systray`

| Key                  | Type    | Default            |
|----------------------|---------|--------------------|
| `systray.is-enabled` | boolean | `false`            |
| `systray.position`   | string  | `"top-right"`      |
| `systray.order`      | string  | `"left-to-right"`  |
| `systray.layer`      | string  | `"above"`          |

Built-in systray dock.  `is-enabled` turns it on, and `position` (one of
`"top-left"`, `"top-right"`, `"bottom-left"`, or `"bottom-right"`)
selects which corner of the first managed screen it docks in.

The key `order` controls where a newly docked icon is placed relative to
the ones already there: `"left-to-right"` appends it after the last
icon, `"right-to-left"` inserts it before the first, and `"ascending"`
/ `"descending"` instead keep the whole row continuously sorted
alphabetically ('A-Z' or 'Z-A') by each icon's window class name,
ignoring insertion order entirely.

The key `layer` controls where the dock sits in the stacking order:
`"below"` keeps it behind every normal client window, `"above"` (the
default) keeps it above normal windows but still lets a fullscreen
window cover it, the same way a fullscreen window covers a taskbar or
panel in most desktop environments, and `"above-all"` keeps it above
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

| Key                        | Type    | Default    |
|-----------------------------|---------|------------|
| `systray.clock.is-enabled` | boolean | `false`    |
| `systray.clock.format`     | string  | `"%H:%M"`  |
| `systray.clock.position`   | string  | `"right"`  |

An optional clock drawn inside the systray dock, next to the icons.
`is-enabled` turns it on; when it is the only reason the tray would
otherwise stay hidden (no icons docked), the tray still shows with just
the clock.

`format` is a `strftime(3)` format string, interpreted in the system's
local time zone.  A few common examples:

| `format`      | Looks like            |
|---------------|------------------------|
| `"%H:%M"`     | `14:07`                |
| `"%H:%M:%S"`  | `14:07:32`              |
| `"%F %R"`     | `2026-08-07 14:07`      |
| `"%a %d %b"`  | `Fri 07 Aug`            |

`position` (`"left"` or `"right"`) controls whether the clock is drawn
before or after the icons, in dock order; it does not affect which
corner of the screen the whole tray sits in, which is still
`systray.position` above.

The clock redraws itself once per second while any part of it (the
tray dock, or the clock specifically) is enabled; a `format` string
without `%S` or other sub-minute fields simply redraws the same text
every second, which is harmless.

```json
"systray": {
    "is-enabled": false,
    "position": "top-right",
    "order": "left-to-right",
    "layer": "above",
    "clock": {
        "is-enabled": true,
        "format": "%F %R",
        "position": "right"
    }
}
```

### 2.10 `xsettings`

| Key                           | Type    | Default     |
|-------------------------------|---------|-------------|
| `xsettings.is-enabled`        | boolean | `false`     |
| `xsettings.gtk-theme-name`    | string  | `"Adwaita"` |
| `xsettings.icon-theme-name`   | string  | `"Adwaita"` |
| `xsettings.cursor-theme-name` | string  | `"Adwaita"` |
| `xsettings.cursor-theme-size` | integer | `24`        |
| `xsettings.dpi`               | integer | `96`        |

Built-in XSETTINGS manager, implementing the freedesktop.org XSETTINGS
specification.  Many GTK and Qt applications have a "use theme colors"
or "use system settings" option that only takes effect if some XSETTINGS
manager is running to tell them what the theme, icon theme, cursor
theme, and display DPI actually are; without one, those applications
silently fall back to their own built-in defaults regardless of what
this option is set to.  When enabled, IcoWM acquires the `_XSETTINGS_Sn`
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
    "gtk-theme-name": "Adwaita",
    "icon-theme-name": "Adwaita",
    "cursor-theme-name": "Adwaita",
    "cursor-theme-size": 24,
    "dpi": 96
}
```

## 3. `bindings.json` -- Keyboard and mouse bindings

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
| `modl` | `Caps_Lock`    | Caps Lock                     |
| `mod1` | `Alt`           | Alt / Meta key                |
| `mod2` | `Num_Lock`     | Num Lock                      |
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

| Key            | Default binding         | Action |
|----------------|-------------------------|--------|
| `close`        | `modc+mod1+c`           | Send `WM_DELETE_WINDOW` to politely close the window. |
| `kill`         | `modc+mod1+mods+Escape` | Forcibly terminate the client process. |
| `iconify`      | `modc+mod1+i`           | Iconify the window (TWM-style desktop icon). |
| `hide`         | `modc+mod1+mods+u`      | Hide the window without iconifying it. |
| `maximize`     | `modc+mod1+m`           | Toggle maximize (full work area). |
| `fullscreen`   | `modc+mod1+f`           | Toggle true fullscreen mode. |
| `shade`        | `modc+mod1+s`           | Roll-up / roll-down the window (shade). |
| `pin`          | `modc+mod1+p`           | Toggle sticky mode (window appears on all desktops). |
| `decorate`     | `modc+mod1+d`           | Toggle window decorations (title bar). |
| `layer`        | `modc+mod1+mods+y`      | Cycle the window stacking layer: *normal* > *above* > *below*. |
| `info`         | `modc+mod1+mods+i`      | Show a popup with window information. |
| `show-desktop` | `modc+mod1+mods+d`      | Toggle show-desktop mode: hide all windows; press again to restore them. |

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
| `show-desktop` | `modc+mod1+mods+d` | Hide all windows and show the empty desktop. |
| `redraw`       | `modc+mod1+mods+r` | Force a full redraw of all windows. |
| `reload`       | `modc+mod1+mods+c` | Reload the configuration files (equivalent to `SIGHUP`). |
| `quit`         | `modc+mod1+mods+x` | Exit IcoWM. |

#### `keyboard.wm.menus`

Keyboard shortcuts for the two menus that have no inherent screen
position of their own for where each one appears when opened this way).


| Key       | Default binding     | Action |
|-----------|---------------------|--------|
| `root`    | `modc+mod1+mods+m`  | Open the desktop (root) context menu, `menu.json`. |
| `windows` | `modc+mod1+mods+w`  | Open the menu listing every window on every desktop. |

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

| Key        | Default binding  | Destination  |
|------------|------------------|--------------|
| `desktop0` | `modc+mod1+0`    | Desktop 0.   |
| `desktop1` | `modc+mod1+1`    | Desktop 1.   |
| `desktop2` | `modc+mod1+2`    | Desktop 2.   |
| `desktop3` | `modc+mod1+3`    | Desktop 3.   |
| `desktop4` | `modc+mod1+4`    | Desktop 4.   |
| `desktop5` | `modc+mod1+5`    | Desktop 5.   |
| `desktop6` | `modc+mod1+6`    | Desktop 6.   |
| `desktop7` | `modc+mod1+7`    | Desktop 7.   |
| `desktop8` | `modc+mod1+8`    | Desktop 8.   |
| `desktop9` | `modc+mod1+9`    | Desktop 9.   |

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

| Key      | Default binding | Action                                       |
|----------|-----------------|----------------------------------------------|
| `move`   | `mod1+button1`  | Click and drag to move the window.           |
| `lower`  | `mod1+button2`  | Lower the window to the bottom of the stack. |
| `resize` | `mod1+button3`  | Click and drag to resize the window.         |

### 3.8 `mouse.cycle`

Mouse button bindings for switching virtual desktops.

| Key                  | Default binding | Action |
|----------------------|-----------------|--------|
| `cycle.desktop.prev` | `button4`       | Scroll up to go to the previous desktop. |
| `cycle.desktop.next` | `button5`       | Scroll down to go to the next desktop. |

## 4. `themes/<name>.json` -- Theme configuration

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

| Key                    | Type    | Default  | Description |
|------------------------|---------|----------|-------------|
| `height`               | integer | `19`     | Title bar height in pixels.  A value of `0` is equivalent to `window.is-decorated: false`: with nothing to draw and nowhere to put buttons, the window is treated as undecorated regardless of `is-decorated`'s own value. |
| `alignment`             | string  | `"left"` | Where the title text sits within the space its buttons leave available.  One of `"left"`, `"center"`, `"right"`. |
| `padding.horizontal`    | integer | `2`      | Horizontal inset, in pixels, between the frame's edge and its outermost buttons on each side, and between a button group and the title text. |
| `padding.vertical`      | integer | `2`      | Vertical inset, in pixels, buttons are kept from the titlebar's top and bottom edge before being centered in whatever room that leaves.  If the titlebar is too short for the padding to fit a full button, this is ignored in favor of plain centering. |
| `buttons.left`          | array of strings | `["pin", "layer"]` | Buttons drawn left-to-right starting at the frame's left edge. |
| `buttons.right`         | array of strings | `["iconize", "hide", "shade", "maximize", "fullscreen", "close"]` | Buttons drawn right-to-left starting at the frame's right edge. |

Accepted button names, for both `buttons.left` and `buttons.right`,
are: `"pin"`, `"layer"`, `"iconize"`, `"hide"`, `"shade"`,
`"maximize"`, `"fullscreen"`, `"close"`.  A button omitted from both
lists is simply never drawn and never clickable; there is no separate
setting to hide a button.  The same name can only usefully appear
once across both lists (whichever list is processed for it first
wins its slot; putting it in both does not draw it twice).

#### `window.active` / `window.inactive`

Appearance of the focused window (`active`) and of windows that do not
have focus (`inactive`).  Both share the same shape:

| Key                 | Type    | Default (active) | Default (inactive) | Description |
|---------------------|---------|-------------------|---------------------|-------------|
| `font`              | string  | `"fixed bold"`    | `"fixed"`           | Title bar font (X core font description; see note below). |
| `color.background`  | string  | `"#9AAEC8"`       | `"#D0D9E5"`         | Title bar background color. |
| `color.foreground`  | string  | `"#253040"`       | `"#4A5566"`         | Title bar text and button color. |
| `border.color`      | string  | `"#4A5566"`       | `"#7F9AB6"`         | Border color. |
| `border.width`      | integer | `2`               | `2`                 | Border thickness in pixels. |

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

#### `icon.active` / `icon.inactive`

Same shape as `window.active` / `window.inactive` above (`font`,
`color.background`, `color.foreground`, `border.color`,
`border.width`), applied to the icon selected in the icon-cycle menu
(`active`) versus every other icon (`inactive`).

| Key                 | Type    | Default (active) | Default (inactive) |
|---------------------|---------|-------------------|---------------------|
| `font`              | string  | `"fixed bold"`    | `"fixed"`           |
| `color.background`  | string  | `"#9AAEC8"`       | `"#D0D9E5"`         |
| `color.foreground`  | string  | `"#253040"`       | `"#4A5566"`         |
| `border.color`      | string  | `"#4A5566"`       | `"#7F9AB6"`         |
| `border.width`      | integer | `1`               | `1`                 |

### 4.3 `systray`

A `font` / `color` / `border` block, the same shape as `window.active`
above, applied to the systray dock itself, plus its own height and the
clock's vertical alignment.

| Key                 | Type    | Default     |
|---------------------|---------|-------------|
| `font`              | string  | `"fixed"`   |
| `color.background`  | string  | `"#D0D9E5"` |
| `color.foreground`  | string  | `"#4A5566"` |
| `border.color`      | string  | `"#7F9AB6"` |
| `border.width`      | integer | `1`         |
| `height`            | integer | `32`        |
| `clock.valign`      | string  | `"center"`  |

`height` is the tray dock's own height in pixels; icons and the clock
(when enabled, see `systray.clock.is-enabled` in `config.json`) are
positioned within it according to `clock.valign` for the clock, and
centered for icons.  It must be at least tall enough to fit one icon or
icons get clipped.

`clock.valign` (one of `"center"`, `"top"`, or `"bottom"`) controls
where the clock text sits vertically within that height.  With the
default `height` of `32`, an icon already fills nearly the whole row,
so `valign` has little visible effect; raising `height` gives it
actual room to work with.  This is unrelated to `systray.clock.position`
in `config.json`, which instead controls whether the clock sits before
or after the icons horizontally, in dock order; that stays a behavior
setting rather than an appearance one, since it changes where among the
icons the clock counts as being docked.

```json
"systray": {
    "font": "fixed",
    "color": { "background": "#D0D9E5", "foreground": "#4A5566" },
    "border": { "color": "#7F9AB6", "width": 1 },
    "height": 32,
    "clock": {
        "valign": "center"
    }
}
```

### 4.4 `menu`

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

`unselected.border` doubles as the frame drawn around the whole
cycle-menu popup window itself, not just individual context-menu rows,
which is why its default width is `1` rather than `0`: a context menu
window currently has no frame of its own, so that same default also
means every ordinary (non-label) row gets a subtle 1px outline unless
lowered to `0`.

| Key                        | Type    | Default     |
|-----------------------------|---------|-------------|
| `unselected.font`          | string  | `"fixed"`   |
| `unselected.color.background` | string | `"#D0D9E5"` |
| `unselected.color.foreground` | string | `"#4A5566"` |
| `unselected.border.color`  | string  | `"#7F9AB6"` |
| `unselected.border.width`  | integer | `1`         |
| `selected.font`            | string  | `"fixed bold"` |
| `selected.color.background` | string | `"#9AAEC8"` |
| `selected.color.foreground` | string | `"#253040"` |
| `selected.border.color`    | string  | `"#4A5566"` |
| `selected.border.width`    | integer | `1`         |
| `label.font`                | string | `"fixed"`   |
| `label.color.background`   | string  | `"#D0D9E5"` |
| `label.color.foreground`   | string  | `"#7F9AB6"` |
| `label.border.color`       | string  | `"#7F9AB6"` |
| `label.border.width`       | integer | `0`         |
| `disabled.color.foreground` | string | `"#A0A8B0"` |
| `separator.color`          | string  | `"#7F9AB6"` |
| `padding.horizontal`       | integer | `12`        |
| `padding.vertical`         | integer | `4`         |

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

`padding.horizontal` and `padding.vertical` are the inset in pixels
between a menu window's own edges and its content: row text (and, for
a submenu, its arrow indicator) for `padding.horizontal`, and the
space above the first row and below the last for `padding.vertical`.
Both apply to every context menu and to the Alt+Tab-style cycle menu
alike, and equally to `unselected`, `selected`, and `label` rows.

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
    }
}
```

### 4.5 `dialog`

Applies to the quit-confirmation dialog and the generic message
dialog.

| Key                          | Type    | Default     |
|-------------------------------|---------|-------------|
| `color.background`           | string  | `"#D0D9E5"` |
| `border.color`                | string  | `"#7F9AB6"` |
| `border.width`                | integer | `2`         |
| `label.font`                  | string  | `"fixed"`   |
| `label.color.foreground`     | string  | `"#4A5566"` |
| `label.padding.horizontal`   | integer | `12`        |
| `label.padding.vertical`     | integer | `12`        |
| `button.unselected.font`     | string  | `"fixed"`   |
| `button.unselected.color.background` | string | `"#D0D9E5"` |
| `button.unselected.color.foreground` | string | `"#4A5566"` |
| `button.unselected.border.color` | string | `"#7F9AB6"` |
| `button.unselected.border.width` | integer | `1`     |
| `button.selected.font`       | string  | `"fixed bold"` |
| `button.selected.color.background` | string | `"#9AAEC8"` |
| `button.selected.color.foreground` | string | `"#253040"` |
| `button.selected.border.color` | string | `"#4A5566"` |
| `button.selected.border.width` | integer | `1`     |
| `button.gap`                  | integer | `12`        |
| `button.padding.horizontal`  | integer | `12`        |
| `button.padding.vertical`    | integer | `6`         |

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

### 4.6 `overlay`

A single `font` / `color` / `border` block, the same shape as
`window.active` (section 4.1), applied to transient informational
overlays that are not menus or dialogs: the client-info popup and the
desktop-switch notification.  Both are single-style, non-interactive
overlays with no selected/unselected state to distinguish.

| Key                 | Type    | Default     |
|---------------------|---------|-------------|
| `font`              | string  | `"fixed"`   |
| `color.background`  | string  | `"#D0D9E5"` |
| `color.foreground`  | string  | `"#4A5566"` |
| `border.color`      | string  | `"#7F9AB6"` |
| `border.width`      | integer | `1`         |

```json
"overlay": {
    "font": "fixed",
    "color": { "background": "#D0D9E5", "foreground": "#4A5566" },
    "border": { "color": "#7F9AB6", "width": 1 }
}
```

### 4.7 Configuration reload and already-open windows

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

---

> **Font format note:**  IcoWM uses the X server's built-in *X core
> font* system (accessed via XCB), which is completely separate from
> client-side font rendering libraries such as FreeType/Fontconfig,
> Pango, or Cairo.  Only **X11 bitmap fonts** (BDF/PCF) are supported;
> TrueType (TTF), OpenType (OTF), and other scalable formats are **not**
> available here.
>
> The `font` field accepts two formats:
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
---

## 5. `randr.json` -- XRandR output profiles

Defines per-output settings applied by IcoWM through the XRandR
extension.  This file is **optional**.  If absent, XRandR hot-plug
event handling still works (screen-change notifications are processed),
but no output profiles are configured.

Up to **8** output entries are supported.

### 5.1 Top-level fields

| Key          | Type    | Default | Description |
|--------------|---------|---------|-------------|
| `is-enabled` | boolean | `false` | Master switch.  Set to `true` to activate output profile management. |

### 5.2 `outputs[]` entries

Each entry in the `outputs` array describes one physical display output.

| Key            | Type    | Default    | Description |
|----------------|---------|------------|-------------|
| `name`         | string  | `""`       | Output connector name as reported by the X server (e.g., `"HDMI-1"`, `"eDP-1"`, `"DP-2"`).  Run `xrandr` in a terminal to list available names. |
| `is-enabled`   | boolean | `false`    | Whether this output should be active. |
| `is-primary`   | boolean | `false`    | Mark this output as the primary display. |
| `resolution.w` | integer | `0`        | Preferred horizontal resolution in pixels. |
| `resolution.h` | integer | `0`        | Preferred vertical resolution in pixels. |
| `position.x`   | integer | `0`        | Horizontal position of this output in the virtual screen. |
| `position.y`   | integer | `0`        | Vertical position of this output in the virtual screen. |
| `rotation`     | string  | `"normal"` | Screen rotation. |

Accepted `rotation` values are: `"normal"`, `"left"` (90°),
`"right"` (270°), `"inverted"` (180°).

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

## 6. `rules.json` -- Per-window rules

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

| Key               | Type              | Default | Description |
|-------------------|-------------------|---------|-------------|
| `match.instance`  | string or array   | unset   | Match the first string in `WM_CLASS` (instance name). |
| `match.class`     | string or array   | unset   | Match the second string in `WM_CLASS` (class name). |
| `match.role`      | string or array   | unset   | Match `WM_WINDOW_ROLE`. |
| `match.title`     | string or array   | unset   | Match the current window title. |
| `match.type`      | string or array   | unset   | Match `_NET_WM_WINDOW_TYPE`. |
| `match.transient` | boolean           | unset   | Match whether the window is transient for another window. |

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

| Key                     | Type                 | Default | Description |
|-------------------------|----------------------|---------|-------------|
| `apply.desktop`         | integer              | unset   | Zero-based desktop index to move the window to. |
| `apply.layer`           | string               | unset   | Stacking layer.  Accepted values: `"below"`, `"normal"`, `"above"`. |
| `apply.focus`           | boolean              | unset   | Whether the matched window should receive focus. |
| `apply.sticky`          | boolean              | unset   | Whether the window should be visible on all desktops. |
| `apply.decorated`       | boolean              | unset   | Whether the window should keep its decorations. |
| `apply.position`        | object or `"center"` | unset   | Where to place the window; see below. |
| `apply.position.x`      | integer              | unset   | Absolute X position in pixels (when `position` is an object). |
| `apply.position.y`      | integer              | unset   | Absolute Y position in pixels (when `position` is an object). |
| `apply.size.width`      | integer              | unset   | Window width in pixels; must be greater than `0`. |
| `apply.size.height`     | integer              | unset   | Window height in pixels; must be greater than `0`. |

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

## 7. `session.json` -- Session lifecycle hooks

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

## 8. `menu.json` -- Root desktop menu

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

| `"type"`      | Description                                           |
|---------------|-------------------------------------------------------|
| `"command"`   | Clickable item that launches an application           |
| `"separator"` | Horizontal dividing line (no other fields needed)     |
| `"label"`     | Non-clickable section heading                         |
| `"submenu"`   | Nested sub-menu revealed on hover/click               |

### 8.3 Entry fields reference

#### `"command"` entry

| Field       | Type   | Required | Description                            |
|-------------|--------|----------|----------------------------------------|
| `"type"`    | string | yes      | Must be `"command"`                    |
| `"name"`    | string | yes      | Label text shown in the menu           |
| `"command"` | string | yes      | Shell command or program to execute    |

The `"command"` value is passed through `wordexp(3)` so environment
variables and simple shell expansions (`~`, `$HOME`, …) are supported.
Command injection via sub-shells is **disabled** (`WRDE_NOCMD`).

If the command cannot be executed, IcoWM logs a warning and shows an
informational dialog so the user is notified immediately.

#### `"separator"` entry

| Field    | Type   | Required | Description         |
|----------|--------|----------|---------------------|
| `"type"` | string | yes      | Must be `"separator"` |

At this moment there's only one kind of `"type"`, which is `"separator"`.

#### `"label"` entry

| Field    | Type   | Required | Description                      |
|----------|--------|----------|----------------------------------|
| `"type"` | string | yes      | Must be `"label"`                |
| `"name"` | string | yes      | Section heading text to display  |

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

## 9. Full examples

### `config.json`

```json
{
    "theme": "default",

    "screens": {
        "count": 2,
        "settings": {
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
                        { "name": "Desktop B", "background-color": "#8a8f94" },
                    ]
                }
            ]
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
        "group-related": true,
        "focus": {
            "policy": "click",
            "is-new-focused": true,
            "is-raised-on-focus": false
        },
        "placement": {
            "policy": "smart"
        }
    },

    "icons": {
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
        "position": "top-right",
        "order": "left-to-right",
        "layer": "above",
        "clock": {
            "is-enabled": true,
            "format": "%F %R",
            "position": "right"
        }
    },

    "xsettings": {
        "is-enabled": false,
        "gtk-theme-name": "Adwaita",
        "icon-theme-name": "Adwaita",
        "cursor-theme-name": "Adwaita",
        "cursor-theme-size": 24,
        "dpi": 96
    },

    "show-desktop-overlay": true,
    "enable-emergency-shortcut": false    
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
            "show-desktop": "modc+mod1+mods+d",
            "redraw": "modc+mod1+mods+r",
            "reload": "modc+mod1+mods+c",
            "quit": "modc+mod1+mods+x",
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
    "name": "Default",

    "window": {
        "general": {
            "border-width": 2,
            "is-decorated": true
        },
        "active": {
            "background-color": "#9AAEC8",
            "foreground-color": "#253040",
            "border-color": "#4A5566",
            "font": "fixed"
        },
        "inactive": {
            "background-color": "#D0D9E5",
            "foreground-color": "#4A5566",
            "border-color": "#7F9AB6",
            "font": "fixed"
        }
    },

    "icon": {
        "general": {
            "border-width": 2,
            "is-captioned": true
        },
        "active": {
            "background-color": "#9AAEC8",
            "foreground-color": "#253040",
            "border-color": "#4A5566",
            "grip-color": "#9AAEC8",
            "font": "fixed"
        },
        "inactive": {
            "background-color": "#D0D9E5",
            "foreground-color": "#4A5566",
            "border-color": "#7F9AB6",
            "grip-color": "#4A5566",
            "font": "fixed"
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
                { "type": "command", "name": "Nexuiz",  "command": "nexuiz" },
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
