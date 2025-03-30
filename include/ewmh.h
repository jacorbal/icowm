/**
 * @file ewmh.h
 *
 * @brief Extended Window Manager Hints (EWMH) interface declaration
 *
 * Function declarations for managing window properties in compliance
 * with the Extended Window Manager Hints (EWMH) specifications.  EWMH
 * is a standard that provides a set of hints and properties for window
 * managers in the X Window System.
 *
 * EWMH property list and its associated function:
 *  - @c _NET_STARTUP_ID: @a ewmh_set_window_startup_id
 *  - @c _NET_WM_ACTIONS: @a ewmh_set_window_actions
 *  - @c _NET_WM_DESKTOP: @a ewmh_set_window_desktop
 *  - @c _NET_WM_ICON: @a ewmh_set_window_icon
 *  - @c _NET_WM_ICON_NAME: @a ewmh_set_window_icon_name
 *  - @c _NET_WM_MOVERESIZE: @a ewmh_set_window_moveresize
 *  - @c _NET_WM_NAME: @a ewmh_set_window_name
 *  - @c _NET_WM_PID: @a ewmh_set_window_pid
 *  - @c _NET_WM_SHOWN: @a ewmh_set_window_shown
 *  - @c _NET_WM_STATE_ABOVE: @a ewmh_set_window_state_above
 *  - @c _NET_WM_STATE: @a ewmh_set_window_state
 *  - @c _NET_WM_STATE_BELOW: @a ewmh_set_window_state_below
 *  - @c _NET_WM_STATE_DEMANDS_ATTENTION:
 *              @a ewmh_set_window_state_demands_attention
 *  - @c _NET_WM_STATE_FOCUSED: @a ewmh_set_window_state_focused
 *  - @c _NET_WM_STATE_FULLSCREEN: @a ewmh_set_window_state_fullscreen
 *  - @c _NET_WM_STATE_HIDDEN: @a ewmh_set_window_state_hidden
 *  - @c _NET_WM_STATE_MAXIMIZED_HORZ:
 *              @a ewmh_set_window_state_maximized_horz
 *  - @c _NET_WM_STATE_MAXIMIZED_VERT:
 *              @a ewmh_set_window_state_maximized_vert
 *  - @c _NET_WM_STATE_MINIMIZED: @a ewmh_set_window_state_minimized
 *  - @c _NET_WM_STATE_MODAL: @a ewmh_set_window_state_modal
 *  - @c _NET_WM_STATE_SHADED: @a ewmh_set_window_state_shaded
 *  - @c _NET_WM_STATE_STICKY: @a ewmh_set_window_state_sticky
 *  - @c _NET_WM_STATE_SKIP_PAGER: @a ewmh_set_window_state_skip_pager
 *  - @c _NET_WM_STATE_SKIP_TASKBAR: @a ewmh_set_window_state_skip_taskbar
 *  - @c _NET_WM_USER_TIME: @a ewmh_set_window_user_time
 *  - @c _NET_WM_USER_TIME_WINDOW: @a ewmh_set_window_user_time_window
 *  - @c _NET_WM_WINDOW_OPACITY: @a ewmh_set_window_opacity
 *  - @c _NET_WM_WINDOW_TYPE: @a ewmh_set_window_type
 *  - @c _NET_WM_WINDOW_TYPE_COMBO: @a ewmh_set_window_type_combo
 *  - @c _NET_WM_WINDOW_TYPE_DESKTOP: @a ewmh_set_window_type_desktop
 *  - @c _NET_WM_WINDOW_TYPE_DIALOG: @a ewmh_set_window_type_dialog
 *  - @c _NET_WM_WINDOW_TYPE_DND: @a ewmh_set_window_type_dnd
 *  - @c _NET_WM_WINDOW_TYPE_DOCK: @a ewmh_set_window_type_dock
 *  - @c _NET_WM_WINDOW_TYPE_DROPDOWN_MENU:
 *              @a ewmh_set_window_type_dropdown_menu
 *  - @c _NET_WM_WINDOW_TYPE_MENU: @a ewmh_set_window_type_menu
 *  - @c _NET_WM_WINDOW_TYPE_NORMAL: @a ewmh_set_window_type_normal
 *  - @c _NET_WM_WINDOW_TYPE_NOTIFICATION:
 *              @a ewmh_set_window_type_notification
 *  - @c _NET_WM_WINDOW_TYPE_POPUP_MENU:
 *              @a ewmh_set_window_type_popup_menu
 *  - @c _NET_WM_WINDOW_TYPE_SPLASH: @a ewmh_set_window_type_splash
 *  - @c _NET_WM_WINDOW_TYPE_TOOLBAR: @a ewmh_set_window_type_toolbar
 *  - @c _NET_WM_WINDOW_TYPE_TOOLTIP: @a ewmh_set_window_type_tooltip
 *  - @c _NET_WM_WINDOW_TYPE_UTILITY: @a ewmh_set_window_type_utility
 */

#ifndef EWMH_H
#define EWMH_H


/* System includes */
#include <stdbool.h>    /* bool */
#include <sys/types.h>  /* pid_t */

/* X11 includes */
#include <X11/Xlib.h>   /* Window, Display, Pixmap */
#include <X11/Xatom.h>  /* Atom */


/* Public interface */
/**
 * @brief Ping an EWMH-compliant window to check if it is responding
 *
 * Sends a ping to the specified window, which can be used to determine
 * if the window is alive and responding.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window to ping
 *
 * @return Status of the operation
 * @retval  true Event was accepted for its sending, but receiving isn't
 *               guaranteed
 * @retval false Event couldn't be sent, or atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
bool ewmh_ping_window(Display *display, Window window);

/**
 * @brief Set the icon for an EWMH-compliant window
 *
 * Sets the icon associated with the specified window, using the EWMH
 * standard.  It updates the @c _NET_WM_ICON property of the window with
 * the provided icon data.
 *
 * @param display    Pointer to the X11 Display structure
 * @param window     X11 window to which the icon belongs
 * @param icon       Pointer to the array containing the icon data
 * @param icon_count Number of icons provided in the array
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -1 If @p icon is @c NULL
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_icon(Display *display, Window window,
        Pixmap *icon, int icon_count);

/**
 * @brief Set the process ID for an EWMH-compliant window
 *
 * Sets the process ID associated with the specified window, using the
 * EWMH standard.  It updates the @c _NET_WM_PID property of the window
 * with the provided PID.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window to which the PID belongs
 * @param pid     New process ID for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_pid(Display *display, Window window, pid_t pid);

/**
 * @brief Set the desktop number for an EWMH-compliant window
 *
 * Sets the desktop number of the specified window, using the EWMH
 * standard.  It updates the @c _NET_WM_DESKTOP property of the window
 * to the provided desktop number.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window whose desktop number is to be set
 * @param desktop New desktop number for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_desktop(Display *display, Window window,
        int desktop);

/**
 * @brief Set the user time for an EWMH-compliant window
 *
 * Sets the user time associated with the specified window, using the
 * EWMH standard.  It updates the @c _NET_WM_USER_TIME property of the
 * window with the provided user time value.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window to which the user time belongs
 * @param time    New user time for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_user_time(Display *display, Window window,
        Time time);

/**
 * @brief Set the user time window for an EWMH-compliant window
 *
 * Sets the user time window associated with the specified window using
 * the EWMH standard.  It updates the @c _NET_WM_USER_TIME_WINDOW
 * property of the window with the specified user time window.
 *
 * @param display          Pointer to the X11 Display structure
 * @param window           X11 window to which user time window belongs
 * @param user_time_window New user time window for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_user_time_window(Display *display, Window window,
        Window user_time_window);

/**
 * @brief Set the opacity for an EWMH-compliant window
 *
 * Sets the opacity of the specified window, using the EWMH standard.
 * It updates the @c _NET_WM_WINDOW_OPACITY property of the window with
 * the provided opacity value.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window whose opacity is to be set
 * @param opacity New opacity value for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note The provided opacity value is validated not to be greater than
 *       @c 0xffffffff, so if the initial value exceeds the maximum, it
 *       will be capped to the maximum opacity level
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_opacity(Display *display, Window window,
        unsigned long int opacity);

/**
 * @brief Set the allowed actions for an EWMH-compliant window
 *
 * Sets the actions that can be performed on the specified window
 * according to the EWMH standard. It updates the @c _NET_WM_ACTIONS
 * property of the window with the provided actions.
 *
 * @param display      Pointer to the X11 Display structure
 * @param window       X11 window whose actions are to be set
 * @param actions      Array of action strings to be set for the window
 * @param action_count Number of actions provided in the array
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the actions
 * @retval -1 If memory allocation fails
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note The action strings are expected to be null-terminated
 * @note Each action should be a standard action recognized by
 *       the window manager, such as "close", "minimize", etc.
 * @note Complexity: @e O(n), where @e n is the number of actions being
 *       set
 */
int ewmh_set_window_actions(Display *display, Window window,
        const char **actions, int action_count);

/**
 * @brief Set the frame extents for an EWMH-compliant window
 *
 * Sets the frame extents of the specified window, using the EWMH
 * standard.  This describes the size of the window frame.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window to set the frame extents for
 * @param extents Pointer to an array containing the frame extents
 *                (left, right, top, bottom)
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the frame extents
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note The extents should contain four values, representing the left,
 *       right, top, and bottom extents
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_frame_extents(Display *display, Window window,
        int *extents);

/**
 * @brief Indicate that a window is shown
 *
 * Notifies the window manager that the window is currently visible to
 * the user.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window to notify
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to update the state
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_shown(Display *display, Window window);

/**
 * @brief Indicate that a window is being moved or resized
 *
 * Updates the state of the specified window to indicate that it is
 * being moved or resized. This may be useful for managing UI elements
 * properly.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window that is being moved or resized
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to update the state
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_moveresize(Display *display, Window window);

/**
 * @brief General function to set the type for an EWMH-compliant window
 *
 * Sets the type of the specified window, using the EWMH standard.  It
 * updates the @c _NET_WM_WINDOW_TYPE property of the window to the
 * provided window type.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window whose type is to be set
 * @param type    New type for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -1 If @p type is @c NULL
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_type(Display *display, Window window,
        const char *type);

/**
 * @brief Set the window name for an EWMH-compliant window
 *
 * Sets the name of the specified window, using the EWMH standard.  It
 * updates the @c _NET_WM_NAME property of the window to the provided
 * name string.
 *
 * @param display A pointer to the X11 Display structure
 * @param window  X11 window whose name is to be set
 * @param state   State to change
 * @param str     New name for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -1 If @p name is @c NULL
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note The name string is expected to be null-terminated
 *       The provided name's length is determined using @a safe_strlen
 * @note This function does not check if the window exists; it is the
 *       caller's responsibility to ensure the window ID is valid
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_state_string(Display *display, Window window,
        const char *state, const char *str);

/**
 * @brief Check whether a state is set or not
 *
 * @param display     Pointer to the X11 Display structure
 * @param window      X11 window whose states are to be set
 * @param state       Status to check if it's already set
 *
 * @return Status of the inquiry
 * @retval  true The status is set
 * @retval false The status was not set, or @p state is @c NULL
 *
 * @note Complexity: @e O(n), where @e n is the current number of states
 *       to transverse until the @p state is found
 */
bool ewmh_is_state_set(Display *display, Window window,
        const char *state);

/**
 * @brief General function to set multiple states for an EWMH-compliant
 *        window
 *
 * Sets multiple states associated with the specified window, using the
 * EWMH standard.  It updates the @c _NET_WM_STATE property of the
 * window with the provided state values.
 *
 * @param display     Pointer to the X11 Display structure
 * @param window      X11 window whose states are to be set
 * @param states      Pointer to the array of states to be set
 * @param state_count Number of states provided in the array
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -1 If @p display or @p window are @c NULL
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_state_multiple(Display *display, Window window,
        Atom *states, int state_count);

/**
 * @brief General window to set a single state for an EWMH-compliant
 *        window
 *
 * Sets a single state associated with the specified window, using the
 * EWMH standard.  It updates the @c _NET_WM_STATE property of the
 * window with the provided state value.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 window whose state is to be set
 * @param state   New state for the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to set the status
 * @retval -1 If @p display or @p window are @c NULL
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_window_state(Display *display, Window window,
        const char *state);

/**
 * @brief General function to unset a single state for an EWMH-compliant
 *        window
 *
 * Unsets a single state associated with the specified window, using the
 * EWMH standard.  It removes the provided state from the
 * @c _NET_WM_STATE property of the window.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 Window ID of the window whose state is to be unset
 * @param state   State to be removed from the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to unset the status
 * @retval -1 If @p display or @p window are @c NULL
 * @retval -2 Nothing was done; atom couldn't be created
 * @retval -3 State was not set; cannot be unset
 *
 * @note Complexity: @e O(1)
 */
int ewmh_unset_window_state(Display *display, Window window,
        const char *state);

/**
 * @brief General function to unset a single state for an EWMH-compliant
 * window by generating a new list of states excluding the one to unset
 *
 * Retrieves the current list of window states associated with the given
 * window.  It then creates a new list of states that excludes the
 * specified state to be removed.  After constructing this new state
 * list, it updates the @c _NET_WM_STATE property of the window by
 * replacing the existing list with the new one.  If the specified state
 * is not found in the current list, it does nothing, ensuring that the
 * state remains unchanged if it was not previously set.
 *
 * @param display Pointer to the X11 Display structure
 * @param window  X11 Window ID of the window whose state is to be unset
 * @param state   State to be removed from the window
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval  1 Failed to unset the status
 * @retval -1 If @p display or @p window are @c NULL
 * @retval -2 Nothing was done; atom couldn't be created
 *
 * @note Complexity: @e O(n), where @e n is the number of states that
 *       initially are on the window
 */
int ewmh_unset_window_state_adjust(Display *display, Window window,
        const char *state);

/**
 * @brief Macro thats sets the window name for an EWMH-compliant window
 *
 * Sets the name of the specified window, using the EWMH standard.  It
 * updates the @c _NET_WM_NAME property of the window to the provided
 * name string.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_string
 */
#define ewmh_set_window_name(d, w, str) \
    ewmh_set_window_state_string(d, w, "_NET_WM_NAME", str)

/**
 * @brief Macro that sets the class name for an EWMH-compliant window
 *
 * Sets the class name of the specified window, using the EWMH standard.
 * It updates the @c _NET_WM_CLASS property of the window to the
 * provided class name string.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_string
 */
#define ewmh_set_window_class(d, w, str) \
    ewmh_set_window_state_string(d, w, "_NET_WM_CLASS", str)

/**
 * @brief Macro that sets the icon name for an EWMH-compliant window
 *
 * Sets the icon name of the specified window, using the EWMH standard.
 * It updates the @c _NET_WM_ICON_NAME property of the window to the
 * provided icon name string.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_string
 */
#define ewmh_set_window_icon_name(d, w, str) \
    ewmh_set_window_state_string(d, w, "_NET_WM_ICON_NAME", str)

/**
 * @brief Macro that sets the startup ID for an EWMH-compliant window
 *
 * Sets the startup ID associated with the specified window, using the
 * EWMH standard.  It updates the @c _NET_STARTUP_ID property of the
 * window to the provided startup ID string.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_string
 */
#define ewmh_set_window_startup_id(d, w, str) \
    ewmh_set_window_state_string(d, w, "_NET_STARTUP_ID", str)

/**
 * @brief Macro that changes the window state to modal
 *
 * Sets the state of the specified window to modal, indicating that this
 * window requires user interaction before they can interact with other
 * windows, using the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_modal(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_MODAL")

/**
 * @brief Macro that changes the window state to skip the pager
 *
 * Sets the state of the specified window to skip the pager, indicating
 * that the window should not be included on a pager, using the EWMH
 * standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_skip_pager(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_SKIP_PAGER")

/**
 * @brief Macro that changes the window state to skip the taskbar
 *
 * Sets the state of the specified window to skip the taskbar,
 * indicating that the window should not be included on a taskbar using
 * the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_skip_taskbar(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_SKIP_TASKBAR")

/**
 * @brief Macro that changes the window state to full screen
 *
 * Sets the state of the specified window to full screen, using the EWMH
 * standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_fullscreen(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_FULLSCREEN")

/**
 * @brief Macro that changes the window state to maximized horizontally
 *
 * Sets the state of the specified window to maximized in the horizontal
 * direction, using the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_maximized_horz(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_MAXIMIZED_HORZ")

/**
 * @brief Macro that changes the window state to maximized vertically
 *
 * Sets the state of the specified window to maximized in the vertical
 * direction, using the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_maximized_vert(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_MAXIMIZED_VERT")

/**
 * @brief Macro that changes the window state to minimized
 *
 * Sets the state of the specified window to minimized, using the EWMH
 * standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_minimized(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_MINIMIZED")

/**
 * @brief Macro that changes the window state to focused
 *
 * Sets the state of the specified window to focused, ensuring it gains
 * input focus according to the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_focused(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_FOCUSED")

/**
 * @brief Macro that changes the window state to sticky
 *
 * Sets the state of the specified window to sticky, meaning it will
 * remain visible on all desktops, using the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_sticky(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_STICKY")

/**
 * @brief Macro that changes the window state to always on top
 *
 * Sets the state of the specified window to above, ensuring it stays on
 * top of other windows, using the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_above(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_ABOVE")

/**
 * @brief Macro that changes the window state to below
 *
 * Sets the state of the specified window to below, ensuring it stays
 * behind other windows according to the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_below(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_BELOW")

/**
 * @brief Macro that changes the window state to shaded
 *
 * Sets the state of the specified window to shade, ensuring it is
 * reduced to it's title bar (if decorated) according to the EWMH
 * standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_shaded(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_SHADED")

/**
 * @brief Macro that changes the window state to hidden
 *
 * Sets the state of the specified window to hidden, ensuring it stays
 * out of sight according to the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_hidden(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_HIDDEN")

/**
 * @brief Macro that changes the window state to demands attention
 *
 * Sets the state of the specified window to demands attention,
 * indicating that the window requires user attention, typically
 * associated with alerts or notifications according to the EWMH
 * standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_state
 */
#define ewmh_set_window_state_demands_attention(d, w) \
    ewmh_set_window_state(d, w, "_NET_WM_STATE_DEMANDS_ATTENTION")

/**
 * @brief Macro alias to @a ewmh_set_window_state_demands_attention
 *
 * This alias provides a more familiar name with the way that state is
 * called all across the program.
 *
 * @see @a ewmh_set_window_state,
 *      @a ewmh_set_window_state_demands_attention
 */
#define ewmh_set_window_state_urgent \
    ewmh_set_window_state_demands_attention

/**
 * @brief Macro that changes the window type to normal
 *
 * Sets the type of the specified window to normal, using the EWMH
 * standard, indicating it is a standard application window.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_normal(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_NORMAL")

/**
 * @brief Macro that changes the window type to dialog
 *
 * Sets the type of the specified window to dialog, using the EWMH
 * standard, indicating it is a dialog window.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_dialog(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_DIALOG")

/**
 * @brief Macro that changes the window type to toolbar
 *
 * Sets the type of the specified window to toolbar, using the EWMH
 * standard, indicating that it is a toolbar window.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_toolbar(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_TOOLBAR")

/**
 * @brief Macro that changes the window type to notification
 *
 * Sets the type of the specified window to notification, using the EWMH
 * standard, indicating that it is a notification window.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_notification(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_NOTIFICATION")

/**
 * @brief Macro that changes the window type to menu
 *
 * Sets the type of the specified window to menu, using the EWMH
 * standard, indicating it is a menu window.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_menu(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_MENU")

/**
 * @brief Macro that changes the window type to utility
 *
 * Sets the type of the specified window to utility, using the EWMH
 * standard, indicating that it is a utility window.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_utility(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_UTILITY")

/**
 * @brief Macro that changes the window type to splash
 *
 * Sets the type of the specified window to splash, using the EWMH
 * standard, indicating that it is a splash screen window.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_splash(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_SPLASH")

/**
 * @brief Macro that changes the window type to desktop
 *
 * Sets the type of the specified window to desktop, using the EWMH
 * standard, indicating that it represents the desktop background.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_desktop(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_DESKTOP")

/**
 * @brief Macro that changes the window type to drop-down menu
 *
 * Sets the type of the specified window to drop-down menu, using the
 * EWMH standard, indicating that it is a drop-down menu.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_dropdown_menu(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU")

/**
 * @brief Macro that changes the window type to popup menu
 *
 * Sets the type of the specified window to popup menu, using the EWMH
 * standard, indicating that it is a popup menu.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_popup_menu(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_POPUP_MENU")

/**
 * @brief Macro that changes the window type to combo
 *
 * Sets the type of the specified window to combo, using the EWMH
 * standard, indicating that it is part of a combo box.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_combo(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_COMBO")

/**
 * @brief Macro that changes the window type to dock
 *
 * Sets the type of the specified window to dnd, indicating that the
 * window is a dock or panel feature, using the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_dock(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_DOCK")

/**
 * @brief Macro that changes the window type to dnd
 *
 * Sets the type of the specified window to dnd, indicating that the
 * window is being dragged, using the EWMH standard.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_dnd(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_DND")

/**
 * @brief Set the tooltip state for an EWMH-compliant window
 *
 * Sets the type of the specified window to tooltip, using the EWMH
 * standard, indicating that it represents a tooltip window.
 *
 * @note Complexity: @e O(1)
 *
 * @see @a ewmh_set_window_type
 */
#define ewmh_set_window_type_tooltip(d, w) \
    ewmh_set_window_type(d, w, "_NET_WM_WINDOW_TYPE_TOOLTIP")


#endif  /* ! EWMH_H */
