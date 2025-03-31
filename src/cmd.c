/**
 * @file cmd.c
 *
 * @brief Implementation of commands over objects
 */

/* Project includes */
#include <actdata.h>
#include <cmds/wcmd.h>
#include <hints/ewmh.h>
#include <window.h>

/* Local includes */
#include <cmd.h>


/**/
void cmd_window_close(window_td *window)
{
    wcmd_window_close(window);
    ewmh_send_client_message(window, "_NET_CLOSE_WINDOW");
}


/**/
void cmd_window_restore(window_td *window)
{
    wcmd_window_restore(window);
    ewmh_set_window_restore(window);
}
