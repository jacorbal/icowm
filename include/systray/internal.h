/**
 * @file systray/internal.h
 *
 * @brief Private state and cross-file declarations shared across the
 *        systray subsystem
 *
 * Declares the single @c s_tray instance that is defined in
 * @c systray.c and shared with every implementation file in
 * @c src/systray/, plus the handful of functions each of those files
 * exposes for the others to call.  The tray is one shared dock, not one
 * instance per file, so its state (and the functions that act directly
 * on it without going through a public @c systray_*.h entry point)
 * cannot be split into fully independent modules the way, say,
 * @c menu/dialog/confirm.c and @c menu/dialog/fortune.c each own their
 * own separate state.
 *
 * @note This header is private to the systray subsystem and must not
 *       be included outside of @c src/systray.c and @c src/systray/,
 *       for it is NOT part of the public API
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SYSTRAY_INTERNAL_H
#define SYSTRAY_INTERNAL_H


/* System includes */
#include <stdbool.h>
#include <stdint.h>
#include <time.h>       /* time_t */

/* XCB includes */
#include <xcb/xcb.h>

/* Types includes */
#include <types/pair.h> /* strut_partial_s */

/* Project includes */
#include <config.h>
#include <surface.h>
#include <wm.h>

/* Local includes */
#include <systray.h>


/**
 * @brief One docked icon window
 */
typedef struct {
    xcb_window_t window;    /**< Icon's (reparented) top-level window */
    char sort_key[64];      /**< Best-effort @c WM_CLASS instance name,
                                 used only for the alphabetical order
                                 policies; empty when unavailable, which
                                 sorts before any named icon */
} systray_icon_td;


/**
 * @brief Module-level built-in systray state
 *
 * A single tray instance for the whole window manager, matching
 * @c config.systray being a single global (not per-surface) setting.
 *
 * @note Defined in @c systray.c
 * @note Every implementation file in @c src/systray/ accesses it
 *       through this declaration
 */
struct systray_state_s {
    xcb_connection_t *connection;
    xcb_ewmh_connection_t *ewmh;    /**< For publishing the tray's own
                                         reserved-space strut */
    surface_td *surface;            /**< Surface the tray is docked on */

    const struct config_theme_s *theme; /**< Shared pointer into
                                             @p wm->config->theme; stays
                                             live-updated across
                                             a configuration reload the
                                             same way @p client->theme
                                             does */

    time_t clock_last_tick;     /**< Second the clock was last rendered
                                     for, to redraw at most once per
                                     second */

    time_t battery_last_poll;   /**< Wall-clock time battery state was
                                     last read; polled far less often
                                     than the clock ticks, since
                                     a percentage does not need
                                     per-second freshness and every poll
                                     costs a handful of file reads */

    xcb_window_t window;            /**< Tray dock window */
    xcb_atom_t selection_atom;      /**< @c _NET_SYSTEM_TRAY_Sn */
    xcb_atom_t manager_atom;        /**< @c MANAGER */
    xcb_atom_t opcode_atom;         /**< @c _NET_SYSTEM_TRAY_OPCODE */
    xcb_atom_t orientation_atom;    /**< @c _NET_SYSTEM_TRAY_ORIENTATION */
    xcb_atom_t visual_atom;         /**< @c _NET_SYSTEM_TRAY_VISUAL */
    xcb_atom_t xembed_atom;         /**< @c _XEMBED */
    enum config_systray_position_e position;
    enum config_systray_order_e order;
    enum config_systray_layer_e layer;
    uint32_t battery_threshold_charged;
    uint32_t battery_threshold_low;
    uint32_t battery_threshold_critical;
    enum config_battery_backend_type_e battery_backend_type;
    uint32_t battery_backend_number;
    uint32_t battery_poll_seconds;
    enum config_systray_text_position_e text_position;
    enum config_systray_text_valign_e text_valign;

    struct {
        enum config_systray_monitor_anchor_e anchor;
        uint32_t index;
    } monitor;

    enum config_systray_text_item_e text_order[2];

    struct {
        uint32_t top;
        uint32_t right;
        uint32_t bottom;
        uint32_t left;
    } strut_margins;                /**< Extra space added to the tray's
                                         own computed strut */

    /**
     * @brief Space this tray currently reserves for itself via
     *        @c _NET_WM_STRUT_PARTIAL / @c _NET_WM_STRUT, kept here so
     *        @a desktop_update_workarea can fold it in the same way it
     *        already folds a real client's own published strut, via
     *        @a systray_get_reserved_strut
     *
     * Every side left at zero (the same all-zero shape @c memset leaves
     * this in before the tray is ever positioned) when the tray is
     * unmapped (disabled, empty, or another tray manager owns the
     * selection) reserving nothing in that case, same as not existing
     * at all, as if it's erased from existence.
     */
    struct strut_partial_s reserved_strut;

    systray_icon_td icons[WM_SYSTRAY_MAX_ICONS];
    uint16_t height;
    uint16_t pixmap_size; /**< @see @p config.theme.systray.pixmap.size */
    uint16_t pixmap_pad;  /**< @see @p config.theme.systray.pixmap.padding */
    uint16_t text_gap;
    uint16_t icon_count;

    bool is_window_ready;           /**< Window created, atoms interned;
                                         persists across is-enabled
                                         toggles so docked icons are
                                         never evicted just because the
                                         tray was disabled */

    bool is_active;                 /**< Whether the tray should be
                                         showing anything at all right
                                         now (@p is-enabled, kept in
                                         sync across a reload); gates
                                         the window's own visibility
                                         and the clock/battery text
                                         refresh, independent of
                                         @p is_selection_owned, so a
                                         restricted-memory session
                                         (which never sets that) still
                                         shows its own status text */

    /**
     * @brief Currently owns the @c _NET_SYSTEM_TRAY_Sn selection
     *
     * Gates accepting new dock requests only, not the window's own
     * visibility.
     *
     * @note Never even attempted at all when
     *       @p config_td.base.systray.is_embedding_enabled is @c false
     */
    bool is_selection_owned;

    bool reserve_space;             /**< Whether the tray publishes its
                                         own strut */
    bool clock_enabled;
    bool battery_enabled;
    uint8_t text_order_count;
    char battery_text[32];      /**< Last rendered battery status */
    char clock_text[64];        /**< Last rendered clock text */
    char clock_format[CONFIG_MAX_LENGTH_NAME];
};

extern struct systray_state_s s_tray;


/* 'systray/text.c' */

/**
 * @brief Format the current local time into @a s_tray.clock_text
 *
 * @note A no-op when the clock is disabled
 * @note Called once up front and again every time @a systray_clock_tick
 *       observes the second has changed
 */
void systray_text_refresh_clock(void);

/**
 * @brief Read and format the current battery status into
 *        @a s_tray.battery_text
 *
 * @note A no-op when the battery status is disabled
 *
 * @see @a battery_status_read for what the formatted text can look like
 */
void systray_text_refresh_battery(void);

/**
 * @brief Pixel width needed for every active @p systray.text.order item
 *        combined, padding and inter-item gaps included
 *
 * @return @c 0 when nothing in @p systray.text.order is both listed and
 *         enabled with non-empty text
 */
uint16_t systray_text_width(void);

/**
 * @brief The already-formatted text buffer and enabled flag for one
 *        @p systray.text.order item
 *
 * @param item        Which item to look up
 * @param out_enabled Receives whether that item is currently enabled
 *
 * @return Pointer to that item's own null-terminated text buffer
 *
 * @note Complexity: @e O(1)
 */
const char *systray_text_for_item(enum config_systray_text_item_e item,
        bool *out_enabled);


/* systray/layout.c */

/**
 * @brief Apply the configured @c systray.layer stacking rule
 *
 * @note Safe to call whenever the tray's stacking might need
 *       reconsidering
 */
void systray_layout_restack(void);


/* systray/protocol.c */

/**
 * @brief Create the tray window and intern its atoms, once!
 *
 * @param wm Window manager state
 *
 * @return Status of the operation
 * @retval  true on success (or if already ready)
 * @retval false if it could not be created
 *
 * @note Does not acquire the selection
 * @note Idempotent; does nothing (beyond returning success) if
 *       @p s_tray.is_window_ready is already @c true
 *
 * @see @p systray_protocol_selection_acquire
 */
bool systray_protocol_window_ensure(const wm_td *wm);

/**
 * @brief Acquire the tray selection on the already-created window
 *
 * @return Status of the operation
 * @retval  true if ownership was acquired (or already held),
 * @retval false if another tray manager already owns the selection or
 *               the window is not ready yet
 */
bool systray_protocol_selection_acquire(void);

/**
 * @brief Release the tray selection, keeping the window and icons
 *
 * The tray window and any currently docked icons are left exactly as
 * they are, just hidden.
 *
 * @note Safe to call when the selection is not currently owned
 */
void systray_protocol_selection_release(void);

/**
 * @brief Dock an icon window: reparent it in, embed it, and reflow
 *
 * @param icon Icon window named by a @c SYSTEM_TRAY_REQUEST_DOCK
 *             request
 */
void systray_protocol_dock(xcb_window_t icon);

/**
 * @brief Re-apply the theme's background color, border color, and
 *        border width to the already-existing tray window
 *
 * @note A no-op if the window does not exist yet or there is no theme
 *       to read from
 */
void systray_protocol_apply_theme_style(void);

/**
 * @brief Re-sort every already-docked icon by the current
 *        @p s_tray.order policy
 *
 * @note A no-op for @c CONFIG_SYSTRAY_ORDER_LEFT_TO_RIGHT and
 *       @c CONFIG_SYSTRAY_ORDER_RIGHT_TO_LEFT
 */
void systray_protocol_resort(void);


#endif  /* ! SYSTRAY_INTERNAL_H */
