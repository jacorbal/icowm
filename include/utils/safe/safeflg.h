/**
 * @file utils/safe/safeflg.h
 *
 * @brief Safe flag management
 *
 * A set of functions for managing and manipulating flags represented as
 * single bits within an unsigned integer.  These functions provide
 * a safe and generic way to toggle, set, and unset flags, ensuring that
 * invalid operations on flags are avoided.  The implementation utilizes
 * bitwise operations to manipulate flag states efficiently.
 *
 * @note The implementation uses @c uint32_t for flag storage, which
 *       allows for a wide range of defined flags while maintaining type
 *       safety and clarity
 * @note These functions are designed to be used with a variety of
 *       enumerations and provide a reusable mechanism for safe flag
 *       manipulation across different contexts within an application
 *
 * @defgroup utils_safe Safe string, memory, and flag helpers
 * @ingroup utils
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef SAFEFLG_H
#define SAFEFLG_H


/* System includes */
#include <stdbool.h>    /* bool */
#include <stdint.h>     /* uint32_t */


/* Public interface */
/**
 * @brief Validate if a flag is within the allowed range
 *
 * Checks if the specified flag is a valid single bit flag (power of 2)
 * and ensures it does not exceed the maximum allowed flags.  It allows
 * the program to ensure that only valid flags are being set, toggled,
 * or unset, thus preventing potential errors in flag management.
 *
 * @param flag      Flag to be validated
 * @param max_flags Maximum value for the flag, which should typically
 *                  be based on the number of defined flags
 *                  (e.g., @c 1u << @p max_flags)
 *
 * @return @c true if the @p flag is valid
 */
bool safeflg_is_valid(uint32_t flag, uint32_t max_flags);

/**
 * @brief Set (enable) the specified flag in the given flag set
 *
 * Sets the specified flag in the flag set, effectively enabling the
 * associated feature.
 *
 * @param flags     Pointer to the flags to be modified
 * @param flag      Flag to be set (enabled)
 * @param max_flags Maximum value for the flag, for range checking
 *
 * @return Status of the operation
 * @retval  0 Success toggling the flag
 * @retval  1 Failed to toggling the flag
 *
 * @note A validity check is performed on the flag to ensure that it is
 *       a valid single bit flag and does not exceed the maximum allowed
 * @see @a safeflg_is_valid
 */
int safeflg_set(uint32_t *flags, uint32_t flag, uint32_t max_flags);

/**
 * @brief Unset (clear) the specified flag in the given flag set
 *
 * Clears the specified flag in the flag set, effectively indicating
 * that the associated feature is now disabled.
 *
 * @param flags     Pointer to the flags to be modified
 * @param flag      Flag to be unset (cleared)
 * @param max_flags Maximum value for the flag, for range checking
 *
 * @return Status of the operation
 * @retval  0 Success clearing the flag
 * @retval  1 Failed to clear the flag
 *
 * @note A validity check is performed on the flag to ensure that it is
 *       a valid single bit flag and does not exceed the maximum allowed
 * @see @a safeflg_is_valid
 */
int safeflg_unset(uint32_t *flags, uint32_t flag, uint32_t max_flags);

/**
 * @brief Toggle the specified flag in the given flag set
 *
 * Flip the state of the specified flag in the flag set.  If the flag is
 * currently set, it will be cleared; if it is cleared, it will be set.
 *
 * @param flags     Pointer to the flags to be modified
 * @param flag      Flag to be toggled
 * @param max_flags Maximum value for the flag, for range checking
 *
 * @return Status of the operation
 * @retval  0 Success toggling the flag
 * @retval  1 Failed to toggling the flag
 *
 * @note A validity check is performed on the flag to ensure that it is
 *       a valid single bit flag and does not exceed the maximum allowed
 * @see @a safeflg_is_valid
 */
int safeflg_toggle(uint32_t *flags, uint32_t flag, uint32_t max_flags);


#endif  /* ! SAFEFLG_H */
