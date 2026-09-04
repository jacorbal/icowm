News for IcoWM
==============

What each release brought, newest first.


`icowm_1.0.1` ("'ovelya")
-------------------------

### September 23, 2026

- **RELEASED.**  From `1.0.1-rc.3` to stable (codename: "'ovelya") as
  the Autumnal Equinox raises its head above the horizon at precisely
  00:05:38 UTC
- **DOCUMENTATION.**  Wiki page on GitHub:
  <https://github.com/jacorbal/icowm/wiki>


`icowm_1.0.1-rc.3`
------------------

### September 16, 2026

- **TESTING.**  `1.0.1-rc.3` out for a final round of field testing
  before promotion to stable

### September 15, 2026

- **PROMOTED.**  From `1.0.1-rc.2` to `1.0.1-rc.3`


`icowm_1.0.1-rc.2`
------------------

### September 13, 2026

- **FIXED.**  The example `gmrun` launcher rule in the docs, which
  matched on the window title and so never actually fired; it now
  matches on the instance name instead

### September 12, 2026

- **FIXED.**  The same flicker once more in the position/size label
  shown while dragging or resizing a window, or dragging an icon
- **FIXED.**  The same flicker in the system tray's clock/battery text
  and in iconified windows' thumbnails and captions, most noticeable
  while dragging an icon over the tray or during an urgent window's
  attention blink
- **FIXED.**  Titlebars flickering blank for an instant on repaint,
  whether from a focus change, an urgency blink, a resize, or a plain
  `Expose`

### September 11, 2026

- **TESTING.**  `1.0.1-rc.2` out for a second round of field testing

### September 10, 2026

- **PROMOTED.**  From `1.0.1-rc.1` to `1.0.1-rc.2`


`icowm_1.0.1-rc.1`
------------------

### September 9, 2026

- **FIXED.**  A launched application appearing on whichever desktop the
  user had since switched to, instead of the desktop it was actually
  launched from

### September 8, 2026

- **FIXED.**  The Alt-Tab cycle menu getting stuck open and unresponsive
  to further input when sloppy focus was enabled
- **FIXED.**  A crash when a client listed in the open Alt-Tab cycle
  menu was closed or destroyed elsewhere while the menu was still open
- **FIXED.**  The focused window losing and instantly regaining focus,
  repainting its decoration for no reason, when the pointer crossed
  between its titlebar and its own content under sloppy focus

### September 7, 2026

- **TESTING.**  `1.0.1-rc.1` out for field testing before promotion

### September 6, 2026

- **FROZEN.**  From `1.0.1-beta.6` to `1.0.1-rc.1`


`icowm_1.0.1-beta.6`
--------------------

### September 4, 2026

- **FIXED.**  `icowm-msg` looking one directory too deep for the control
  socket when `XDG_RUNTIME_DIR` was set, since it and the window manager
  each kept their own copy of that path template and the two had quietly
  drifted apart

### September 3, 2026

- **CHANGED.**  `windows.focus.delay-ms` now defaults to 250 ms instead
  of 0, so sloppy focus no longer follows the pointer the instant it
  enters a window; set it back to 0 for the old behavior

### September 2, 2026

- **ADDED.**  `rules.json` can now set a window's initial state,
  iconified, fullscreen, maximized, shaded, or hidden, before it is ever
  shown

### September 1, 2026

- **ADDED.**  `windows.focus.delay-ms`, a hover delay before sloppy
  focus takes a window
- **ADDED.**  `active` and `index` monitor targets for window placement,
  and four corner positions for the root and window context menus

### August 31, 2026

- **ADDED.**  Run dialog cursor and navigation
- **FIXED.**  Land a menu click on the row the pointer is over

### August 30, 2026

- **FIXED.**  Raise a transient family by its top-level window, so
  a dialog cannot stay buried under what covers its parent
- **ADDED.**  Window inspector
- **ADDED.**  Manual window placement policy
- **ADDED.**  Send a window to the back with the middle button
- **FIXED.**  Refuse maximize and fullscreen to a modal dialog
- **FIXED.**  Stop root menu outliving its entries
- **IMPROVED.**  Smart window placement policy
- **IMPROVED.**  Cascade window placement policy

### August 29, 2026

- **IMPROVED.**  Workareas are computed only when the strut changed
- **IMPROVED.**  System tray remembers what it was last stacked

### August 21, 2026

- **PROMOTED.**  From `1.0.1-beta.5` to `1.0.1-beta.6`
