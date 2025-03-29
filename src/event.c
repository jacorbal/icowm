/**
 * @file event.c
 *
 * @brief Event structure implementation
 */

/* System includes */
#include <stdlib.h>     /* NULL, free, malloc */

/* Project includes */
#include <action.h>
#include <logger.h>
#include <priority.h>

/* Local includes */
#include <event.h>


/* Initializes a new event structure */
event_td *event_init(void *object, void *object_data,
        action_td action, enum priority_e priority)
{
    event_td *event;

    LOGGER_TRACE("Initializing event data structure", L_NARG);
    event = malloc(sizeof(event_td));
    if (event == NULL) {
        LOGGER_WARNING("Failed to allocate memory for event data" \
                "structure", L_NARG);
        return NULL;
    }

    event->action = action;
    event->priority = priority;
    event->object = object;     /* Will need casting later */

    return event;
}


/* Deallocate memory for an event structure */
void event_destroy(event_td *event)
{
    LOGGER_TRACE("Destroying event data structure", L_NARG);
    if (event != NULL) {
        free(event);
    }
}
