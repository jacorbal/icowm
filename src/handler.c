/**
 * @file handler.c
 *
 * @brief X event handler implementations
 *
 * The original monolithic handler has been divided into four translation
 * units, each responsible for one family of X events:
 *
 *   - handler/configure.c: @c CONFIGURE_REQUEST, @c CONFIGURE_NOTIFY
 *   - handler/map.c: @c MAP_REQUEST, @c UNMAP_NOTIFY, @c DESTROY_NOTIFY
 *   - handler/focus.c: @c PROPERTY_NOTIFY, @c FOCUS_IN, @c MAPPING_NOTIFY
 *   - handler/expose.c: @c EXPOSE
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
