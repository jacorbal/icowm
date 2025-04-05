/**
 * @file hints/state.h
 *
 * @brief Interface for managing the states of X11 windows 
 * 
 * Interface for managing the states of X11 windows in accordance with
 * the Extended Window Manager Hints (EWMH) and Inter-Client
 * Communication Conventions Manual (ICCCM).
 */

#ifndef HINTS_STATE_H
#define HINTS_STATE_H


/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window, Atom */


/**
 * @brief Set a window property as an array of atoms
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property will be set
 * @param property_name Name of the property to be set
 * @param atoms         Pointer to the array of atoms
 * @param count         Number of atoms in the array
 *
 * @return Status of the operation: 0 on success, non-zero on error
 *
 * @note Complexity: @e O(1)
 *
 * Example:
 * @code
 *  void set_allowed_actions(Display *display, Window window) {
 *      Atom actions[3];
 *      actions[0] = XInternAtom(display, "_NET_WM_ACTION_MOVE", False);
 *      actions[1] = XInternAtom(display, "_NET_WM_ACTION_RESIZE", False);
 *      actions[2] = XInternAtom(display, "_NET_WM_ACTION_CLOSE", False);
 *
 *      set_window_property(display, window,
 *              "_NET_WM_ALLOWED_ACTIONS", actions, 3);
 *  }
 *
 *  void set_window_type(Display *display, Window window) {
 *      Atom types[2];
 *      types[0] = XInternAtom(display,
 *              "_NET_WM_WINDOW_TYPE_NORMAL", False);
 *      types[1] = XInternAtom(display,
 *              "_NET_WM_WINDOW_TYPE_DIALOG", False);
 *      set_window_property(display, window,
 *              "_NET_WM_WINDOW_TYPE", types, 2);
 *  }
 * @endcode
 */
int set_window_property(Display *display, Window window,
        const char *property_name, Atom *atoms, unsigned long count);

/**
 * @brief Get the list of states for a given window
 *
 * @param display Pointer to the X display
 * @param window  Window from which to get the states
 * @param nitems  Pointer to store the number of items in the list
 *
 * @return Pointer to the array of states, or @c NULL on error
 */
Atom *get_window_state(Display *display, Window window,
        unsigned long *nitems);

/**
 * @brief Set the @c WM_STATE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property will be set
 * @param states  Pointer to the array of states to be set
 * @param count   Number of states in the array
 * @param action  Action to be performed (1: add, 0: remove)
 *
 * @note Complexity: @e O(1)
 */
void set_window_state(Display *display, Window window, Atom *states,
        unsigned long count, int action);

/**
 * @brief Set the states for a given window
 *
 * @param display Pointer to the X display
 * @param window  Window to set the states for
 * @param states  Pointer to the array of states
 * @param nstates Number of states in the array
 */
void set_window_states(Display *display, Window window, Atom *states,
        unsigned long nstates);

/**
 * @brief Add an atom to a specified property of a window
 *
 * Reads the current list of atoms from the specified property, appends
 * the new atom if it's not already present, and writes the updated list
 * back to the property.
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to modify
 * @param atom          Atom to add
 *
 * @return 0 on success, or a non-zero value on error
 */
int add_atom_to_property(Display *display, Window window,
        const char *property_name, Atom atom);

/**
 * @brief Add a new state to the window's state list
 *
 * @param display Pointer to the X display
 * @param window  Window to modify
 * @param state   New state to add
 *
 * @return Status of the operation
 * @retval  0 The state was added successfully
 * @retval  1 Couldn't perform the operation
 * @retval  2 The state already exists
 *
 * Example:
 * @code
 *  Atom action_fullscreen = XInternAtom(display,
 *          "_NET_WM_STATE_FULLSCREEN", False);
 *  add_window_state(display, window, action_fullscreen);
 * @endcode
 */
int add_window_state(Display *display, Window window, Atom state);

/**
 * @brief Add multiple states to the window's state list
 *
 * Retrieve the current values of the specified window's property, adds
 * the new values (states or actions) if they do not already exist, and
 * sets the updated list back to the window's property.
 *
 * @param display        Pointer to the X display
 * @param window         Window to modify
 * @param new_states     Array of new states to add
 * @param num_new_states Number of new states in the array
 *
 * Example:
 * @code
 *  Atom state_above = XInternAtom(display,
 *      "_NET_WM_STATE_ABOVE", False);
 *  Atom state_fullscreen = XInternAtom(display,
 *      "_NET_WM_STATE_FULLSCREEN", False);
 *  Atom new_states[] = {state_above, state_fullscreen};
 *  add_window_states_multiple(display, window, new_states, 2);
 * @endcode
 */
void add_window_states_multiple(Display *display, Window window,
        Atom *new_states, unsigned long num_new_states);

/**
 * @brief Remove an atom from a specified property of a window
 *
 * Reads the current list of atoms from the specified property, removes
 * the specified atom, and writes the updated list back to the property.
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name The name of the property to modify
 * @param atom          Atom to remove
 *
 * @return 0 on success, or a non-zero value on error
 */
int remove_atom_from_property(Display *display, Window window,
        const char *property_name, Atom atom);

/**
 * @brief Remove a state from the window's state list
 *
 * @param display Pointer to the X display
 * @param window  Window to modify
 * @param state   State to remove
 *
 * @return Status of the operation
 * @retval  0 The state was removed successfully
 * @retval  1 Couldn't perform the operation
 * @retval  2 The state didn't exist
 *
 * Example:
 * @code
 *  Atom state_maximized_horz = XInternAtom(display,
 *          "_NET_WM_STATE_MAXIMIZED_HORZ", False);
 *  remove_window_state(display, window, state_maximized_horz);
 * @endcode
 */
int remove_window_state(Display *display, Window window, Atom state);

/**
 * @brief Remove multiple states or actions from the window's
 *        property list
 *
 * Retrieves the current values of the specified window's property and
 * removes the specified values (states or actions), setting the updated
 * list back to the window's property.
 *
 * @param display    Pointer to the X display
 * @param window     Window to modify
 * @param values     Array of states or actions to remove
 * @param num_values Number of values in the array
 *
 * Example:
 * @code
 *  Atom action_above = XInternAtom(display,
 *          "_NET_WM_ACTION_ABOVE", False);
 *  Atom action_fullscreen = XInternAtom(display,
 *          "_NET_WM_ACTION_FULLSCREEN", False);
 *  Atom actions_to_remove[] = { action_above, action_fullscreen };
 *  remove_window_states_multiple(display, window, actions_to_remove, 2);
 * @endcode
 */
void remove_window_states_multiple(Display *display, Window window,
        Atom *values, unsigned long num_values);


#endif  /* ! HINTS_STATE */
