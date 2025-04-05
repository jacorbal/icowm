/**
 * @file ewmh.h
 *
 * @brief Functions for managing window properties and states per EWMH
 */

#ifndef HINTS_EWMH_H
#define HINTS_EWMH_H


/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window, Atom */


/* Public interface */
/**
 * @brief Set the @c WM_NAME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param name    The name to be set for the window
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_name(Display *display, Window window, const char *name);

/**
 * @brief Retrieve the @c WM_NAME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Pointer to the string value of the @c WM_NAME property, or
 *         @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
char *ewmh_get_wm_name(Display *display, Window window);

/**
 * @brief Set the @c WM_VISIBLE_NAME property of a window
 *
 * @param display      Pointer to the X display
 * @param window       Window to which the property will be set
 * @param visible_name Visible name to be set for the window
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_visible_name(Display *display, Window window,
        const char *visible_name);

/**
 * @brief Retrieve the @c WM_VISIBLE_NAME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Pointer to the string value of the @c WM_VISIBLE_NAME
 *         property, or @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
char *ewmh_get_wm_visible_name(Display *display, Window window);

/**
 * @brief Set the @c WM_ICON_NAME property of a window
 *
 * @param display   Pointer to the X display
 * @param window    Window to which the property will be set
 * @param icon_name Icon name to be set for the window
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_icon_name(Display *display, Window window,
        const char *icon_name);

/**
 * @brief Retrieve the @c WM_ICON_NAME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Pointer to the string value of the @c WM_ICON_NAME property,
 *         or @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
char *ewmh_get_wm_icon_name(Display *display, Window window);

/**
 * @brief Set the @c WM_VISIBLE_ICON_NAME property of a window
 *
 * @param display           Pointer to the X display
 * @param window            Window to which the property will be set
 * @param visible_icon_name Visible icon name to be set for the window
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_visible_icon_name(Display *display, Window window,
        const char *visible_icon_name);

/**
 * @brief Retrieve the @c WM_VISIBLE_ICON_NAME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Pointer to the string value of the @c WM_VISIBLE_ICON_NAME
 *         property, or @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
char *ewmh_get_wm_visible_icon_name(Display *display, Window window);

/**
 * @brief Set the @c WM_PID property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param pid     Process ID to be set for the window
 *
 * @return Status of the operation: 0 on success, non-zero on error
 */
int ewmh_set_wm_pid(Display *display, Window window, unsigned long pid);

/**
 * @brief Retrieve the @c WM_PID property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Process ID associated with the @c WM_PID property, or 0 on
 *         error
 *
 * @note Complexity: @e O(1)
 */
unsigned long ewmh_get_wm_pid(Display *display, Window window);

/**
 * @brief Set the @c ACTIVE_WINDOW property of a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property will be set
 * @param active_window Active window to be set
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_active_window(Display *display, Window window,
        Window active_window);

/**
 * @brief Retrieve the @c ACTIVE_WINDOW property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return The active window associated with the @c ACTIVE_WINDOW
 *         property, or 0 on error
 *
 * @note Complexity: @e O(1)
 */
Window ewmh_get_active_window(Display *display, Window window);

/**
 * @brief Set the @c WM_USER_TIME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param time    User time to be set for the window
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_user_time(Display *display, Window window, Time time);

/**
 * @brief Retrieve the @c WM_USER_TIME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return User time associated with the @c WM_USER_TIME property
 *
 * @note Complexity: @e O(1)
 */
Time ewmh_get_wm_user_time(Display *display, Window window);

/**
 * @brief Set the @c WM_USER_TIME_WINDOW property of a window
 *
 * @param display      Pointer to the X display
 * @param window       Window to which the property will be set
 * @param time_window  User time window to be set for the window
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_user_time_window(Display *display, Window window,
        Window time_window);

/**
 * @brief Retrieve the @c WM_USER_TIME_WINDOW property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return User time associated with the @c WM_USER_TIME_WINDOW property
 *
 * @note Complexity: @e O(1)
 */
Window ewmh_get_wm_user_time_window(Display *display, Window window);

/**
 * @brief Set the @c WM_STRUT property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param strut   Pointer to an array of long values representing the
 *                strut
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_strut(Display *display, Window window, long *strut);

/**
 * @brief Retrieve the @c WM_STRUT property of a window
 *
 * @param display   Pointer to the X display
 * @param window    Window from which the property will be read
 * @param strut_out Pointer to a location where the retrieved strut will
 *                  be stored
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1) for the property retrieval; however, it
 *       allocates memory for the output, which will be @e O(1) since
 *       the size is fixed
 */
int ewmh_get_strut(Display *display, Window window, long **strut_out);

/**
 * @brief Set the @c WM_DESKTOP property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param desktop Desktop number to be set for the window
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_desktop(Display *display, Window window, long desktop);

/**
 * @brief Retrieve the @c WM_DESKTOP property of a window
 *
 * @param display     Pointer to the X display
 * @param window      Window from which the property will be read
 * @param desktop_out Pointer to a location where the retrieved desktop
 *                    number will be stored
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_get_desktop(Display *display, Window window,
        long *desktop_out);

/**
 * @brief Set the @c WM_OPACITY property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param opacity Opacity level to be set for the window
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_opacity(Display *display, Window window,
        unsigned long opacity);

/**
 * @brief Retrieve the @c WM_OPACITY property of a window
 *
 * @param display     Pointer to the X display
 * @param window      Window from which the property will be read
 * @param opacity_out Pointer to a location where the retrieved opacity
 *                    will be stored
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_get_wm_opacity(Display *display, Window window,
        unsigned long *opacity_out);

/**
 * @brief Set the @c WM_BYPASS_COMPOSITOR property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param bypass  Bypass state to be set for the window
 *                (1 to bypass; 0 to not bypass)
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_bypass_compositor(Display *display, Window window,
        int bypass);

/**
 * @brief Retrieve the @c WM_BYPASS_COMPOSITOR property of a window
 *
 * @param display    Pointer to the X display
 * @param window     Window from which the property will be read
 * @param bypass_out Pointer to a location where the bypass state will
 *                   be stored
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_get_wm_bypass_compositor(Display *display, Window window,
        int *bypass_out);

/**
 * @brief Set the @c WM_FRAME_EXTENTS property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param extents Pointer to an array of long values representing the
 *                frame extents
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_frame_extents(Display *display, Window window,
        long *extents);

/**
 * @brief Retrieve the @c WM_FRAME_EXTENTS property of a window
 *
 * @param display     Pointer to the X display
 * @param window      Window from which the property will be read
 * @param extents_out Pointer to a location where the retrieved extents
 *                    will be stored
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_get_wm_frame_extents(Display *display, Window window,
        long *extents_out);

/**
 * @brief Retrieve the @c WM_ALLOWED_ACTIONS property of a window
 *
 * @param display     Pointer to the X display
 * @param window      Window from which the property will be read
 * @param actions_out Pointer to a location where the retrieved actions
 *                    will be stored
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(n), where @e n is the number of allowed
 *       actions
 */
int ewmh_get_allowed_actions(Display *display, Window window,
        Atom **actions_out);

/**
 * @brief Retrieve the @c WM_STATE property of a window
 *
 * @param display    Pointer to the X display
 * @param window     Window from which the property will be read
 * @param states_out Pointer to a location where the retrieved states
 *                   will be stored
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(n), where @e n is the number of states in the
 *       state array
 */
int ewmh_get_window_state(Display *display, Window window,
        Atom **states_out);

/**
 * @brief Retrieve the @c WM_WINDOW_TYPE property of a window
 *
 * @param display   Pointer to the X display
 * @param window    Window from which the property will be read
 * @param types_out Pointer to a location where the retrieved types will
 *                  be stored
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(n), where @e n is the number of types in the
 *       type array
 */
int ewmh_get_window_type(Display *display, Window window,
        Atom **types_out);

/**
 * @brief Set the @c WM_STATE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param states  Pointer to the array of states to be set
 * @param nstates Number of states in the array
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_state(Display *display, Window window, Atom *states,
        unsigned long nstates);

/**
 * @brief Retrieve the @c WM_STATE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 * @param nitems  Pointer to a location where the number of items will
 *                be stored
 *
 * @return Pointer to the array of states associated with the
 *         @c WM_STATE property
 *
 * @note Complexity: @e O(1)
 */
Atom *ewmh_get_wm_state(Display *display, Window window,
        unsigned long *nitems);

/**
 * @brief Add a state to the @c WM_STATE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the state will be added
 * @param state   State to be added
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_add_wm_state(Display *display, Window window, Atom state);

/**
 * @brief Remove a state from the @c WM_STATE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the state will be removed
 * @param state   State to be removed
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_remove_wm_state(Display *display, Window window, Atom state);

/**
 * @brief Set the @c WM_ALLOWED_ACTIONS property of a window
 *
 * @param display  Pointer to the X display
 * @param window   Window to which the property will be set
 * @param actions  Pointer to the array of allowed actions
 * @param nactions Number of allowed actions
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_allowed_actions(Display *display, Window window,
        Atom *actions, unsigned long nactions);

/**
 * @brief Retrieve the @c WM_ALLOWED_ACTIONS property of a window
 *
 * @param display  Pointer to the X display
 * @param window   Window from which the property will be read
 * @param nactions Pointer to a location where the number of actions
 *                 will be stored
 *
 * @return Pointer to the array of actions associated with the
 *         @c WM_ALLOWED_ACTIONS property
 *
 * @note Complexity: @e O(1)
 */
Atom *ewmh_get_wm_allowed_actions(Display *display, Window window,
        unsigned long *nactions);

/**
 * @brief Set the @c WM_WINDOW_TYPE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param types   Pointer to the array of window types
 * @param ntypes  Number of window types
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_set_wm_window_type(Display *display, Window window,
        Atom *types, unsigned long ntypes);

/**
 * @brief Retrieve the @c WM_WINDOW_TYPE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 * @param ntypes  Pointer to a location where the number of types will
 *                be stored
 *
 * @return Pointer to the array of types associated with the
 *         @c WM_WINDOW_TYPE property
 *
 * @note Complexity: @e O(1)
 */
Atom *ewmh_get_wm_window_type(Display *display, Window window,
        unsigned long *ntypes);

/**
 * @brief Add an action to the @c WM_ALLOWED_ACTIONS property of a
 *        window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the action will be added
 * @param action  Action to be added
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_add_wm_allowed_action(Display *display, Window window,
        Atom action);

/**
 * @brief Add a type to the @c WM_WINDOW_TYPE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the type will be added
 * @param type    Type to be added
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_add_wm_window_type(Display *display, Window window, Atom type);

/**
 * @brief Remove an action from the @c WM_ALLOWED_ACTIONS property of
 *        a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the action will be removed
 * @param action  Action to be removed
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_remove_wm_allowed_action(Display *display, Window window,
        Atom action);

/**
 * @brief Remove a type from the @c WM_WINDOW_TYPE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the type will be removed
 * @param type    Type to be removed
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 */
int ewmh_remove_wm_window_type(Display *display, Window window,
        Atom type);

/**
 * @brief Macro that evaluates to generalized function of setting window
 *        property as an alias for setting allowed action
 *
 * @see @a ewmh_set_window_property
 */
#define ewmh_set_allowed_actions(d, w, n, a, c) \
    ewmh_set_window_property(d, w, n, a, c)

/**
 * @brief Macro that evaluates to generalized function of setting window
 *        property as an alias for setting a window type
 *
 * @see @a ewmh_set_window_property
 */
#define ewmh_set_window_type(d, w, n, a, c) \
    ewmh_set_window_property(d, w, n, a, c)


#endif  /* ! HINTS_EWMH_H */
