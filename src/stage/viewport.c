/**
 * @file stage/viewport.c
 *
 * @brief Configured pannable-viewport size queries for a stage
 *
 * One of the files @c stage/ is made of; see
 * @c stage.c's comment for why.
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
#include <stage.h>
#include <stage/viewport.h>


/* Read the configured viewport size for a stage's screen */
void stage_viewport_dims(const stage_td *stage,
        uint32_t *columns_out, uint32_t *rows_out)
{
    if (columns_out == NULL || rows_out == NULL) {
        return;
    }

    *columns_out = 1u;
    *rows_out = 1u;

    if (stage != NULL && stage->config != NULL &&
            stage->id < (uint32_t) CONFIG_MAX_SCREENS) {
        *columns_out = stage->config->base.screens[stage->id]
            .viewport.columns;
        *rows_out = stage->config->base.screens[stage->id]
            .viewport.rows;
    }
}


/* Whether the configured viewport spans more than a single screen
 * along either axis */
bool stage_viewport_has_room(const stage_td *stage)
{
    uint32_t columns;
    uint32_t rows;

    stage_viewport_dims(stage, &columns, &rows);

    return columns > 1u || rows > 1u;
}
