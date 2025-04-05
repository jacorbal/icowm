/**
 * @file hints/property.h
 *
 * @brief Functions for managing window properties in X11
 *
 * Function declarations for setting and getting various window
 * properties according to the Extended Window Manager Hints (EWMH) and
 * Inter-Client Communication Conventions Manual (ICCCM).
 * The functions include operations for atoms, strings, integers,
 * pixmaps, and time.
 */

#ifndef HINTS_PROPERTY_H
#define HINTS_PROPERTY_H


/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window, Atom, Time */


/* Public interface */
                                                          /* Mutators */

/**
 * @brief Write an array of atoms to a specified property of a window
 *
 * This function sets a property for a window by writing an array of
 * atoms to it.
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property will be written
 * @param property_name Name of the property to write
 * @param atoms         Pointer to an array of atoms to write
 * @param nitems        Number of atoms in the array
 *
 * @return Status of the operations: 0 on success, non-zero on error
 * @retval  0 Success
 *
 * @note The property will be overwritten by this function call
 *
 * Example
 * @code
 *  unsigned long nitems;
 *  Atom *states = property_get_atom_array(display, window,
 *          "_NET_WM_STATE", &nitems);
 *
 *  // Establish the states that are going to be eliminated
 *  Atom state_hidden = XInternAtom(display,
 *          "_NET_WM_STATE_HIDDEN", False);
 *  Atom state_shaded = XInternAtom(display,
 *          "_NET_WM_STATE_SHADED", False);
 *
 *  // Set new array for remainig states
 *  Atom *new_states = malloc((nitems - 2) * sizeof(Atom));
 *  unsigned long new_nitems = 0;
 *
 *  // Copy existin states ignoring those who will be eliminated
 *  for (unsigned long i = 0; i < nitems; ++i) {
 *      if (states[i] != state_hidden && states[i] != state_shaded) {
 *          new_states[new_nitems++] = states[i];
 *      }
 *  }
 *
 *  // Write the new array
 *  property_set_atom_array(display, window, "_NET_WM_STATE",
 *          new_states, new_nitems);
 *
 *  free(new_states);
 *  XFree(states);
 * @endcode
 */
int property_set_atom_array(Display *display, Window window,
        const char *property_name, Atom *atoms, unsigned long nitems);

/**
 * @brief Write an atom property to a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to be written
 * @param atom          Atom value to set for the property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(1)
 */
int property_set_atom(Display *display, Window window,
        const char *property_name, Atom atom);

/**
 * @brief Write a string array property to a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to be written
 * @param values        Array of string values to set for the property
 * @param num_values    Number of strings in the array
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(n), where @e n is the number of strings
 *       multiplied by the average length of the strings in the array
 */
int property_set_string_array(Display *display, Window window,
        const char *property_name, const char **values, int num_values);

/**
 * @brief Write a string property to a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to be written
 * @param value         String value to set for the property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
int property_set_string(Display *display, Window window,
        const char *property_name, const char *value);

/**
 * @brief Write a string property (UTF-8) to a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to be written
 * @param value         String value to set for the property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
int property_set_string_utf8(Display *display, Window window,
        const char *property_name, const char *value);

/**
 * @brief Write an integer property to a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to be written
 * @param value         Integer value to set for the property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(1)
 */
int property_set_int(Display *display, Window window,
        const char *property_name, int value);

/**
 * @brief Write a cardinal property (unsigned long) to a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to be written
 * @param value         Cardinal value to set for the property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(1)
 */
int property_set_cardinal(Display *display, Window window,
        const char *property_name, unsigned long value);

/**
 * @brief Write a pixmap property to a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to be written
 * @param pixmap        Pixmap to set for the property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(1)
 */
int property_set_pixmap(Display *display, Window window,
        const char *property_name, Pixmap pixmap);

/**
 * @brief Write a window property to a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to be written
 * @param value         Handle to set for the property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(1)
 */
int property_set_window(Display *display, Window window,
        const char *property_name, Window value);

/**
 * @brief Write a time property to a window
 *
 * @param display       Pointer to the X display
 * @param window        Window to which the property belongs
 * @param property_name Name of the property to be written
 * @param value         Time value to set for the property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(1)
 */
int property_set_time(Display *display, Window window,
        const char *property_name, Time value);

                                                         /* Accessors */

/**
 * @brief Read an array of atoms from a specified property of a window
 *
 * Retrieve the current list of atoms from a specified property of
 * a window in the X11 environment.
 *
 * @param display       Pointer to the X display
 * @param window        Window from which to read the property
 * @param property_name Name of the property to read
 * @param nitems        Pointer to an unsigned long variable that will
 *                      contain the number of atoms read
 *
 * @return Pointer to an array of atoms, or @c NULL on error
 *
 * @note The caller is responsible for freeing the returned array with
 *       @a XFree() when it is no longer needed
 *
 * Example:
 * @code
 *  unsigned long nitems;
 *  Atom action_fullscreen = XInternAtom(display,
 *          "_NET_WM_ACTION_FULLSCREEN", False);
 *  Atom *actions = property_get_atom_array(display, window,
 *          "_NET_WM_ACTION", &nitems);
 *  int found_fullscreen = 0;
 *
 *  for (unsigned long i = 0; i < nitems; ++i) {
 *      if (actions[i] == action_fullscreen) {
 *          found_fullscreen = 1;
 *          break;
 *      }
 *  }
 * @endcode
 */
Atom* property_get_atom_array(Display *display, Window window,
        const char *property_name, unsigned long *nitems);

/**
 * @brief Read an atom property from a window
 *
 * @param display       Pointer to the X display
 * @param window        Window from which to read the property
 * @param property_name Name of the property to be read
 *
 * @return Atom value read from the property, or @c None
 *
 * @note Complexity: @e O(1)
 */
Atom property_get_atom(Display *display, Window window,
        const char *property_name);

/**
 * @brief Read a string array property from a window
 *
 * @param display       Pointer to the X display
 * @param window        Window from which the property will be read
 * @param property_name Name of the property to be read
 * @param values        Pointer to a location to store the array of
 *                      string values
 * @param nitems        Pointer to a location to store the number of
 *                      items read
 *
 * @return Status of the operation
 * @retval  0 Success
 * @retval -1 Error reading property
 *
 * @note Complexity: @e O(n), where @e n is the number of strings in the
 *       array
 */
int property_get_string_array(Display *display, Window window,
        const char *property_name, char ***values,
        unsigned long *nitems);

/**
 * @brief Read a string property from a window
 *
 * @param display       Pointer to the X display
 * @param window        Window from which to read the property
 * @param property_name Name of the property to be read
 *
 * @return String value read from the property, or @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
char *property_get_string(Display *display, Window window,
        const char *property_name);

/**
 * @brief Read a UTF-8 string property from a window.
 *
 * @param display       Pointer to the X display
 * @param window        Window from which to read the property
 * @param property_name Name of the property to be read
 *
 * @return String value read from the property, or @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
char *property_get_string_utf8(Display *display, Window window,
        const char *property_name);

/**
 * @brief Read an integer property from a window
 *
 * @param display       Pointer to the X display
 * @param window        Window from which to read the property
 * @param property_name Name of the property to be read
 *
 * @return Integer value read from the property, or -1 on error
 *
 * @note Complexity: @e O(1)
 */
int property_get_int(Display *display, Window window,
        const char *property_name);

/**
 * @brief Read a cardinal property from a window
 *
 * @param display       Pointer to the X display
 * @param window        Window from which to read the property
 * @param property_name Name of the property to be read
 *
 * @return Cardinal value read from the property, or 0 on error
 *
 * @note Complexity: @e O(1)
 */
unsigned long property_get_cardinal(Display *display, Window window,
        const char *property_name);

/**
 * @brief Read a pixmap property from a window
 *
 * @param display       Pointer to the X display
 * @param window        Window from which to read the property
 * @param property_name Name of the property to be read
 *
 * @return Pixmap read from the property, or 0 on error
 *
 * @note Complexity: @e O(1)
 */
Pixmap property_get_pixmap(Display *display, Window window,
        const char *property_name);

/**
 * @brief Read a window property from a window
 *
 * @param display       Pointer to the X display
 * @param window        Window from which to read the property
 * @param property_name Name of the property to be read
 *
 * @return Window handle read from the property, or 0 on error
 *
 * @note Complexity: @e O(1)
 */
Window property_get_window(Display *display, Window window,
        const char *property_name);

/**
 * @brief Read a time property from a window
 *
 * @param display       Pointer to the X display
 * @param window        Window from which to read the property
 * @param property_name Name of the property to be read
 *
 * @return Time value read from the property, or 0 on error
 *
 * @note Complexity: @e O(1)
 */
Time property_get_time(Display *display, Window window,
        const char *property_name);


#endif  /* ! HINTS_PROPERTY_H */
