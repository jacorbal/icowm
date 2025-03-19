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
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
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

    unsigned int w, h;          /**< Screen width and height */
    Colormap colormaps;         /**< Color maps */

    unsigned int desktop_cur;   /**< Index of current desktop */ 
    list_td *desktops;       /**< List of desktops */
} screen_td;


/* Public interface */
/**
 * @brief Initialize a new screen
 *
 * @param display   Pointer to X11 display
 * @param screen_id Screen identifier
 *
 * @return Pointer to new screen, or @c NULL otherwise
 */
screen_td *screen_init(Display *display, const unsigned int screen_id);

/**
 * @brief Free allocated memory for a screen
 *
 * @param screen Screen to deallocate
 */
void screen_destroy(screen_td *screen);

/**
 * @brief Macro that evaluates to the desktop count of the screen
 */
#define screen_desktop_count(s) ((s) ? s->desktops->size : 0)


#endif  /* ! SCREEN_H */
