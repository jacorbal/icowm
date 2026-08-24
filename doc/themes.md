# IcoWM Theme Reference (`themes/<name>.json`)

Description of every field a theme file may set, its type, accepted
values, and built-in default value, covering the visual appearance of
windows, desktop icons, the systray, menus, and every other themeable
surface IcoWM draws.

---

## Table of Contents

1. [`window`](#1-window)
2. [`icon`](#2-icon)
3. [`cycle`](#3-cycle)
4. [`systray`](#4-systray)
5. [`desktop`](#5-desktop)
6. [`menu`](#6-menu)
7. [`dialog`](#7-dialog)
8. [`overlay`](#8-overlay)
9. [`xsettings`](#9-xsettings)
10. [Font format reference](#10-font-format-reference)
11. [`scratchpad`](#11-scratchpad)
12. [`search`](#12-search)
13. [`prompt`](#13-prompt)
14. [Full example: `themes/default.json`](#14-full-example-themesdefaultjson)

For everything that is not a theme file, see [`config.md`](config.md)
instead.

---

Controls the visual appearance of windows, desktop icons, and the
systray.  Theme files live in the `themes/` subdirectory of the
configuration directory.  The name in `config.json`'s `"theme"` field
selects which file is loaded.

All color values are hex strings in the form `"#RRGGBB"` or `"RRGGBB"`.

`name` is the only top-level field IcoWM actually reads.  `-author`,
`-creation-date`, and `-modified-date` may also appear at the top level,
but they are purely comments for whoever maintains the file; IcoWM never
parses or acts on them, nor it will not parse any key that begings with
the characters `-` or `_`.

```json
{
    "name": "Default theme",
    "-author": "Jane Doe",
    "-creation-date": "Sat Aug  1 03:57:54 UTC 2026",
    "-modified-date": "Sat Aug  7 15:08:21 UTC 2026",

    "cycle": { "...": "..." },
    "desktop": { "...": "..." },
    "dialog": { "...": "..." },
    "icon": { "...": "..." },
    "menu": { "...": "..." },
    "overlay": { "...": "..." },
    "prompt": { "...": "..." },
    "scracthpad": { "...": "..." },
    "search": { "...": "..." },
    "window": { "...": "..." },
    "xsettings": { "...": "..." }
}
```

## 1. `window`

Appearance settings for managed windows.

| Key            | Type    | Default | Description |
|----------------|---------|---------|-------------|
| `is-decorated` | boolean | `true`  | When `false`, windows start without any decoration (no title bar, no themed border).  Equivalent to setting `titlebar.height` to `0`; see below. |

#### `window.titlebar`

| Key                  | Type             | Default            | Description |
|----------------------|------------------|--------------------|-------------|
| `height`             | integer          | `19`               | Title bar height in pixels.  A value of `0` is equivalent to `window.is-decorated: false`: with nothing to draw and nowhere to put buttons, the window is treated as undecorated regardless of `is-decorated`'s value. |
| `alignment`          | string           | `"left"`           | Where the title text sits within the space its buttons leave available.  One of `"left"`, `"center"`, `"right"`. |
| `padding.horizontal` | integer          | `2`                | Horizontal inset, in pixels, between the frame's edge and its outermost buttons on each side, and between a button group and the title text. |
| `padding.vertical`   | integer          | `2`                | Vertical inset, in pixels, buttons are kept from the titlebar's top and bottom edge before being centered in whatever room that leaves.  If the titlebar is too short for the padding to fit a full button, this is ignored in favor of plain centering. |
| `buttons.left`       | array of strings | `["pin", "layer"]` | Buttons drawn left-to-right starting at the frame's left edge. |
| `buttons.right`      | array of strings | `["iconize", "hide", "shade", "maximize", "fullscreen", "close"]` | Buttons drawn right-to-left starting at the frame's right edge. |
| `buttons.color.on`   | string           | `"#253040"`        | Color for a button whose state is currently engaged: pinned, a non-normal layer, or simply the window being focused for every other button. |
| `buttons.color.off`  | string           | `"#4A5566"`        | Color for a button otherwise, i.e., not engaged. |

Accepted button names, for both `buttons.left` and `buttons.right`, are:
`"pin"`, `"layer"`, `"iconize"`, `"hide"`, `"shade"`, `"maximize"`,
`"fullscreen"`, `"close"`.  A button omitted from both lists is simply
never drawn and never clickable; there is no separate setting to hide
a button.  The same name can only usefully appear once across both lists
(whichever list is processed for it first wins its slot; putting it in
both does not draw it twice).

`buttons.color` is independent of `window.active`/`window.inactive`'s
`color.foreground` below, so a theme can restyle button glyphs without
the title text changing color to match, or the other way around.  There
is deliberately no third color for a button that cannot currently do
anything (e.g., maximize on a non-resizable client): that button is not
drawn at all rather than needing a separate color for that case.

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
| `opacity`          | integer | `100`            | `100`              | Desired opacity, 0 to 100, published on the frame through `_NET_WM_WINDOW_OPACITY`.  IcoWM never composites anything itself, so this has no visible effect at all unless a compositing manager, e.g., picom, is also running and reading the property back off the window.  See `config.md` §7 for a per-window override in `rules.json`. |

`border.width` need not match between `active` and `inactive`.  When
they differ, a decorated window's frame actually grows or shrinks by the
difference every time it gains or loses focus, so its content never has
to resize; see `config.md` §2.13 for the one case this resizing does not
happen automatically.

## 2. `icon`

Appearance settings for iconified windows.

| Key            | Type    | Default | Description |
|----------------|---------|---------|-------------|
| `is-captioned` | boolean | `true`  | When `true`, the icon displays the window title below the icon graphic. |
| `show-pixmaps` | boolean | `true`  | When `true`, draws the client's `_NET_WM_ICON` image, centered in and clipped to the icon's square graphic area, above the caption (the two never overlap).  Not every application publishes this property; one that does not simply shows no icon graphic, same as when this is `false`.  Scaled to a consistent size regardless of whichever size the application published, since these vary widely from one application to another (not currently configurable from a JSON file, only at compile time).  The built image is cached per client and only rebuilt when the application actually changes its `_NET_WM_ICON` property; every other redraw (an unrelated window on the same desktop moving, an `Expose` after a virtual terminal switch, cycling selection past it) reuses the cached one instead of re-fetching and re-processing the same image again.  Forced to `false` automatically in restricted-memory mode (see `-M`), regardless of what this file says. |
| `show-hints`   | boolean | `true`  | When `true`, draws small state-hint indicators in the icon's top corners.  A filled square in the top-left when the client is sticky/pinned, and a single letter in the top-right for whichever state it was in right before being iconified (`f`: fullscreen; `m`: maximized; `h`: maximized horizontally; `v`: maximized vertically; none for plain normal). |

#### `icon.active` / `icon.inactive`

Same shape as `window.active` / `window.inactive` above (`font`,
`color.background`, `color.foreground`, `border.color`, `border.width`),
applied to the icon selected in the icon-cycle menu (`active`) versus
every other icon (`inactive`).

| Key                | Type    | Default (active) | Default (inactive) |
|--------------------|---------|------------------|--------------------|
| `font`             | string  | `"fixed bold"`   | `"fixed"`          |
| `color.background` | string  | `"#9AAEC8"`      | `"#D0D9E5"`        |
| `color.foreground` | string  | `"#253040"`      | `"#4A5566"`        |
| `border.color`     | string  | `"#4A5566"`      | `"#7F9AB6"`        |
| `border.width`     | integer | `1`              | `1`                |
| `opacity`          | integer | `100`            | `100`              |

## 3. `cycle`

Border shown around whichever window or icon is currently selected while
cycling (`Alt+Tab` and its icon-menu counterpart).  Deliberately
separate from `window.active`/`icon.active`'s border: those answer "is
this client focused", not "is this the one the cycle is pointing at
right now", and a client already focused before cycling began can
otherwise end up displayed with the exact same color as the one
currently selected, the only difference being a few pixels of width,
easy to miss at a glance.

Applied as a separate overlay drawn around the target, never as a change
to the target's border width, so cycling through selections never shifts
the target's position by however many pixels `cycle.border.width`
happens to be, regardless of what `window.active`/`inactive` (or
`icon.active`/`inactive`) themselves are configured to.

| Key                  | Type    | Default     | Description |
|----------------------|---------|-------------|-------------|
| `cycle.border.color` | string  | `"#C9A227"` | Border color as a hex color `"#RRGGBB"` or `"RRGGBB"`. |
| `cycle.border.width` | integer | `4`         | Border width in pixels. |

A theme that moves `window.active`/`inactive` away from this project's
default color family should reconsider this field too, for the same
reason a theme changing `active` without also changing `inactive` risks
leaving the two indistinguishable from one another: nothing here derives
`cycle.border.color` from the theme's other colors automatically, so
a color chosen to stand out against one theme's palette is not
guaranteed to still stand out against a different one.

```json
"cycle": {
    "border": {
        "color": "#C9A227",
        "width": 4
    }
}
```

## 4. `systray`

A `font` / `color` / `border` block, the same shape as `window.active`
above, applied to the systray dock itself, plus its height, each docked
icon's size and padding, and the placement of the clock/battery text
within it.

| Key                | Type    | Default     |
|--------------------|---------|-------------|
| `font`             | string  | `"fixed"`   |
| `color.background` | string  | `"#D0D9E5"` |
| `color.foreground` | string  | `"#4A5566"` |
| `border.color`     | string  | `"#7F9AB6"` |
| `border.width`     | integer | `1`         |
| `opacity`          | integer | `100`       |
| `height`           | integer | `22`        |
| `pixmap.size`      | integer | `24`        |
| `pixmap.padding`   | integer | `4`         |
| `text.gap`         | integer | `12`        |
| `text.valign`      | string  | `"center"`  |

`pixmap.size` is the side length, in pixels, every docked icon's embed
window is forced to regardless of whatever size it originally requested;
`pixmap.padding` is the space, in pixels, kept around and between icons.

`height` is the tray dock's height in pixels; icons and the clock and/or
battery status text (when either is enabled) are positioned within it
according to `text.valign`, and centered for icons.  It is clamped up to
at least `pixmap.size` if set any smaller, so a single icon never gets
clipped; with the default `height` of `22` actually sitting below the
default `pixmap.size` of `24`, that clamp is exactly what applies in
practice, leaving `text.valign` no visible room to work with until
`height` is raised past `pixmap.size`.

`text.gap` is the horizontal space, in pixels, between the clock and
battery text when both are shown; without it the two would run together
as if they were one string, e.g., "N/A Fri 23:39" instead of the string
"N/A   Fri 23:39".  It has no effect on the inset between the text block
as a whole and the tray's edges, which is fixed to `pixmap.padding`
above.  Which side of the icons the text sits on stays a behavior
setting rather than an appearance one, since it changes where among the
icons the text counts as being docked; only its internal spacing and
vertical alignment are theme concerns.

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

## 5. `desktop`

The desktop's default background color, used only as a fallback: see the
explanation right after the table below for exactly when it applies.

| Key                | Type   | Default     |
|--------------------|--------|-------------|
| `color.background` | string | `"#5F7187"` |

This is deliberately not the same tone as `systray.color.background` or
the other UI-chrome colors above (`"#D0D9E5"`-family): a desktop
background is a large, full-screen area rather than a small UI element,
so it wants a more neutral, less attention-grabbing tone, and a darker
one gives windows placed on top of it more contrast to stand out against
than a light background would.  It still reads as the same overall
blue-gray palette as the rest of the default theme, close to
`window.active.color.foreground`'s `"#4A5566"`, rather than an unrelated
new hue.

This value is used only when a desktop's entry in
`topology.screens.desktops` (see `config.md` §2.2) does not set its
`background-color`; a desktop that does set one always keeps it,
regardless of this.  It is also only ever used when no external tool
(`xsetbg`, `feh`, `nitrogen`, `hsetroot`, and so on) has painted the
root window with its wallpaper image, exactly the same way an explicit
per-desktop `background-color` is: icowm never overwrites an externally
set wallpaper with either one.

```json
"desktop": {
    "color": { "background": "#5F7187" }
}
```

## 6. `menu`

Applies to every context menu (root menu, per-window menu, the
all-desktops window list, and their submenus) and to the `Alt+Tab`-style
cycle menu's window chrome (its per-row entries in list mode use this
too).  The cycle menu's individual icon cells keep using `icon.active`
/ `icon.inactive` (§2) instead of this, since that already themes "the
icon currently selected while cycling" specifically; likewise, the
border drawn around the actual window or icon being previewed while
cycling uses `window.active` / `window.inactive` (§1), since that is
a highlight on the real window, not on the menu.

`unselected.border`, `selected.border`, and `label.border` each style
one row's outline; the menu window's outer frame is a separate field,
`border` (below the per-entry styles in the table), so raising or
lowering an entry's border never changes whether the window itself has
a frame, and vice versa.  The cycle menu's window shares this same
`border` for its outer frame, so a context menu and the cycle menu
always present the same outer border, regardless of whatever an entry's
border happens to be set to.

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
| `opacity`                     | integer | `100`          |
| `padding.horizontal`          | integer | `12`           |
| `padding.vertical`            | integer | `4`            |
| `show-pixmaps`                | boolean | `true`         |

`opacity` (0 to 100) is a sibling of `border` above, not of
`unselected`/`selected`/`label`: it is the whole menu window's opacity,
published through `_NET_WM_WINDOW_OPACITY`, the same way `border` is the
window's single outer frame regardless of which row is highlighted.
`_NET_WM_WINDOW_OPACITY` is a per-window property, so it cannot vary row
by row the way each row's colors can; the cycle menu's window shares
this same field, the same way it already shares `border`.  As with every
opacity field in this file, a compositing manager, e.g., picom, must
also be running and reading the property back for this to have any
visible effect at all.

`unselected` styles an entry that is neither hovered nor the
keyboard-navigated selection; `selected` styles the entry that is.
`label` styles a non-interactive heading row: it never borrows
`unselected` or `selected` even though it can look similar by default,
so it can be told apart (e.g., dimmer text, no border) without also
having to look like a normal, hoverable entry.  Any of the three styles'
`border.width` can be raised above `0` to draw an outline around that
kind of row; `unselected`/`selected` default to a subtle `1`, `label` to
`0` so heading rows stay plain.  `disabled.color.foreground` colors the
text of an entry that cannot currently be activated (e.g., "maximize" on
a client that cannot be resized); its background still comes from
`unselected` or `selected` depending on whether it happens to also be
the current selection.  `separator.color` is the line color for
a separator between groups of entries.  `border.color` and
`border.width` are the menu window's outer frame, entries aside; see the
paragraph above the table for how this differs from any entry's
`border`.

`padding.horizontal` and `padding.vertical` are the inset in pixels
between a menu window's edges and its content: row text (and, for
a submenu, its arrow indicator) for `padding.horizontal`, and the space
above the first row and below the last for `padding.vertical`.  Both
apply to every context menu and to the `Alt+Tab`-style cycle menu alike,
and equally to `unselected`, `selected`, and `label` rows.

`show-pixmaps` controls whether an entry that represents a client window
(the per-window context menu, and the cycle menu's list mode) draws that
client's `_NET_WM_ICON` image beside its label, the same
`icon.show-pixmaps` (§2) controls for iconified windows; entries that do
not represent a specific client (labels, separators, submenu headers)
are unaffected either way.

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

## 7. `dialog`

Applies to the quit-confirmation dialog and the generic message dialog.

| Key                                  | Type    | Default        |
|--------------------------------------|---------|----------------|
| `color.background`                   | string  | `"#D0D9E5"`    |
| `border.color`                       | string  | `"#7F9AB6"`    |
| `border.width`                       | integer | `2`            |
| `opacity`                            | integer | `100`          |
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

`color.background`, `border`, and `opacity` are the dialog window's
background, frame, and opacity.  `label` styles the prompt or message
text (e.g., "Are you sure you want to exit IcoWM?"); `label.padding` is
the inset between the dialog window's edges and that text.
`button.unselected` and `button.selected` style the dialog's buttons
(e.g., "Cancel" / "Exit"), the same
not-selected/keyboard-navigated-choice distinction as `menu` above; the
message dialog's single "OK" button always uses `button.selected`, since
there is nothing else it could be navigated away from.

`button.gap` is the horizontal space between adjacent buttons.
`button.padding` is the inset between a button's edges and its label,
shared by both `unselected` and `selected` so a button does not change
size (and shove its neighbor sideways) as the highlight moves onto or
off of it; each button is still sized wide and tall enough for whichever
of the two fonts is larger, and its label stays centered within that
fixed size regardless of which font ends up drawn, so switching to
a wider `selected` font (bold by default) never looks off-center.

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

## 8. `overlay`

A single `font` / `color` / `border` block, the same shape as
`window.active` (§1), applied to transient informational overlays that
are not menus or dialogs: the client-info popup and the desktop-switch
notification.  Both are single-style, non-interactive overlays with no
selected/unselected state to distinguish.

| Key                | Type    | Default     |
|--------------------|---------|-------------|
| `font`             | string  | `"fixed"`   |
| `color.background` | string  | `"#D0D9E5"` |
| `color.foreground` | string  | `"#4A5566"` |
| `border.color`     | string  | `"#7F9AB6"` |
| `border.width`     | integer | `1`         |
| `opacity`          | integer | `100`       |

```json
"overlay": {
    "font": "fixed",
    "color": { "background": "#D0D9E5", "foreground": "#4A5566" },
    "border": { "color": "#7F9AB6", "width": 1 }
}
```

## 9. `xsettings`

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
display DPI actually are; without one, those applications silently fall
back to their built-in defaults regardless of what this option is set
to.  When enabled, IcoWM acquires the `_XSETTINGS_Sn` manager selection
on the first managed screen and publishes `Net/ThemeName`,
`Net/IconThemeName`, `Gtk/CursorThemeName`, `Gtk/CursorThemeSize`, and
`Xft/DPI` (as `dpi * 1024`, per the specification) from the values
below.

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

## 10. Font format reference

> Every `font` field in a theme accepts the same string, tried through
> two backends in order:
>
> 1. **X core fonts** (accessed via XCB), IcoWM's original text
>    rendering path.  Only **X11 bitmap fonts** (BDF/PCF) are available
>    through this backend; it is tried first because it has no per-glyph
>    rasterization cost and every X server ships the `fixed` family it
>    falls back to below.
>
> 2. **TrueType/OpenType**, via fontconfig (font matching), FreeType2
>    (rasterization), and the X RENDER extension (compositing), used
>    automatically whenever a `font` string does not resolve to an
>    X core font, e.g., a family name such as `"DejaVu Sans"` that most
>    systems only have as a scalable font, not as a legacy X bitmap one.
>    This is what gives window titles, menus, and the systray
>    clock/battery text real anti-aliasing and full UTF-8 support
>    (accented characters, non-Latin scripts, and so on), neither of
>    which the X core font backend can provide.
>
> If a `font` string resolves through neither backend, IcoWM falls back
> to `"fixed"`, so text rendering is never left completely broken by
> a single bad theme value.
>
> No separate field or prefix selects which backend is used: it is
> decided purely by whether the string resolves as an X core font first.
> A short description like `"fixed bold 13"` almost always takes the
> X core font path, since `fixed` is an X bitmap family;
> a TrueType/OpenType family name takes the fontconfig path instead,
> using fontconfig's pattern syntax rather than the short description
> syntax below.
>
> #### X core font syntax
>
> The `font` field accepts two X core font formats:
>
> 1. **Short description:**
>   `"[family] [bold] [italic|oblique] [size] [registry-encoding]"`
>
>    IcoWM parses this and constructs the appropriate XLFD wildcard
>    pattern internally.  All fields after `family` are optional and can
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
> fontconfig's pattern syntax, the same one used by tools such as
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

## 11. `scratchpad`

The scratchpad's border (see `config.md` §2.11), since it is always
undecorated and so never has any other decoration to theme.  Same as
`window.active.border` by default, since the scratchpad's window is
meant to stand out the same way the active window's border already does.

| Key                       | Type    | Default     | Description |
|---------------------------|---------|-------------|-------------|
| `scratchpad.border.color` | string  | `"#4A5566"` | Border color as a hex color `"#RRGGBB"` or `"RRGGBB"`. |
| `scratchpad.border.width` | integer | `2`         | Border width in pixels; `0` disables the border entirely, the same way `window.titlebar.height` of `0` disables the titlebar. |

```json
"scratchpad": {
    "border": {
        "color": "#4A5566",
        "width": 2
    }
}
```

## 12. `search`

Theme for the fuzzy window-search widget (see `bindings.json` in
`config.md` §3.5, `search_windows`).  Its dedicated section rather than
reusing `menu` above: the two happen to share identical values by
default, but nothing ties them together, so a person can make the widget
stand out from ordinary context menus if they want to.

`input` styles the query bar itself (what is actually typed); `selected`
and `unselected` style a result row depending on whether it is the
current hovered or keyboard-navigated one.

| Key                                  | Type      | Default     | Description |
|--------------------------------------|-----------|-------------|-------------|
| `search.input.font`                  | string    | `"fixed"`   | Font for the query bar. |
| `search.input.color.background`      | string    | `"#9AAEC8"` | Query bar background. |
| `search.input.color.foreground`      | string    | `"#253040"` | Query bar text. |
| `search.unselected.font`             | string    | `"fixed"`   | Font for a result row that is neither hovered nor the keyboard-navigated selection. |
| `search.unselected.color.background` | string    | `"#D0D9E5"` | Unselected row background. |
| `search.unselected.color.foreground` | string    | `"#4A5566"` | Unselected row text. |
| `search.selected.font`               | string    | `"fixed"`   | Font for the hovered or keyboard-navigated result row. |
| `search.selected.color.background`   | string    | `"#9AAEC8"` | Selected row background. |
| `search.selected.color.foreground`   | string    | `"#253040"` | Selected row text. |
| `search.border.color`                | string    | `"#7F9AB6"` | Widget window's outer frame color. |
| `search.border.width`                | integer   | `2`         | Widget window's outer frame width in pixels. |

```json
"search": {
    "input": {
        "font": "fixed",
        "color": { "background": "#9AAEC8", "foreground": "#253040" }
    },
    "unselected": {
        "font": "fixed",
        "color": { "background": "#D0D9E5", "foreground": "#4A5566" }
    },
    "selected": {
        "font": "fixed",
        "color": { "background": "#9AAEC8", "foreground": "#253040" }
    },
    "border": { "color": "#7F9AB6", "width": 2 }
}
```

## 13. `prompt`

Theme for the built-in run-box (see `config.md` §2.12).  `label` styles
the "Run:" prompt itself; `input` styles the typed command, drawn right
next to it with its independent font and colors, so the two can be told
apart at a glance the same way `label` and `input` can be given
different backgrounds below.

| Key                             | Type    | Default        | Description |
|---------------------------------|---------|----------------|-------------|
| `prompt.label.font`             | string  | `"fixed bold"` | Font for the "Run:" prompt. |
| `prompt.label.color.background` | string  | `"#9AAEC8"`    | Prompt background. |
| `prompt.label.color.foreground` | string  | `"#253040"`    | Prompt text. |
| `prompt.input.font`             | string  | `"fixed"`      | Font for the typed command. |
| `prompt.input.color.background` | string  | `"#9AAEC8"`    | Typed-command background. |
| `prompt.input.color.foreground` | string  | `"#253040"`    | Typed-command text. |
| `prompt.border.color`           | string  | `"#7F9AB6"`    | Box's outer frame color. |
| `prompt.border.width`           | integer | `2`            | Box's outer frame width in pixels. |

```json
"prompt": {
    "label": {
        "font": "fixed bold",
        "color": { "background": "#9AAEC8", "foreground": "#253040" }
    },
    "input": {
        "font": "fixed",
        "color": { "background": "#9AAEC8", "foreground": "#253040" }
    },
    "border": { "color": "#7F9AB6", "width": 2 }
}
```

## 14. Full example: `themes/default.json`

```json
{
    "name": "Default theme",

    "desktop": {
        "color": {
            "background": "#5f7187"
        }
    },

    "cycle": {
        "border": {
            "color": "#c9a227",
            "width": 4
        }
    },

    "window": {
        "is-decorated": true,
        "active": {
            "opacity": 100,
            "border": {
                "color": "#4a5566",
                "width": 2
            },
            "color": {
                "background": "#9aaec8",
                "foreground": "#253040"
            },
            "font": "fixed bold"
        },
        "inactive": {
            "border": {
                "color": "#7f9ab6",
                "width": 2
            },
            "color": {
                "background": "#d0d9e5",
                "foreground": "#4a5566"
            },
            "font": "fixed"
        },
        "titlebar": {
            "alignment": "center",
            "buttons": {
                "color": {
                    "off": "#7086a0",
                    "on": "#253f60"
                },
                "left": [ "pin", "layer" ],
                "right": [ "close", "maximize", "shade", "iconize" ]
            },
            "height": 22,
            "padding": {
                "horizontal": 2,
                "vertical": 2
            }
        }
    },

    "icon": {
        "is-captioned": true,
        "show-hints": true,
        "show-pixmaps": true,
        "active": {
            "opacity": 100,
            "border": {
                "color": "#4a5566",
                "width": 1
            },
            "color": {
                "background": "#9aaec8",
                "foreground": "#253040"
            },
            "font": "fixed bold"

        },
        "inactive": {
            "opacity": 100,
            "border": {
                "color": "#7f9ab6",
                "width": 1
            },
            "color": {
                "background": "#d0d9e5",
                "foreground": "#4a5566"
            },
            "font": "fixed"
        }
    },

    "menu": {
        "opacity": 100,
        "show-pixmaps": true,
        "border": {
            "color": "#7f9ab6",
            "width": 2
        },
        "disabled": {
            "color": {
                "foreground": "#717b88"
            }
        },
        "label": {
            "border": {
                "color": "#7f9ab6",
                "width": 0
            },
            "color": {
                "background": "#48607f",
                "foreground": "#d0d9e5"
            },
            "font": "fixed"
        },
        "padding": {
            "horizontal": 12,
            "vertical": 4
        },
        "selected": {
            "border": {
                "color": "#4a5566",
                "width": 0
            },
            "color": {
                "background": "#9aaec8",
                "foreground": "#253040"
            },
            "font": "fixed"
        },
        "separator": {
            "color": "#7f9ab6"
        },
        "unselected": {
            "border": {
                "color": "#7f9ab6",
                "width": 0
            },
            "color": {
                "background": "#d0d9e5",
                "foreground": "#4a5566"
            },
            "font": "fixed"
        }
    },

    "dialog": {
        "opacity": 100,
        "border": {
            "color": "#7f9ab6",
            "width": 2
        },
        "button": {
            "gap": 24,
            "padding": {
                "horizontal": 12,
                "vertical": 6
            },
            "selected": {
                "border": {
                    "color": "#4a5566",
                    "width": 1
                },
                "color": {
                    "background": "#9aaec8",
                    "foreground": "#253040"
                },
                "font": "fixed bold"
            },
            "unselected": {
                "border": {
                    "color": "#7f9ab6",
                    "width": 1
                },
                "color": {
                    "background": "#d0d9e5",
                    "foreground": "#4a5566"
                },
                "font": "fixed"
            }
        },
        "color": {
            "background": "#d0d9e5"
        },
        "label": {
            "color": {
                "foreground": "#4a5566"
            },
            "font": "fixed bold",
            "padding": {
                "horizontal": 12,
                "vertical": 12
            }
        }
    },

    "overlay": {
        "opacity": 100,
        "font": "fixed",
        "border": {
            "color": "#7f9ab6",
            "width": 1
        },
        "color": {
            "background": "#d0d9e5",
            "foreground": "#4a5566"
        }
    },

    "systray": {
        "opacity": 100,
        "font": "fixed bold",
        "height": 24,
        "border": {
            "color": "#7f9ab6",
            "width": 1
        },
        "color": {
            "background": "#d0d9e5",
            "foreground": "#4a5566"
        },
        "pixmap": {
            "size": 24,
            "padding": 2
        },
        "text": {
            "gap": 12,
            "valign": "center"
        }

    },

    "search": {
        "input": {
            "font": "fixed bold",
            "color": {
                "background": "#9aaec8",
                "foreground": "#253040"
            }
        },
        "selected": {
            "font": "fixed",
            "color": {
                "background": "#9aaec8",
                "foreground": "#253040"
            }
        },
        "unselected": {
            "font": "fixed",
            "color": {
                "background": "#d0d9e5",
                "foreground": "#4a5566"
            }
        },
        "border": {
            "color": "#7f9ab6",
            "width": 2
        }
    },

    "prompt": {
        "label": {
            "font": "fixed bold",
            "color": {
                "background": "#9aaec8",
                "foreground": "#253040"
            }
        },
        "input": {
            "font": "fixed bold",
            "color": {
                "background": "#d0d9e5",
                "foreground": "#253040"
            }
        },
        "border": {
            "color": "#7f9ab6",
            "width": 2
        }
    },

    "scratchpad": {
        "border": {
            "color": "#4a5566",
            "width": 2
        }
    },

    "xsettings": {
        "dpi": 96,
        "is-enabled": false,
        "theme": {
            "gtk-theme-name": "Adwaita",
            "icon-theme-name": "Adwaita",
            "cursor-theme-name": "Adwaita",
            "cursor-theme-size": 24
        }
    }
}
```
