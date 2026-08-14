/**
 * @file ipc.h
 *
 * @brief IPC control socket interface
 *
 * A Unix domain socket external tools can connect to, to query or
 * control this running IcoWM instance without going through X11
 * directly.  This header only covers bringing the listening socket
 * itself up and down; accepting connections, reading messages, and
 * dispatching them is a separate, later piece of the same feature.
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

/* Project includes */
#include <wm.h>

/* JSON includes */
#include <cjson/cJSON.h>

/* Public interface */
/**
 * @brief Every event a client can @c subscribe to over the socket
 *
 * A bitmask (see @c ipc_broadcast_event and each connected client's
 * own subscription set), not a plain sequential enum, so a future
 * addition never renumbers an existing one an already-running
 * client might still be relying on.
 */
/**
 * @brief Every event this window manager can broadcast over IPC
 *
 * Plain @c #define constants of type @c uint32_t rather than a
 * regular C @c enum: @c IPC_EVENT_STACKING_CHANGED's own value
 * (@c 1u @c << @c 31, i.e., 2147483648) exceeds @c INT_MAX, and ISO C
 * requires every enumerator's value to be representable as an @c
 * int -- an @c enum simply cannot host it under strict @c -std=c99
 * @c -pedantic compilation, regardless of the expression used to
 * compute it. Every function that used to take an @c enum @c ipc_
 * event_type_e parameter now takes a plain @c uint32_t instead, for
 * the same reason.
 */
#define IPC_EVENT_WINDOW_MAPPED ((uint32_t) 1u << 0)  /**< A client
                                                                was
                                                                mapped */
#define IPC_EVENT_WINDOW_CLOSED ((uint32_t) 1u << 1)  /**< A client
                                                                was
                                                                destroyed */
#define IPC_EVENT_DESKTOP_SWITCHED ((uint32_t) 1u << 2)  /**< A
                                                                surface's
                                                                own
                                                                current
                                                                desktop
                                                                changed */
#define IPC_EVENT_FOCUS_CHANGED ((uint32_t) 1u << 3)  /**< A
                                                                desktop's
                                                                own
                                                                active
                                                                client
                                                                changed */
#define IPC_EVENT_URGENCY_SET ((uint32_t) 1u << 4)  /**< A
                                                                client's
                                                                urgency
                                                                hint was
                                                                set */
#define IPC_EVENT_URGENCY_CLEARED ((uint32_t) 1u << 5)  /**< A
                                                                client's
                                                                urgency
                                                                hint was
                                                                cleared */
#define IPC_EVENT_WINDOW_MOVED ((uint32_t) 1u << 6)  /**< A
                                                                client's
                                                                own
                                                                position
                                                                changed */
#define IPC_EVENT_WINDOW_RESIZED ((uint32_t) 1u << 7)  /**< A
                                                                client's
                                                                own size
                                                                changed */
#define IPC_EVENT_RULE_APPLIED ((uint32_t) 1u << 8)  /**< A rule
                                                                changed
                                                                one of a
                                                                client's
                                                                own
                                                                properties */
#define IPC_EVENT_PIN_SET ((uint32_t) 1u << 9)  /**< A client
                                                                was
                                                                pinned */
#define IPC_EVENT_PIN_CLEARED ((uint32_t) 1u << 10) /**< A client
                                                                was
                                                                unpinned */
#define IPC_EVENT_FULLSCREEN_SET ((uint32_t) 1u << 11) /**< A
                                                                  client
                                                                  entered
                                                                  fullscreen */
#define IPC_EVENT_FULLSCREEN_CLEARED ((uint32_t) 1u << 12) /**< A
                                                                  client
                                                                  left
                                                                  fullscreen */
#define IPC_EVENT_SHADE_SET ((uint32_t) 1u << 13) /**< A client
                                                                was
                                                                shaded */
#define IPC_EVENT_SHADE_CLEARED ((uint32_t) 1u << 14) /**< A client
                                                                was
                                                                unshaded */
#define IPC_EVENT_HIDE_SET ((uint32_t) 1u << 15) /**< A client
                                                                was
                                                                hidden */
#define IPC_EVENT_HIDE_CLEARED ((uint32_t) 1u << 16) /**< A client
                                                                was
                                                                unhidden */
#define IPC_EVENT_DECORATION_SET ((uint32_t) 1u << 17) /**< A
                                                                  client's
                                                                  own
                                                                  decoration
                                                                  was
                                                                  shown */
#define IPC_EVENT_DECORATION_CLEARED ((uint32_t) 1u << 18) /**< A
                                                                  client's
                                                                  own
                                                                  decoration
                                                                  was
                                                                  hidden */
#define IPC_EVENT_CLIENT_ICONIFIED ((uint32_t) 1u << 19) /**< A
                                                                  client
                                                                  was
                                                                  iconified */
#define IPC_EVENT_CLIENT_DEICONIFIED ((uint32_t) 1u << 20) /**< A
                                                                  client
                                                                  was
                                                                  restored
                                                                  from
                                                                  being
                                                                  iconified */
#define IPC_EVENT_LAYER_CHANGED ((uint32_t) 1u << 21) /**< A
                                                                client's
                                                                own
                                                                stacking
                                                                layer
                                                                changed */
#define IPC_EVENT_CLIENT_DESKTOP_CHANGED ((uint32_t) 1u << 22) /**< A
                                                                      client
                                                                      moved
                                                                      to a
                                                                      different
                                                                      desktop */
#define IPC_EVENT_CLIENT_RENAMED ((uint32_t) 1u << 23) /**< A
                                                                client's
                                                                own
                                                                displayed
                                                                title was
                                                                overridden */
#define IPC_EVENT_CLIENT_RECLASSED ((uint32_t) 1u << 24) /**< A
                                                                client's
                                                                own
                                                                'WM_CLASS'
                                                                was
                                                                overridden */
#define IPC_EVENT_CLIENT_REROLED ((uint32_t) 1u << 25) /**< A
                                                                client's
                                                                own
                                                                window
                                                                role was
                                                                overridden */
#define IPC_EVENT_CLIENT_ICON_CHANGED ((uint32_t) 1u << 26) /**< A
                                                                   client's
                                                                   own
                                                                   displayed
                                                                   icon
                                                                   was
                                                                   overridden */
#define IPC_EVENT_DESKTOP_BACKGROUND_CHANGED ((uint32_t) 1u << 27) /**<
                                                                     A
                                                                     desktop's
                                                                     own
                                                                     solid
                                                                     background
                                                                     color
                                                                     was
                                                                     set */
#define IPC_EVENT_DESKTOP_SHOWN ((uint32_t) 1u << 28) /**< Every
                                                                client on
                                                                a desktop
                                                                was shown
                                                                at once */
#define IPC_EVENT_DESKTOP_HIDDEN ((uint32_t) 1u << 29) /**< Every
                                                                client on
                                                                a desktop
                                                                was hidden
                                                                at once */
#define IPC_EVENT_CONFIG_RELOADED ((uint32_t) 1u << 30) /**< Every
                                                                configuration
                                                                file was
                                                                reloaded */
#define IPC_EVENT_STACKING_CHANGED ((uint32_t) 1u << 31) /**< A
                                                                client's
                                                                own
                                                                position
                                                                within its
                                                                layer's
                                                                stacking
                                                                order
                                                                changed */

/**
 * @brief Initialize the IPC control socket
 *
 * Resolves the XDG runtime directory (@c XDG_DIR_RUNTIME; see @c
 * utils/config/path.h), creating it with mode @c IPC_RUNTIME_DIR_
 * MODE if it does not already exist, then creates, binds, and
 * listens on a non-blocking Unix domain socket named @c IPC_SOCKET_
 * FILENAME inside it (see defs/ipc.h for both).
 *
 * Any file already at that path is unlinked first, on the
 * assumption that it is a stale leftover from a previous run that
 * did not shut down cleanly (a crash, or @c SIGKILL) rather than a
 * second live instance: IcoWM already refuses to start at all when
 * another window manager (itself included) already holds
 * @c SubstructureRedirect on the same screen (see @c startup_
 * subscribe_root_events), so by the time this function is ever
 * called that possibility has already been ruled out.
 *
 * A no-op, returning success, if the socket is already up (calling
 * this twice without an intervening @c ipc_destroy is not an error).
 *
 * @return 0 on success, -1 on failure (reason logged)
 *
 * @note Complexity: @e O(1)
 */
int ipc_init(void);

/**
 * @brief Return the listening socket's own file descriptor
 *
 * Meant to be added to the caller's own @c poll (or equivalent) set
 * alongside the X connection's descriptor; this header does not
 * itself read from or accept connections on it.
 *
 * @return The descriptor, or -1 when @c ipc_init was never called,
 *         or failed, or @c ipc_destroy has since been called
 *
 * @note Complexity: @e O(1)
 */
int ipc_socket_fd(void);

/**
 * @brief Destroy the IPC control socket
 *
 * Closes the listening descriptor and removes the socket's own file
 * from disk.  A no-op if the socket is not currently up.
 *
 * @note Complexity: @e O(1)
 */
void ipc_destroy(void);

/**
 * @brief List every file descriptor the IPC subsystem currently
 *        wants polled
 *
 * Includes the listening socket itself (see @c ipc_socket_fd) and
 * every currently connected client, so a caller only ever needs to
 * add this one call's own output to its own @c poll set; it never
 * needs to know the difference between the listening socket and an
 * already-connected client itself, since @c ipc_handle_readable
 * below handles that distinction internally.
 *
 * @param out_fds Destination array
 * @param max     Capacity of @p out_fds
 *
 * @return How many descriptors were written, from @c 0 (the socket
 *         is not up at all) up to @p max
 *
 * @note Complexity: @e O(1)
 */
int ipc_poll_fds(int *out_fds, int max);

/**
 * @brief Handle a single IPC-related descriptor a @c poll call
 *        reported as readable
 *
 * When @p fd is the listening socket, accepts one pending connection
 * (logging and dropping it immediately if @c IPC_MAX_CLIENTS is
 * already reached).  When @p fd is an already-connected client,
 * reads whatever is currently available into that client's own
 * buffer, and for every complete newline-terminated line found,
 * dispatches it via @c ipc_commands_dispatch (see ipc/commands.h)
 * and writes the response straight back, newline-terminated in
 * turn.  A client's own connection is closed, and its slot freed,
 * when it disconnects, sends more than @c IPC_MSG_MAX_LENGTH bytes
 * without a newline, or its descriptor otherwise errors.
 *
 * @param wm Window manager instance, passed through to @c ipc_
 *           commands_dispatch for any command actually dispatched
 *           this call
 * @param fd One of the descriptors a previous @c ipc_poll_fds call
 *           returned, now reported readable; harmless no-op if @p
 *           fd does not currently belong to this subsystem at all
 *
 * @note Complexity: @e O(n), where @e n is the number of complete
 *       lines found in this one read
 */
void ipc_handle_readable(wm_td *wm, int fd);

/**
 * @brief Subscribe one connection to one or more events
 *
 * Called from @c ipc_commands_dispatch itself as a special case,
 * ahead of the ordinary command table (see @c ipc/commands.c): this
 * is the one command whose own effect belongs to the connection
 * that sent it, not to the window manager, so it needs to know
 * which client sent it in a way none of the other 55 handlers ever
 * do.
 *
 * @param client_idx Index into this file's own connected-client
 *                    table (see @c ipc_handle_readable's own
 *                    caller), naming the connection to subscribe
 * @param args        The request object; must have a non-empty
 *                    array @c "events" of recognized event names
 *                    (the @c IPC_EVENT_* constants above), rejected as a whole
 *                    if any single one is not
 *
 * @return The standard success or failure response
 *
 * @note Complexity: @e O(n), where @e n is the length of @p args's
 *       own @c "events" array
 */
cJSON *ipc_client_subscribe(int client_idx, const cJSON *args);

/**
 * @brief Unsubscribe one connection from one or more events
 *
 * @param client_idx Index into this file's own connected-client
 *                    table, naming the connection to unsubscribe
 * @param args        The request object; an @c "events" array
 *                    unsubscribes from only those (an unrecognized
 *                    or never-subscribed name among them is not an
 *                    error, since there is nothing to undo either
 *                    way), while leaving it out entirely
 *                    unsubscribes from everything at once
 *
 * @return The standard success response (always succeeds)
 *
 * @note Complexity: @e O(n), where @e n is the length of @p args's
 *       own @c "events" array, or @e O(1) when left out entirely
 */
cJSON *ipc_client_unsubscribe(int client_idx, const cJSON *args);

/**
 * @brief Send one event line to every currently subscribed client
 *
 * A safe no-op when the socket is not up at all (@c ipc_init was
 * never called, failed, @c -s was given, or @c ipc_destroy has since
 * run) or when no connected client is currently subscribed to @p
 * type, so every call site throughout the rest of the project can
 * call this unconditionally, the same way it already calls @c
 * LOGGER_* macros unconditionally, without needing to first check
 * whether anyone is listening.
 *
 * @param type   Which event this is; only clients subscribed to
 *               this exact bit (see the @c IPC_EVENT_* constants above) are
 *               sent anything at all
 * @param fields The event's own fields beyond its shared @c "event"
 *               name field, or @c NULL for one with none.
 *               However this call ends, whether any client was
 *               actually subscribed or not, @p fields is always
 *               freed before it returns: the caller never needs an
 *               @c cJSON_Delete of its own after calling this
 *
 * @note Complexity: @e O(n), where @e n is @c IPC_MAX_CLIENTS
 */
void ipc_broadcast_event(uint32_t type, cJSON *fields);


#endif  /* ! IPC_H */
