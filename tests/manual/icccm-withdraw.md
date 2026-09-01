# Verifying the ICCCM handover on exit

What is under test: every client is left **mapped**, **reparented to the
root window**, and with **`WM_STATE` set to `NormalState`** when IcoWM
exits, so that another window manager can adopt them and so that nothing
the person had open becomes unreachable.  ICCCM §4.1.4.

This needs a running X server, so it is a manual check rather than
a unit test.  A nested server keeps it away from the real session.

## Setup

    Xephyr -screen 1024x768 :9 &
    DISPLAY=:9 icowm &

## The case that used to fail

The interesting windows are the ones IcoWM unmaps in the ordinary course
of things: those on a desktop that is not the current one, and iconified
ones.  A window on the visible desktop was mapped all along and proves
nothing.

    DISPLAY=:9 xterm -title on-desktop-1 &
    # switch to desktop 2, then:
    DISPLAY=:9 xterm -title on-desktop-2 &
    # iconify this one, then switch back to desktop 1

At this point `on-desktop-2` is unmapped, and so is the iconified one.

## Check before exiting

    DISPLAY=:9 xwininfo -root -tree | grep on-desktop

Both appear, nested inside an IcoWM frame, and at least one reports
`IsUnMapped`.

## Exit and check again

    DISPLAY=:9 icowm-msg exit_wm
    DISPLAY=:9 xwininfo -root -tree | grep on-desktop

Both must now be:

- **direct children of the root window**, with no frame between;
- reported `IsViewable`, not `IsUnMapped`.

And the property, per window id from the listing above:

    DISPLAY=:9 xprop -id <id> WM_STATE

must report `window state: Normal`.

## The real proof

    DISPLAY=:9 twm &

Every window that was open before is present and usable under the new
window manager.  That is the whole point of the handover; anything still
unmapped is a window the user has lost.

## What this does not cover

A death by fatal signal runs none of this, so windows unmapped at that
moment stay unmapped.  Nothing can be done about that from inside
a signal handler, and it is not what this check is for.
