/**
 * @file tests/systray/test_text.c
 *
 * @brief Test battery for systray clock/battery text formatting and
 *        measurement
 *
 * s_tray (systray/internal.h) is the shared, module-level state
 * this whole file operates on; this test file owns the one real
 * instance, the same way test_clients.c owns 'wm'.
 * battery_status_read (systray/battery.c, its own dedicated test
 * elsewhere), text_renderer_use_font and text_string_measure
 * (render/text.c, XCB-backed), and systray_layout_reflow
 * (systray/layout.c, XCB-backed) are all stubbed below as
 * controllable stand-ins.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <string.h>
#include <time.h>

/* Default initial values */
#include <defs/loop.h>

/* Local includes */
#include <surface.h>
#include <harness/tap.h>
#include <systray/internal.h>


struct systray_state_s s_tray;


static const char *s_battery_stub_text = NULL;

void battery_status_read(enum config_battery_backend_type_e backend_type,
        uint32_t backend_number, uint32_t threshold_charged,
        uint32_t threshold_low, uint32_t threshold_critical,
        char *out_text, size_t out_text_size)
{
    (void) backend_type;
    (void) backend_number;
    (void) threshold_charged;
    (void) threshold_low;
    (void) threshold_critical;

    if (s_battery_stub_text != NULL) {
        snprintf(out_text, out_text_size, "%s", s_battery_stub_text);
    } else {
        out_text[0] = '\0';
    }
}


static int s_reflow_calls = 0;

void systray_layout_reflow(void)
{
    s_reflow_calls++;
}


/** Stand-in for text_renderer_use_font: the width below does not
 *  depend on which font is selected, so selecting one always
 *  succeeds and does nothing */
int text_renderer_use_font(xcb_connection_t *connection,
        const char *font)
{
    (void) connection;
    (void) font;
    return 0;
}

/** Controllable stand-in for text_string_measure: returns a fixed
 *  width per character, so this file's width calculations stay
 *  simple and exact to hand-compute */
uint16_t text_string_measure(const char *text)
{
    return (uint16_t) (strlen(text) * 10u);
}


/* systray_text_refresh_clock: disabled leaves the buffer empty */
static void s_test_refresh_clock_disabled(void)
{
    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.clock_enabled = false;
    strcpy(s_tray.clock_text, "stale");

    systray_text_refresh_clock();

    TAP_EQ_STR(s_tray.clock_text, "",
            "a disabled clock is cleared to an empty string");
}


/* systray_text_refresh_clock: enabled formats the current time and
 * records the tick second */
static void s_test_refresh_clock_enabled(void)
{
    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.clock_enabled = true;
    strcpy(s_tray.clock_format, "%H:%M:%S");

    systray_text_refresh_clock();

    TAP_OK(s_tray.clock_text[0] != '\0',
            "an enabled clock produces non-empty formatted text");
    TAP_EQ_INT((long) strlen(s_tray.clock_text), 8,
            "\"%H:%M:%S\" formats to exactly 8 characters");
    TAP_OK(s_tray.clock_last_tick != 0,
            "clock_last_tick is recorded after refreshing");
}


/* systray_text_refresh_battery: disabled leaves the buffer empty,
 * without ever consulting battery_status_read */
static void s_test_refresh_battery_disabled(void)
{
    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.battery_enabled = false;
    strcpy(s_tray.battery_text, "stale");

    systray_text_refresh_battery();

    TAP_EQ_STR(s_tray.battery_text, "",
            "a disabled battery status is cleared to an empty string");
}


/* systray_text_refresh_battery: enabled copies through whatever
 * battery_status_read reports */
static void s_test_refresh_battery_enabled(void)
{
    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.battery_enabled = true;
    s_battery_stub_text = "87%";

    systray_text_refresh_battery();

    TAP_EQ_STR(s_tray.battery_text, "87%",
            "the battery backend's own text is copied through");
    TAP_OK(s_tray.battery_last_poll != 0,
            "battery_last_poll is recorded after refreshing");

    s_battery_stub_text = NULL;
}


/* systray_text_for_item routes BATTERY and everything else (CLOCK)
 * to their own separate buffer and enabled flag */
static void s_test_text_for_item_routing(void)
{
    bool enabled = false;
    const char *text;

    memset(&s_tray, 0, sizeof(s_tray));
    strcpy(s_tray.battery_text, "42%");
    s_tray.battery_enabled = true;
    strcpy(s_tray.clock_text, "10:30");
    s_tray.clock_enabled = false;

    text = systray_text_for_item(CONFIG_SYSTRAY_TEXT_BATTERY, &enabled);
    TAP_EQ_STR(text, "42%", "BATTERY routes to battery_text");
    TAP_OK(enabled, "...with battery's own enabled flag");

    text = systray_text_for_item(CONFIG_SYSTRAY_TEXT_CLOCK, &enabled);
    TAP_EQ_STR(text, "10:30", "CLOCK routes to clock_text");
    TAP_OK(!enabled, "...with clock's own enabled flag");
}


/* systray_text_width sums every enabled, non-empty item's own
 * measured width plus the inter-item gap and padding on both sides;
 * a disabled or empty item contributes nothing at all, not even a
 * gap */
static void s_test_text_width_calculation(void)
{
    uint16_t width;

    memset(&s_tray, 0, sizeof(s_tray));
    strcpy(s_tray.clock_text, "10:30:00");  /* 8 chars -> 80px */
    s_tray.clock_enabled = true;
    strcpy(s_tray.battery_text, "99%");     /* 3 chars -> 30px */
    s_tray.battery_enabled = true;
    s_tray.text_order[0] = CONFIG_SYSTRAY_TEXT_CLOCK;
    s_tray.text_order[1] = CONFIG_SYSTRAY_TEXT_BATTERY;
    s_tray.text_order_count = 2u;
    s_tray.text_gap = 5u;
    s_tray.pixmap_pad = 3u;

    width = systray_text_width();

    /* 80 + gap(5) + 30 + 2*pad(3) = 121 */
    TAP_EQ_INT((long) width, 121,
            "width sums both items, one gap, and padding on both sides");
}


/* systray_text_width: nothing enabled or all text empty yields 0,
 * not just the padding alone */
static void s_test_text_width_nothing_shown(void)
{
    uint16_t width;

    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.text_order[0] = CONFIG_SYSTRAY_TEXT_CLOCK;
    s_tray.text_order_count = 1u;
    s_tray.clock_enabled = false;
    s_tray.pixmap_pad = 3u;

    width = systray_text_width();

    TAP_EQ_INT((long) width, 0,
            "nothing enabled: width is 0, not just the padding");
}


/* systray_clock_ms_remaining: an inactive tray, or one with neither
 * clock nor battery enabled, has nothing to wait for */
static void s_test_ms_remaining_inactive(void)
{
    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.is_active = false;
    TAP_EQ_INT(systray_clock_ms_remaining(), -1,
            "an inactive tray has nothing to wait for");

    s_tray.is_active = true;
    s_tray.clock_enabled = false;
    s_tray.battery_enabled = false;
    TAP_EQ_INT(systray_clock_ms_remaining(), -1,
            "active but nothing enabled: still nothing to wait for");
}


/* systray_clock_ms_remaining: the clock is due (0ms) whenever the
 * wall-clock second has moved past its own last recorded tick */
static void s_test_ms_remaining_clock_due(void)
{
    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.is_active = true;
    s_tray.clock_enabled = true;
    s_tray.clock_last_tick = time(NULL) - 5;  /* stale by 5 seconds */

    TAP_EQ_INT(systray_clock_ms_remaining(), 0,
            "a stale clock tick is immediately due");
}


/* systray_clock_ms_remaining: with the clock already fresh and no
 * battery enabled, falls back to the flat poll interval */
static void s_test_ms_remaining_falls_back_to_poll(void)
{
    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.is_active = true;
    s_tray.clock_enabled = true;
    s_tray.clock_last_tick = time(NULL);  /* fresh: just ticked now */
    s_tray.battery_enabled = false;

    TAP_EQ_INT(systray_clock_ms_remaining(), WM_SYSTRAY_CLOCK_POLL_MS,
            "neither due yet: falls back to the flat poll interval");
}


/* systray_clock_tick refreshes only what is actually due, and
 * reflows the layout exactly once when something changed, not at
 * all when nothing was */
static void s_test_clock_tick_refreshes_and_reflows(void)
{
    memset(&s_tray, 0, sizeof(s_tray));
    s_tray.is_active = true;
    s_tray.clock_enabled = true;
    strcpy(s_tray.clock_format, "%H:%M");
    s_tray.clock_last_tick = time(NULL) - 5;  /* due */
    s_tray.battery_enabled = false;

    s_reflow_calls = 0;
    systray_clock_tick();

    TAP_OK(s_tray.clock_text[0] != '\0',
            "the due clock was actually refreshed");
    TAP_EQ_INT(s_reflow_calls, 1,
            "layout reflowed exactly once after a real change");

    /* Ticking again immediately: nothing is due anymore, no reflow */
    s_reflow_calls = 0;
    systray_clock_tick();
    TAP_EQ_INT(s_reflow_calls, 0,
            "ticking again with nothing due does not reflow");
}


int main(void)
{
    TAP_PLAN(20);

    s_test_refresh_clock_disabled();
    s_test_refresh_clock_enabled();
    s_test_refresh_battery_disabled();
    s_test_refresh_battery_enabled();
    s_test_text_for_item_routing();
    s_test_text_width_calculation();
    s_test_text_width_nothing_shown();
    s_test_ms_remaining_inactive();
    s_test_ms_remaining_clock_due();
    s_test_ms_remaining_falls_back_to_poll();
    s_test_clock_tick_refreshes_and_reflows();

    return TAP_DONE();
}
