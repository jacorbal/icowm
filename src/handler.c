/**
 * @file handler.c
 *
 * @brief X event handler implementations
 *
 * The original monolithic handler has been divided into eight
 * translation units, each responsible for one family of X events:
 *
 *   - handler/configure.c: @c CONFIGURE_REQUEST, @c CONFIGURE_NOTIFY
 *   - handler/map.c: @c MAP_REQUEST, @c UNMAP_NOTIFY, @c DESTROY_NOTIFY
 *   - handler/focus.c: @c PROPERTY_NOTIFY, @c FOCUS_IN, @c MAPPING_NOTIFY
 *   - handler/expose.c: @c EXPOSE
 *   - handler/message.c: @c CLIENT_MESSAGE dispatcher (dispatches EWMH
 *     and ICCCM protocol messages to handler/ewmhmsg.c's own
 *     per-atom sub-handlers)
 *   - handler/randr.c: RandR extension screen/output change events
 *   - handler/sync.c: XSync extension @c _NET_WM_SYNC_REQUEST alarms
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
