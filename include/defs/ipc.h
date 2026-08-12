/**
 * @file defs/ipc.h
 *
 * @brief Naming and limits for the IPC control socket
 *
 * @ingroup defs
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_IPC_H
#define DEFS_IPC_H


/** File name of the listening socket inside its own runtime
 *  directory (see 'xdg_resolve_dir' with 'XDG_DIR_RUNTIME') */
#define IPC_SOCKET_FILENAME "socket"

/** Directory permission bits for the runtime directory the socket
 *  lives in.  Owner-only, matching what the XDG Base Directory
 *  Specification itself requires of '$XDG_RUNTIME_DIR': no other
 *  user can even list, let alone connect to, anything inside it,
 *  regardless of the socket file's own permissions (which the
 *  kernel does not consistently enforce for 'AF_UNIX' the same way
 *  it does for a regular file's read/write bits) */
#define IPC_RUNTIME_DIR_MODE (0700)

/** Backlog passed to 'listen'; this is a single local control
 *  socket, not a network-facing server, so a handful of queued
 *  connections is generous already */
#define IPC_LISTEN_BACKLOG (4)

/** '/tmp' fallback used when '$XDG_RUNTIME_DIR' is unset; the
 *  numeric user ID is appended by whoever builds this path (see
 *  'ipc_init' in ipc.c), since this file only owns the fixed part
 *  of the name */
#define IPC_TMP_FALLBACK_PREFIX "/tmp/icowm-"

/** Maximum number of simultaneously connected IPC clients; a small
 *  local control socket, not a network-facing server, so this is
 *  generous already */
#define IPC_MAX_CLIENTS (8)

/** Maximum length, in bytes, of one buffered line (request or
 *  response) the IPC transport will hold per client at once; a
 *  request or response line longer than this is rejected rather
 *  than silently growing the buffer without bound */
#define IPC_MSG_MAX_LENGTH (4096)

/**
 * @brief Version of the line-JSON IPC wire protocol itself
 *
 * Not this program's own version: a client only needs to know
 * whether the shape of the messages it is about to send matches
 * what this running instance understands, not which release of
 * IcoWM it is talking to.  Bumped only when the wire protocol
 * itself changes in a way an existing client could not already
 * handle (a command's own argument or response shape changing,
 * for instance); a new command being added does not require a
 * bump, since an unaware client simply never sends it.
 */
#define IPC_PROTOCOL_VERSION (1)


#endif  /* ! DEFS_IPC_H */
