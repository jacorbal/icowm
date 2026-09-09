/**
 * @file surface/viewport.c
 *
 * @brief Configured pannable-viewport size queries for a surface
 *
 * One of the files @c surface/ is made of; see
 * @c surface.c's comment for why.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdbool.h>
#include <stddef.h>     /* NULL */
#include <stdint.h>

/* Defs includes */
#include <defs/config.h>  /* CONFIG_MAX_SCREENS */

/* Project includes */
#include <config.h>

/* Local includes */
#include <surface.h>


/* Read the configured viewport size for a surface's screen */
void surface_viewport_dims(const surface_td *surface,
        uint32_t *columns_out, uint32_t *rows_out)
{
    if (columns_out == NULL || rows_out == NULL) {
        return;
    }

    *columns_out = 1u;
    *rows_out = 1u;

    if (surface != NULL && surface->config != NULL &&
            surface->id < (uint32_t) CONFIG_MAX_SCREENS) {
        *columns_out = surface->config->base.screens[surface->id]
            .viewport.columns;
        *rows_out = surface->config->base.screens[surface->id]
            .viewport.rows;
    }
}


/* Whether the configured viewport spans more than a single screen
 * along either axis */
bool surface_viewport_has_room(const surface_td *surface)
{
    uint32_t columns;
    uint32_t rows;

    surface_viewport_dims(surface, &columns, &rows);

    return columns > 1u || rows > 1u;
}
