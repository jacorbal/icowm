/**
 * @file hints/ewmh/app.h
 *
 * @brief Base file for EWMH application window properties
 */

#ifndef HINTS_EWMH_APP_H
#define HINTS_EWMH_APP_H


/* System includes */
#include <stdint.h>     /* int32_t, uint32_t */

/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window */


/* Public interface */
                                                         /* Accessors */
/**
 * @brief Get the window name (EWMH)
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the name from
 *
 * @return Dynamically allocated UTF-8 string representing the window
 *         name, or @c NULL on failure
 *
 * @note Returned string list must be deallocated after use
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       property
 */
char *ewmh_net_wm_name(Display *display, Window window);

/**
 * @brief Get the visible name for a window
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the visible name from
 *
 * @return Dynamically allocated UTF-8 string representing the visible
 *         name, or @c NULL on failure
 *
 * @note Returned string list must be deallocated after use
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       property
 */
char *ewmh_net_wm_visible_name(Display *display, Window window);

/**
 * @brief Get the icon name of a window
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the icon name from
 *
 * @return Dynamically allocated UTF-8 string representing the visible
 *         icon name, or @c NULL on failure
 *
 * @note Returned string list must be deallocated after use
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       property
 */
char *ewmh_net_wm_icon_name(Display *display, Window window);

/**
 * @brief Get the visible icon name of a window
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the visible icon name from
 *
 * @return Dynamically allocated UTF-8 string representing the visible
 *         name, or @c NULL on failure
 *
 * @note Returned string list must be deallocated after use
 * @note Complexity: @e O(n), where @e n is the length of the string
 *       property
 */
char *ewmh_net_wm_visible_icon(Display *display, Window window);

/**
 * @brief Get the desktop index for a window
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the desktop index from
 *
 * @return Desktop index, or 0 on failure
 *
 * @note Complexity: @e O(1)
 */
uint32_t ewmh_fetch_net_wm_desktop(Display *display, Window window);

/**
 * @brief Get the type associated with a window
 *
 * @param display   X display connection
 * @param window    X11 window to retrieve the types from
 * @param types_out Array to fill with type string
 * @param max_types Maximum number of types to retrieve
 *
 * @return Number of types retrieved, or a negative value on failure
 *
 * @note Array @p types_out should be pre-allocated
 * @note Complexity: @e O(n), where @e n is the number of types
 *       retrieved
 */
uint32_t ewmh_fetch_net_wm_window_type(Display *display, Window window,
        char **types_out, int max_types);

/**
 * @brief Get the state information for a window
 *
 * @param display    X display connection
 * @param window     X11 window to retrieve the state from
 * @param states_out Array to fill with state strings
 * @param max_states Maximum number of states to retrieve
 *
 * @return Number of states retrieved, or a negative value on failure
 *
 * @note Complexity: @e O(n), where @e n is the number of states
 *       retrieved
 */
int ewmh_fetch_net_wm_state(Display *display, Window window,
        char **states_out, int max_states);

/**
 * @brief Get the current allowed operations supported for a window
 *
 * @param display     X display connection
 * @param window      X11 window to retrieve the actions from
 * @param actions     Array to fill with actions
 * @param max_actions Maximum number of actions to retrieve
 *
 * @return Number of actions retrieved, or -1 on failure
 *
 * @note Complexity: @e O(n), where @e n is the number of actions
 */
int ewmh_fetch_net_wm_allowed_actions(Display *display, Window window,
        char **actions, int max_actions);

/**
 * @brief Get the strut property from a window
 *
 * @param display   X display connection
 * @param window    X11 window to retrieve the property from
 * @param strut_out Array to fill with strut values
 *
 * @note Complexity: @e O(1)
 */
void ewmh_fetch_net_wm_strut(Display *display, Window window,
        uint32_t *strut_out);

/**
 * @brief Get the partial strut property from a window
 *
 * @param display   X display connection
 * @param window    X11 window to retrieve the property from
 * @param strut_out Array to fill with strut values
 *
 * @note Complexity: @e O(1)
 */
void ewmh_fetch_net_wm_strut_partial(Display *display, Window window,
        uint32_t *strut_out);

/**
 * @brief Get the icon geometry property of a window
 *
 * @param display      X display connection
 * @param window       X11 window to retrieve the property from
 * @param geometry_out Array to fill with geometry values
 *
 * @note Geometry comprises @p x, @p y, @p width, @p height (4 values)
 * @note Complexity: @e O(1)
 */
void ewmh_fetch_net_wm_icon_geometry(Display *display, Window window,
        uint32_t *geometry_out);

/**
 * @brief Get the array of possible icons for the client
 *
 * @param display   X display connection
 * @param window    X11 window to retrieve the property from
 * @param icons_out Array to fill with icon dimensions
 * @param max_icons Maximum number of icons to retrieve
 *
 * @return Number of icons retrieved, or a negative value on failure
 *
 * @note Array @p icons_out should be pre-allocated
 * @note Complexity: @e O(n), where @e n is the number of icons
 *       retrieved
 */
int ewmh_fetch_net_wm_icon(Display *display, Window window,
        uint32_t **icons_out, int max_icons);

/**
 * @brief Get the handled icons for a (iconified) window
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the property on
 *
 * @return 1 if the window handles icons, 0 otherwise
 *
 * @note Complexity: @e O(1)
 */
uint32_t ewmh_fetch_net_wm_handled_icons(Display *display,
        Window window);

/**
 * @brief Get the PID of the window manager
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the PID from
 *
 * @return Process ID of the window, or 0 on failure
 *
 * @note Complexity: @e O(1)
 */
uint32_t ewmh_fetch_net_wm_pid(Display *display, Window window);

/**
 * @brief Get the user time for the last activity on a window
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the user time from
 *
 * @return User time (in milliseconds), or 0 on failure
 *
 * @note Complexity: @e O(1)
 */
uint32_t ewmh_fetch_net_wm_user_time(Display *display, Window window);

/**
 * @brief Get the XID of the window where the client sets the user time
 *        property
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the user time window from
 *
 * @return X11 window ID for the user time, or 0 on failure
 *
 * @note Complexity: @e O(1)
 */
Window ewmh_fetch_net_wm_user_time_window(Display *display,
        Window window);

/**
 * @brief Get the frame extents of a window
 *
 * @param display     X display connection
 * @param window      X11 window to retrieve the frame extents from
 * @param extents_out Array to fill with frame extents values (left,
 *                    right, top, bottom)
 *
 * @note Complexity: @e O(1)
 */
void ewmh_fetch_net_wm_frame_extents(Display *display, Window window,
        int *extents_out);

/**
 * @brief Get the @c _NET_WM_BYPASS_COMPOSITOR property of a window
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the property from
 *
 * @return 1 if the window bypasses the compositor, 0 otherwise
 *
 * @note Complexity: @e O(1)
 */
uint32_t ewmh_fetch_net_wm_bypass_compositor(Display *display,
        Window window);

/**
 * @brief Get the opacity level of a window
 *
 * @param display X display connection
 * @param window  X11 window to retrieve the property from
 *
 * @return Opacity value (0 = fully transparent;
 *                        0xFFFFFFFF = fully opaque),
 *         or 0 on failure
 *
 * @note Complexity: @e O(1)
 */
uint32_t ewmh_fetch_net_wm_window_opacity(Display *display,
        Window window);


                                                          /* Mutators */
/**
 * @brief Set the name property of the client
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param name    New name to set
 *
 * @note Sets the name for the window in UTF-8 encoding
 * @note Complexity: @e O(n), where @e n is the length of the string to
 *       set
 */
void ewmh_alter_net_wm_name(Display *display, Window window,
        const char *name);

/**
 * @brief Set the visible name property of the client
 *
 * @param display      X display connection
 * @param window       X11 window to set the property on
 * @param visible_name New visible name to set
 *
 * @note Sets the visible name for the window in UTF-8 encoding
 * @note Complexity: @e O(n), where @e n is the length of the string to
 *       set
 */
void ewmh_alter_net_wm_visible_name(Display *display, Window window,
        const char *visible_name);

/**
 * @brief Set the icon name property of the client window
 *
 * @param display   X display connection
 * @param window    X11 window to set the property on
 * @param icon_name New icon name to set
 *
 * @note Sets the icon name for the window in UTF-8 encoding
 * @note Complexity: @e O(n), where @e n is the length of the string to
 *       set
 */
void ewmh_alter_net_wm_icon_name(Display *display, Window window,
        const char *icon_name);

/**
 * @brief Set the visible icon name property of a window
 *
 * @param display           X display connection
 * @param window            X11 window to set the property on
 * @param visible_icon_name New visible icon name to set
 *
 * @note Sets the visible icon name for the window in UTF-8 encoding
 * @note Complexity: @e O(n), where @e n is the length of the string to
 *       set
 */
void ewmh_alter_net_wm_visible_icon_name(Display *display,
        Window window, const char *visible_icon_name);

/**
 * @brief Set the desktop index a window is in
 *
 * Assigns a desktop index to the window.
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param desktop Index of the desktop to assign
 *
 * @note Desktop index is zero-based
 * @note A value of @c 0xFFFFFFFF indicates that the window should
 *       appear on all desktops
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_desktop(Display *display, Window window,
        uint32_t desktop);

/**
 * @brief Set the type of window
 *
 * @param display   X display connection
 * @param window    X11 window to set the property on
 * @param types     Array of types to set (as strings)
 * @param num_types Number of types in the array
 *
 * @note Complexity: @e O(n), where @e n is the number of types being
 *       set
 */
void ewmh_alter_net_wm_window_type(Display *display, Window window,
        const char **types, int num_types);

/**
 * @brief Set the state of a window
 *
 * @param display    X display connection
 * @param window     X11 window to set the property on
 * @param states     Array of states to set (as strings)
 * @param num_states Number of states in the array
 *
 * @note Complexity: @e O(n), where @e n is the number of states being
 *       set
 */
void ewmh_alter_net_wm_state(Display *display, Window window,
        const char **states, int num_states);

/**
 * @brief Set allowed operations of a window
 *
 * @param display     X display connection
 * @param window      X11 window to set the property on
 * @param actions     Array of actions to set (as strings)
 * @param num_actions Number of actions in the array
 *
 * @note Complexity: @e O(n), where @e n is the number of actions being
 *       set
 */
void ewmh_alter_net_wm_allowed_actions(Display *display, Window window,
        const char **actions, int num_actions);

/**
 * @brief Set the strut property of a window
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param left    Left strut size
 * @param right   Right strut size
 * @param top     Top strut size
 * @param bottom  Bottom strut size
 *
 * @note @c _NET_WM_STRUT is equivalent to a @c _NET_WM_STRUT_PARTIAL
 *       property where all start values are 0 and all end values are
 *       the height or width of the logical screen
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_strut(Display *display, Window window,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom);

/**
 * @brief Set the partial strut property of a window
 *
 * @param display        X display connection
 * @param window         X11 window to set the property on
 * @param left           Left strut size
 * @param right          Right strut size
 * @param top            Top strut size
 * @param bottom         Bottom strut size
 * @param left_start_y   Starting Y for left strut
 * @param left_end_y     Ending Y for left strut
 * @param right_start_y  Starting Y for right strut
 * @param right_end_y    Ending Y for right strut
 * @param top_start_x    Starting X for top strut
 * @param top_end_x      Ending X for top strut
 * @param bottom_start_x Starting X for bottom strut
 * @param bottom_end_x   Ending X for bottom strut
 *
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_strut_partial(Display *display, Window window,
        uint32_t left, uint32_t right, uint32_t top, uint32_t bottom,
        uint32_t left_start_y, uint32_t left_end_y,
        uint32_t right_start_y, uint32_t right_end_y,
        uint32_t top_start_x, uint32_t top_end_x,
        uint32_t bottom_start_x, uint32_t
        bottom_end_x);

/**
 * @brief Set the icon geometry property of a window
 *
 * It specifies the geometry of a possible icon in case the window is
 * iconified.
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param x       X position of the icon
 * @param y       Y position of the icon
 * @param width   Width of the icon
 * @param height  Height of the icon
 *
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_icon_geometry(Display *display, Window window,
        int32_t x, int32_t y, uint32_t width, uint32_t height);

/**
 * @brief Set the window manager title
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param icons   Array of icons @c (CARDINAL[][2+n]/32), where each
 *                icon is @p (width, height)
 * @param num_icons Number of icons (arrays in the icons array)
 *
 * @note Complexity: @e O(n), where @e n is the number of icons
 */
void ewmh_alter_net_wm_icon(Display *display, Window window,
        uint32_t **icons, int num_icons);

/**
 * @brief Set the PID for the window manager
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param pid     Process ID to set
 *
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_pid(Display *display, Window window,
        uint32_t pid);

/**
 * @brief Set the handled icons property of a (iconfied) window
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 *
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_handled_icons(Display *display, Window window);

/**
 * @brief Set the @c _NET_WM_USER_TIME property of a window
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param time    User time to set (in milliseconds)
 *
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_user_time(Display *display, Window window,
        uint32_t time);

/**
 * @brief Set the @c _NET_WM_USER_TIME_WINDOW property of a window
 *
 * @param display     X display connection
 * @param window      X11 window to set the property on
 * @param time_window Window ID to set for user time
 *
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_user_time_window(Display *display, Window window,
        Window time_window);

/**
 * @brief Set the frame extents of a window
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param left    Left extent size
 * @param right   Right extent size
 * @param top     Top extent size
 * @param bottom  Bottom extent size
 *
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_frame_extents(Display *display, Window window,
        int32_t left, int32_t right, int32_t top, int32_t bottom);

/**
 * @brief Set the preferred bypass compositor value for a window
 *
 * @param display  X display connection
 * @param window   X11 window to set the property on
 * @param bypass   Value to set
 *
 * @note A value of 0 indicates no preference.  A value of 1 hints the
 *       compositor to disabling compositing of this window.  A value of
 *       2 hints the compositor to not disabling compositing of this
 *       window.  All other values are reserved and should be treated
 *       the same as a value of 0.
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_bypass_compositor(Display *display,
        Window window, uint32_t bypass);

/**
 * @brief Set the opacity level of a window
 *
 * @param display X display connection
 * @param window  X11 window to set the property on
 * @param opacity Opacity level to set
 *
 * @note If @p opacity is greater than @c 0xFFFFFFFF, it will be
 *       restricted to its maximum possible value
 * @note Levels: @c 0 = fully transparent; @c 0xFFFFFFFF = fully opaque
 * @note Complexity: @e O(1)
 */
void ewmh_alter_net_wm_window_opacity(Display *display, Window window,
        uint32_t opacity);


#endif  /* ! HINTS_EWMH_APP_H */
