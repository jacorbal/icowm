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
/*#include <actdata.h>*/
/*#include <window.h>*/


/* '<actdata.h>': Forward declaration of the type 'action_data_window_td' */
typedef struct action_data_window_s action_data_window_td;

/* '<window.h>': Forward declaration of the type 'window_td' */
typedef struct window_s window_td;


/* Public interface */
/**
 */
void wcmd_window_close(window_td *window);

/**
 */
void wcmd_window_restore(window_td *window);

/**
 */
void wcmd_window_focus(window_td *window);

/**
 */
void wcmd_window_unfocus(window_td *window);

/**
 */
void wcmd_window_move(window_td *window,
        action_data_window_td *window_data);

/**
 */
void wcmd_window_resize(window_td *window,
        action_data_window_td *window_data);

/**
 */
void wcmd_window_rename(window_td *window,
        action_data_window_td *window_data);

/**
 */
void wcmd_window_reclass(window_td *window,
        action_data_window_td *window_data);


#endif  /* ! WCMD_H */
