/**
 * @file hints/ewmh/ewmh.h
 *
 * @brief Declaration of basic routines to fetch and alter EWMH data
 */

#ifndef HINTS_EWMH_H
#define HINTS_EWMH_H


/* System includes */
#include <stdint.h>     /* uint32_t */

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window */


/**
 * @brief Maximum size (in bytes) for properties associated with the
 *        Extended Window Manager Hints (EWMH) specification
 *
 * The EWMH properties may include various types of data, such as window
 * titles, states, and types.  In practice, the maximum size for
 * a property in X11 systems is typically around 32 KiB (32768 bytes).
 *
 * By setting this value to 32 * 1024 (32 KiB), we ensure that we can
 * handle the largest expected data sizes for EWMH properties without
 * risking truncation or errors when retrieving them.
 */
#define HINT_EWMH_MAX_PROPERTY_SIZE (32768) /* 32 KiB */


/* Public interface */
/**
 * @brief Get a string property from a window
 *
 * Retrieves a UTF-8 string property from a specified window.
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the property from
 * @param prop    Property name to retrieve
 *
 * @return Pointer to a dynamically allocated UTF-8 string representing
 *         the property value, or @c NULL if the property could not be
 *         retrieved.
 *
 * @note The returned string is dynamically allocated and should be
 *       freed by the caller
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       property
 */
char *ewmh_fetch_string_property(Display *display,
        Window window, const char *prop);

/**
 * @brief Get an unsigned integer property from a window
 *
 * Retrieves an unsigned integer (@c CARDINAL) property from a specified
 * window.  If the property does not exist or cannot be retrieved, it
 * returns zero.
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the property from
 * @param prop    Property name to retrieve
 *
 * @return The value of the property as an unsigned integer or 0 if it
 *         could not be retrieved.
 *
 * @note Complexity: @e O(1)
 */

uint32_t ewmh_fetch_uint_property(Display *display,
        Window window, const char *prop);

/**
 * @brief Set an unsigned integer property on a window
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param prop    Property name to set
 * @param value   Value to set
 *
 * @note Complexity: @e O(1)
 */
void ewmh_alter_uint_property(Display *display,
        Window window, const char *prop, uint32_t value);

/**
 * @brief Set a string property on a window
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param prop    Property name to set
 * @param value   Value to set (UTF-8 string)
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
void ewmh_alter_string_property(Display *display,
        Window window, const char *prop, const char *value);


#endif  /* ! HINTS_EWMH_H */
