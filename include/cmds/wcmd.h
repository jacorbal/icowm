/**
 * @file wcmd.h
 *
 * @brief Functions on executions over windows using the X11 interface
 */

#ifndef WCMD_H
#define WCMD_H


/* System includes */
#include <sys/types.h>  /* pid_t */

/* Project includes */
#include <actdata.h>
#include <window.h>


/* '<actdata.h>': Forward declaration of the type 'action_data_window_td' */
//typedef struct action_data_window_s action_data_window_td;

/* '<window.h>': Forward declaration of the type 'window_td' */
//typedef struct window_s window_td;


/* Public interface */
/**
 * @brief Perform the action to close the window
 *
 * @param window Window to close
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_close(window_td *window);

/**
 * @brief Restore the window to its original state
 *
 * @param window Window to restore
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_restore(window_td *window);

/**
 @brief Focus on the given window
 *
 * @param window Window to focus
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_focus(window_td *window);

/**
 * @brief Remove focus from the given window
 *
 * @param window Window to unfocus
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_unfocus(window_td *window);

/**
* @brief Move the window to a new position
 *
 * @param window      Window to move
 * @param window_data Data containing new position
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_move(window_td *window,
        action_data_window_td *window_data);

/**
 * @brief Resize the window to new dimensions
 *
 * @param window      Window to resize
 * @param window_data Data containing new size
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_resize(window_td *window,
        action_data_window_td *window_data);

/**
* @brief Rename the window
 *
 * @param window      Window to rename
 * @param window_data Data containing new name
 *
 * @note Complexity: @e O(n), where @e n is the length of the new name
 */
void wcmd_window_rename(window_td *window,
        action_data_window_td *window_data);

/**
 * @brief Change the class of the window
 *
 * @param window      Window to reclassify
 * @param window_data Data containing new class
 *
 * @note Complexity: @e O(1), where @e n is the length of the class name
 */
void wcmd_window_reclass(window_td *window,
        action_data_window_td *window_data);

/**
 * @brief Perform the action to maximize a window horizontally
 *
 * @param window Window to maximize horizontally
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_maximize_horz(window_td *window);

/**
 * @brief Perform the action to maximize a window vertically
 *
 * @param window Window to maximize vertically
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_maximize_vert(window_td *window);

/**
 * @brief Perform the action to maximize a window entirely
 *
 * @param window Window to maximize
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_maximize(window_td *window);

/**
 * @brief Perform the action to iconify (and minimize it)
 *
 * @param window Window to iconify
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_iconify(window_td *window);

/**
 * @brief Hide the window by minimizing it without iconifying
 *
 * @param window Window to hide
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_hide(window_td *window);

/**
 * @brief Show (unhide) the window
 *
 * @param window Window to hide
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_unhide(window_td *window);

/**
 * @brief Shade (roll-up) the window
 *
 * @param window Window to shade
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_shade(window_td *window);

/**
 * @brief Unshade (roll-down) the window
 *
 * @param window Window to unshade
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_unshade(window_td *window);

/**
 * @brief Toggle window shading
 *
 * @param window Window to toggle shade in
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_toggle_shade(window_td *window);

/**
 * @brief Set the window to sticky mode
 *
 * @param window Window to make sticky
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_sticky(window_td *window);

/**
 * @brief Remove sticky mode from the window
 *
 * @param window Window to unstick
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_unsticky(window_td *window);

/**
 * @brief Toggle sticky mode for the window
 *
 * @param window Window to toggle sticky state
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_toggle_sticky(window_td *window);

/**
 * @brief Set the window to full screen mode
 *
 * @param window Window to maximize
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_fullscreen(window_td *window);

/**
 * @brief Remove full screen mode from the window
 *
 * @param window Window to unmaximize
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_unfullscreen(window_td *window);

/**
 * @brief Toggle full screen mode for the window
 *
 * @param window Window to toggle full screen state
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_toggle_fullscreen(window_td *window);

/**
 * @brief Raise the window to the top of stack
 *
 * @param window Window to raise
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_raise(window_td *window);

/**
 * @brief Lower the window to the bottom of stack
 *
 * @param window Window to lower
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_lower(window_td *window);

/**
 * @brief Layer the window above others
 *
 * @param window Window to layer above
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_layer_above(window_td *window);

/**
 * @brief Layer the window in normal position
 *
 * @param window Window to normalize
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_layer_normal(window_td *window);

/**
 * @brief Layer the window below others
 *
 * @param window Window to layer below
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_layer_below(window_td *window);

/**
 * @brief Mark the window as urgent
 *
 * @param window Window to mark as urgent
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_set_urgent(window_td *window);

/**
 * @brief Clear urgency marking from the window
 *
 * @param window Window to clear urgency
 *
 * @note Complexity: @e O(1)
 */
void wcmd_window_clear_urgent(window_td *window);

/**
 * @brief Set the icon for the window
 *
 * @param window      Window to set icon
 * @param window_data Data containing icon information
 *
 * @note Complexity: @e O(1), where @e n is the length of the icon name
 */
void wcmd_window_set_icon(window_td *window,
        action_data_window_td *window_data);


#endif  /* ! WCMD_H */
