# icowm-msg Manual

What `icowm-msg` is, how to invoke it, how it turns its own
command-line arguments into a request, and what its exit status
means.  For the socket it talks to (where it lives, the wire
protocol itself, and the full list of every command with its own
arguments), see [`icowm.md`](icowm.md) section 5 instead: this
file only covers what is specific to `icowm-msg` itself, not the
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
   - [9.1 What each event reports](#91-what-each-event-reports)
   - [9.2 Example](#92-example)
   - [9.3 Exit status while watching](#93-exit-status-while-watching)

---

## 1. What `icowm-msg` is

`icowm-msg` is a small, standalone command-line client for IcoWM's
own IPC control socket (`icowm.md` section 5). It builds one JSON
request line out of its own command-line arguments, sends it to a
running IcoWM's control socket, and prints back whatever IcoWM
answers with.

It is built and installed alongside IcoWM itself, as a second,
entirely separate binary (see `make help`): building or rebuilding
one never forces a rebuild of the other. It is also deliberately
independent of the rest of the project at the source level: it links
only against `cjson`, for the JSON it reads and writes, and never
touches X11, XCB, or any font library, since a small IPC client has
no reason to pull in anything IcoWM itself needs only to actually be
a window manager.

## 2. Usage

```sh
icowm-msg <command> [<key>=<value> ...]
icowm-msg -h
icowm-msg -v
```

`<command>` is any of the command names `icowm.md` section 5.3
documents (`get_version`, `goto_desktop`, `move_client`, and so on).
Each `<key>=<value>` becomes one field of the request object
alongside `"cmd"`, naming that command's own argument: which keys a
given command accepts, and what each one means, is also documented
there, not repeated here.

The full response line IcoWM sends back is printed to `stdout`
exactly as received, on one line, whether the command succeeded or
not.

## 3. Argument value types

Every `<key>=<value>` argument is split on its own first `=`; the
value half is sent as:

| Value                                                     | Sent as |
|-----------------------------------------------------------|---------|
| Exactly `true` or `false`                                 | A JSON boolean |
| Parses in full as a number (e.g. `23068673`, `-5`, `3.5`) | A JSON number |
| Anything else                                             | A JSON string, verbatim |

This means a numeric ID never needs quoting on the command line
(`client_id=23068673`, not `client_id="23068673"`), and a value
that happens to look like a number but is meant as text (a client's
own new name that is only digits, say) is sent as a number instead;
none of the commands `icowm.md` section 5.3 documents currently
have a string argument this could affect, but it is worth knowing
about if a future one ever does.

An argument not in `key=value` form at all (no `=` in it), or with
an empty key before the `=`, is rejected before anything is even
sent, with an explanation on `stderr` naming the offending argument.

## 4. Exit status

| Exit status | Meaning |
|-------------|---------|
| `0`         | The command reached IcoWM and it reported success (`"ok": true` in the printed response) |
| `1`         | The command reached IcoWM but it reported failure (`"ok": false`); the reason is in the printed response's own `"error"` field |
| `2`         | The request never reached IcoWM at all: no socket at the resolved path (see section 7), a connection failure, a response IcoWM sent back that does not itself parse as JSON, or a local argument-parsing error (missing `<command>`, a malformed `key=value`, an unrecognized option). Nothing is printed to `stdout` in this case; the reason is on `stderr` |

A script that only cares whether the command worked can check the
exit status alone, without parsing the response at all.

## 5. Options

| Option        | What it does |
|---------------|--------------|
| `-h`          | Show usage, a few examples, and this same option list, then exit |
| `-v`          | Show `icowm-msg`'s own name, IcoWM's own short name and version, its license, its copyright line, and its author, then exit |
| `-K`          | List every command name this build knows about, one per line, then exit; see its own note below on how this list is kept |
| `-w <events>` | Subscribe instead of sending a command; see section 9 |
| `-n <count>`  | Stop watching after this many events; only meaningful together with `-w` (see section 9); rejected as an error on its own |

`-h`, `-v`, and `-K` all exit `0`. Any other option is rejected: usage
is printed to `stderr` and `icowm-msg` exits `2`.

`-K`'s own list is a plain, hand-maintained snapshot of the server's
own command table, kept here so it works offline the same way `-h`
and `-v` already do, rather than needing a running IcoWM to query.
That means it can, in principle, drift out of sync with the server's
own table over time. It is used only to answer `-K`'s own question,
never to locally validate or reject a command before sending it: an
ordinary command still reaches the server exactly as documented
throughout the rest of this file, unfiltered, so a stale `-K` listing
here only makes its own output incomplete, never breaks a command the
server itself would otherwise accept.

## 6. Examples

```sh
$ icowm-msg get_version
{"ok":true,"protocol_version":1}

$ icowm-msg list_desktops
{"ok":true,"desktops":[{"id":0,"name":"Work","surface_id":0,"current":true}]}

$ icowm-msg goto_desktop desktop_id=1
{"ok":true}

$ icowm-msg goto_desktop desktop_id=99
{"ok":false,"error":"no such desktop on that surface"}

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

There is nothing listening at the resolved socket path yet. This
means one of:

- IcoWM is not currently running.
- IcoWM was started with `-s` (`icowm.md` section 3.1), which
  disables the socket entirely for that run, on purpose.
- IcoWM is running, but its IPC socket failed to come up (see its
  own log: `ipc_init` logs a warning and continues without the
  socket rather than refusing to start over this alone; every other
  part of IcoWM keeps working normally either way).
- `icowm-msg` resolved a different path than the one IcoWM's own
  instance actually bound, most often because `$XDG_RUNTIME_DIR` is
  set to something different in the shell running `icowm-msg` than
  it was in the session IcoWM itself started under (a remote shell,
  a different user, or a terminal from before `$XDG_RUNTIME_DIR` was
  changed, for instance). `icowm-msg` resolves the socket path the
  same way IcoWM itself does (`icowm.md` section 5.1): under
  `$XDG_RUNTIME_DIR/icowm/`, or `/tmp/icowm-<uid>/icowm/` when that
  variable is unset, so the two need to agree on that variable to
  find the same socket.

**A response is printed, but the exit status is always `2`**

This does not happen for a well-formed response: any response
`icowm-msg` successfully reads and prints already parses as JSON, by
construction. If it were ever seen, it would mean IcoWM sent back
something that could not be parsed as JSON at all (not: `"ok":
false`, which parses fine and exits `1`), and would be worth
reporting as an IcoWM bug rather than an `icowm-msg` one.

**`icowm-msg: argument 'foo' is not in 'key=value' form`**

An argument after `<command>` had no `=` in it at all. Every
argument past the command name must be `key=value`; see section 3.

## 8. Command reference

Every command IcoWM currently understands, with its own arguments
(an argument in `[brackets]` is optional) and a one-line summary.
This is a compact index only; the full explanation of each,
including what each response field means and the two actions
deliberately left out of this catalog, is in
[`icowm.md`](icowm.md) section 5.3.

| Command                       | Arguments                                | Description |
|-------------------------------|------------------------------------------|-------------|
| `get_version`                 | none                                     | Reports the wire protocol version |
| `list_desktops`               | none                                     | Lists every desktop on every managed surface |
| `list_clients`                | none                                     | Lists every managed client, with geometry and state flags |
| `get_focused`                 | none                                     | Reports the active client of every managed surface |
| `close_client`                | `client_id`                              | Closes the client politely, or destroys its window |
| `kill_client`                 | `client_id`                              | Forcibly terminates the client's own X connection |
| `focus_client`                | `client_id`                              | Moves input focus to the client (does not raise it) |
| `unfocus_client`              | `client_id`                              | Takes input focus away from the client |
| `iconify_client`              | `client_id`                              | Iconifies (minimizes) the client |
| `deiconify_client`            | `client_id`                              | Restores the client if it was iconified |
| `hide_client`                 | `client_id`                              | Hides the client without iconifying it |
| `unhide_client`               | `client_id`                              | Undoes `hide_client` |
| `pin_client`                  | `client_id`                              | Makes the client visible on every desktop |
| `unpin_client`                | `client_id`                              | Undoes `pin_client` |
| `toggle_pin_client`           | `client_id`                              | Toggles `pin_client`/`unpin_client` |
| `urge_client`                 | `client_id`                              | Marks the client urgent |
| `unurge_client`               | `client_id`                              | Undoes `urge_client` |
| `center_client`               | `client_id`                              | Centers the client on its own screen |
| `move_client_to_next_monitor` | `client_id`                              | Moves the client to the next physical monitor |
| `maximize_client_horz`        | `client_id`                              | Maximizes the client horizontally only |
| `maximize_client_vert`        | `client_id`                              | Maximizes the client vertically only |
| `maximize_client`             | `client_id`                              | Maximizes the client both horizontally and vertically |
| `raise_client`                | `client_id`                              | Raises the client within its own layer |
| `lower_client`                | `client_id`                              | Lowers the client within its own layer |
| `set_layer_above_client`      | `client_id`                              | Moves the client to the "always on top" layer |
| `set_layer_normal_client`     | `client_id`                              | Moves the client back to the ordinary layer |
| `set_layer_below_client`      | `client_id`                              | Moves the client to the "always below" layer |
| `cycle_layer_client`          | `client_id`                              | Cycles the client through above, normal, and below |
| `shade_client`                | `client_id`                              | Rolls the client up into just its own titlebar |
| `unshade_client`              | `client_id`                              | Undoes `shade_client` |
| `toggle_shade_client`         | `client_id`                              | Toggles `shade_client`/`unshade_client` |
| `fullscreen_client`           | `client_id`                              | Makes the client fill its own screen |
| `unfullscreen_client`         | `client_id`                              | Undoes `fullscreen_client` |
| `toggle_fullscreen_client`    | `client_id`                              | Toggles `fullscreen_client`/`unfullscreen_client` |
| `toggle_decorate_client`      | `client_id`                              | Shows or hides the client's own titlebar and border |
| `send_client_to_front`        | `client_id`                              | Raises the client to the front of its own desktop's window stack |
| `send_client_to_back`         | `client_id`                              | Sends the client to the back of its own desktop's window stack |
| `move_client`                 | `client_id` `x` `y`                      | Moves the client so its own top-left corner is at that position |
| `resize_client`               | `client_id` `x` `y` `w` `h`              | Moves and resizes the client in one step |
| `move_client_to_monitor`      | `client_id` `monitor_index`              | Moves the client to that physical monitor |
| `rename_client`               | `client_id` `name`                       | Overrides the client's own window title |
| `reclass_client`              | `client_id` `class_name` `instance_name` | Overrides the client's own `WM_CLASS` |
| `rerole_client`               | `client_id` `role`                       | Overrides the client's own window role |
| `set_client_icon`             | `client_id` `icon_name`                  | Overrides which icon IcoWM shows for the client |
| `set_desktop_background`      | `desktop_id` [`surface_id`] `color`      | Sets that desktop's own solid background color |
| `show_desktop`                | `desktop_id` [`surface_id`] `show`       | Shows or hides every client on that desktop at once |
| `send_client_to_desktop`      | `client_id` `target_desktop_id`          | Moves the client to another desktop on the same surface |
| `iconify_all`                 | [`desktop_id`] [`surface_id`]            | Iconifies every client on that desktop at once |
| `deiconify_all`               | [`desktop_id`] [`surface_id`]            | Restores every iconified client on that desktop at once |
| `rearrange_desktop`           | [`desktop_id`] [`surface_id`]            | Re-applies the configured placement policy on that desktop |
| `goto_desktop`                | `desktop_id` [`surface_id`]              | Switches the resolved surface to that desktop |
| `goto_next_desktop`           | [`surface_id`]                           | Switches the resolved surface to its own next desktop |
| `goto_prev_desktop`           | [`surface_id`]                           | Switches the resolved surface to its own previous desktop |
| `exit_wm`                     | none                                     | Requests that IcoWM stop and exit |
| `reload_config`               | none                                     | Reloads every configuration file |
| `toggle_scratchpad`           | [`desktop_id`[, `surface_id`]]           | Launches the scratchpad, or shows/hides it if already running |

## 9. Watching for events

`icowm-msg -w <events>` switches out of the normal one-request,
one-response flow entirely: instead of sending a command, it
subscribes to one or more events and prints one JSON line per event
as IcoWM reports them, for as long as the connection stays open.
`<command>` and any `key=value` arguments are not used in this mode
at all.

```sh
icowm-msg -w <events> [-n <count>]
```

`<events>` is a comma-separated list of event names (no spaces),
e.g. `window_mapped,desktop_switched`. `-n <count>` stops watching
after that many events have arrived, printing them and then exiting
`0`; left out, `icowm-msg` watches forever, until the connection
drops or the process is killed. `-n` on its own, without `-w`, is
rejected as an error, since it has nothing to count events for.

### 9.1 What each event reports

| Event              | Fields |
|--------------------|--------|
| `window_mapped`    | `client_id`, `desktop_id`, `surface_id`: a client was just mapped onto that desktop |
| `window_closed`    | `client_id`, `desktop_id`, `surface_id`: a client was just destroyed |
| `desktop_switched` | `surface_id`, `desktop_id`: that surface's own current desktop just changed to `desktop_id` |
| `focus_changed`    | `surface_id`, `client_id`: that client just became the active one on its own surface |
| `urgency_set`      | `client_id`, `desktop_id`, `surface_id`: that client's urgency hint was just set |
| `urgency_cleared`  | `client_id`, `desktop_id`, `surface_id`: that client's urgency hint was just cleared |
| `window_moved`     | `client_id`, `desktop_id`, `surface_id`: that client's own position just changed (see the `list_clients` command for its current `x`/`y`) |
| `window_resized`   | `client_id`, `desktop_id`, `surface_id`: that client's own size just changed (see the `list_clients` command for its current `w`/`h`) |
| `rule_applied`     | `client_id`, `desktop_id`, `surface_id`: a loaded rule just changed one or more of that client's own properties |
| `pin_set`          | `client_id`, `desktop_id`, `surface_id`: that client was just pinned (visible on every desktop) |
| `pin_cleared`      | `client_id`, `desktop_id`, `surface_id`: that client was just unpinned |
| `fullscreen_set`   | `client_id`, `desktop_id`, `surface_id`: that client just entered full screen |
| `fullscreen_cleared` | `client_id`, `desktop_id`, `surface_id`: that client just left full screen |
| `shade_set`        | `client_id`, `desktop_id`, `surface_id`: that client was just shaded (rolled up into its titlebar) |
| `shade_cleared`    | `client_id`, `desktop_id`, `surface_id`: that client was just unshaded |
| `hide_set`         | `client_id`, `desktop_id`, `surface_id`: that client was just hidden |
| `hide_cleared`     | `client_id`, `desktop_id`, `surface_id`: that client was just unhidden |
| `decoration_set`   | `client_id`, `desktop_id`, `surface_id`: that client's own titlebar and border were just shown |
| `decoration_cleared` | `client_id`, `desktop_id`, `surface_id`: that client's own titlebar and border were just hidden |
| `client_iconified` | `client_id`, `desktop_id`, `surface_id`: that client was just iconified |
| `client_deiconified` | `client_id`, `desktop_id`, `surface_id`: that client was just restored from being iconified |
| `layer_changed`    | `client_id`, `desktop_id`, `surface_id`: that client's own stacking layer just changed (see `list_clients` for its current layer) |
| `client_desktop_changed` | `client_id`, `desktop_id`, `surface_id`: that client just moved to a different desktop (`desktop_id` is the new one) |
| `client_renamed`   | `client_id`, `desktop_id`, `surface_id`, `name`: that client's own displayed title was just overridden |
| `client_reclassed` | `client_id`, `desktop_id`, `surface_id`, `class_name`, `instance_name`: that client's own `WM_CLASS` was just overridden |
| `client_reroled`   | `client_id`, `desktop_id`, `surface_id`, `role`: that client's own window role was just overridden |
| `client_icon_changed` | `client_id`, `desktop_id`, `surface_id`, `icon_name`: that client's own displayed icon was just overridden |
| `desktop_background_changed` | `desktop_id`, `surface_id`: that desktop's own solid background color was just set |
| `desktop_shown`    | `desktop_id`, `surface_id`: every client on that desktop was just shown at once |
| `desktop_hidden`   | `desktop_id`, `surface_id`: every client on that desktop was just hidden at once |
| `config_reloaded`  | none: every configuration file was just reloaded |
| `stacking_changed` | `client_id`, `desktop_id`, `surface_id`: that client's own position within its layer's stacking order just changed |

Every event line also carries its own `"event"` field naming which
one it is, the same as every other field name above; there is no
separate envelope to unwrap.

### 9.2 Example

```sh
$ icowm-msg -w window_mapped,desktop_switched -n 2
{"ok":true}
{"client_id":23068673,"desktop_id":0,"surface_id":0,"event":"window_mapped"}
{"surface_id":0,"desktop_id":1,"event":"desktop_switched"}
```

The very first line is always the `subscribe` request's own
response (`{"ok":true}`, or, on failure, `{"ok":false,"error":...}`,
followed by `icowm-msg` exiting `1` without watching anything at
all); every line after that is one event.

### 9.3 Exit status while watching

The same three-way split as section 4 applies, adapted to what
"the command" means in this mode:

| Exit status | Meaning |
|-------------|---------|
| `0`         | `-n <count>` was given and that many events were printed |
| `1`         | The `subscribe` request itself was rejected (`"ok": false`); the reason is in that first printed line |
| `2`         | The connection could never be made, the `subscribe` request could not be sent, or the connection was lost while still watching (a server restart, a crash, `icowm` exiting) |

That last `2` case is worth calling out on its own: unlike the
ordinary request/response mode, a watch that has already printed
real events can still end in failure, if the connection drops
before `-n` is reached (or, with no `-n` at all, at any point,
since nothing but the connection itself ever ends it). `icowm-msg`
reports this on `stderr` before exiting, the same way it reports
any other connection failure:

```sh
$ icowm-msg -w window_mapped
{"ok":true}
{"client_id":23068673,"desktop_id":0,"surface_id":0,"event":"window_mapped"}
icowm-msg: connection closed with no response
$ echo $?
2
```

A script that wants to tell "watched successfully to completion"
apart from "the connection dropped partway through" checks the
exit status, not just whether any event lines were printed at all.
