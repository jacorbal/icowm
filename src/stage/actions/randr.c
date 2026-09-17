/**
 * @file stage/actions/randr.c
 *
 * @brief RandR output, CRTC, mode, and profile management for a stage
 *
 * One of the files @c stage/actions/ is made of; everything here
 * revolves around the RandR extension itself (output lookup, CRTC
 * allocation, mode matching, profile snapshot/apply/ revert), as
 * opposed to @c stage/actions/client.c's client show/hide/reflow
 * concerns, which never touch RandR directly.
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
#include <stdlib.h>     /* free */
#include <string.h>     /* memcpy, memset, NULL */
#include <strings.h>    /* strcasecmp */

/* XCB includes */
#include <xcb/xcb.h>
#include <xcb/randr.h>

/* Utils includes */
#include <utils/geom.h>
#include <utils/xcb/reply.h>
#include <utils/xcb/connection.h>

/* Project includes */
#include <client.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <stage.h>
#include <stage/action.h>
#include <stage/monitor.h>


/**
 * @brief One CRTC's state, as it was immediately before
 *        @a stage_action_randr_apply_profiles changed it, so
 *        @a stage_action_randr_revert_profiles can restore exactly
 *        that afterward
 */
struct stage_randr_snapshot_s {
    stage_td *stage;            /**< Stage this CRTC belongs to;
                                         entries for different stages
                                         may sit side by side in the
                                         same array, one config reload
                                         being free to touch more than
                                         one */
    xcb_randr_crtc_t crtc;
    xcb_randr_mode_t prior_mode;    /**< @c XCB_NONE means this CRTC was
                                         off (driving nothing) before;
                                         reverting restores that, not
                                         any particular prior mode */
    xcb_randr_output_t output_id;
    int16_t prior_x;
    int16_t prior_y;
    uint16_t prior_rotation;
};

/** Every CRTC @a stage_action_randr_apply_profiles actually changed
 *  since the last @a stage_action_randr_snapshot_begin, across every
 *  stage a single config reload called it for, in application order;
 *  entries belonging to the same stage stay contiguous, since every
 *  one of a stage's own CRTCs is snapshotted in a single call before
 *  the next stage's own call ever runs */
static struct stage_randr_snapshot_s
    s_randr_snapshot[CONFIG_RANDR_MAX_OUTPUTS];

/** Number of valid entries in @a s_randr_snapshot */
static uint32_t s_randr_snapshot_count = 0u;

/**
 * @brief One stage's primary output, as it was immediately before
 *        @a stage_action_randr_apply_profiles changed it
 */
struct stage_randr_primary_snapshot_s {
    stage_td *stage;
    xcb_randr_output_t prior_primary;
};

/**
 * @brief Every stage whose primary output
 *        @a stage_action_randr_apply_profiles actually changed since
 *        the last @a stage_action_randr_snapshot_begin; capped the
 *        same way @a s_randr_snapshot's own stages are bounded, one
 *        entry per stage rather than per CRTC
 */
static struct stage_randr_primary_snapshot_s
    s_randr_primary_snapshot[CONFIG_MAX_SCREENS];

/** Number of valid entries in @a s_randr_primary_snapshot */
static uint32_t s_randr_primary_snapshot_count = 0u;


/**
 * @brief Find the RandR output whose name matches a configured
 *        profile's, among those the screen currently reports
 *
 * Unlike the RandR 1.5 monitor list (@a stage_monitor_refresh_all
 * itself), where each entry's name is an X atom, the older per-output
 * API used here returns its name as plain bytes directly.
 *
 * @param connection XCB connection
 * @param res_reply  Already-fetched current screen resources
 * @param name       Output name to match (e.g., "HDMI-1"), compared
 *                      case-insensitively
 * @param out_output_id Receives the matching output, only when one is
 *                      found
 *
 * @return That output's info (caller's to free), or @c NULL if none
 *         of the screen's outputs currently has this name
 *
 * @note No atom resolution needed
 * @note Complexity: @e O(n), where @e n is the number of outputs the
 *       screen currently reports
 */
static xcb_randr_get_output_info_reply_t *
s_stage_randr_find_output_by_name(xcb_connection_t *connection,
        xcb_randr_get_screen_resources_current_reply_t *res_reply,
        const char *name, xcb_randr_output_t *out_output_id)
{
    int output_count;
    xcb_randr_output_t *outputs;
    /* Declared once here, outside the loop, rather than once per
     * iteration inside it. */
    char output_name[CONFIG_RANDR_OUTPUT_NAME_LENGTH] = {0};

    output_count =
        xcb_randr_get_screen_resources_current_outputs_length(res_reply);
    outputs =
        xcb_randr_get_screen_resources_current_outputs(res_reply);

    for (int i = 0; i < output_count; ++i) {
        xcb_randr_get_output_info_cookie_t info_cookie;
        xcb_randr_get_output_info_reply_t *info_reply;
        int name_len;
        const uint8_t *name_bytes;

        info_cookie = xcb_randr_get_output_info(connection, outputs[i],
                res_reply->config_timestamp);
        info_reply = xcb_randr_get_output_info_reply(connection,
                info_cookie, NULL);
        if (info_reply == NULL) {
            continue;
        }

        name_len = xcb_randr_get_output_info_name_length(info_reply);
        name_bytes = xcb_randr_get_output_info_name(info_reply);
        if (name_len < 0) {
            name_len = 0;
        }
        if ((size_t) name_len >= sizeof(output_name)) {
            name_len = (int) sizeof(output_name) - 1;
        }
        /* Re-zeroed on every iteration reusing this same array, so
         * a shorter name this time around can never leave a longer
         * previous iteration's trailing bytes still in place past
         * 'name_len'. */
        memset(output_name, 0, sizeof(output_name));
        if (name_len > 0) {
            memcpy(output_name, name_bytes, (size_t) name_len);
        }

        if (strcasecmp(output_name, name) == 0) {
            *out_output_id = outputs[i];
            return info_reply;
        }
        free(info_reply);
    }

    return NULL;
}


/**
 * @brief Find a free CRTC compatible with a given output
 *
 * Only needed when the output has none assigned yet (@p info->crtc is
 * @c XCB_NONE).  Tries every CRTC XRandR lists as compatible with this
 * specific output, in the order given, and returns the first one
 * currently driving no output at all.
 *
 * @param connection XCB connection
 * @param info       That output's info, already fetched
 * @param timestamp  Config timestamp from the same screen-resources
 *                   query @p info itself came from
 *
 * @return A free, compatible CRTC, or @c XCB_NONE if none is free
 *
 * @note Complexity: @e O(c), where @e c is the number of CRTCs
 *       compatible with this output
 */
static xcb_randr_crtc_t s_stage_randr_find_free_crtc(
        xcb_connection_t *connection,
        xcb_randr_get_output_info_reply_t *info,
        xcb_timestamp_t timestamp)
{
    int crtc_count;
    xcb_randr_crtc_t *crtcs;

    crtc_count = xcb_randr_get_output_info_crtcs_length(info);
    crtcs = xcb_randr_get_output_info_crtcs(info);

    for (int i = 0; i < crtc_count; ++i) {
        xcb_randr_get_crtc_info_cookie_t ci_cookie;
        xcb_randr_get_crtc_info_reply_t *ci_reply;
        bool is_free;

        ci_cookie = xcb_randr_get_crtc_info(connection, crtcs[i],
                timestamp);
        ci_reply = xcb_randr_get_crtc_info_reply(connection, ci_cookie,
                NULL);
        if (ci_reply == NULL) {
            continue;
        }
        is_free = ci_reply->num_outputs == 0u;
        free(ci_reply);

        if (is_free) {
            return crtcs[i];
        }
    }

    return (xcb_randr_crtc_t) XCB_NONE;
}


/**
 * @brief Find a RandR mode matching a given resolution
 *
 * Same lookup @a stage_action_set_resolution does against the
 * screen's mode list, extracted here so applying a per-output profile
 * can reuse the exact same technique.
 *
 * @param res_reply  Already-fetched current screen resources
 * @param resolution Resolution to match; either dimension @c 0 always
 *                   misses, since that means "not configured" rather
 *                   than a literal 0x0 mode to look for
 *
 * @return The matching mode, or @c XCB_NONE if none of the screen's
 *         modes has this exact resolution
 *
 * @note Complexity: @e O(n), where @e n is the number of modes the
 *       screen currently reports
 */
static xcb_randr_mode_t s_stage_randr_find_mode(
        xcb_randr_get_screen_resources_current_reply_t *res_reply,
        struct dimensions_s resolution)
{
    int nmodes;
    xcb_randr_mode_info_t *modes;

    if (resolution.w == 0u || resolution.h == 0u) {
        return (xcb_randr_mode_t) XCB_NONE;
    }

    nmodes =
        xcb_randr_get_screen_resources_current_modes_length(res_reply);
    modes = xcb_randr_get_screen_resources_current_modes(res_reply);

    for (int i = 0; i < nmodes; ++i) {
        if (modes[i].width == (uint16_t) resolution.w &&
                modes[i].height == (uint16_t) resolution.h) {
            return modes[i].id;
        }
    }

    return (xcb_randr_mode_t) XCB_NONE;
}


/**
 * @brief Save a CRTC's current state into the next free
 *        @a s_randr_snapshot slot, if there is room
 *
 * At most one snapshot per configured profile, itself already bounded
 * to that same limit) or when @p take_snapshot is @c false, so every
 * call site can pass it unconditionally rather than guarding each one
 * individually.
 *
 * @param take_snapshot Whether snapshotting is active for this call
 *                       to @a stage_action_randr_apply_profiles at
 * @param stage      Stage @p crtc belongs to
 * @param crtc       all CRTC being changed
 * @param output_id  Output it drives
 * @param prior_mode Its mode immediately before the change,
 *                       @c XCB_NONE if it was off
 * @param prior_x        Its X position immediately before the change
 * @param prior_y        Its Y position immediately before the change
 * @param prior_rotation Its rotation immediately before the change
 *
 * @note A no-op past @c CONFIG_RANDR_MAX_OUTPUTS entries (cannot happen
 *       in practice)
 * @note Complexity: @e O(1)
 */
static void s_stage_randr_snapshot_save(bool take_snapshot,
        stage_td *stage, xcb_randr_crtc_t crtc,
        xcb_randr_output_t output_id, xcb_randr_mode_t prior_mode,
        int16_t prior_x, int16_t prior_y, uint16_t prior_rotation)
{
    struct stage_randr_snapshot_s *slot;

    if (!take_snapshot ||
            s_randr_snapshot_count >=
                    (uint32_t) CONFIG_RANDR_MAX_OUTPUTS) {
        return;
    }

    slot = &s_randr_snapshot[s_randr_snapshot_count];
    slot->stage = stage;
    slot->crtc = crtc;
    slot->output_id = output_id;
    slot->prior_mode = prior_mode;
    slot->prior_x = prior_x;
    slot->prior_y = prior_y;
    slot->prior_rotation = prior_rotation;
    ++s_randr_snapshot_count;
}


/**
 * @brief Turn an output's CRTC off, blanking it
 *
 * The disabled-profile half of @a s_stage_randr_apply_profile.
 * A plain @a xcb_randr_set_crtc_config with no mode and no outputs
 * detaches this CRTC from the output entirely, physically disabling it,
 * the same as unplugging it (though the connector itself stays
 * electrically live, so the monitor may still report as connected).
 *
 * @param stage Stage the output belongs to, for its
 *                      connection and for logging
 * @param res_reply     Already-fetched current screen resources
 * @param crtc          The output's currently-assigned CRTC
 * @param output_id     Output @p crtc drives, for the snapshot only
 * @param name          Output name, for logging only
 * @param take_snapshot Whether to save this CRTC's state first,
 *                      so @a stage_action_randr_revert_profiles can
 *                      turn it back on exactly as it was
 *
 * @note Complexity: @e O(1)
 *
 * @see @a s_stage_randr_snapshot_save
 */
static bool s_stage_randr_blank_crtc(stage_td *stage,
        xcb_randr_get_screen_resources_current_reply_t *res_reply,
        xcb_randr_crtc_t crtc, xcb_randr_output_t output_id,
        const char *name, bool take_snapshot)
{
    xcb_randr_get_crtc_info_cookie_t ci_cookie;
    xcb_randr_set_crtc_config_cookie_t cfg_cookie;
    xcb_randr_set_crtc_config_reply_t *cfg_reply;
    bool ok;

    if (take_snapshot) {
        xcb_randr_get_crtc_info_reply_t *ci_reply;

        ci_cookie = xcb_randr_get_crtc_info(xcb_connection_get(), crtc,
                res_reply->config_timestamp);
        ci_reply = xcb_randr_get_crtc_info_reply(xcb_connection_get(),
                ci_cookie, NULL);
        if (ci_reply != NULL) {
            s_stage_randr_snapshot_save(true, stage, crtc, output_id,
                    ci_reply->mode, ci_reply->x, ci_reply->y,
                    ci_reply->rotation);
        }
        free(ci_reply);
    }

    cfg_cookie = xcb_randr_set_crtc_config(xcb_connection_get(), crtc,
            XCB_CURRENT_TIME, res_reply->config_timestamp, 0, 0,
            (xcb_randr_mode_t) XCB_NONE,
            (uint16_t) XCB_RANDR_ROTATION_ROTATE_0, 0u, NULL);
    cfg_reply = xcb_randr_set_crtc_config_reply(xcb_connection_get(),
            cfg_cookie, NULL);

    ok = cfg_reply != NULL &&
        cfg_reply->status == XCB_RANDR_SET_CONFIG_SUCCESS;
    if (!ok) {
        LOGGER_WARNING("XRandR: failed to blank output '%s'" \
                " (its configured profile is disabled) on stage %u",
                name, stage->id);
    } else {
        LOGGER_NOTICE("XRandR: output '%s' blanked (its configured" \
                " profile is disabled)", name);
    }
    free(cfg_reply);
    return ok;
}


/**
 * @brief Clamp an @c int32_t coordinate to the @c int16_t range
 *        @a xcb_randr_set_crtc_config itself requires
 *
 * A configured position genuinely outside this range does not fit any
 * real display layout, but clamping instead of a plain cast (which
 * would silently wrap to an unrelated, even more nonsensical value)
 * keeps an absurd configured value from producing one.
 *
 * @param value       Value to clamp
 * @param axis        Axis name ("x" or "y"), for the log message only
 * @param output_name Output name, for the log message only
 *
 * @return @p value clamped to [@c INT16_MIN, @c INT16_MAX]
 *
 * @note Complexity: @e O(1)
 */
static int16_t s_stage_randr_clamp_position(int32_t value,
        const char *restrict axis, const char *restrict output_name)
{
    if (value < (int32_t) INT16_MIN || value > (int32_t) INT16_MAX) {
        LOGGER_WARNING("XRandR: configured position.%s=%d for output" \
                " '%s' is out of range; clamped to %d",
                axis, (int) value, output_name,
                (value < (int32_t) INT16_MIN)
                    ? (int) INT16_MIN : (int) INT16_MAX);
    }

    return (int16_t) ((value < (int32_t) INT16_MIN) ? INT16_MIN
            : (value > (int32_t) INT16_MAX) ? INT16_MAX : value);
}


/**
 * @brief Apply one configured, enabled RandR output profile to its
 *        matching, currently-connected output
 *
 * Reuses the output's current CRTC if it already has one, or claims
 * a free compatible one otherwise.
 *
 * Resolution is taken from the profile if configured and a matching
 * mode exists; otherwise the CRTC's already-active mode is kept,
 * falling back to the output's first preferred mode if it had none (a
 * freshly-claimed CRTC on an output with no prior mode of its own).
 * Position and rotation always come straight from the profile.
 * 'is_primary' is applied as a separate, independent request afterward,
 * since RandR has no way to bundle it into the same one.
 *
 * @param stage Stage the output belongs to, for its
 *                        connection and for logging
 * @param res_reply Already-fetched current screen resources,
 *                        shared across every profile one call batch
 *                        applies
 * @param output_id       Output this profile matched by name
 * @param info            That output's info, already fetched
 * @param profile         Configured profile to apply
 * @param current_primary Output the X server currently calls primary,
 *                        so that a profile asking to be primary only
 *                        issues the request where it is not already
 * @param take_snapshot Whether to save the CRTC's prior state first,
 *                        so @a stage_action_randr_revert_profiles can
 *                        put it back exactly as it was
 *
 * @note Complexity: @e O(c), where @e c is the number of CRTCs
 *       compatible with this output, only when it has none active yet
 */
static bool s_stage_randr_apply_profile(stage_td *stage,
        xcb_randr_get_screen_resources_current_reply_t *res_reply,
        xcb_randr_output_t output_id,
        xcb_randr_get_output_info_reply_t *info,
        const struct config_randr_output_s *profile,
        xcb_randr_output_t current_primary, bool take_snapshot)
{
    xcb_randr_crtc_t crtc;
    xcb_randr_crtc_t prior_crtc;
    xcb_randr_mode_t mode;
    bool mode_was_configured;
    xcb_randr_get_crtc_info_cookie_t ci_cookie;
    xcb_randr_get_crtc_info_reply_t *ci_reply;
    xcb_randr_set_crtc_config_cookie_t cfg_cookie;
    int16_t pos_x;
    int16_t pos_y;
    bool crtc_matches_current;
    bool changed = false;

    prior_crtc = info->crtc;
    crtc = (prior_crtc != (xcb_randr_crtc_t) XCB_NONE) ? prior_crtc
        : s_stage_randr_find_free_crtc(xcb_connection_get(), info,
                res_reply->config_timestamp);
    if (crtc == (xcb_randr_crtc_t) XCB_NONE) {
        LOGGER_WARNING("XRandR: no free CRTC compatible with output" \
                " '%s' on stage %u; cannot apply its profile",
                profile->name, stage->id);
        return false;
    }

    /* Fetched once up front, regardless of whether a resolution was
     * even configured: needed both for the "keep whatever mode is
     * already active" fallback below and, more importantly, to compare
     * the profile's desired state against this CRTC's actual current
     * one, so an already-matching profile issues no XRandR write at all
     * (see 'crtc_matches_current' below) rather than reasserting an
     * identical configuration on every reload. */
    ci_cookie = xcb_randr_get_crtc_info(xcb_connection_get(), crtc,
            res_reply->config_timestamp);
    ci_reply = xcb_randr_get_crtc_info_reply(xcb_connection_get(),
            ci_cookie, NULL);

    mode = s_stage_randr_find_mode(res_reply, profile->preferred_res);
    /* Whether the profile itself actually resolved to a specific mode,
     * as opposed to the "keep whatever is already active" fallback
     * taken below.
     *
     * 'crtc_matches_current' needs to know this, since deliberately
     * mirroring 'ci_reply->mode' back into 'mode' just below would
     * otherwise make that comparison match by construction forever
     * after the first real apply, hiding every later change to this
     * same profile's position or rotation that arrives without ever
     * specifying a resolution of its own (see this function's comment
     * for the fuller explanation).
     *
     * No resolution requested means no resolution change to detect, so
     * its comparison simply does not apply. */
    mode_was_configured = mode != (xcb_randr_mode_t) XCB_NONE;
    if (!mode_was_configured) {
        /* No resolution configured, or none of the screen's modes
         * matches it exactly: keep the CRTC's already-active
         * mode instead of forcing a guess */
        mode = (ci_reply != NULL)
            ? ci_reply->mode : (xcb_randr_mode_t) XCB_NONE;

        if (mode == (xcb_randr_mode_t) XCB_NONE &&
                info->num_preferred > 0u) {
            const xcb_randr_mode_t *info_modes =
                xcb_randr_get_output_info_modes(info);
            mode = info_modes[0];
        }
    }

    if (mode == (xcb_randr_mode_t) XCB_NONE) {
        LOGGER_WARNING("XRandR: no usable mode for output '%s' on" \
                " stage %u; cannot apply its profile",
                profile->name, stage->id);
        free(ci_reply);
        return false;
    }

    pos_x = s_stage_randr_clamp_position(profile->position.x, "x",
            profile->name);
    pos_y = s_stage_randr_clamp_position(profile->position.y, "y",
            profile->name);

    /* Only meaningful when this CRTC was already driving this same
     * output before this call (a freshly claimed, previously-free CRTC
     * is by definition a real change.
     *
     * It was driving nothing at all); comparing mode (only when the
     * profile actually configured one; see 'mode_was_configured'
     * above)/position/rotation against its already-active configuration
     * is what lets an unchanged 'randr.json' profile skip the write
     * entirely on every reload instead of reasserting an identical
     * configuration each time. */
    crtc_matches_current =
        (prior_crtc != (xcb_randr_crtc_t) XCB_NONE) &&
        ci_reply != NULL &&
        (!mode_was_configured || ci_reply->mode == mode) &&
        ci_reply->x == pos_x &&
        ci_reply->y == pos_y &&
        ci_reply->rotation == profile->rotation;

    if (!crtc_matches_current) {
        /* Saved from 'ci_reply' (this CRTC's real prior state) when it
         * already had one, or as "was off" when it did not (a freshly
         * claimed CRTC, 'prior_crtc == XCB_NONE').  Either way exactly
         * what 'stage_action_randr_revert_profiles' needs to put
         * back. */
        if (prior_crtc != (xcb_randr_crtc_t) XCB_NONE && ci_reply != NULL) {
            s_stage_randr_snapshot_save(take_snapshot, stage, crtc,
                    output_id, ci_reply->mode, ci_reply->x, ci_reply->y,
                    ci_reply->rotation);
        } else {
            s_stage_randr_snapshot_save(take_snapshot, stage, crtc,
                    output_id, (xcb_randr_mode_t) XCB_NONE, 0, 0,
                    (uint16_t) XCB_RANDR_ROTATION_ROTATE_0);
        }
    }
    free(ci_reply);

    if (!crtc_matches_current) {
        xcb_randr_set_crtc_config_reply_t *cfg_reply;

        cfg_cookie = xcb_randr_set_crtc_config(xcb_connection_get(), crtc,
                XCB_CURRENT_TIME, res_reply->config_timestamp,
                pos_x, pos_y, mode,
                profile->rotation, 1u, &output_id);
        cfg_reply = xcb_randr_set_crtc_config_reply(xcb_connection_get(),
                cfg_cookie, NULL);

        if (cfg_reply == NULL ||
                cfg_reply->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
            LOGGER_WARNING("XRandR: failed to apply profile to" \
                    " output '%s' on stage %u",
                    profile->name, stage->id);
            free(cfg_reply);
            return false;
        }
        free(cfg_reply);

        LOGGER_NOTICE("XRandR: applied profile to output '%s' on" \
                " stage %u (mode=%u, pos=%+d%+d, rot=%u)",
                profile->name, stage->id, (unsigned int) mode,
                profile->position.x, profile->position.y,
                (unsigned int) profile->rotation);
        changed = true;
    }

    if (profile->is_primary && output_id != current_primary) {
        xcb_randr_set_output_primary(xcb_connection_get(),
                stage->screen->root, output_id);
        LOGGER_NOTICE("XRandR: marked output '%s' as primary on" \
                " stage %u", profile->name, stage->id);
        changed = true;
    }

    return changed;
}


/* Clear every RandR snapshot from a previous config reload, so the one
 * about to start accumulates only its own stages' changes */
void stage_action_randr_snapshot_begin(void)
{
    s_randr_snapshot_count = 0u;
    s_randr_primary_snapshot_count = 0u;
}


/* Apply every configured RandR output profile that matches
 * a currently-connected output */
bool stage_action_randr_apply_profiles(stage_td *stage,
        bool take_snapshot)
{
    xcb_randr_get_screen_resources_current_cookie_t res_cookie;
    xcb_randr_get_screen_resources_current_reply_t *res_reply;
    xcb_randr_get_output_primary_cookie_t primary_cookie;
    xcb_randr_get_output_primary_reply_t *primary_reply;
    xcb_generic_error_t *res_error = NULL;
    xcb_generic_error_t *primary_error = NULL;
    xcb_randr_output_t current_primary;
    bool any_changed = false;

    if (stage == NULL || xcb_connection_get() == NULL ||
            stage->screen == NULL || stage->config == NULL ||
            !stage->config->randr.is_enabled) {
        return false;
    }

    /* Both requests go out before either answer is awaited: they are
     * independent of one another, so asking together costs one round
     * trip to the server rather than two.  The primary output's reply
     * is only consulted further down, once the resources have been
     * checked.
     *
     * The primary output is fetched once up front and shared by every
     * profile below (see 's_stage_randr_apply_profile'), so an output
     * already marked primary skips 'xcb_randr_set_output_primary'
     * instead of reissuing it every reload regardless of whether it
     * would change anything. */
    res_cookie = xcb_randr_get_screen_resources_current(
            xcb_connection_get(), stage->screen->root);
    primary_cookie = xcb_randr_get_output_primary(xcb_connection_get(),
            stage->screen->root);

    res_reply = xcb_randr_get_screen_resources_current_reply(
            xcb_connection_get(), res_cookie, &res_error);
    if (res_reply == NULL) {
        xcb_reply_log_error(res_error, "the XRandR screen resources");
        LOGGER_WARNING("XRandR: failed to query screen resources on" \
                " stage %u; cannot apply output profiles",
                stage->id);
        return false;
    }

    primary_reply = xcb_randr_get_output_primary_reply(
            xcb_connection_get(), primary_cookie, &primary_error);
    xcb_reply_log_error(primary_error, "the primary XRandR output");
    current_primary = (primary_reply != NULL)
        ? primary_reply->output : (xcb_randr_output_t) XCB_NONE;
    free(primary_reply);

    if (take_snapshot &&
            s_randr_primary_snapshot_count <
                    (uint32_t) CONFIG_MAX_SCREENS) {
        s_randr_primary_snapshot[s_randr_primary_snapshot_count].stage
            = stage;
        s_randr_primary_snapshot[s_randr_primary_snapshot_count]
            .prior_primary = current_primary;
        ++s_randr_primary_snapshot_count;
    }

    for (uint32_t i = 0u; i < stage->config->randr.output_count; ++i) {
        const struct config_randr_output_s *profile =
            &stage->config->randr.outputs[i];
        xcb_randr_output_t output_id = (xcb_randr_output_t) XCB_NONE;
        xcb_randr_get_output_info_reply_t *info;

        if (profile->name[0] == '\0') {
            continue;
        }

        info = s_stage_randr_find_output_by_name(xcb_connection_get(),
                res_reply, profile->name, &output_id);
        if (info == NULL) {
            LOGGER_DEBUG("XRandR: output '%s' (configured profile)" \
                    " is not currently connected on stage %u",
                    profile->name, stage->id);
            continue;
        }

        if (profile->is_enabled) {
            if (s_stage_randr_apply_profile(stage, res_reply,
                        output_id, info, profile, current_primary,
                        take_snapshot)) {
                any_changed = true;
            }
        } else if (info->crtc != (xcb_randr_crtc_t) XCB_NONE) {
            if (s_stage_randr_blank_crtc(stage, res_reply,
                        info->crtc, output_id, profile->name,
                        take_snapshot)) {
                any_changed = true;
            }
        }
        free(info);
    }

    free(res_reply);

    if (any_changed) {
        stage_monitor_refresh_all(stage);
        stage->is_outdated = true;
    } else {
        LOGGER_DEBUG("XRandR: every configured output profile on" \
                " stage %u already matches its output's current" \
                " state; nothing applied", stage->id);
    }

    return any_changed;
}


/* Undo every snapshot taken since the most recent
 * 'stage_action_randr_snapshot_begin', across every stage a single
 * config reload touched, not only the first */
void stage_action_randr_revert_profiles(void)
{
    uint32_t i = 0u;

    if (xcb_connection_get() == NULL ||
            (s_randr_snapshot_count == 0u &&
             s_randr_primary_snapshot_count == 0u)) {
        s_randr_snapshot_count = 0u;
        s_randr_primary_snapshot_count = 0u;
        return;
    }

    /* 's_randr_snapshot' entries for the same stage are always
     * contiguous, one stage's own call to 'stage_action_apply_
     * randr_profiles' saving every one of its own CRTCs before the next
     * stage's own call ever runs, so a single forward pass grouping
     * consecutive same-stage entries into one run is enough; there is
     * no need to sort or otherwise regroup them. */
    while (i < s_randr_snapshot_count) {
        stage_td *const stage = s_randr_snapshot[i].stage;
        xcb_randr_get_screen_resources_current_cookie_t res_cookie;
        xcb_randr_get_screen_resources_current_reply_t *res_reply;
        xcb_generic_error_t *res_error = NULL;
        uint32_t run_end = i;

        while (run_end < s_randr_snapshot_count &&
                s_randr_snapshot[run_end].stage == stage) {
            ++run_end;
        }

        if (stage == NULL || stage->screen == NULL) {
            i = run_end;
            continue;
        }

        /* The 'config-timestamp' 'xcb_randr_set_crtc_config' itself
         * requires (its second time argument) has to be one the server
         * actually issued, not 'XCB_CURRENT_TIME': the same reasoning
         * 's_stage_randr_apply_profile' and
         * 's_stage_randr_blank_crtc' already follow, both using
         * a screen-resources reply's 'config_timestamp' rather than
         * that constant. Fetched once per stage, not once per CRTC,
         * since every CRTC in this run shares the same one. */
        res_cookie = xcb_randr_get_screen_resources_current(
                xcb_connection_get(), stage->screen->root);
        res_reply = xcb_randr_get_screen_resources_current_reply(
                xcb_connection_get(), res_cookie, &res_error);
        if (res_reply == NULL) {
            xcb_reply_log_error(res_error, "the XRandR screen" \
                    " resources");
            LOGGER_WARNING("XRandR: failed to query screen resources" \
                    " on stage %u; cannot revert its output" \
                    " profiles", stage->id);
            i = run_end;
            continue;
        }

        for (; i < run_end; ++i) {
            const struct stage_randr_snapshot_s *snap =
                &s_randr_snapshot[i];
            xcb_randr_set_crtc_config_cookie_t cfg_cookie;
            xcb_randr_set_crtc_config_reply_t *cfg_reply;
            bool was_off =
                snap->prior_mode == (xcb_randr_mode_t) XCB_NONE;

            cfg_cookie = xcb_randr_set_crtc_config(xcb_connection_get(),
                    snap->crtc, XCB_CURRENT_TIME,
                    res_reply->config_timestamp,
                    (was_off) ? 0 : snap->prior_x,
                    (was_off) ? 0 : snap->prior_y,
                    (was_off)
                        ? (xcb_randr_mode_t) XCB_NONE
                        : snap->prior_mode,
                    (was_off)
                        ? (uint16_t) XCB_RANDR_ROTATION_ROTATE_0
                        : snap->prior_rotation,
                    (was_off) ? 0u : 1u,
                    (was_off) ? NULL : &snap->output_id);
            cfg_reply = xcb_randr_set_crtc_config_reply(
                    xcb_connection_get(), cfg_cookie, NULL);

            if (cfg_reply == NULL ||
                    cfg_reply->status != XCB_RANDR_SET_CONFIG_SUCCESS) {
                LOGGER_WARNING("XRandR: failed to revert CRTC %u on" \
                        " stage %u to its prior state",
                        (unsigned int) snap->crtc, stage->id);
            } else {
                LOGGER_NOTICE("XRandR: reverted CRTC %u on stage" \
                        " %u to its prior state",
                        (unsigned int) snap->crtc, stage->id);
            }
            free(cfg_reply);
        }
        free(res_reply);

        stage_monitor_refresh_all(stage);
        stage->is_outdated = true;
    }

    for (uint32_t p = 0u; p < s_randr_primary_snapshot_count; ++p) {
        stage_td *const stage = s_randr_primary_snapshot[p].stage;

        if (stage == NULL || stage->screen == NULL) {
            continue;
        }

        xcb_randr_set_output_primary(xcb_connection_get(),
                stage->screen->root,
                s_randr_primary_snapshot[p].prior_primary);

        /* Applied again even for a stage a CRTC run above already
         * refreshed: harmless, and simpler than tracking which stages
         * the loop above already touched to skip a repeat. */
        stage_monitor_refresh_all(stage);
        stage->is_outdated = true;
    }

    s_randr_snapshot_count = 0u;
    s_randr_primary_snapshot_count = 0u;
}
