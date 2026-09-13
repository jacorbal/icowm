EWMH & ICCCM Compliance Document
=================================

Listed below are all the NetWM/EWMH hints decided upon on
freedesktop.org, and the ICCCM identifiers a window manager is expected
to support, alongside IcoWM's current level of compliance with each
spec.  Beside each hint is the version of the spec which IcoWM is
compliant up to for the hint.

Compliance:

  - [`-`] = none,
  - [`/`] = partial,
  - [`+`] = complete,
  - [`*`] = compliant, but something else needs checking
  - [`?`] = unknown

Verified directly against the source (`src/wm/ewmh.c`'s `_NET_SUPPORTED`
array, and every handler that reads or writes each property) rather than
assumed from either spec alone.

EWMH Compliance
---------------

### Root Window Properties

- [`+`] `_NET_SUPPORTED` (1.3)
- [`+`] `_NET_CLIENT_LIST` (1.3)
- [`+`] `_NET_CLIENT_LIST_STACKING` (1.3)
- [`+`] `_NET_NUMBER_OF_DESKTOPS` (1.3)
- [`+`] `_NET_DESKTOP_GEOMETRY` (1.3)

    Reports the screen size multiplied by the configured viewport's
    columns and rows (1x1 when none is configured, matching the screen
    size exactly only then), not a fixed size IcoWM cannot
    exceed.  A pager's request to change it is still ignored, which the
    specification allows a window manager whose desktop size is set by
    its own configuration, not by a pager, to give.

- [`+`] `_NET_DESKTOP_VIEWPORT` (1.3)

    One pair per desktop, each the pixel origin that desktop's viewport
    is currently panned to, non-zero once it has actually panned away
    from its top-left page. A pager's request to move it is honored,
    (@a scmd_surface_viewport_set, @c cmds/surface.c), translating every
    non-sticky client on the current desktop by the resulting delta and
    clamping the requested origin to the pannable area.

- [`+`] `_NET_CURRENT_DESKTOP` (1.3)
- [`+`] `_NET_DESKTOP_NAMES` (1.3)
- [`+`] `_NET_ACTIVE_WINDOW` (1.3)

    Published as focus moves, and honored as a request using all three
    fields the message carries (§3), not some of them.

    The source indication decides whether the request is weighed at all:
    one declaring itself to come from a pager, a taskbar or
    a notification is the person acting through it, and is honored
    outright, switching desktop, de-iconifying and focusing as needed.

    A request from an application is weighed.  The timestamp used is the
    one in the message, the requesting client's own last user activity
    when it asked, and not the target window's, which would be stale for
    any window not on screen and so would refuse every request to raise
    an iconified one.  A zero, which §3 says to ignore, falls back to
    the target's own.

    The requestor's active window is read too, since §3 notes the window
    manager may be likelier to obey when honoring the request hands
    focus from one active window to another.  An asker naming a window
    that really does hold focus here is already what the person is
    working in, and is honored without further weighing.  Named windows
    are checked rather than trusted, or any client could name the
    focused one to get through.

    A request that fails all of that is refused with
    `_NET_WM_STATE_DEMANDS_ATTENTION`, as §5 provides for
    (`src/handler/message.c`).

- [`+`] `_NET_WORKAREA` (1.3)
- [`+`] `_NET_SUPPORTING_WM_CHECK` (1.3)
- [`-`] `_NET_VIRTUAL_ROOTS` (1.3)

    IcoWM does not use virtual roots, so this is not needed, and it is
    not advertised in `_NET_SUPPORTED` at all.

- [`+`] `_NET_DESKTOP_LAYOUT` (1.3)
- [`+`] `_NET_SHOWING_DESKTOP` (1.3)

### Root Window Messages

- [`+`] `_NET_CLOSE_WINDOW` (1.3)
- [`+`] `_NET_MOVERESIZE_WINDOW` (1.3)
- [`+`] `_NET_WM_MOVERESIZE` (1.3)
- [`+`] `_NET_RESTACK_WINDOW` (1.3)
- [`+`] `_NET_REQUEST_FRAME_EXTENTS` (1.3)

### Application Window Properties

- [`+`] `_NET_WM_NAME` (1.3)
- [`+`] `_NET_WM_VISIBLE_NAME` (1.3)

    Set whenever a client's title is actually truncated to fit the
    titlebar's width, and deleted the moment it no longer is, rather
    than left set to a stale or redundant value
    (`client_sync_visible_name`, `src/client.c`, called from
    `src/render/desktop.c`).

- [`+`] `_NET_WM_ICON_NAME` (1.3)
- [`+`] `_NET_WM_VISIBLE_ICON_NAME` (1.3)

    Same mechanism as `_NET_WM_VISIBLE_NAME` above, for a client's
    on-screen icon caption instead of its titlebar
    (`client_sync_visible_name`, `src/client.c`, called from
    `src/render/icon.c`).

- [`+`] `_NET_WM_DESKTOP` (1.3)
- [`+`] `_NET_WM_WINDOW_TYPE` (1.3)

    ` _DESKTOP`, ` _DOCK`, ` _TOOLBAR`, ` _MENU`, ` _UTILITY`,
    ` _SPLASH`, ` _DIALOG`, ` _NORMAL`, and ` _NOTIFICATION` are all
    recognized; `_NET_WM_WINDOW_` `TYPE_NOTIFICATION` governs whether
    input focus follows a newly mapped window the same way most other
    window managers reserve it for.

    IcoWM also sets this property on its own windows, not only reads it
    off a client's: `menu/context/ctxmenu.c` and `menu/cycle.c` mark
    themselves ` _MENU`; the three dialogs under `menu/dialog/` and
    `menu/search.c` mark themselves ` _DIALOG`; `menu/popup.c` and
    `input/mouse/drag/overlay.c` mark themselves ` _TOOLTIP`;
    `menu/notify.c` marks itself ` _NOTIFICATION`; and
    `systray/protocol.c`'s tray bar marks itself ` _DOCK`, which closes
    the gap between that type's long-standing presence in
    `_NET_SUPPORTED` and no window of IcoWM's own ever having carried
    it.  `render/outline.c`'s drag outline and `xsettings.c`'s
    selection-owner window carry no type, deliberately: the former has
    no EWMH semantic to claim, and the latter is never mapped.

- [`+`] `_NET_WM_STATE` (1.3)

    ` _MODAL`, ` _STICKY`, ` _MAXIMIZED_VERT`, ` _MAXIMIZED_HORZ`,
    ` _SHADED`, ` _SKIP_TASKBAR`, ` _SKIP_PAGER`, ` _HIDDEN`,
    ` _FULLSCREEN`, ` _ABOVE`, ` _BELOW`, ` _DEMANDS_ATTENTION`, and
    ` _FOCUSED` are all supported both ways (a client's initial state is
    honored, and a pager or other tool changing it through a client
    message is honored too).  They are held independent of one another,
    as the spec requires: a window may be maximized on one axis, on
    both, or on both while also full screen, and every combination reads
    back off the property exactly as it stands.  `_ADD`, `_REMOVE` and
    `_TOGGLE` each act on the one state named and leave the others
    alone, so removing ` _MAXIMIZED_HORZ` keeps a vertical maximization
    the client never asked to lose, and a window iconified while
    maximized comes back maximized (ICCCM §2.1.1's independent-flags
    reading).

- [`+`] `_NET_WM_ALLOWED_ACTIONS` (1.3)

    ` _MOVE`, ` _RESIZE`, ` _MINIMIZE`, ` _SHADE`, ` _STICK`,
    ` _MAXIMIZE_HORZ`, ` _MAXIMIZE_VERT`, ` _FULLSCREEN`,
    ` _CHANGE_DESKTOP`, ` _CLOSE`, ` _ABOVE`, and ` _BELOW` are all
    advertised, each only where it can actually be carried out.

    Moving, resizing and maximizing are withheld from a client whose
    `WM_NORMAL_HINTS` pins both axes, its declared minimum and maximum
    being the same size, and from one that is currently fullscreen,
    since it holds the monitor whole until that state is dropped.
    Advertising them regardless told a client it could ask for what
    would be refused.  Fullscreen itself stays advertised for
    a fixed-size client, deliberately: it overrides the client's own
    geometry rather than honoring it, and some clients check this
    property before asking (`src/cmds/client/flags.c`,
    `src/client/props.c`).

- [`+`] `_NET_WM_STRUT` (1.3)
- [`+`] `_NET_WM_STRUT_PARTIAL` (1.3)
- [`/`] `_NET_WM_ICON_GEOMETRY` (1.3)

    Written by IcoWM itself when it creates its on-screen icon for an
    iconified client (`cmds/client/visibility.c`), advertising that
    icon's position and size to any interested pager, but only ever in
    that one direction.  Never read back from a client when
    a third-party tool (a stand-alone taskbar such as `tint2`) is the
    one that set it, and IcoWM has no animation of any kind between
    a window and its iconified state to feed that value into regardless.
    Skipped entirely (neither read nor written) whenever
    `_NET_WM_HANDLED_ICONS` defers icon display to that third party in
    the first place, which is precisely when the spec's read direction
    would matter most.

- [`*`] `_NET_WM_ICON` (1.3)

    Read and rendered by `render/wmicon.c` when a client provides one
    and the active theme's 'icons.show-pixmaps' is enabled; falls back
    to a themed glyph otherwise.  Never written by IcoWM itself, since
    IcoWM is not the one supplying a client's icon.

- [`+`] `_NET_WM_PID` (1.3)

    Read to associate a managed window with its owning process (used,
    among other things, to match a scratchpad's launched process to the
    window it eventually creates).

- [`-`] `_NET_WM_HANDLED_ICONS` (1.3)

    Only ever read (`cmds/client/visibility.c` checks whether a pager
    has set this on the root window, and defers to it if so); IcoWM
    itself never sets it, since IcoWM always handles iconified windows'
    on-screen icons itself.

- [`+`] `_NET_WM_USER_TIME` (1.3)
- [`+`] `_NET_WM_USER_TIME_WINDOW` (1.3)

    Checked first, before falling back to reading `_NET_WM_USER_TIME`
    directly off the client's toplevel  Some toolkits (e.g., GTK) set
    the property on a dedicated, often-unmapped window instead,
    specifically so that every tool interested in the toplevel's other
    properties is not woken up on every keypress.

- [`+`] `_NET_FRAME_EXTENTS` (1.3)
- [`-`] `_NET_WM_OPAQUE_REGION` (1.5)

    IcoWM never draws an ARGB, alpha-channel window of its own; every
    window it creates uses the parent visual's opaque depth, which
    a compositor already treats as fully opaque with no hint needed, so
    there is no per-window region left to describe.

- [`/`] `_NET_WM_BYPASS_COMPOSITOR` (1.5)

    Set only on the 4 outline strip windows `render/outline.c` creates
    (`s_render_outline_place`), a rapidly-repainted drag indicator where
    compositing would add a visible lag.  Left off every other
    icowm-owned window (menus, dialogs, tooltips, notifications, the
    systray dock): each already carries its own `_NET_WM_WINDOW_TYPE`,
    which lets the user's own compositor configuration decide how to
    treat it; forcing a bypass there would take that choice away.

### Window Manager Protocols

- [`+`] `_NET_WM_PING` (1.3)
- [`+`] `_NET_WM_SYNC_REQUEST` (1.3)

    Conditional on the X server's SYNC extension being available
    (`wm_sync_available`); both `_NET_WM_SYNC_REQUEST` and its
    `_NET_WM_SYNC_REQUEST_COUNTER` are only advertised when it is.

- [`+`] `_NET_WM_FULLSCREEN_MONITORS` (1.3)

### Other

- [`+`] Startup Notification (libstartup-notification wire protocol)

    `_NET_STARTUP_INFO_BEGIN` / `_NET_STARTUP_INFO` client messages are
    emitted for a launched application (`cctl/sn.c`), matching the
    message-splitting behavior libstartup-notification's reference
    implementation uses.

- [`+`] System Tray Protocol Specification (0.3)

    `_NET_SYSTEM_TRAY_OPCODE`, `_NET_SYSTEM_TRAY_ORIENTATION` (fixed
    horizontal), and `_NET_SYSTEM_TRAY_VISUAL` are all implemented
    (`systray/protocol.c`); see restricted-memory mode's note below.

- [`*`] Restricted-memory mode (`memguard`)

    IcoWM's low-resource profile never acquires the systray selection at
    all (`is_embedding_enabled` fixed false), so no third-party
    application can dock an icon while it is active; the tray's clock
    and battery text are unaffected.  Not a spec compliance gap in the
    ordinary sense, since nothing here is incorrect for a client that
    does try to dock, but worth flagging for anyone auditing tray
    behavior specifically under that mode.

ICCCM Compliance
----------------

Verified the same way as the EWMH section above.  Every identifier below
was individually confirmed by reading its handling code.

### Manager Selections (ICCCM 2.8)

- [`+`] `WM_Sn` (2.0)

    Checked, per screen, before taking `SubstructureRedirect`: refuses
    to start against an already-running window manager's ownership
    unless `-r` is given, in which case the previous owner's
    relinquishment (a `DestroyNotify` on its selection-owning window,
    bounded by `WM_SN_REPLACE_TIMEOUT_MS`) is waited for before taking
    over (`wm/startup/selection.c`).  The `MANAGER` `ClientMessage`
    announcement the same section calls for is sent on every successful
    acquisition, replacing an existing owner or not.

### Client Properties (ICCCM 4.1)

- [`+`] `WM_NAME` (2.0)
- [`+`] `WM_ICON_NAME` (2.0)
- [`+`] `WM_NORMAL_HINTS` (2.0)

    `PMinSize`, `PMaxSize`, `PBaseSize`, `PResizeInc`, `PWinGravity`,
    and `PAspect` are all read and applied (`src/client/props.c`).

    `PMaxSize` binds when maximizing too, which §4.1.2.3 asks for and
    nothing in EWMH exempts maximization from: a dialog or a fixed-size
    utility stops at the size it declared rather than stretching to the
    workarea (`src/cmds/client/maximize.c`).  Full screen is the
    deliberate exception, EWMH having that state ignore the client's own
    constraints and take the monitor whole.

- [`*`] `WM_NORMAL_HINTS` `USPosition`/`PPosition` (2.0)

    Read into `client->size_hints`, but neither flag changes IcoWM's
    placement policy.  A client's requested position is not
    distinguished from one left to the window manager to decide.

- [`+`] `WM_HINTS` (2.0)

    Input model, initial state, window group, and urgency are all read
    (`src/client.c`).  All four input models of §4.1.7 are honored
    separately: `SetInputFocus` is issued only for a client whose
    `input` field is true, Passive and Locally Active, and
    `WM_TAKE_FOCUS` is sent to any client registering that protocol,
    which covers Locally and Globally Active, so a No Input client
    receives neither.  A legacy `icon_pixmap`/`icon_mask` bitmap icon is
    read as a fallback when `_NET_WM_ICON` is absent
    (`src/render/wmicon.c`).

- [`+`] `WM_CLASS` (2.0)
- [`+`] `WM_TRANSIENT_FOR` (2.0)

    Read and stored (`src/client.c`).  §4.1.2.6's special case (the root
    window, meaning transient for the client's whole group rather than
    a single specific window) is distinctly handled:
    `is_transient_for_group` is set at read time, and
    `client_group_transient_anchor` (`cmds/client/transient.c`)
    resolves, fresh each time rather than a stored pointer, whichever
    currently-mapped sibling sharing the client's group leader should
    stand in for a specific parent, wherever `transient_parent` itself
    would otherwise be read.  Initial centering
    (`policy/placement/window.c`), layer stacking
    (`cmds/client/layer.c`), raising together (`desktop/dclient.c`), and
    focus redirect to an open dialog (`cmds/client/transient.c`).

- [`+`] `WM_PROTOCOLS` (2.0)

    `WM_DELETE_WINDOW` and `WM_TAKE_FOCUS` are both recognized alongside
    `_NET_WM_PING` and `_NET_WM_SYNC_REQUEST` (`src/client.c`).

    The `WM_TAKE_FOCUS` message carries a real timestamp and never
    `CurrentTime`, which §4.1.7 forbids in as many words: the client is
    to echo that value back in its own `SetInputFocus`, and is itself
    forbidden from using `CurrentTime` there, so a Locally or Globally
    Active client sent one is left with nothing valid to answer with.
    The timestamp used is that of the most recent genuine key or button
    press, kept by `client_note_user_time` (`src/client.c`) since focus
    is also granted from places holding no event of their own, such as
    a fallback after a window closed or a desktop switch.

- [`+`] `WM_CLIENT_LEADER` (2.0)

    Read to cluster windows belonging to the same application for
    placement purposes, together with `WM_HINTS`' window group
    (`src/client.c`).

- [`/`] `WM_COLORMAP_WINDOWS` (2.0)

    Read at manage time and whenever it changes, and installed (on
    colormap focus, which follows input focus, the common policy) for
    the listed subwindows, each cached to avoid a fresh round trip on
    every focus change (`src/client/props.c`,
    `src/cmds/client/focus.c`).  A client that omits this property but
    still uses a non-default colormap on its top-level window is not
    covered: that fallback (installing the top-level's current colormap
    attribute directly) would need a round trip on every single focus
    change for a case rarer than the one already covered, so it was left
    out deliberately rather than an oversight.  Rare in practice either
    way (this matters only on 8-bit PseudoColor-class displays,
    essentially extinct).

- [`/`] `WM_CLIENT_MACHINE` (2.0)

    Read in `src/client.c` (client_init, alongside `_NET_WM_PID`) and
    compared against this host's own name so a `_NET_WM_PID` naming
    another host's process table is never trusted; when the two
    disagree, or a client sets no `WM_CLIENT_MACHINE` at all,
    `src/cmds/client/focus.c`'s kill-escalation guard skips registering
    that PID for `SIGKILL`, relying on `xcb_kill_client` alone, which is
    location-agnostic by nature.  Not read for any other purpose (a
    session manager or remote-process indicator, neither of which IcoWM
    implements).

### Window Manager Withdrawal (ICCCM 4.1.4)

- [`+`] Releasing clients on exit (2.0)

    Every managed client is reparented back to the root window, at the
    absolute position it occupied inside its frame, then mapped and
    given `WM_STATE` `Normal` before the frames are destroyed, so that
    another window manager may adopt it and so that nothing the person
    had open becomes unreachable (`src/wm.c`).  The map matters because
    IcoWM unmaps routinely, every client on a desktop that is not the
    current one and every iconified one among them; a window left
    unmapped with nobody managing it is gone, its process still running
    but no `MapRequest` ever coming for it again.  The map is issued
    strictly after the reparent, since `X` unmaps a mapped window itself
    as the first step of reparenting it, and the whole sequence is
    flushed before `xcb_disconnect`, which makes no promise about
    a request still in the buffer.

    Only for an orderly exit.  A death by fatal signal runs none of this
    and cannot be made to: almost nothing this needs is safe to call
    from a signal handler.

### Client to Window Manager Communication (ICCCM 4.2)

- [`+`] `ConfigureRequest` handling (2.0)

    Honored, including border-width and stacking changes
    (`src/handler/configure.c`).

- [`+`] Synthetic `ConfigureNotify` (2.0)

    Sent after a move/resize so a client can recompute its absolute
    screen position even when its size does not itself change
    (`src/client/geom.c`).

- [`+`] `WM_CHANGE_STATE` (2.0)

    A client requesting to be iconified through this client message is
    honored (`src/handler/message.c`).

- [`/`] Colormap installation (2.0)

    `ColormapNotify` is handled (`src/handler/colormap.c`), and
    a colormap is installed on focus change for the case
    `WM_COLORMAP_WINDOWS` above covers; see that entry for the one case
    deliberately left out.

### Window Manager Properties (ICCCM 4.3)

- [`+`] `WM_STATE` (2.0)

    Set on every managed client window (`Withdrawn`/`Normal`/ `Iconic`),
    read by pagers that scan the window tree directly instead of relying
    on EWMH alone (`src/cmds/client/visibility.c`,
    `src/cmds/client/ewmh.c`), and set back to `Normal` on every client
    as IcoWM releases them (§4.1.4 above).

- [`+`] `WM_ICON_SIZE` (2.0)

    Advertised on the root window, matching the fixed square size
    IcoWM's on-screen icons use (`src/wm/ewmh.c`).

Notes on this document
----------------------

This list reflects `src/wm/ewmh.c`'s `_NET_SUPPORTED` array (and
`wm/startup/selection.c` for `WM_Sn`, ICCCM §2.8, a manager selection
rather than a root-window property, so outside that array entirely) plus
a targeted search of every other `_NET_*` and `WM_*` identifier
referenced anywhere in the source, each one individually confirmed by
reading its handling code rather than inferred from the array alone.
