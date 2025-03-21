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
#include <config.h>
#include <desktop.h>
#include <logger.h>

/* Local includes */
#include <screen.h>


/* Initialize a new screen */
screen_td *screen_init(Display *display, const unsigned int screen_id,
        unsigned int desktop_count, config_td *config)
{
    screen_td *screen;
    Screen *xscreen;

    logger_msg(LOG_DEBUG, "Initializing screen %u", screen_id);
    screen = malloc(sizeof(screen_td));
    if (screen == NULL) {
        logger_msg(LOG_FATAL,
                "Cannot allocate memory for screen %u", screen_id);
        return NULL;
    }

    logger_msg(LOG_TRACE,
            "Retrieving screen information from X server");
    xscreen = ScreenOfDisplay(display, screen_id);
    if (xscreen == NULL) {
        logger_msg(LOG_FATAL,
                "Failed to retrieve information for screen %u",
                screen_id);
        return NULL;
    }

    screen->id = screen_id;
    screen->display = display;
    screen->config = config;

    screen->dim.w = (unsigned int) xscreen->width;
    screen->dim.h = (unsigned int) xscreen->height;

    /* DPI = px / (mm/25.4);  1 in = 25.4 mm*/
    screen->dpi.x = (unsigned int)
        ((float) screen->dim.w /
         (((float) XDisplayWidthMM(display, (int) screen_id) / 25.4f)));
    screen->dpi.y = (unsigned int)
        ((float) screen->dim.h /
         (((float) XDisplayHeightMM(display, (int) screen_id) / 25.4f)));

    screen->depth = DefaultDepth(display, screen_id);
    screen->colormaps = DefaultColormap(display, screen_id);
    screen->visual = DefaultVisual(display, screen_id);
    screen->root = RootWindow(display, screen_id);

    /* Handle desktops */
    logger_msg(LOG_TRACE, "Setting up all %d desktops", desktop_count);

    logger_msg(LOG_TRACE,
            "Initializing desktop list structure for screen %u",
            screen_id);
    screen->desktops = list_init((void(*)(void *)) desktop_destroy);
    if (screen->desktops == NULL) {
        logger_msg(LOG_FATAL,
                "Failed to allocate memory for desktops on screen %u",
                screen_id);
        free(screen);
        return NULL;
    }

    /* Initialize desktops */
    screen->desktop_count = desktop_count;
    for (unsigned int i = 0; i < desktop_count; ++i) {
        desktop_td *desktop = desktop_init(screen_id, i,
                &(screen->config->base), &(screen->config->theme));
        if (screen == NULL) {
            logger_msg(LOG_FATAL, "Failed to initialize desktop" \
                    " %u on screen %u", i, screen_id);
            list_destroy(screen->desktops);
            return NULL;
        }

        logger_msg(LOG_TRACE,
                "Inserting desktop %u of screen %u into list",
                i, screen_id);
        if (list_ins_next(screen->desktops,
                    list_tail(screen->desktops),
                    (const void *) desktop) != 0) {
            logger_msg(LOG_FATAL,
                    "Failed to insert desktop" \
                    " %u on screen %u into desktop list", i, screen_id);
            desktop_destroy(desktop);
            list_destroy(screen->desktops);
            return NULL;
        }
    }

    return screen;
}

/* Free allocated memory for a screen */
void screen_destroy(screen_td *screen)
{
    logger_msg(LOG_DEBUG, "Deallocating structure for screen %u",
            screen->id);
    if (screen == NULL) {
        return;
    }

    logger_msg(LOG_TRACE,
            "Deallocating desktops on screen %u", screen->id);
    list_destroy(screen->desktops);

    logger_msg(LOG_TRACE, "Destroying screen %u", screen->id);
    free(screen);
}
