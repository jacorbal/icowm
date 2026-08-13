IcoWM
=====

**IcoWM** is a minimalist stacking window manager for the X Window
System that was meticulously crafted with the inclusion of desktop icons
for inactive windows, placing a significant emphasis on the utilization
of iconification (iconization) in a manner reminiscent of traditional
TWM aesthetics, but with a modern touch.

Basic features are:

  - **Support for multiple monitors.**
    Integration and management of multiple display devices, permitting
    the desktop environment to be extended gracefully across whichever
    number of screens the underlying hardware happens to provide.

  - **XRandR support.**
    Dynamic screen configuration by way of the XRandR extension.
    Output profiles, namely resolution, position, rotation, and the
    primary designation, are defined in a dedicated `randr.json` file, and
    XRandR's own screen-change and output-change notifications are
    attended to within the main event loop, such that surface
    geometry, work areas, and managed clients are all reflowed
    automatically, without any need of a restart.

  - **Configurable keyboard and mouse controls.**
    Full latitude to tailor keyboard shortcuts and mouse actions alike,
    as defined within `bindings.json`.

  - **Virtual desktops.**
    The organization of open applications into discrete workspaces, so
    as to minimize visual clutter; each screen may be furnished with
    its own independent set of virtual desktops.

  - **Configurable focus policies.**
    Both click-to-focus (`"click"`) and follow-mouse (`"sloppy"`)
    policies are supported, and may be selected through the
    configuration file as the occasion demands.

  - **Window placement policies.**
    Four strategies govern the placement of newly mapped windows:
    *smart* (minimum-overlap), *cascade*, *centered*, and
    *under-mouse*.  Transient and dialog windows are, as a matter of
    course, centered upon their parent.

  - **Window edge snapping.**
    Windows snap, during interactive movement, to the edges of the
    screen and to the borders of other windows alike, governed by a
    configurable snap threshold.

  - **Fullscreen support.**
    Windows may be toggled into and out of fullscreen mode at will,
    with the corresponding `_NET_WM_STATE_FULLSCREEN` EWMH state
    properly advertised and maintained throughout.

  - **Theming.**
    The visual appearance of the environment, namely colors, border
    widths, and font settings for windows and icons alike, is governed
    by a JSON theme file, such that the creation and sharing of custom
    themes is a straightforward affair.

  - **Extended Window Manager Hints (EWMH) compliance.**
    Compatibility with panels, taskbars, and pagers is furnished by way
    of `_NET_SUPPORTED`, `_NET_ACTIVE_WINDOW`, `_NET_WM_DESKTOP`,
    `_NET_WM_STATE`, `_NET_WM_STRUT_PARTIAL`, `_NET_WM_PING`, and their
    kindred atoms.

  - **Inter-Client Communication Conventions Manual (ICCCM) compliance.**
    Due respect is paid to `WM_DELETE_WINDOW`, `WM_TAKE_FOCUS`,
    `WM_TRANSIENT_FOR`, `WM_NORMAL_HINTS`, `WM_HINTS`, and
    `WM_PROTOCOLS`, so as to ensure correct behaviour with both modern
    and legacy X11 applications alike.

  - **Panel and dock awareness.**
    `_NET_WM_STRUT_PARTIAL` reservations are read from docks and
    panels, such that window placement and icon layout each respect
    the work area actually available.

  - **Window cycling.**
    An interactive window-cycle menu, not unlike the familiar Alt+Tab,
    permits swift, keyboard-driven navigation amongst open clients and
    iconified windows alike.

  - **Fuzzy window search.**
    A dedicated search widget filters open windows by their own title
    as the query is typed, fuzzy-matched and ranked by how well each
    one fits, letting a single window among many be reached by a few
    keystrokes rather than by cycling through them one at a time.

  - **System tray.**
    A built-in systray, conforming to the freedesktop.org System Tray
    protocol, offers a docking place for application icons, together
    with an optional clock and an optional battery indicator (drawing
    upon either an ACPI or an APM backend) of its own.

  - **XSettings propagation.**
    A built-in XSettings manager publishes the active theme's font and
    styling preferences to XSettings-aware GTK and Qt applications, so
    that such applications may render in a manner consonant with the
    rest of the desktop, without further intervention on the part of
    the user.

  - **Startup notification.**
    The freedesktop.org Startup Notification protocol is observed for
    launched applications, such that a busy cursor, or whatever
    other indication a compliant application cares to offer, may
    accompany the interval between a program's invocation and the
    appearance of its first window.

  - **Dynamic configuration management.**
    Configuration files are read upon initialization, and may
    thereafter be reloaded in response to a `SIGHUP` signal, without
    any need to restart the window manager itself.

  - **IPC control socket.**
    A local Unix domain socket, speaking plain newline-delimited
    JSON, permits an external script to query IcoWM's own state or
    drive it directly, reaching the self-same catalog of actions
    already available to the keyboard and mouse.  `icowm-msg`, a
    small command-line client built and installed alongside IcoWM
    itself, is furnished for exactly this purpose.

  - **Per-window rules.**
    Optional rules may match a window by its instance, class, role,
    title, type, or transient status, and thereupon apply a desktop,
    layer, focus behaviour, or initial geometry, with a deterministic
    precedence whereby the last matching rule prevails.  Rules may be
    confined to the moment a window is mapped, or may also apply upon
    subsequent property changes.

  - **Session hooks.**
    Optional asynchronous hooks, namely `on-start`, `on-reload`, and
    `on-exit`, may launch external processes, such as a panel, a
    compositor, or sundry daemons, with the process identifier and
    termination status of each duly logged.

  - **Internationalization.**
    Every dialog message, button, and menu label is translatable via
    `gettext`, so IcoWM's own user-facing text may render in the
    user's own locale; the command line and diagnostic log messages
    are deliberately left untranslated, as these serve a diagnostic,
    not an end-user, audience.

  - **Iconifying (traditional).**
    Rather than modern minimization to a crowded taskbar, the window is
    iconified upon the desktop in the traditional manner of TWM.  Icon
    placement follows a configurable policy: a top or bottom row, a
    left or right column, or the smart choice of the first free slot.

  - **Scratchpad.**
    A single dedicated client, any graphical program, launched on
    demand and toggled visible or hidden instead of iconified or
    restored, in the manner of a dropdown terminal.  Hiding it never
    terminates the underlying process, so the same client, with
    whatever state it was left in, is shown again next time.

  - **Restricted-memory mode.**
    A dedicated run-time mode, invoked with `icowm -M <mib>`, tailors
    IcoWM to genuinely memory-constrained systems: an aging machine, a
    low-power single-board computer, or a virtual machine allotted but
    a modest share of memory.  A companion compile-time option,
    `make COMPACT=1`, may be combined with it for a build sized for
    such a target from the very ground up.  See `doc/icowm.md` for
    the whole of it.

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
traditional iconification not as a vestigial convenience, but as
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

Full documentation resides in [`doc/icowm.md`](doc/icowm.md),
covering what IcoWM is, how it is started, every command-line option it
accepts, and the IPC control socket in full; in
[`doc/icowm-msg.md`](doc/icowm-msg.md), which describes `icowm-msg`,
the command-line client for that socket, in its own entirety; and in
[`doc/config.md`](doc/config.md), which describes
every configuration file, in its entirety, field by field.

Dependencies
------------

The construction of IcoWM requires, at the outset, a C99 compiler,
GNU Make, and `pkg-config` (this last being employed to locate the
libraries enumerated below; should it be wanting, the build reverts,
without complaint, to a fixed link line naming that self-same set of
libraries).

  - **libxcb**, together with the following extension libraries:
    `xcb-keysyms`, `xcb-util`, `xcb-icccm`, `xcb-ewmh`, `xcb-randr`,
    `xcb-sync`, `xcb-cursor`, `xcb-render`, and `xcb-renderutil`.

  - **FreeType2** and **fontconfig**, called upon for TrueType and
    OpenType text rendering whensoever a theme's font fails to resolve
    to an X core font (as, for instance, `"DejaVu Sans Mono:size=8"`,
    in contradistinction to an X bitmap font description such as
    `"fixed bold 13"`; the font format note at the close of section 4
    in `doc/config.md` sets out the full syntax of both).

  - **cJSON**, by whose offices every JSON configuration file is read.

  - **pthread**, required for the event queue's own internal mutex;
    this is furnished as part of the C library itself on any POSIX
    system, and no separate package need be sought.

Upon Debian and Ubuntu, the following installs the whole of the above:

```sh
sudo apt install build-essential pkg-config \
    libxcb1-dev libxcb-keysyms1-dev libxcb-util-dev \
    libxcb-icccm4-dev libxcb-ewmh-dev libxcb-randr0-dev \
    libxcb-sync-dev libxcb-cursor-dev libxcb-render0-dev \
    libxcb-render-util0-dev libfreetype-dev libfontconfig-dev \
    libcjson-dev
```

Other distributions furnish equivalent packages, ordinarily under
similarly-named `xcb-util-*`/`xcb-util-*-devel` or `libxcb-*-dev`
packages, together with a `cjson`/`libcjson` development package;
one's own distribution's package search ought to be consulted for
the precise names in use.

License
-------

This software is offered under the auspices of the 'ISC License'.
The [`LICENSE`](LICENSE) file within this repository may be consulted
directly, or further information gathered from [ISC Open Source
Software Licenses](https://www.isc.org/licenses/).

Copyright (c) 2026, J. A. Corbal.

Contact information
-------------------

Correspondence, and further particulars, may be found at the
following:

  - GitHub repository: <https://github.com/jacorbal/icowm/>
  - Web page: <https://jacorbal.org/icowm/>
  - E-mail: <jacorbal@gmail.com>
