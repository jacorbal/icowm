# icowm-msg Manual

What `icowm-msg` is, how to invoke it, how it turns its own
command-line arguments into a request, and what its exit status
means.  For the socket it talks to (where it lives, the wire
protocol itself, and the full list of every command with its own
arguments), see [`manual.md`](manual.md) section 5 instead: this
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

---

## 1. What `icowm-msg` is

`icowm-msg` is a small, standalone command-line client for IcoWM's
own IPC control socket (`manual.md` section 5). It builds one JSON
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

`<command>` is any of the command names `manual.md` section 5.3
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

| Value | Sent as |
|-------|----------|
| Exactly `true` or `false` | A JSON boolean |
| Parses in full as a number (e.g. `23068673`, `-5`, `3.5`) | A JSON number |
| Anything else | A JSON string, verbatim |

This means a numeric ID never needs quoting on the command line
(`client_id=23068673`, not `client_id="23068673"`), and a value
that happens to look like a number but is meant as text (a client's
own new name that is only digits, say) is sent as a number instead;
none of the commands `manual.md` section 5.3 documents currently
have a string argument this could affect, but it is worth knowing
about if a future one ever does.

An argument not in `key=value` form at all (no `=` in it), or with
an empty key before the `=`, is rejected before anything is even
sent, with an explanation on `stderr` naming the offending argument.

## 4. Exit status

| Exit status | Meaning |
|-------------|----------|
| `0` | The command reached IcoWM and it reported success (`"ok": true` in the printed response) |
| `1` | The command reached IcoWM but it reported failure (`"ok": false`); the reason is in the printed response's own `"error"` field |
| `2` | The request never reached IcoWM at all: no socket at the resolved path (see section 7), a connection failure, a response IcoWM sent back that does not itself parse as JSON, or a local argument-parsing error (missing `<command>`, a malformed `key=value`, an unrecognized option). Nothing is printed to `stdout` in this case; the reason is on `stderr` |

A script that only cares whether the command worked can check the
exit status alone, without parsing the response at all.

## 5. Options

| Option | What it does |
|--------|----------------|
| `-h` | Show usage, a few examples, and this same option list, then exit |
| `-v` | Show `icowm-msg`'s own name, IcoWM's own short name and version, its license, its copyright line, and its author, then exit |

Both exit `0`. Any other option is rejected: usage is printed to
`stderr` and `icowm-msg` exits `2`.

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
- IcoWM was started with `-s` (`manual.md` section 3.1), which
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
  same way IcoWM itself does (`manual.md` section 5.1): under
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
(`?` marks an optional one) and a one-line summary. This is a
compact index only; the full explanation of each, including what
each response field means and the two actions deliberately left
out of this catalog, is in [`manual.md`](manual.md) section 5.3.

| Command                    | Arguments                                  | Description |
|----------------------------|--------------------------------------------|-------------|
| `get_version`              | none                                       | Reports the wire protocol version |
| `list_desktops`            | none                                       | Lists every desktop on every managed surface |
| `list_clients`             | none                                       | Lists every managed client, with geometry and state flags |
| `get_focused`              | none                                       | Reports the active client of every managed surface |
| `close_client`             | `client_id`                                | Closes the client politely, or destroys its window |
| `kill_client`              | `client_id`                                | Forcibly terminates the client's own X connection |
| `focus_client`             | `client_id`                                | Moves input focus to the client (does not raise it) |
| `unfocus_client`           | `client_id`                                | Takes input focus away from the client |
| `iconify_client`           | `client_id`                                | Iconifies (minimizes) the client |
| `deiconify_client`         | `client_id`                                | Restores the client if it was iconified |
| `hide_client`              | `client_id`                                | Hides the client without iconifying it |
| `unhide_client`            | `client_id`                                | Undoes `hide_client` |
| `sticky_client`            | `client_id`                                | Makes the client visible on every desktop |
| `unsticky_client`          | `client_id`                                | Undoes `sticky_client` |
| `toggle_sticky_client`     | `client_id`                                | Toggles `sticky_client`/`unsticky_client` |
| `set_urgent_client`        | `client_id`                                | Marks the client urgent |
| `clear_urgent_client`      | `client_id`                                | Undoes `set_urgent_client` |
| `center_client`            | `client_id`                                | Centers the client on its own screen |
| `move_client_next_monitor` | `client_id`                                | Moves the client to the next physical monitor |
| `maximize_client_horz`     | `client_id`                                | Maximizes the client horizontally only |
| `maximize_client_vert`     | `client_id`                                | Maximizes the client vertically only |
| `maximize_client`          | `client_id`                                | Maximizes the client both horizontally and vertically |
| `raise_client`             | `client_id`                                | Raises the client within its own layer |
| `lower_client`             | `client_id`                                | Lowers the client within its own layer |
| `layer_above_client`       | `client_id`                                | Moves the client to the "always on top" layer |
| `layer_normal_client`      | `client_id`                                | Moves the client back to the ordinary layer |
| `layer_below_client`       | `client_id`                                | Moves the client to the "always below" layer |
| `cycle_layer_client`       | `client_id`                                | Cycles the client through above, normal, and below |
| `shade_client`             | `client_id`                                | Rolls the client up into just its own titlebar |
| `unshade_client`           | `client_id`                                | Undoes `shade_client` |
| `toggle_shade_client`      | `client_id`                                | Toggles `shade_client`/`unshade_client` |
| `fullscreen_client`        | `client_id`                                | Makes the client fill its own screen |
| `unfullscreen_client`      | `client_id`                                | Undoes `fullscreen_client` |
| `toggle_fullscreen_client` | `client_id`                                | Toggles `fullscreen_client`/`unfullscreen_client` |
| `toggle_decoration_client` | `client_id`                                | Shows or hides the client's own titlebar and border |
| `send_client_to_front`     | `client_id`                                | Raises the client to the front of its own desktop's window stack |
| `send_client_to_back`      | `client_id`                                | Sends the client to the back of its own desktop's window stack |
| `move_client`              | `client_id`, `x`, `y`                      | Moves the client so its own top-left corner is at that position |
| `resize_client`            | `client_id`, `x`, `y`, `w`, `h`            | Moves and resizes the client in one step |
| `move_client_to_monitor`   | `client_id`, `monitor_index`               | Moves the client to that physical monitor |
| `rename_client`            | `client_id`, `name`                        | Overrides the client's own window title |
| `reclass_client`           | `client_id`, `class_name`, `instance_name` | Overrides the client's own `WM_CLASS` |
| `rerole_client`            | `client_id`, `role`                        | Overrides the client's own window role |
| `set_client_icon`          | `client_id`, `icon_name`                   | Overrides which icon IcoWM shows for the client |
| `set_desktop_background`   | `desktop_id`, `surface_id`?, `color`       | Sets that desktop's own solid background color |
| `show_desktop`             | `desktop_id`, `surface_id`?, `show`        | Shows or hides every client on that desktop at once |
| `send_client_to_desktop`   | `client_id`, `target_desktop_id`           | Moves the client to another desktop on the same surface |
| `iconify_all`              | `desktop_id`?, `surface_id`?               | Iconifies every client on that desktop at once |
| `deiconify_all`            | `desktop_id`?, `surface_id`?               | Restores every iconified client on that desktop at once |
| `rearrange`                | `desktop_id`?, `surface_id`?               | Re-applies the configured placement policy on that desktop |
| `goto_desktop`             | `desktop_id`, `surface_id`?                | Switches the resolved surface to that desktop |
| `next_desktop`             | `surface_id`?                              | Switches the resolved surface to its own next desktop |
| `prev_desktop`             | `surface_id`?                              | Switches the resolved surface to its own previous desktop |
| `wm_exit`                  | none                                       | Requests that IcoWM stop and exit |
| `reload_config`            | none                                       | Reloads every configuration file |
