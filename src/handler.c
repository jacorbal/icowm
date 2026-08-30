/**
 * @file handler.c
 *
 * @brief X event handler implementations
 *
 * The original monolithic handler has been divided into eight
 * translation units, each responsible for one family of X events:
 *
 * - @c handler/configure.c: @c CONFIGURE_REQUEST, @c CONFIGURE_NOTIFY
 * - @c handler/map.c: @c MAP_REQUEST, @c UNMAP_NOTIFY, and
 *   @c DESTROY_NOTIFY
 * - @c handler/focus.c: @c PROPERTY_NOTIFY, @c FOCUS_IN and
 *   @c MAPPING_NOTIFY
 * - @c handler/expose.c: @c EXPOSE
 * - @c handler/message.c: @c CLIENT_MESSAGE dispatcher (dispatches EWMH
 *   and ICCCM protocol messages to @c handler/ewmh.c's
 *   per-atom sub-handlers)
 * - @c handler/randr.c: RandR extension screen/output change events
 * - @c handler/sync.c: XSync extension @c _NET_WM_SYNC_REQUEST alarms
 *
 * This file is intentionally empty; all implementations live in the
 * modules listed above.  The public API declared in handler.h remains
 * unchanged.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* suppress ISO C empty-translation-unit warning */
typedef int handler_empty_tu_placeholder;
