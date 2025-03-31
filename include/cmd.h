/**
 * @file cmd.h
 *
 * @brief Commands over objects (windows, desktops, screens)
 *
 * This module consolidates functions that perform various tasks into
 * a single action.  The goal is to clearly separate the code associated
 * with 'Xlib' from the hint functions.  When a "command" is executed,
 * all relevant functions are invoked sequentially.
 *
 * Regarding complexity, all these functions are technically
 * @e O(1).  However, there is an important caveat: each function calls
 * a hint function to establish states and properties, sometimes sending
 * messages to the client using functions with the signature
 * @a *_send_client_messages, which are typically @e O(n) in complexity.
 * Nevertheless, with @e n -> 1, I classify their complexity as @e O(1).
 */

#ifndef CMD_H
#define CMD_H


/* Project includes */
#include <cmds/wcmd.h>
#include <hints/ewmh/ewmh.h>

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
 * @brief Restore a window
 *
 * @param window Window to restore
 *
 * @note Complexity: @e O(1)
 */
void cmd_window_restore(window_td *window);


#endif  /* ! CMD_H */
