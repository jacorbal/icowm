/**
 * @file hints/icccm.h
 *
 * @brief Functions for managing window properties and states per ICCCM
 */

#ifndef HINTS_ICCCM_H
#define HINTS_ICCCM_H


/* X11 includes */
#include <X11/Xlib.h>   /* Display, Window, Atom */


/* Public interface */
/**
 * @brief Set the @c WM_NAME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property belongs
 * @param name    String value to set for the @c WM_NAME property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
int icccm_set_wm_name(Display *display, Window window,
        const char *name);

/**
 * @brief Get the @c WM_NAME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Pointer to the string value of the @c WM_NAME property, or
 *         @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
char *icccm_get_wm_name(Display *display, Window window);

/**
 * @brief Set the @c WM_ICON_NAME property of a window
 *
 * @param display   Pointer to the X display
 * @param window    Window to which the property belongs
 * @param icon_name String value to set for the @c WM_ICON_NAME property
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
int icccm_set_wm_icon_name(Display *display, Window window,
        const char *icon_name);

/**
 * @brief Get the @c WM_ICON_NAME property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Pointer to the string value of the @c WM_ICON_NAME property,
 *         or @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the length of the string
 */
char *icccm_get_wm_icon_name(Display *display, Window window);

/**
 * @brief Set the @c WM_CLASS property of a window
 *
 * @param display        Pointer to the X display
 * @param window         Window to which the property belongs
 * @param class_name     String value for the class name
 * @param class_instance String value for the class instance
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(n), where @e n is the total length of
 *       @p class_name and @p class_instance
 */
int icccm_set_wm_class(Display *display, Window window,
        const char *class_name, const char *class_instance);

/**
 * @brief Get the @c WM_CLASS property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 * @param nitems  Pointer to a location to store the number of strings
 *                read
 *
 * @return Pointer to an array of strings representing the @c WM_CLASS
 *         property, or @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the number of strings in the
 *       array
 */
char **icccm_get_wm_class(Display *display, Window window,
        unsigned long *nitems);

/**
 * @brief Set the @c WM_PROTOCOLS property of a window
 *
 * @param display   Pointer to the X display
 * @param window    Window to which the property belongs
 * @param protocols Array of atom values to set for @c WM_PROTOCOLS
 * @param nitems    Number of atoms in the protocols array
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(n), where @e n is the number of atoms in the
 *       protocols array
 */
int icccm_set_wm_protocols(Display *display, Window window,
        Atom *protocols, unsigned long nitems);

/**
 * @brief Get the @c WM_PROTOCOLS property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 * @param nitems  Pointer to a location to store number of atoms read
 *
 * @return Pointer to an array of Atom values representing the
 *         @c WM_PROTOCOLS property, or @c NULL on error
 *
 * @note Complexity: @e O(n), where @e n is the number of atoms in the
 *       array
 */
Atom *icccm_get_wm_protocols(Display *display, Window window,
        unsigned long *nitems);

/**
 * @brief Set the @c WM_HINTS property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property belongs
 * @param hints   Pointer to @c XWMHints structure containing hints to
 *                set
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(1)
 */
int icccm_set_wm_hints(Display *display, Window window,
        XWMHints *hints);

/**
 * @brief Get the @c WM_HINTS property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Pointer to @c XWMHints structure containing the hints, or
 *         @c NULL on error
 *
 * @note Memory for @c XWMHints must be freed using @a XFree
 * @note Complexity: @e O(1)
 */
XWMHints *icccm_get_wm_hints(Display *display, Window window);

/**
 * @brief Set the @c WM_SIZE_HINTS property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property belongs
 * @param hints   Pointer to @c XSizeHints structure containing size
 *                hints to set
 *
 * @note Complexity: @e O(1)
 */
void icccm_set_wm_size_hints(Display *display, Window window,
        XSizeHints *hints);

/**
 * @brief Get the @c WM_SIZE_HINTS property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Pointer to @c XSizeHints structure containing the size hints,
 *         or @c NULL on error
 *
 * @note Memory for @c XSizeHints must be freed using @a XFree
 * @note Complexity: @e O(1)
 */
XSizeHints *icccm_get_wm_size_hints(Display *display, Window window);

/**
 * @brief Set the @c WM_NORMAL_HINTS property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property belongs
 * @param hints   Pointer to @c XSizeHints structure containing normal
 *                hints to set
 *
 * @note Complexity: @e O(1)
 */
void icccm_set_wm_normal_hints(Display *display, Window window,
        XSizeHints *hints);

/**
 * @brief Get the @c WM_NORMAL_HINTS property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return Pointer to @c XSizeHints structure containing the normal
 *         hints, or @c NULL on error
 *
 * @note Memory for the @c XSizeHints must be freed using @a XFree
 * @note Complexity: @e O(1)
 */
XSizeHints *icccm_get_wm_normal_hints(Display *display, Window window);

/**
 * @brief Set the @c WM_STATE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window to which the property belongs
 * @param state   Long value representing the state to set
 *
 * @return Status of the operation
 * @retval  0 Success
 *
 * @note Complexity: @e O(1)
 */
int icccm_set_wm_state(Display *display, Window window, long state);

/**
 * @brief Get the @c WM_STATE property of a window
 *
 * @param display Pointer to the X display
 * @param window  Window from which the property will be read
 *
 * @return State of the window, or -1 on error
 *
 * @note Complexity: @e O(1)
 */
long icccm_get_wm_state(Display *display, Window window);


#endif  /* ! HINTS_ICCCM_H */
