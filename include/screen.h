/**
 * @file screen.h
 *
 * @brief Screen structure declaration
 */

#ifndef SCREEN_H
#define SCREEN_H


/* External libraries */
#include <X11/Xlib.h>   /* Window, Colormap */

/* ADT */
#include <adt/cdlist.h> /* Doubly linked circular list */

/* Common type structures */
#include <types/pair.h> /* dimensions_s, size_s */

/* Project includes */
#include <config.h>
#include <desktop.h>


/**
 * @brief Screen structure
 */
typedef struct {
    unsigned int id;            /**< Screen unique identifier or index */

    Display *display;           /**< Pointer to X11 display */
    Screen *screen;             /**< Pointer to X11 screen */
    Visual *visual;             /**< Pointer to X11 associated visual */
    Window root;                /**< Root window for this screen */

    int depth;                  /**< Color depth */
    struct size_s dpi;          /**< Screen DPI (dots per inch) */
    Colormap colormaps;         /**< Color maps */

    struct dimensions_s dim;    /**< Screen dimensions (px) */

    unsigned int desktop_count; /**< Number of desktops */
    unsigned int desktop_cur;   /**< Index of current desktop */ 
    cdlist_td *desktops;        /**< Circular list of desktops */

    config_td *config;          /**< Configuration */
} screen_td;


/* Public interface */
/**
 * @brief Initialize a new screen
 *
 * @param display       Pointer to X11 display
 * @param screen_id     Screen identifier
 * @param desktop_count Number of desktops on this screen
 *
 * @return Pointer to new screen, or @c NULL otherwise
 *
 * @note Complexity: @e O(1)
 */
screen_td *screen_init(Display *display, const unsigned int screen_id,
        unsigned int desktop_count, config_td *config);

/**
 * @brief Free allocated memory for a screen
 *
 * @param screen Screen to deallocate
 *
 * @note Complexity: @e O(n), where @e n is the number of desktop, as it
 *       iterates through the array of windows to free each one of them
 */
void screen_destroy(screen_td *screen);

/**
 * @brief Update the screen by updating all its desktops
 *
 * @param screen Screen to update
 *
 * @note Complexity: @e O(n), where @e n is the number of desktops
 */
void screen_update(screen_td *screen);

/**
 * @brief Macro that evaluates to the desktop count of the screen
 *
 * @note Complexity: @e O(1)
 */
#define screen_desktop_count(s) ((s) ? s->desktops->size : 0)


#endif  /* ! SCREEN_H */
