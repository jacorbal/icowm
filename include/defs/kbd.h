/**
 * @file defs/kbd.h
 *
 * @brief X11 keysym and modifier-mask constants used across the
 *        keyboard input subsystem
 *
 * Every fixed X11 keysym value (as defined by the X11 protocol's own
 * @c keysymdef.h, never configurable) and modifier-mask alias this
 * window manager compares a pressed key or held modifier against
 * lives here, so a given key is always named the same way regardless
 * of which of input/kbd/event.c, input/kbd/bind.c, input/kbd/modal.c,
 * or menu/context/ctxmenu.c happens to be checking for it.
 *
 * @ingroup defs
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_KBD_H
#define DEFS_KBD_H


/* System includes */
#include <stdint.h>

/* XCB includes */
#include <xcb/xcb.h>


/**
 * @brief Fixed X11 keysym values this window manager checks a
 *        pressed key against
 *
 * Named the same as the corresponding @c XK_* macro in the X11
 * protocol's own @c keysymdef.h, minus the @c XK_ prefix, so any of
 * these can be looked up there directly by name if ever in doubt.
 * @c KS_FKEY_BASE is the one exception: not a keysym in its own
 * right, but the arithmetic base @c KS_FKEY_BASE @c + @c n resolves
 * to @c F1 through @c F12 for @c n in @c 1..12 (see
 * @c s_bindings_parse_keysym_token in input/kbd/bind.c).
 */
#define KS_SPACE (0x0020u)
#define KS_BACKSPACE (0xff08u)
#define KS_TAB (0xff09u)
#define KS_RETURN (0xff0du)
#define KS_PAUSE (0xff13u)
#define KS_SYS_REQ (0xff15u)
#define KS_ESCAPE (0xff1bu)
#define KS_HOME (0xff50u)
#define KS_LEFT (0xff51u)
#define KS_UP (0xff52u)
#define KS_RIGHT (0xff53u)
#define KS_DOWN (0xff54u)
#define KS_PAGE_UP (0xff55u)
#define KS_PAGE_DOWN (0xff56u)
#define KS_END (0xff57u)
#define KS_PRINT (0xff61u)
#define KS_INSERT (0xff63u)
#define KS_BREAK (0xff6bu)
#define KS_NUM_LOCK (0xff7fu)
#define KS_KP_ENTER (0xff8du)
#define KS_FKEY_BASE (0xffbdu)
#define KS_SHIFT_L (0xffe1u)
#define KS_SHIFT_R (0xffe2u)
#define KS_CONTROL_L (0xffe3u)
#define KS_CONTROL_R (0xffe4u)
#define KS_META_L (0xffe7u)
#define KS_META_R (0xffe8u)
#define KS_ALT_L (0xffe9u)
#define KS_ALT_R (0xffeau)
#define KS_SUPER_L (0xffebu)
#define KS_SUPER_R (0xffecu)
#define KS_HYPER_L (0xffedu)
#define KS_HYPER_R (0xffeeu)
#define KS_DELETE (0xffffu)


/**
 * @brief Strip locking modifier bits from a modifier mask
 *
 * Removes @c XCB_MOD_MASK_LOCK (@c Caps_Lock) and @c XCB_MOD_MASK_2
 * (@c Num_Lock) from @p m so that comparisons against configured
 * bindings are not affected by the state of these locking keys.
 *
 * @param m Modifier mask to strip (any integer type)
 *
 * @return A @c uint16_t with locking bits cleared
 */
#define INPUT_STRIP_LOCK_MASK(m) \
    ((uint16_t) ((unsigned int)(m) & \
        ~((unsigned int) XCB_MOD_MASK_LOCK | \
            (unsigned int) XCB_MOD_MASK_2)))

/* Readable aliases for common modifier masks */
#define MOD_SHIFT XCB_MOD_MASK_SHIFT    /**< Shift modifier */
#define MOD_CTRL XCB_MOD_MASK_CONTROL  /**< Control modifier */
#define MOD_ALT XCB_MOD_MASK_1        /**< Alt/Mod1 modifier */
#define MOD_SUPER XCB_MOD_MASK_4        /**< Super/Win/Mod4 modifier */
#define MOD_HYPER XCB_MOD_MASK_5        /**< Hyper/Mod5 modifier */
#define MOD_NUMLOCK XCB_MOD_MASK_2        /**< Num_Lock/Mod2 (locking) */
#define MOD_CAPSLOCK XCB_MOD_MASK_LOCK     /**< Caps_Lock (locking) */


#endif  /* ! DEFS_KBD_H */
