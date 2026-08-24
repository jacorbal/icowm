/**
 * @file client/predicates.h
 *
 * @brief Client state predicates and flag mutators
 *
 * One macro per question worth asking about a client, and one per
 * flag worth setting, so that no caller has to know which word of
 * @c client_properties_s holds what, nor how it is encoded.
 *
 * @ingroup client
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef CLIENT_PREDICATES_H
#define CLIENT_PREDICATES_H


/* System includes */
#include <stdint.h>

/* Utils includes */
#include <utils/safe/safeflg.h>

/* Local includes */
#include <client/state.h>

/**
 * @brief Macro that evaluates to the client iconify state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_iconified(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_ICONIFIED)

/**
 * @brief Macro that evaluates to the client maximization state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_maximized(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_MAXIMIZED)

/**
 * @brief Macro that evaluates to the client horizontal maximization
 *        state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_maximized_horz(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_MAXIMIZED_HORZ)

/**
 * @brief Macro that evaluates to the client vertical maximization
 *        state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_maximized_vert(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_MAXIMIZED_VERT)

/**
 * @brief Macro that evaluates to whether the client is maximized in any
 *        way (fully, horizontally-only, or vertically-only)
 *
 * Used wherever an operation needs to know only that the client's
 * @p layout.geometry.old already holds a valid pre-maximize geometry
 * (regardless of which maximize variant is currently active), most
 * notably to decide whether it is safe to call @c client_geometry_save
 * again without stranding that original geometry.
 *
 * @note Complexity: @e O(1)
 *
 * @see @c ccmd_client_maximize, @c ccmd_client_maximize_horz,
 *      @c ccmd_client_maximize_vert, and @c ccmd_client_iconify.
 */
#define client_is_maximized_any(w) \
    (client_is_maximized(w) || client_is_maximized_horz(w) || \
     client_is_maximized_vert(w))

/**
 * @brief Macro that evaluates to the client full screen state
 *
 * @note Complexity: @e O(1)
 */
#define client_is_fullscreen(w) \
    ((w)->properties.state == (uint16_t) CLIENT_STATE_FULLSCREEN)

/**
 * @brief Macro that evaluates to the client hidden flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_hidden(w) \
    ((w)->properties.flags & CLIENT_FLAG_HIDDEN)

/**
 * @brief Macro that evaluates to the client focusable flag
 *
 * @c CLIENT_FLAG_FOCUSABLE is cleared for a window whose own
 * @c _NET_WM_WINDOW_TYPE marks it as a kind that should never take
 * real keyboard focus (a dock or a notification; see @c client.c).
 * This is a distinct concept from @a client_accepts_input_focus:
 * this one is about the window's own @e type, that one is about its
 * ICCCM input model.  A plain @c CLIENT_TYPE_NORMAL window is always
 * focusable by this macro's own measure, regardless of what its
 * @c WM_HINTS may say about whether it actually accepts input.
 *
 * @note Complexity: @e O(1)
 */
#define client_is_focusable(w) \
    ((w)->properties.flags & CLIENT_FLAG_FOCUSABLE)

/**
 * @brief Macro that evaluates to whether a client can receive real
 *        keyboard focus under its own declared ICCCM input model
 *
 * ICCCM §4.1.7 defines three ways a client may end up receiving
 * keyboard focus: a @e Passive client (@c WM_HINTS input field
 * @c true, no @c WM_TAKE_FOCUS) takes it via @c SetInputFocus alone;
 * a @e Locally @e Active or @e Globally @e Active client (registered
 * @c WM_TAKE_FOCUS) additionally or exclusively takes it via that
 * protocol message; a @e No @e Input client (input @c false, no
 * @c WM_TAKE_FOCUS) never takes real keyboard focus at all, by its
 * own explicit declaration.  This macro evaluates true for the
 * first two and false for the third, mirroring Openbox's own
 * @c can_focus @c || @c focus_notify check in @c focus_valid_target
 * (@c focus.c).
 *
 * This is a distinct concept from @a client_is_focusable: that one
 * is about the window's own @e type (a dock or notification never
 * wants focus, whatever its input model says); this one is about
 * the ICCCM input model any window, dock or not, may declare.  A
 * caller that skips this check before routing a client into
 * @a focus_apply (@c policy/focus.c) risks unfocusing whatever
 * already holds real keyboard focus in favor of a client that can
 * never actually receive it, leaving keyboard input directed
 * nowhere until the person clicks something else by hand.
 *
 * @note Complexity: @e O(1)
 */
#define client_accepts_input_focus(w) \
    ((w)->hints_icccm.hints.has_input_hint || \
     (w)->hints_icccm.protocols.has_take_focus)

/**
 * @brief Macro that evaluates to the client shade flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_shaded(w) \
    ((w)->properties.flags & CLIENT_FLAG_SHADED)

/**
 * @brief Macro that evaluates to whether this client currently holds
 *        real X11 input focus
 *
 * @see @c CLIENT_FLAG_FOCUSED's comment above for why this is
 *      its own tracked flag rather than derived from @c desktop->
 *      client_active_id on demand
 *
 * @note Complexity: @e O(1)
 */
#define client_is_focused(w) \
    ((w)->properties.flags & CLIENT_FLAG_FOCUSED)

/**
 * @brief Macro that evaluates to the client pinned flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_pinned(w) \
    ((w)->properties.flags & CLIENT_FLAG_PIN)

/**
 * @brief Macro that evaluates to the negation of the client pinned
 *        flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_unpinned(w) \
    (!client_is_pinned(w))

/**
 * @brief Macro that evaluates to the client decoration flag
 *
 * Normalized to @c 0 or @c 1, unlike leaving the raw flag bit's own
 * numeric value (@c CLIENT_FLAG_DECORATED, not necessarily @c 1)
 * exposed: a caller comparing this against a proper @c bool with
 * @c != or @c ==, as @c ccmd_client_toggle_decorate's own callers in
 * @c rules/apply.c and @c handler/focus.c both do, would otherwise
 * mismatch and toggle decoration off by mistake, every single time,
 * whenever the client already happened to be decorated (the common
 * case for an ordinary client) and the caller wanted it to stay that
 * way.
 *
 * @note Complexity: @e O(1)
 */
#define client_is_decorated(w) \
    (((w)->properties.flags & CLIENT_FLAG_DECORATED) != 0u)

/**
 * @brief Macro that evaluates to the client urgency flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_urgent(w) \
    ((w)->properties.flags & CLIENT_FLAG_URGENT)

/**
 * @brief Macro that compares two @c user_time values safely across
 *        the 32-bit wraparound X11 timestamps undergo roughly every
 *        49.7 days of continuous X server uptime
 *
 * Nothing breaks server-side at that wraparound; the millisecond
 * counter, defined by the X11 protocol itself as a plain @c CARD32,
 * just wraps back to 0 and keeps counting, ordinary unsigned
 * overflow.  But a naive @c a @c > @c b comparison breaks exactly
 * once per wraparound: right after it, every fresh timestamp is
 * numerically small again, so it would wrongly look older than any
 * timestamp from just before the wraparound.  Subtracting first and
 * reinterpreting the result as signed sidesteps this entirely, the
 * same idiom X11 itself already relies on for its own timestamps,
 * as long as the two values being compared are never more than
 * roughly half the 32-bit range (about 24.8 days) apart, which two
 * genuine user-interaction timestamps meaningfully compared against
 * each other never are in practice.
 *
 * @param a First timestamp
 * @param b Second timestamp
 *
 * @return Whether @p a happened after @p b
 * @retval  true @p a is the more recent timestamp
 * @retval false @p a is not more recent than @p b
 *
 * @note Complexity: @e O(1)
 */
#define client_user_time_is_newer(a, b) \
    (((int32_t) ((a) - (b))) > 0)

/**
 * @brief Macro that evaluates to the client disabled flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_disabled(w) \
    ((w)->properties.flags & CLIENT_FLAG_DISABLED)

/**
 * @brief Macro that evaluates to the client resizable flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_resizable(w) \
    ((w)->properties.flags & CLIENT_FLAG_RESIZABLE)

/**
 * @brief Macro that evaluates to the client modal flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_modal(w) \
    ((w)->properties.flags & CLIENT_FLAG_MODAL)

/**
 * @brief Macro that evaluates to the client unresponsive flag
 *
 * @note Complexity: @e O(1)
 */
#define client_is_unresponsive(w) \
    ((w)->properties.flags & CLIENT_FLAG_UNRESPONSIVE)

/**
 * @brief Macro that sets the modal flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_mark_modal(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_MODAL, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the modal flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_unmark_modal(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_MODAL, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the unresponsive flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_mark_unresponsive(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_UNRESPONSIVE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the unresponsive flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_mark_responsive(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_UNRESPONSIVE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the locked flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_lock(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_LOCKED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the locked flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_unlock(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_LOCKED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that marks a client as currently holding real X11
 *        input focus
 *
 * @note Complexity: @e O(1)
 */
#define client_focus_mark(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears a client's own currently-focused flag
 *
 * @note Complexity: @e O(1)
 */
#define client_unfocus_mark(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that evaluates to the locked flag of a client
 *
 * @note Complexity: @e O(1)
 */
#define client_is_locked(w) \
    ((w)->properties.flags & CLIENT_FLAG_LOCKED)

/**
 * @brief Macro that sets a client's own no-focus-fallback flag
 *
 * @note Complexity: @e O(1)
 */
#define client_set_no_focus_fallback(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_NO_FOCUS_FALLBACK, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that evaluates to a client's own no-focus-fallback flag
 *
 * @note Complexity: @e O(1)
 */
#define client_has_no_focus_fallback(w) \
    ((w)->properties.flags & CLIENT_FLAG_NO_FOCUS_FALLBACK)

/**
 * @brief Macro that sets the hidden flag of a client
 *
 * @param w Pointer to the client structure whose visibility is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_hide(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_HIDDEN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the hidden flag of a client
 *
 * @param w Pointer to the client structure whose visibility is to be
 *          set
 *
 * @note Complexity: @e O(1)
 */
#define client_unhide(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_HIDDEN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the hidden flag of a client
 *
 * @param w Pointer to the client structure whose visibility is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_hide(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_HIDDEN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the focus flag of a client
 *
 * @param w Pointer to the client structure whose focus is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_allow_focus(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the focus flag of a client
 *
 * @param w Pointer to the client structure whose focus is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_forbid_focus(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_FOCUSABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the shade flag of a client
 *
 * @param w Pointer to the client structure whose shade is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_shade(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_SHADED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the shade flag of a client
 *
 * @param w Pointer to the client structure whose shade is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unshade(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_SHADED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the shade flag of a client
 *
 * @param w Pointer to the client structure whose shade is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_shade(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_SHADED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the pin flag of a client
 *
 * @param w Pointer to the client structure whose pin is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_pin(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_PIN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the pin flag of a client
 *
 * @param w Pointer to the client structure whose pin is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unpin(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_PIN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the pin flag of a client
 *
 * @param w Pointer to the client structure whose pin is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_pin(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_PIN, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the decoration flag of a client
 *
 * @param w Pointer to the client structure whose decoration is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_decorate(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_DECORATED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the decoration flag of a client
 *
 * @param w Pointer to the client structure whose decoration is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_undecorate(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_DECORATED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the decoration flag of a client
 *
 * @param w Pointer to the client structure whose decoration is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_decorate(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_DECORATED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the urgent flag of a client
 *
 * @param w Pointer to the client structure whose urgent is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_urge(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_URGENT, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the urgent flag of a client
 *
 * @param w Pointer to the client structure whose urgent is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_unurge(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_URGENT, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the urgent flag of a client
 *
 * @param w Pointer to the client structure whose urgent is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_urge(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_URGENT, (1 << CLIENT_FLAG_MAX))


/**
 * @brief Macro that sets the resizable flag of a client
 *
 * @param w Pointer to the client structure whose resizable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_allow_resize(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_RESIZABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the resizable flag of a client
 *
 * @param w Pointer to the client structure whose resizable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_forbid_resize(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_RESIZABLE, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the disable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be set
 *
 * @note Complexity: @e O(1)
 */
#define client_disable(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_DISABLED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the disable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be
 *          cleared
 *
 * @note Complexity: @e O(1)
 */
#define client_enable(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_DISABLED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that toggles the disable flag of a client
 *
 * @param w Pointer to the client structure whose disable is to be
 *          toggled
 *
 * @note Complexity: @e O(1)
 */
#define client_toggle_disable(w) \
    safeflg_toggle(&((w)->properties.flags), \
            CLIENT_FLAG_DISABLED, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the skip-taskbar flag of a client
 *
 * @param w Pointer to the client structure
 *
 * @note Complexity: @e O(1)
 */
#define client_skip_taskbar(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_SKIP_TASKBAR, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the skip-taskbar flag of a client
 *
 * @param w Pointer to the client structure
 *
 * @note Complexity: @e O(1)
 */
#define client_unskip_taskbar(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_SKIP_TASKBAR, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that sets the skip-pager flag of a client
 *
 * @param w Pointer to the client structure
 *
 * @note Complexity: @e O(1)
 */
#define client_skip_pager(w) \
    safeflg_set(&((w)->properties.flags), \
            CLIENT_FLAG_SKIP_PAGER, (1 << CLIENT_FLAG_MAX))

/**
 * @brief Macro that clears the skip-pager flag of a client
 *
 * @param w Pointer to the client structure
 *
 * @note Complexity: @e O(1)
 */
#define client_unskip_pager(w) \
    safeflg_unset(&((w)->properties.flags), \
            CLIENT_FLAG_SKIP_PAGER, (1 << CLIENT_FLAG_MAX))


#endif  /* ! CLIENT_PREDICATES_H */
