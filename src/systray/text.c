/**
 * @file systray/text.c
 *
 * @brief Clock and battery status text: reading, formatting, and
 *        measuring the text shown alongside docked icons
 *
 * Only the text content itself lives here: reading the system clock and
 * battery state into @a s_tray.clock_text / @a s_tray.battery_text, and
 * measuring how much pixel width the currently enabled items need.
 * Actually drawing that text on screen is part of the tray's overall
 * visual layout instead; see @c systray/layout.c.
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
#include <stdint.h>
#include <time.h>       /* strftime, localtime, time, NULL */

/* Default initial values */
#include <defs/loop.h>

/* Project includes */
#include <render/text.h>

#include <xcb/xcb_ewmh.h>

/* Local includes */
#include <systray/battery.h>
#include <systray/internal.h>


/* Format the current local time into 's_tray.clock_text' */
void systray_text_refresh_clock(void)
{
    time_t now;
    const struct tm *local;

    if (!s_tray.clock_enabled) {
        s_tray.clock_text[0] = '\0';
        return;
    }

    now = time(NULL);
    local = localtime(&now);
    if (local == NULL) {
        s_tray.clock_text[0] = '\0';
        return;
    }

    if (strftime(s_tray.clock_text, sizeof(s_tray.clock_text),
            s_tray.clock_format, local) == 0u) {
        s_tray.clock_text[0] = '\0';
    }

    s_tray.clock_last_tick = now;
}


/* Read and format the battery status into 's_tray.battery_text' */
void systray_text_refresh_battery(void)
{
    if (!s_tray.battery_enabled) {
        s_tray.battery_text[0] = '\0';
        return;
    }

    battery_status_read(s_tray.battery_backend_type,
            s_tray.battery_backend_number,
            s_tray.battery_threshold_charged,
            s_tray.battery_threshold_low,
            s_tray.battery_threshold_critical,
            s_tray.battery_text, sizeof(s_tray.battery_text));

    s_tray.battery_last_poll = time(NULL);
}


/* The already-formatted text buffer and enabled flag for one
 * 'systray.text.order' item */
const char *systray_text_for_item(enum config_systray_text_item_e item,
        bool *out_enabled)
{
    if (item == CONFIG_SYSTRAY_TEXT_BATTERY) {
        if (out_enabled != NULL) {
            *out_enabled = s_tray.battery_enabled;
        }
        return s_tray.battery_text;
    }

    if (out_enabled != NULL) {
        *out_enabled = s_tray.clock_enabled;
    }
    return s_tray.clock_text;
}


/* Pixel width needed for every active 'systray.text.order' item
 * combined, padding and inter-item gaps included */
uint16_t systray_text_width(void)
{
    uint16_t total = 0u;
    uint16_t shown = 0u;

    if (s_tray.theme != NULL) {
        (void) text_renderer_use_font(s_tray.connection,
                s_tray.theme->systray.style.font);
    }

    for (uint8_t i = 0u; i < s_tray.text_order_count; ++i) {
        bool enabled = false;
        const char *text = systray_text_for_item(s_tray.text_order[i],
                &enabled);

        if (!enabled || text[0] == '\0') {
            continue;
        }
        if (shown > 0u) {
            total = (uint16_t) (total + s_tray.text_gap);
        }
        total = (uint16_t) (total + text_string_measure(text));
        ++shown;
    }

    if (shown == 0u) {
        return 0u;
    }

    return (uint16_t) (total + 2u * s_tray.pixmap_pad);
}


/* How many milliseconds until the systray text (clock and/or battery)
 * needs its next redraw */
int systray_clock_ms_remaining(void)
{
    time_t now;

    if (!s_tray.is_active ||
            (!s_tray.clock_enabled && !s_tray.battery_enabled)) {
        return -1;
    }

    now = time(NULL);

    if (s_tray.clock_enabled && now != s_tray.clock_last_tick) {
        return 0;
    }
    if (s_tray.battery_enabled &&
            (now - s_tray.battery_last_poll) >=
                (time_t) s_tray.battery_poll_seconds) {
        return 0;
    }

    /* Neither is due yet.  A flat, small poll timeout is used instead
     * of computing the exact remaining fraction of a second: good
     * enough for a display that only needs second-level precision for
     * the clock (the battery's own interval is far coarser still), and
     * simpler than reasoning about clock skew between 'time(NULL)'
     * calls. */
    return WM_SYSTRAY_CLOCK_POLL_MS;
}


/* Redraw the clock and/or battery status, whichever is due */
void systray_clock_tick(void)
{
    time_t now;
    bool changed = false;

    if (!s_tray.is_active ||
            (!s_tray.clock_enabled && !s_tray.battery_enabled)) {
        return;
    }

    now = time(NULL);

    if (s_tray.clock_enabled && now != s_tray.clock_last_tick) {
        systray_text_refresh_clock();
        changed = true;
    }
    if (s_tray.battery_enabled &&
            (now - s_tray.battery_last_poll) >=
                (time_t) s_tray.battery_poll_seconds) {
        systray_text_refresh_battery();
        changed = true;
    }

    if (changed) {
        systray_layout_reflow();
    }
}
