# icowm-msg Manual

What `icowm-msg` is, how to invoke it, how it turns its command-line
arguments into a request, and what its exit status means.  For the
socket it talks to (where it lives, the wire protocol itself, and the
full list of every command with its arguments), see
[`icowm.md`](icowm.md) §5 instead.

This file only covers what is specific to `icowm-msg` itself, not the
protocol underneath it, which is part of IcoWM proper and documented
there.

---

## Table of Contents

1. [What `icowm-msg` is](#1-what-icowm-msg-is)
2. [Usage](#2-usage)
3. [Argument value types](#3-argument-value-types)
4. [Exit status](#4-exit-status)
5. [Options](#5-options)
6. [Examples](#6-examples)
7. [Troubleshooting](#7-troubleshooting)
8. [Command reference](#8-command-reference)
9. [Watching for events](#9-watching-for-events)
   - [9.1. What each event reports](#91-what-each-event-reports)
   - [9.2. Example](#92-example)
   - [9.3. Exit status while watching](#93-exit-status-while-watching)

---

## 1. What `icowm-msg` is

`icowm-msg` is a small, standalone command-line client for IcoWM's IPC
control socket (`icowm.md` §5).  It builds one JSON request line out of
its command-line arguments, sends it to a running IcoWM's control
socket, and prints back whatever IcoWM answers with.

It is built and installed alongside IcoWM itself, as a second, entirely
separate binary (see `make help`): building or rebuilding one never
forces a rebuild of the other.  It is also deliberately independent of
the rest of the project at the source level: it links only against
`cjson`, for the JSON it reads and writes, and never touches X11, XCB,
or any font library, since a small IPC client has no reason to pull in
anything IcoWM itself needs only to actually be a window manager.

## 2. Usage

```sh
icowm-msg <command> [<key>=<value> ...]
icowm-msg -w <events> [-n <count>]
icowm-msg -K
icowm-msg -W
icowm-msg -h
icowm-msg -v
```

`<command>` is any of the command names `icowm.md` §5.3 documents
(`get_version`, `goto_desktop`, `move_client`, and so on).  Each
`<key>=<value>` becomes one field of the request object alongside
`"cmd"`, naming that command's argument.  Which keys a given command
accepts, and what each one means, is also documented there, not repeated
here.

The full response line IcoWM sends back is printed to `stdout` exactly
as received, on one line, whether the command succeeded or not.

`-w <events> [-n <count>]` is a different mode entirely, covered in full
in §9: it subscribes to events instead of sending a command.  Options
`-K` and `-W` each list one build-in reference (every known command
name, every known event name) and exit; see §5.

## 3. Argument value types

Every `<key>=<value>` argument is split on its first `=`; the value half
is sent as:

| Value                                                                                 | Sent as |
|---------------------------------------------------------------------------------------|---------|
| Exactly `true` or `false`                                                             | A JSON boolean |
| Parses in full as a number (e.g., `23068673`, `-5`, `3.5`)                            | A JSON number |
| Parses in full as a hexadecimal number, prefixed `0x` (e.g., `0x1600001`, `0xaaccff`) | A JSON number, its decimal value |
| Anything else                                                                         | A JSON string, verbatim |

This means a numeric ID never needs quoting on the command line
(`client_id=23068673`, not `client_id="23068673"`), and a value that
happens to look like a number but is meant as text (a client's new name
that is only digits, say) is sent as a number instead; none of the
commands `icowm.md` §5.3 documents currently have a string argument this
could affect, but it is worth knowing about if a future one ever does.
The `0x` form is only ever a convenience for values more naturally read
that way (a window ID copied from `list_clients`, a packed `RRGGBB`
color): it reaches IcoWM exactly as its decimal equivalent would, since
JSON itself has no separate hexadecimal number syntax to send it as.
For example, these two commands are equivalent:

```sh
icowm-msg move_client client_id=23068673 x=100 y=200
icowm-msg move_client client_id=0x1600001 x=100 y=200
```

as are these two:

```sh
icowm-msg set_desktop_background desktop_id=0 color=11193599
icowm-msg set_desktop_background desktop_id=0 color=0xaaccff
```

An argument not in `key=value` form at all (no `=` in it), or with an
empty key before the `=`, is rejected before anything is even sent, with
an explanation on `stderr` naming the offending argument.

## 4. Exit status

| Exit status | Meaning |
|-------------|---------|
| `0`         | The command reached IcoWM and it reported success (`"ok": true` in the printed response) |
| `1`         | The command reached IcoWM but it reported failure (`"ok": false`); the reason is in the printed response's `"error"` field |
| `2`         | The request never reached IcoWM at all: no socket at the resolved path (see §7), a connection failure, a response IcoWM sent back that does not itself parse as JSON, or a local argument-parsing error (missing `<command>`, a malformed `key=value`, an unrecognized option).  Nothing is printed to `stdout` in this case; the reason is on `stderr` |

A script that only cares whether the command worked can check the exit
status alone, without parsing the response at all.

## 5. Options

| Option        | What it does |
|---------------|--------------|
| `-h`          | Show usage, a few examples, and this same option list, then exit |
| `-v`          | Show `icowm-msg`'s name, IcoWM's short name and version, its license, its copyright line, and its author, then exit |
| `-K`          | List every command name this build knows about, one per line, then exit; see its note below on how this list is kept |
| `-W`          | List every event name this build knows about, one per line, then exit (the same names §9.1 documents, and the same values `-w` itself accepts, comma-separated); see its note below on how this list is kept |
| `-w <events>` | Subscribe instead of sending a command; see §9 |
| `-n <count>`  | Stop watching after this many events; only meaningful together with `-w` (see §9); rejected as an error on its own |

`-h`, `-v`, `-K`, and `-W` all exit `0`.  Any other option is rejected:
usage is printed to `stderr` and `icowm-msg` exits `2`.

`-K`'s list is a plain, hand-maintained snapshot of the server's command
table, kept here so it works offline the same way `-h` and `-v` already
do, rather than needing a running IcoWM to query.  That means it can, in
principle, drift out of sync with the server's table over time.  It is
used only to answer `-K`'s question, never to locally validate or reject
a command before sending it: an ordinary command still reaches the
server exactly as documented throughout the rest of this file,
unfiltered, so a stale `-K` listing here only makes its output
incomplete, never breaks a command the server itself would otherwise
accept.

`-W`'s list is kept the exact same hand-maintained way, for the exact
same reason, against the server's event-name table instead of its
command one; the same drift caveat applies, and the same way it is
harmless: `-w` itself always still reaches the server exactly as typed,
unfiltered, regardless of whether `-W`'s listing has fallen behind.

## 6. Examples

```sh
$ icowm-msg get_version
{"ok":true,"protocol_version":1}

$ icowm-msg list_desktops
{"ok":true,"desktops":[{"id":0,"name":"Work","stage_id":0,"current":true}]}

$ icowm-msg goto_desktop desktop_id=1
{"ok":true}

$ icowm-msg goto_desktop desktop_id=99
{"ok":false,"error":"no such desktop on that stage"}

$ icowm-msg move_client client_id=23068673 x=100 y=200
{"ok":true}
```

A shell script that only needs the exit status, ignoring the
response line entirely:

```sh
if icowm-msg iconify_all desktop_id=0 >/dev/null; then
    echo "iconified everything on desktop 0"
fi
```

A script that also wants a field out of the response can pipe the
one printed line into any JSON tool, `jq` for instance:

```sh
$ icowm-msg get_focused | jq -r '.focused[0].client_id'
23068673
```

## 7. Troubleshooting

**`icowm-msg: failed to connect to '<path>': No such file or
directory`, followed by `Is IcoWM running, with its IPC socket up?`**

There is nothing listening at the resolved socket path yet.  This means
one of:

- IcoWM is not currently running.
- IcoWM was started with `-s` (`icowm.md` §3.1), which disables the
  socket entirely for that run, on purpose.
- IcoWM is running, but its IPC socket failed to come up (see its log:
  `ipc_init` logs a warning and continues without the socket rather than
  refusing to start over this alone; every other part of IcoWM keeps
  working normally either way).
- `icowm-msg` resolved a different path than the one IcoWM's instance
  actually bound, most often because `$XDG_RUNTIME_DIR` is set to
  something different in the shell running `icowm-msg` than it was in
  the session IcoWM itself started under (a remote shell, a different
  user, or a terminal from before `$XDG_RUNTIME_DIR` was changed, for
  instance).  `icowm-msg` resolves the socket path the same way IcoWM
  itself does (`icowm.md` §5.1): under `$XDG_RUNTIME_DIR/icowm/`, or
  `/tmp/icowm-<uid>/` when that variable is unset, the fallback having
  no `icowm/` of its own because the directory is already named after
  the program.  The two need to agree on that variable to find the same
  socket.

**A response is printed, but the exit status is always `2`**

This does not happen for a well-formed response: any response
`icowm-msg` successfully reads and prints already parses as JSON, by
construction.  If it were ever seen, it would mean IcoWM sent back
something that could not be parsed as JSON at all (not: `"ok": false`,
which parses fine and exits `1`), and would be worth reporting as an
IcoWM bug rather than an `icowm-msg` one.

**`icowm-msg: argument 'foo' is not in 'key=value' form`**

An argument after `<command>` had no `=` in it at all.  Every argument
past the command name must be `key=value`; see §3.

## 8. Command reference

Every command IcoWM currently understands, with its arguments (an
argument in `[brackets]` is optional) and a one-line summary.  This is
a compact index only; the full explanation of each, including what every
response field means, is in [`icowm.md`](icowm.md) §5.3.

| Command                        | Arguments                                | Description |
|--------------------------------|------------------------------------------|-------------|
| `get_version`                  | *none*                                   | Reports the wire protocol version |
| `list_desktops`                | *none*                                   | Lists every desktop on every managed stage |
| `list_clients`                 | *none*                                   | Lists every managed client, with geometry and state flags |
| `get_focused`                  | *none*                                   | Reports the active client of every managed stage |
| `close_client`                 | `client_id`                              | Closes the client politely, or destroys its window |
| `kill_client`                  | `client_id`                              | Forcibly terminates the client's X connection |
| `focus_client`                 | `client_id`                              | Moves input focus to the client (does not raise it) |
| `unfocus_client`               | `client_id`                              | Takes input focus away from the client |
| `iconify_client`               | `client_id`                              | Iconifies (minimizes) the client |
| `deiconify_client`             | `client_id`                              | Restores the client if it was iconified |
| `hide_client`                  | `client_id`                              | Hides the client without iconifying it |
| `unhide_client`                | `client_id`                              | Undoes `hide_client` |
| `pin_client`                   | `client_id`                              | Makes the client visible on every desktop |
| `unpin_client`                 | `client_id`                              | Undoes `pin_client` |
| `toggle_pin_client`            | `client_id`                              | Toggles `pin_client`/`unpin_client` |
| `urge_client`                  | `client_id`                              | Marks the client urgent |
| `unurge_client`                | `client_id`                              | Undoes `urge_client` |
| `center_client`                | `client_id`                              | Centers the client on its screen |
| `move_client_to_monitor_north` | `client_id`                              | Moves the client to the monitor north of its current one, resolved by real physical position; a no-op with one monitor or none, or when none lies to the north |
| `move_client_to_monitor_south` | `client_id`                              | The same, toward the monitor south of the current one |
| `move_client_to_monitor_east`  | `client_id`                              | The same, toward the monitor east of the current one |
| `move_client_to_monitor_west`  | `client_id`                              | The same, toward the monitor west of the current one |
| `maximize_client_horz`         | `client_id`                              | Maximizes the client horizontally only |
| `maximize_client_vert`         | `client_id`                              | Maximizes the client vertically only |
| `maximize_client`              | `client_id`                              | Maximizes the client both horizontally and vertically |
| `raise_client`                 | `client_id`                              | Raises the client within its layer |
| `lower_client`                 | `client_id`                              | Lowers the client within its layer |
| `set_layer_above_client`       | `client_id`                              | Moves the client to the "always on top" layer |
| `set_layer_normal_client`      | `client_id`                              | Moves the client back to the ordinary layer |
| `set_layer_below_client`       | `client_id`                              | Moves the client to the "always below" layer |
| `cycle_layer_client`           | `client_id`                              | Cycles the client through above, normal, and below |
| `shade_client`                 | `client_id`                              | Rolls the client up into just its titlebar |
| `unshade_client`               | `client_id`                              | Undoes `shade_client` |
| `toggle_shade_client`          | `client_id`                              | Toggles `shade_client`/`unshade_client` |
| `fullscreen_client`            | `client_id`                              | Makes the client fill its screen |
| `unfullscreen_client`          | `client_id`                              | Undoes `fullscreen_client` |
| `toggle_fullscreen_client`     | `client_id`                              | Toggles `fullscreen_client`/`unfullscreen_client` |
| `toggle_decorate_client`       | `client_id`                              | Shows or hides the client's titlebar and border |
| `send_client_to_front`         | `client_id`                              | Raises the client to the front of its desktop's window stack |
| `send_client_to_back`          | `client_id`                              | Sends the client to the back of its desktop's window stack |
| `move_client`                  | `client_id` `x` `y`                      | Moves the client so its top-left corner is at that position |
| `move_resize_client`           | `client_id` `x` `y` `w` `h`              | Moves and resizes the client in one step |
| `resize_client`                | `client_id` `w` `h`                      | Resizes the client only, leaving position alone |
| `move_client_to_monitor`       | `client_id` `monitor_index`              | Moves the client to that physical monitor by its index, rather than by compass direction |
| `rename_client`                | `client_id` `name`                       | Overrides the client's window title |
| `reclass_client`               | `client_id` `class_name` `instance_name` | Overrides the client's `WM_CLASS` |
| `rerole_client`                | `client_id` `role`                       | Overrides the client's window role |
| `set_client_icon`              | `client_id` `icon_name`                  | Overrides which icon IcoWM shows for the client |
| `set_desktop_background`       | `desktop_id` [`stage_id`] `color`        | Sets that desktop's solid background color |
| `show_desktop`                 | `desktop_id` [`stage_id`] `show`         | Shows or hides every client on that desktop at once |
| `send_client_to_desktop`       | `client_id` `target_desktop_id`          | Moves the client to another desktop on the same stage |
| `iconify_all`                  | [`desktop_id`] [`stage_id`]              | Iconifies every client on that desktop at once |
| `deiconify_all`                | [`desktop_id`] [`stage_id`]              | Restores every iconified client on that desktop at once |
| `rearrange_desktop`            | [`desktop_id`] [`stage_id`]              | Re-applies the configured placement policy on that desktop |
| `goto_desktop`                 | `desktop_id` [`stage_id`]                | Switches the resolved stage to that desktop |
| `goto_north_desktop`           | [`stage_id`]                             | Switches the resolved stage to the desktop north of its current one |
| `goto_south_desktop`           | [`stage_id`]                             | The same, toward the desktop south of the current one |
| `goto_east_desktop`            | [`stage_id`]                             | The same, toward the desktop east of the current one |
| `goto_west_desktop`            | [`stage_id`]                             | The same, toward the desktop west of the current one |
| `add_desktop`                  | [`stage_id`]                             | Adds a new desktop after the resolved stage's last one, growing its configured grid layout by a row or column first if it does not already have a gap cell for it.  Refused, with an error, once the hardcoded number of max desktops allowed is already reached, or under restricted-memory mode (`-M`), which is always locked to a single desktop |
| `remove_desktop`               | [`stage_id`]                             | Removes the resolved stage's last desktop, moving any client still on it to the one before it, switching the current view there too if it was the one removed.  Shrinks the grid layout back down if that was its last member.  Refused, with an error, while only one desktop remains |
| `toggle_scratchpad`            | [`desktop_id`] [`stage_id`]              | Launches the scratchpad, or shows/hides it if already running |
| `toggle_strutless_maximize`    | [`stage_id`]                             | Toggles whether panel and tray struts are set aside when computing that stage's work areas |
| `reload_config`                | *none*                                   | Reloads every configuration file |
| `restart_wm`                   | *none*                                   | Requests that IcoWM stop and restart itself in place, keeping every managed client open |
| `exit_wm`                      | *none*                                   | Requests that IcoWM stop and exit |

## 9. Watching for events

`icowm-msg -w <events>` switches out of the normal one-request,
one-response flow entirely: instead of sending a command, it subscribes
to one or more events and prints one JSON line per event as IcoWM
reports them, for as long as the connection stays open.  `<command>` and
any `key=value` arguments are not used in this mode at all.

```sh
icowm-msg -w <events> [-n <count>]
```

`<events>` is a comma-separated list of event names (no spaces), e.g.,
`window_mapped,desktop_switched`.  `-n <count>` stops watching after
that many events have arrived, printing them and then exiting `0`; left
out, `icowm-msg` watches forever, until the connection drops or the
process is killed.  `-n` on its own, without `-w`, is rejected as an
error, since it has nothing to count events for.

### 9.1. What each event reports

| Event                        | Fields |
|------------------------------|--------|
| `window_mapped`              | `client_id`, `desktop_id`, `stage_id`: a client was just mapped onto that desktop |
| `window_closed`              | `client_id`, `desktop_id`, `stage_id`: a client was just destroyed |
| `desktop_switched`           | `stage_id`, `desktop_id`: that stage's current desktop just changed to `desktop_id` |
| `focus_changed`              | `stage_id`, `client_id`: that client just became the active one on its stage |
| `urgency_set`                | `client_id`, `desktop_id`, `stage_id`: that client's urgency hint was just set |
| `urgency_cleared`            | `client_id`, `desktop_id`, `stage_id`: that client's urgency hint was just cleared |
| `window_moved`               | `client_id`, `desktop_id`, `stage_id`: that client's position just changed (see the `list_clients` command for its current `x`/`y`) |
| `window_resized`             | `client_id`, `desktop_id`, `stage_id`: that client's size just changed (see the `list_clients` command for its current `w`/`h`) |
| `rule_applied`               | `client_id`, `desktop_id`, `stage_id`: a loaded rule just changed one or more of that client's properties |
| `pin_set`                    | `client_id`, `desktop_id`, `stage_id`: that client was just pinned (visible on every desktop) |
| `pin_cleared`                | `client_id`, `desktop_id`, `stage_id`: that client was just unpinned |
| `fullscreen_set`             | `client_id`, `desktop_id`, `stage_id`: that client just entered full screen |
| `fullscreen_cleared`         | `client_id`, `desktop_id`, `stage_id`: that client just left full screen |
| `shade_set`                  | `client_id`, `desktop_id`, `stage_id`: that client was just shaded (rolled up into its titlebar) |
| `shade_cleared`              | `client_id`, `desktop_id`, `stage_id`: that client was just unshaded |
| `hide_set`                   | `client_id`, `desktop_id`, `stage_id`: that client was just hidden |
| `hide_cleared`               | `client_id`, `desktop_id`, `stage_id`: that client was just unhidden |
| `decoration_set`             | `client_id`, `desktop_id`, `stage_id`: that client's titlebar and border were just shown |
| `decoration_cleared`         | `client_id`, `desktop_id`, `stage_id`: that client's titlebar and border were just hidden |
| `client_iconified`           | `client_id`, `desktop_id`, `stage_id`: that client was just iconified |
| `client_deiconified`         | `client_id`, `desktop_id`, `stage_id`: that client was just restored from being iconified |
| `layer_changed`              | `client_id`, `desktop_id`, `stage_id`: that client's stacking layer just changed (see `list_clients` for its current layer) |
| `client_desktop_changed`     | `client_id`, `desktop_id`, `stage_id`: that client just moved to a different desktop (`desktop_id` is the new one) |
| `client_renamed`             | `client_id`, `desktop_id`, `stage_id`, `name`: that client's displayed title was just overridden |
| `client_reclassed`           | `client_id`, `desktop_id`, `stage_id`, `class_name`, `instance_name`: that client's `WM_CLASS` was just overridden |
| `client_reroled`             | `client_id`, `desktop_id`, `stage_id`, `role`: that client's window role was just overridden |
| `client_icon_changed`        | `client_id`, `desktop_id`, `stage_id`, `icon_name`: that client's displayed icon was just overridden |
| `desktop_background_changed` | `desktop_id`, `stage_id`: that desktop's solid background color was just set |
| `desktop_shown`              | `desktop_id`, `stage_id`: every client on that desktop was just shown at once |
| `desktop_hidden`             | `desktop_id`, `stage_id`: every client on that desktop was just hidden at once |
| `stacking_changed`           | `client_id`, `desktop_id`, `stage_id`: that client's position within its layer's stacking order just changed |
| `config_reloaded`            | *none*: every configuration file was just reloaded |

Every event line also carries its `"event"` field naming which one it
is, the same as every other field name above; there is no separate
envelope to unwrap.

### 9.2. Example

```sh
$ icowm-msg -w window_mapped,desktop_switched -n 2
{"ok":true}
{"client_id":23068673,"desktop_id":0,"stage_id":0,"event":"window_mapped"}
{"stage_id":0,"desktop_id":1,"event":"desktop_switched"}
```

The very first line is always the `subscribe` request's response
(`{"ok":true}`, or, on failure, `{"ok":false,"error":...}`, followed by
`icowm-msg` exiting `1` without watching anything at all); every line
after that is one event.

### 9.3. Exit status while watching

The same three-way split as §4 applies, adapted to what "the command"
means in this mode:

| Exit status | Meaning |
|-------------|---------|
| `0`         | `-n <count>` was given and that many events were printed |
| `1`         | The `subscribe` request itself was rejected (`"ok": false`); the reason is in that first printed line |
| `2`         | The connection could never be made, the `subscribe` request could not be sent, or the connection was lost while still watching (a server restart, a crash, `icowm` exiting) |

That last `2` case is worth calling out on its own.  Unlike the ordinary
request/response mode, a watch that has already printed real events can
still end in failure, if the connection drops before `-n` is reached
(or, with no `-n` at all, at any point, since nothing but the connection
itself ever ends it).  `icowm-msg` reports this on `stderr` before
exiting, the same way it reports any other connection failure:

```sh
$ icowm-msg -w window_mapped
{"ok":true}
{"client_id":23068673,"desktop_id":0,"stage_id":0,"event":"window_mapped"}
icowm-msg: connection closed with no response
$ echo $?
2
```

A script that wants to tell "watched successfully to completion" apart
from "the connection dropped partway through" checks the exit status,
not just whether any event lines were printed at all.
