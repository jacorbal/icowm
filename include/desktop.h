/**
 * @file desktop.h
 */

#ifndef DESKTOP_H
#define DESKTOP_H


/* ADT */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <window.h>


/**
 * @brief Desktop structure
 */
typedef struct {
    unsigned int screen_id;     /**< Screen index */
    unsigned int id;            /**< Desktop index */
//    char *name;                 /**< Desktop name  */

    list_td *windows;           /**< Windows array for this desktop */
} desktop_td;


/* Public interface */
/**
 * @brief Initialize a new desktop
 *
 * @param screen_id  Screen identifier where this desktop belongs
 * @param desktop_id Desktop identifier
 *
 * @return Pointer to new desktop or @c NULL otherwise
 */
desktop_td *desktop_init(unsigned int screen_id, unsigned int desktop_id);

/**
 * @brief Free memory for allocated desktop
 *
 * @param desktop Desktop to deallocate
 *
 */
void desktop_destroy(desktop_td *desktop);

/**
 * @brief Clear a desktop by removing all its windows
 *
 * This function deallocates each and every window of the desktop and
 * resets the window counter to zero.
 *
 * @param desktop Desktop to be cleared from windows
 */
void desktop_clear(desktop_td *desktop);

/**
 * @brief Add a previously allocated window to the desktop
 *
 * @param desktop Pointer to the desktop where to add the new window
 * @param window  Pointer to the window to be added to the desktop
 *
 * @return Status of the operation
 * @retval 0 Success on removal
 */
int desktop_window_add(desktop_td *desktop, window_td *window);

/**
 * @brief Remove a window from the desktop
 *
 * @param desktop Pointer to the desktop where to remove the window
 * @param window  Pointer to the window to be removed from the desktop
 *
 * @return Status of the operation
 * @retval 0 Success on removal
 */
int desktop_window_rem(desktop_td *desktop, window_td *window);

/**
 * @brief Remove a window from the desktop searching by id
 *
 * @param desktop   Pointer to the desktop where to remove the window
 * @param window_id Identifier of the window to be removed
 *
 * @return Status of the operation
 * @retval 0 Success on removal
 */
int desktop_window_rem_by_id(desktop_td *desktop,
        unsigned int window_id);

/**
 * @brief Macro that evaluates to the window count of the desktop
 */
#define desktop_window_count(d) ((d) ? d->windows->size : 0)


#endif  /* ! DESKTOP_H */
