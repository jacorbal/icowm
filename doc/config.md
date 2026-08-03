# IcoWM Configuration Reference

Description of every configuration file used by IcoWM, the purpose of
each file, and every supported option together with its type, accepted
values, and built-in default value.

---

## Table of Contents

1. [Directory layout](#1-directory-layout)
2. [`config.json` -- Base configuration](#2-configjson--base-configuration)
   - [2.1 `theme`](#21-theme)
   - [2.2 `screens`](#22-screens)
   - [2.3 `programs`](#23-programs)
   - [2.4 `windows`](#24-windows)
   - [2.5 `icons`](#25-icons)
3. [`bindings.json` -- Keyboard and mouse bindings](#3-bindingsjson--keyboard-and-mouse-bindings)
   - [3.1 Binding syntax](#31-binding-syntax)
   - [3.2 `modifiers`](#32-modifiers)
   - [3.3 `keyboard.launch`](#33-keyboardlaunch)
   - [3.4 `keyboard.window`](#34-keyboardwindow)
   - [3.5 `keyboard.wm`](#35-keyboardwm)
   - [3.6 `keyboard.cycle`](#36-keyboardcycle)
   - [3.7 `mouse.window`](#37-mousewindow)
   - [3.8 `mouse.cycle`](#38-mousecycle)
4. [`themes/<name>.json` -- Theme configuration](#4-themesnamejson--theme-configuration)
   - [4.1 `window`](#41-window)
   - [4.2 `icon`](#42-icon)
5. [`randr.json` -- XRandR output profiles](#5-randrjson--xrandr-output-profiles)
   - [5.1 Top-level fields](#51-top-level-fields)
   - [5.2 `outputs[]` entries](#52-outputs-entries)
6. [Full examples](#6-full-examples)

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
├── config.json       Base configuration (optional)
├── bindings.json     Keyboard & mouse bindings (optional)
├── randr.json        XRandR output profiles (optional)
└── themes/
    └── default.json  Theme file referenced by 'config.json' (optional)
```

- All files are optional; they fall back to built-in defaults when
  absent.
- Theme files are loaded from the `themes/` sub-directory.  The theme
  name field in `config.json` must match the filename without the
  `.json` extension.
- `randr.json` is completely optional; XRandR hot-plug event handling
  is always active regardless of this file.

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

| Key | Type | Default |
|---|---|---|
| `screens.count` | integer | `1` |

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
                    { "name": "Secondary", "background-color": "#c0c0c0" }
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

| Key | Type | Default | Description |
|---|---|---|---|
| `programs.terminal` | string | `"xterm"` | Terminal emulator. |
| `programs.launcher` | string | `"gmrun"` | Application launcher / run dialog. |
| `programs.file-manager` | string | `"pcmanfm"` | File manager. |
| `programs.editor` | string | `"gvim"` | Text editor. |
| `programs.web-browser` | string | `"firefox"` | Web browser. |

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

Accepted values: `"north-west"`, `"north"`, `"north-east"`, `"east"`,
`"south-east"`, `"south"`, `"south-west"`, `"west"`, `"center"`,
`"static"`.

#### `windows.snap`

| Key            | Type    | Default |
|----------------|---------|---------|
| `windows.snap` | integer | `4`     |

Snap threshold in pixels.  When a window being dragged comes within
this many pixels of a screen edge or another window border, it snaps
into alignment.  Set to `0` to disable snapping.

#### `windows.has-grips`

| Key                 | Type    | Default |
|---------------------|---------|---------|
| `windows.has-grips` | boolean | `true`  |

When `true`, decorated resizable windows draw corner resize grips.

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
"focus": {
    "policy":            "click",
    "is-new-focused":    true,
    "is-raised-on-focus": false
}
```

#### `windows.placement`

| Key                | Type   | Default   | Description |
|--------------------|--------|-----------|-------------|
| `placement.policy` | string | `"smart"` | How newly mapped windows are placed. |

Accepted placement policy values:

| Value           | Behavior |
|-----------------|----------|
| `"smart"`       | Finds the position that minimizes overlap with existing windows. |
| `"cascade"`     | Places windows in a stepped diagonal sequence. |
| `"centered"`    | Centers the window on the screen. |
| `"under-mouse"` | Places the window under the current pointer position. |

Transient (dialog) windows are always centered over their parent
window, regardless of this setting.

```json
"placement": {
    "policy": "smart"
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

| Key                      | Type   | Default    | Description |
|--------------------------|--------|------------|-------------|
| `icons.placement.policy` | string | `"bottom"` | Where new icons are placed. |

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

## 3. `bindings.json` -- Keyboard and mouse bindings

Defines all keyboard shortcuts and mouse button bindings.  This file is
optional; if absent, the built-in defaults listed in the tables below
are used.

### 3.1 Binding syntax

A binding is a `+`-separated chain of modifier aliases and a final key
or button name:

```
modifier1+modifier2+KeyName
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

| Key          | Default binding         | Action |
|--------------|-------------------------|--------|
| `close`      | `modc+mod1+c`           | Send `WM_DELETE_WINDOW` to politely close the window. |
| `kill`       | `modc+mod1+mods+Escape` | Forcibly terminate the client process. |
| `iconify`    | `modc+mod1+i`           | Iconify the window (TWM-style desktop icon). |
| `hide`       | `modc+mod1+mods+h`      | Hide the window without iconifying it. |
| `maximize`   | `modc+mod1+m`           | Toggle maximize (full work area). |
| `fullscreen` | `modc+mod1+f`           | Toggle true fullscreen mode. |
| `shade`      | `modc+mod1+s`           | Roll-up / roll-down the window (shade). |
| `pin`        | `modc+mod1+p`           | Toggle sticky mode (window appears on all desktops). |
| `decorate`   | `modc+mod1+d`           | Toggle window decorations (title bar). |
| `layer`      | `modc+mod1+mods+y`      | Cycle the window stacking layer: *normal* > *above* > *below*. |
| `info`       | `modc+mod1+mods+i`      | Show a popup with window information. |

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

| Key      | Default binding    | Action |
|----------|--------------------|--------|
| `redraw` | `modc+mod1+mods+r` | Force a full redraw of all windows. |
| `reload` | `modc+mod1+mods+c` | Reload the configuration files (equivalent to `SIGHUP`). |
| `quit`   | `modc+mod1+mods+x` | Exit IcoWM. |

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

| Key | Default binding | Action |
|---|---|---|
| `cycle.desktop.prev` | `button4` | Scroll up to go to the previous desktop. |
| `cycle.desktop.next` | `button5` | Scroll down to go to the next desktop. |

## 4. `themes/<name>.json` -- Theme configuration

Controls the visual appearance of windows and desktop icons.  Theme
files live in the `themes/` subdirectory of the configuration directory.
The name in `config.json`'s `"theme"` field selects which file is
loaded.

All color values are hex strings in the form `"#RRGGBB"` or `"RRGGBB"`.

### 4.1 `window`

Appearance settings for managed windows.

#### `window.general`

| Key            | Type    | Default | Description                 |
|----------------|---------|---------|-----------------------------|
| `border-width` | integer | `2`     | Border thickness in pixels. |
| `is-decorated` | boolean | `true`  | When `false`, windows start without any decoration (title bar is hidden). |

#### `window.active`

Appearance of the currently focused window.

| Key                | Type   | Default     | Description                 |
|--------------------|--------|-------------|-----------------------------|
| `background-color` | string | `"#9AAEC8"` | Title bar background color. |
| `foreground-color` | string | `"#253040"` | Title bar text color.       |
| `border-color`     | string | `"#4A5566"` | Border color.               |
| `grip-color`       | string | `"#9AAEC8"` | Resize grip color.          |
| `font`             | string | `"fixed"`   | Title bar font (X core font description; see note below). |

#### `window.inactive`

Appearance of windows that do not have focus.

| Key                | Type   | Default     | Description                 |
|--------------------|--------|-------------|-----------------------------|
| `background-color` | string | `"#D0D9E5"` | Title bar background color. |
| `foreground-color` | string | `"#4A5566"` | Title bar text color.       |
| `border-color`     | string | `"#7F9AB6"` | Border color.               |
| `grip-color`       | string | `"#4A5566"` | Resize grip color.          |
| `font`             | string | `"fixed"`   | Title bar font.             |

### 4.2 `icon`

Appearance settings for iconified windows.

#### `icon.general`

| Key            | Type    | Default | Description                      |
|----------------|---------|---------|----------------------------------|
| `border-width` | integer | `2`     | Icon border thickness in pixels. |
| `is-captioned` | boolean | `true`  | When `true`, the icon displays the window title below the icon graphic. |

#### `icon.active`

Appearance of the currently focused icon.

| Key                | Type   | Default     | Description              |
|--------------------|--------|-------------|--------------------------|
| `background-color` | string | `"#9AAEC8"` | Icon background color.   |
| `foreground-color` | string | `"#253040"` | Icon caption text color. |
| `border-color`     | string | `"#4A5566"` | Icon border color.       |
| `font`             | string | `"fixed"`   | Icon caption font.       |

#### `icon.inactive`

Appearance of icons that do not have focus.

| Key                | Type   | Default     | Description              |
|--------------------|--------|-------------|--------------------------|
| `background-color` | string | `"#D0D9E5"` | Icon background color.   |
| `foreground-color` | string | `"#4A5566"` | Icon caption text color. |
| `border-color`     | string | `"#7F9AB6"` | Icon border color.       |
| `font`             | string | `"fixed"`   | Icon caption font.       |

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
>    - `"fixed"` -- the `fixed` alias (available on every X server)
>    - `"fixed 12"` -- `fixed` family at 12 pixels
>    - `"fixed bold 12"` -- `fixed` family, bold weight, 12 pixels
>    - `"fixed bold 12 iso8859-15"` -- `fixed`, bold, 12 pixels,
>       ISO 8859-15 charset
>    - `"courier bold italic 17"` -- Courier, bold italic, 17 pixels
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
| `rotation`     | string  | `"normal"` | Screen rotation.  Accepted values: `"normal"`, `"left"` (90°), `"right"` (270°), `"inverted"` (180°). |

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

## 6. Full examples

### `config.json`

```json
{
    "theme": "default",

    "screens": {
        "count": 1,
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
                }
            ]
        }
    },

    "programs": {
        "terminal": "xterm",
        "launcher": "gmrun",
        "file-manager": "pcmanfm",
        "editor": "gvim",
        "web-browser": "firefox"
    },

    "windows": {
        "gravity": "north-west",
        "has-grips": true
        "snap": 4,
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
            "redraw": "modc+mod1+mods+r",
            "reload": "modc+mod1+mods+c",
            "quit": "modc+mod1+mods+x"
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
