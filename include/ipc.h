/**
 * @file ipc.h
 *
 * @brief IPC control socket interface
 *
 * A Unix domain socket external tools can connect to, to query or
 * control this running IcoWM instance without going through X11
 * directly.  This header only covers bringing the listening socket
 * itself up and down, that is, accepting connections, reading messages,
 * and dispatching them is a separate, later piece of the same feature.
 *
 * @defgroup ipc IPC control socket
 * @ingroup wm
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef IPC_H
#define IPC_H


/* System includes */
#include <stdint.h>

/* Type includes */
#include <types/handles.h>

/* JSON includes */
#include <cjson/cJSON.h>


/**
 * @brief Every event a client can @c subscribe to over the socket
 *
 * Plain @c #define constants of type @c uint64_t rather than a regular
 * C @c enum: most of their values exceed @c INT_MAX, and ISO C requires
 * every enumerator's value to be representable as an @c int, i.e., an
 * @c enum simply cannot host them under strict @c -std=c99 @c -pedantic
 * compilation, regardless of the expression used to compute them.
 * Every function taking such a value takes a plain @c uint64_t rather
 * than an @c (enum ipc_event_type_e), for the same reason.
 *
 * Every name follows one scheme: the subject first (@c client_,
 * @c desktop_, @c scratchpad_, @c config_), then what happened to it,
 * @c _set and @c _cleared for a state it gains or loses, and
 * @c _changed for a value that changes.
 *
 * @see @a ipc_broadcast_event
 *
 * @defgroup ipc_events IPC broadcast events
 * @ingroup ipc
 * @{
 */
/** A client was mapped */
#define IPC_EVENT_CLIENT_MAPPED ((uint64_t) 1u << 0)

/** A client was destroyed */
#define IPC_EVENT_CLIENT_CLOSED ((uint64_t) 1u << 1)

/**
 * @brief A stage's current desktop changed, or its
 *        strutless-maximization mode was toggled
 */
#define IPC_EVENT_DESKTOP_SWITCHED ((uint64_t) 1u << 2)

/** A desktop's active client changed */
#define IPC_EVENT_CLIENT_FOCUSED ((uint64_t) 1u << 3)

/** A client's urgency hint was set */
#define IPC_EVENT_CLIENT_URGENCY_SET ((uint64_t) 1u << 4)

/** A client's urgency hint was cleared */
#define IPC_EVENT_CLIENT_URGENCY_CLEARED ((uint64_t) 1u << 5)

/** A client's position changed */
#define IPC_EVENT_CLIENT_MOVED ((uint64_t) 1u << 6)

/** A client's size changed */
#define IPC_EVENT_CLIENT_RESIZED ((uint64_t) 1u << 7)

/** A rule changed one of a client's properties */
#define IPC_EVENT_CLIENT_RULE_APPLIED ((uint64_t) 1u << 8)

/** A client was pinned */
#define IPC_EVENT_CLIENT_PIN_SET ((uint64_t) 1u << 9)

/** A client was unpinned */
#define IPC_EVENT_CLIENT_PIN_CLEARED ((uint64_t) 1u << 10)

/** A client entered fullscreen */
#define IPC_EVENT_CLIENT_FULLSCREEN_SET ((uint64_t) 1u << 11)

/** A client left fullscreen */
#define IPC_EVENT_CLIENT_FULLSCREEN_CLEARED ((uint64_t) 1u << 12)

/** A client was shaded */
#define IPC_EVENT_CLIENT_SHADE_SET ((uint64_t) 1u << 13)

/** A client was unshaded */
#define IPC_EVENT_CLIENT_SHADE_CLEARED ((uint64_t) 1u << 14)

/** A client was hidden */
#define IPC_EVENT_CLIENT_HIDE_SET ((uint64_t) 1u << 15)

/** A client was unhidden */
#define IPC_EVENT_CLIENT_HIDE_CLEARED ((uint64_t) 1u << 16)

/** A client's decoration was shown */
#define IPC_EVENT_CLIENT_DECORATION_SET ((uint64_t) 1u << 17)

/** A client's decoration was hidden */
#define IPC_EVENT_CLIENT_DECORATION_CLEARED ((uint64_t) 1u << 18)

/** A client was iconified */
#define IPC_EVENT_CLIENT_ICONIFY_SET ((uint64_t) 1u << 19)

/** A client was restored from being iconified */
#define IPC_EVENT_CLIENT_ICONIFY_CLEARED ((uint64_t) 1u << 20)

/** A client's stacking layer changed */
#define IPC_EVENT_CLIENT_LAYER_CHANGED ((uint64_t) 1u << 21)

/** A client moved to a different desktop */
#define IPC_EVENT_CLIENT_DESKTOP_CHANGED ((uint64_t) 1u << 22)

/** A client's displayed title was overridden */
#define IPC_EVENT_CLIENT_RENAMED ((uint64_t) 1u << 23)

/** A client's @c WM_CLASS was overridden */
#define IPC_EVENT_CLIENT_RECLASSED ((uint64_t) 1u << 24)

/** A client's window role was overridden */
#define IPC_EVENT_CLIENT_REROLED ((uint64_t) 1u << 25)

/** A client's displayed icon was overridden */
#define IPC_EVENT_CLIENT_ICON_CHANGED ((uint64_t) 1u << 26)

/** A desktop's solid background color was set */
#define IPC_EVENT_DESKTOP_BACKGROUND_CHANGED ((uint64_t) 1u << 27)

/** Every client on a desktop was shown at once */
#define IPC_EVENT_DESKTOP_SHOWN ((uint64_t) 1u << 28)

/** Every client on a desktop was hidden at once */
#define IPC_EVENT_DESKTOP_HIDDEN ((uint64_t) 1u << 29)

/** Every configuration file was reloaded */
#define IPC_EVENT_CONFIG_RELOADED ((uint64_t) 1u << 30)

/** A client's position within its layer's stacking order changed */
#define IPC_EVENT_CLIENT_STACKING_CHANGED ((uint64_t) 1u << 31)

/** A client was maximized on an axis it was not before */
#define IPC_EVENT_CLIENT_MAXIMIZE_SET ((uint64_t) 1u << 32)

/** A client stopped being maximized on an axis it was before */
#define IPC_EVENT_CLIENT_MAXIMIZE_CLEARED ((uint64_t) 1u << 33)

/** A desktop was added to a stage */
#define IPC_EVENT_DESKTOP_ADDED ((uint64_t) 1u << 34)

/** A desktop was removed from a stage */
#define IPC_EVENT_DESKTOP_REMOVED ((uint64_t) 1u << 35)

/** The scratchpad was shown */
#define IPC_EVENT_SCRATCHPAD_SHOWN ((uint64_t) 1u << 36)

/** The scratchpad was hidden */
#define IPC_EVENT_SCRATCHPAD_HIDDEN ((uint64_t) 1u << 37)

/** @} */


/**
 * @brief Initialize the IPC control socket
 *
 * Resolves the XDG runtime directory, creating it with mode
 * @c IPC_RUNTIME_DIR_MODE if it does not already exist, then creates,
 * binds, and listens on a non-blocking Unix domain socket named
 * @c IPC_SOCKET_FILENAME inside it.
 *
 * Any file already at that path is unlinked first, on the assumption
 * that it is a stale leftover from a previous run that did not shut
 * down cleanly (a crash, or @c SIGKILL) rather than a second live
 * instance: IcoWM already refuses to start at all when another window
 * manager (itself included) already holds @c SubstructureRedirect on
 * the same screen, so by the time this function is ever called that
 * possibility has already been ruled out.
 *
 * @return Status of the operation
 * @retval  0 on success
 * @retval -1 on failure (reason logged)
 *
 * @note A no-op, returning success, if the socket is already up
 *       (calling this twice without an intervening @a ipc_destroy is
 *       not an error)
 * @note Complexity: @e O(1)
 *
 * @see @c XDG_DIR_RUNTIME in @c utils/config/path.h
 * @see @c defs/ipc.h for @c IPC_* definitions
 * @see @a wm_startup_subscribe_root_events
 */
int ipc_init(void);

/**
 * @brief Destroy the IPC control socket
 *
 * Closes the listening descriptor and removes the socket's file
 * from disk.
 *
 * @note A no-op if the socket is not currently up
 * @note Complexity: @e O(1)
 */
void ipc_destroy(void);

/**
 * @brief List every file descriptor the IPC subsystem currently
 *        wants polled
 *
 * Includes the listening socket itself and every currently connected
 * client, so a caller only ever needs to add this one call's output
 * to its @p poll set; it never needs to know the difference between
 * the listening socket and an already-connected client itself, since
 * @a ipc_handle_readable below handles that distinction internally.
 *
 * @param out_fds Destination array
 * @param max     Capacity of @p out_fds
 *
 * @return How many descriptors were written, from @c 0 (the socket is
 *         not up at all) up to @p max
 *
 * @note Complexity: @e O(1)
 */
int ipc_poll_fds(int *out_fds, int max);

/**
 * @brief Handle a single IPC-related descriptor a @p poll call reported
 *        as readable
 *
 * When @p fd is the listening socket, accepts one pending connection
 * (logging and dropping it immediately if @c IPC_MAX_CLIENTS is already
 * reached).  When @p fd is an already-connected client, reads whatever
 * is currently available into that client's buffer, and for every
 * complete newline-terminated line found, dispatches it via
 * @a ipc_commands_dispatch and writes the response straight back,
 * newline-terminated in turn.  A client's connection is closed, and
 * its slot freed, when it disconnects, sends more than
 * @c IPC_MSG_MAX_LENGTH bytes without a newline, or its descriptor
 * otherwise errors.
 *
 * @param wm Window manager instance, passed through to
 *           @c ipc_commands_dispatch for any command actually
 *           dispatched this call
 * @param fd One of the descriptors a previous @a ipc_poll_fds call
 *           returned, now reported readable; harmless no-op if @p fd
 *           does not currently belong to this subsystem at all
 *
 * @note Complexity: @e O(n), where @e n is the number of complete lines
 *       found in this one read
 *
 * @see @a ipc_commands_dispatch in @c ipc/commands.h
 */
void ipc_handle_readable(wm_td *wm, int fd);

/**
 * @brief Subscribe one connection to one or more events
 *
 * Called from @a ipc_commands_dispatch itself as a special case, ahead
 * of the ordinary command table.  This is the one command whose
 * effect belongs to the connection that sent it, not to the window
 * manager, so it needs to know which client sent it in a way none of
 * the other 55 handlers ever do.
 *
 * @param client_idx Index into this file's connected-client table,
 *                    naming the connection to subscribe
 * @param args The request object; must have a non-empty
 *                    array @p events of recognized event names,
 *                    rejected as a whole if any single one is not
 *
 * @return The standard success or failure response
 *
 * @note Complexity: @e O(n), where @e n is the length of @p args's
 *       @p events array
 *
 * @see @a ipc_commands_dispatch in @c ipc/commands.c; and 
 *      @c IPC_EVENT_* constants above
 */
cJSON *ipc_client_subscribe(int client_idx, const cJSON *args);

/**
 * @brief Unsubscribe one connection from one or more events
 *
 * @param client_idx Index into this file's connected-client table,
 *                   naming the connection to unsubscribe
 * @param args The request object; an @p events array unsubscribes
 *                   from only those (an unrecognized or
 *                   never-subscribed name among them is not an error,
 *                   since there is nothing to undo either way), while
 *                   leaving it out entirely unsubscribes from
 *                   everything at once
 *
 * @return The standard success response (always succeeds)
 *
 * @note Complexity: @e O(n), where @e n is the length of @p args's
 *       @p events array, or @e O(1) when left out entirely
 */
cJSON *ipc_client_unsubscribe(int client_idx, const cJSON *args);

/**
 * @brief Send one event line to every currently subscribed client
 *
 * A safe no-op when the socket is not up at all (@a ipc_init was never
 * called, failed, @c -s was given, or @a ipc_destroy has since run) or
 * when no connected client is currently subscribed to @p type, so every
 * call site throughout the rest of the project can call this
 * unconditionally, the same way it already calls @c LOGGER_* macros
 * unconditionally, without needing to first check whether anyone is
 * listening.
 *
 * @param type Which event this is; only clients subscribed to
 *               this exact bit are sent anything at all
 * @param fields The event's fields beyond its shared @p event name
 *               field, or @c NULL for one with none.  However this call
 *               ends, whether any client was actually subscribed or
 *               not, @p fields is always freed before it returns.  The
 *               caller never needs an @a cJSON_Delete of its own after
 *               calling this
 *
 * @note Complexity: @e O(n), where @e n is @c IPC_MAX_CLIENTS
 *
 * @see @c IPC_EVENT_* constants above
 */
void ipc_broadcast_event(uint64_t type, cJSON *fields);


#endif  /* ! IPC_H */
