IcoWM
=====

**IcoWM** is a minimalist stacking window manager for the X Window
System that was meticulously crafted with the inclusion of desktop icons
for inactive windows, placing a significant emphasis on the utilization
of iconification (iconization) in a manner reminiscent of traditional
TWM aesthetics, but with a modern touch.

Basic features are:

  - **Support for multiple monitors.**
    Integration and management of multiple display devices to extend the
    desktop environment across various screens.

  - **XRandR support.**
    Dynamic screen configuration via the XRandR extension.  Output
    profiles (resolution, position, rotation, primary flag) are defined
    in a dedicated `randr.json` file.  XRandR extension events
    (screen-change and output-change notifications from the X server)
    are dispatched within the main event loop; the window manager
    automatically adjusts surface geometry, refreshes work areas, and
    reflows managed clients without requiring a restart.

  - **Configurable keyboard and mouse controls.**
    Full flexibility to customize input methods tailoring keyboard
    shortcuts and mouse actions defined in `bindings.json`.

  - **Virtual desktops.**
    Organization of open applications into discrete workspaces,
    minimizing visual clutter.  Each screen can have its own independent
    set of virtual desktops.

  - **Configurable focus policies.**
    Both *click-to-focus* and *follow-mouse* (sloppy focus) policies are
    supported and selectable through the configuration file.

  - **Window placement policies.**
    Four strategies for placing newly mapped windows: *smart*
    (minimum-overlap), *cascade*, *centered*, and *under-mouse*.
    Transient/dialog windows are automatically centered over their
    parent.

  - **Window edge snapping.**
    Windows snap to screen edges and to other window borders during
    interactive movement, with a configurable snap threshold.

  - **Fullscreen support.**
    Windows can be toggled into and out of fullscreen mode, with the
    corresponding `_NET_WM_STATE_FULLSCREEN` EWMH state properly
    advertised and maintained.

  - **Theming.**
    Visual appearance (colors, border widths, and font settings for
    both windows and icons) is controlled by a JSON theme file, making
    it straightforward to create and share custom themes.

  - **Extended Window Manager Hints (EWMH) compliance.**
    Provides compatibility with panels, taskbars, and pagers through
    `_NET_SUPPORTED`, `_NET_ACTIVE_WINDOW`, `_NET_WM_DESKTOP`,
    `_NET_WM_STATE`, `_NET_WM_STRUT_PARTIAL`, `_NET_WM_PING`, and
    related atoms.

  - **Inter-Client Communication Conventions Manual (ICCCM) compliance.**
    Respects `WM_DELETE_WINDOW`, `WM_TAKE_FOCUS`, `WM_TRANSIENT_FOR`,
    `WM_NORMAL_HINTS`, `WM_HINTS`, and `WM_PROTOCOLS`, ensuring correct
    behaviour with both modern and legacy X11 applications.

  - **Panel and dock awareness.**
    `_NET_WM_STRUT_PARTIAL` reservations are read from docks and panels
    so that window placement and icon layout respect the available work
    area.

  - **Window cycling.**
    An interactive window-cycle menu (similar to Alt+Tab) allows quick
    keyboard-driven navigation across open clients and iconified
    windows.

  - **Dynamic configuration management.**
    Configuration files are read upon initialization and can
    subsequently be reloaded in response to a `SIGHUP` signal, without
    restarting the window manager.

  - **Per-window rules.**
    Optional rules can match window
    instance/class/role/title/type/transient and apply desktop, layer,
    focus and initial geometry with deterministic precedence (last
    matching rule wins).  Rules can be map-only or also apply on
    property changes.

  - **Session hooks.**
    Optional asynchronous hooks (`on-start`, `on-reload`, `on-exit`) can
    launch external processes (panel/compositor/daemons) with PID and
    termination logging.

  - **Iconifying (classical).**
    Instead of classical minimization, the window is iconified on the
    desktop in TWM-style.  Icon placement follows a configurable policy
    (top/bottom row, left/right column, or smart first-free slot).

IcoWM aspires to blend a lightweight design *ethos* with usability.

The primary goal of this endeavor is to create a window manager that can
be entirely navigated through keyboard commands, whilst still
accommodating optional mouse interaction.  This dual capability fosters
an environment conducive to efficiency, especially for those users who
prefer the elegance of keyboard-driven workflows.

The icon functionality has been retained despite its diminished
prevalence in contemporary interfaces, for this feature harkens back to
an era when applications were elegantly transformed into icons, which is
a stylistic choice that has largely been overshadowed by contemporary
minimization practices towards a crowded taskbar.  Thus, IcoWM retains
classical iconification not as a vestigial convenience, but as
a principal element of its intended mode of use, thereby permitting
windows to be set aside as actual desktop icons, rather than being
reduced solely to entries within such bars.

Likewise, although IcoWM remains a stacking window manager, it seeks to
borrow something of the discipline more commonly associated with
keyboard-centred tiling environments.  Its aim is not to impose an
overly aggressive automatism upon window placement, but rather to
provide a mode of interaction that is orderly, intelligible, and
deliberate, wherein changes of focus, iconification, and navigation
between desktops are treated as essential operations.

Dependencies
------------

Building IcoWM requires a C99 compiler, GNU Make, and `pkg-config`
(used to locate the libraries below; if unavailable, the build falls
back to a fixed link line for the same set of libraries).

  - **libxcb** and the following extension libraries: `xcb-keysyms`,
    `xcb-util`, `xcb-icccm`, `xcb-ewmh`, `xcb-randr`, `xcb-sync`,
    `xcb-cursor`, `xcb-render`, and `xcb-renderutil`.

  - **FreeType2** and **fontconfig**, for TrueType/OpenType text
    rendering when a theme's font does not resolve to an X core font
    (e.g., `"DejaVu Sans Mono:size=8"` instead of an X bitmap font
    description such as `"fixed bold 13"`; see the font format note
    at the end of section 4 in [`doc/config.md`](doc/config.md) for
    the full syntax of both).

  - **cJSON**, for reading every JSON configuration file.

  - **pthread**, for the event queue's internal mutex; part of the C
    library itself on any POSIX system, with no separate package
    needed.

On Debian and Ubuntu, the following installs everything above:

```sh
sudo apt install build-essential pkg-config \
    libxcb1-dev libxcb-keysyms1-dev libxcb-util-dev \
    libxcb-icccm4-dev libxcb-ewmh-dev libxcb-randr0-dev \
    libxcb-sync-dev libxcb-cursor-dev libxcb-render0-dev \
    libxcb-render-util0-dev libfreetype-dev libfontconfig-dev \
    libcjson-dev
```

Other distributions provide equivalent packages, typically under
similarly named `xcb-util-*`/`xcb-util-*-devel` or `libxcb-*-dev`
packages and a `cjson`/`libcjson` development package; consult your
distribution's package search for the exact names.

License
-------

This software is licensed under the 'ISC License'.
Read the [`LICENSE`](LICENSE) file on this repository, or gather more
information on [ISC Open Source Software
Licenses](https://www.isc.org/licenses/).

Copyright (c) 2026, J. A. Corbal.

Contact information
-------------------

  - GitHub repository: <https://github.com/jacorbal/icowm/>
  - Web page: <https://jacorbal.org/icowm/>
  - E-mail: <jacorbal@gmail.com>
