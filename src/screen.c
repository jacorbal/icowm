/**
 * @file screen.c
 *
 * @brief Screen handling implementation
 */

/* System includes */
#include <stdlib.h>     /* free, malloc */

/* External libraries */
#include <X11/Xlib.h>   /* Display, Screen */

/* ADT */
#include <adt/list.h>   /* Singly linked list */

/* Project includes */
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <screen.h>


/* Initialize a new screen */
screen_td *screen_init(Display *display, const unsigned int screen_id)
{
    screen_td *screen;
    Screen *xscreen;

    logger_msg(LOG_DEBUG, "Initializing screen %#x", screen_id);
    screen = malloc(sizeof(screen_td));
    if (screen == NULL) {
        logger_msg(LOG_FATAL,
                "Cannot allocate memory for screen %#x", screen_id);
        return NULL;
    }

    logger_msg(LOG_TRACE, "Getting screen information from X");
    xscreen = ScreenOfDisplay(display, screen_id);
    if (xscreen == NULL) {
        logger_msg(LOG_FATAL,
                "Failed to get screen %#x information", screen_id);
        return NULL;
    }

    screen->id = screen_id;
    screen->display = display;
    screen->w = (unsigned int) xscreen->width;
    screen->h = (unsigned int) xscreen->height;
    screen->colormaps = DefaultColormap(display, screen_id);
    screen->visual = DefaultVisual(display, screen_id);
    screen->root = RootWindow(display, screen_id);

    /* Handle desktops */
    unsigned int DESKTOP_COUNT = 2;//TODO: Get it from configuration!
    logger_msg(LOG_TRACE, "Setting all %d desktops", DESKTOP_COUNT);

    logger_msg(LOG_TRACE,
            "Initializing desktop list structure for screen %#x",
            screen_id);
    screen->desktops = list_init((void(*)(void *)) desktop_destroy);
    if (screen->desktops == NULL) {
        logger_msg(LOG_FATAL,
                "Failed to allocate memory for desktops on screen %#x",
                screen_id);
        free(screen);
        return NULL;
    }

    /* Initialize desktops */
    for (unsigned int i = 0; i < DESKTOP_COUNT; ++i) {
        desktop_td *desktop = desktop_init(screen_id, i);
        if (screen == NULL) {
            logger_msg(LOG_FATAL, "Failed to initialize desktop" \
                    " %#x on screen %#x", i, screen_id);
            list_destroy(screen->desktops);
            return NULL;
        }

        logger_msg(LOG_TRACE,
                "Inserting desktop %u of screen %#x in list",
                i, screen_id);
        if (list_ins_next(screen->desktops,
                    list_tail(screen->desktops),
                    (const void *) desktop) != 0) {
            logger_msg(LOG_FATAL,
                    "Failed to insert desktop" \
                    " %#x on screen %#x into desktop list", i, screen_id);
            desktop_destroy(desktop);
            list_destroy(screen->desktops);
            return NULL;
        }
    }

    screen->desktop_cur = 1; //TODO: set inaugural
    logger_msg(LOG_TRACE,
            "Setting desktop %#x as the initial one on screen %#x",
            screen->desktop_cur, screen_id);

    return screen;
}

/* Free allocated memory for a screen */
void screen_destroy(screen_td *screen)
{
    logger_msg(LOG_DEBUG, "Deallocating screen %#x structure", screen->id);
    if (screen == NULL) {
        return;
    }

    logger_msg(LOG_TRACE,
            "Deallocating desktops on screen %#x", screen->id);
    list_destroy(screen->desktops);

    logger_msg(LOG_TRACE, "Destroying screen %#x", screen->id);
    free(screen);
}
