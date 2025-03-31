/**
 * @file cmd.h
 *
 * @brief Commands over objects
 */

#ifndef CMD_H
#define CMD_H


/* Project includes */
#include <cmds/wcmd.h>
#include <hints/ewmh.h>

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
void cmd_window_close(window_td *window);

/**
 */
void cmd_window_restore(window_td *window);


#endif  /* ! CMD_H */
