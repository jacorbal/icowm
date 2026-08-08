/**
 * @file defs/uistr.h
 *
 * @brief Every user-facing text string in the window manager, in one
 *        place
 *
 * Menu labels, dialog prompts, and button labels live here as plain
 * macros, not scattered as literals through the source files that use
 * them. This is groundwork for gettext: once wired in, each of these
 * becomes an argument to a translation lookup (e.g., @c _(STR_FOO))
 * instead of a raw literal, and this file becomes the single place a
 * translator's @c .pot extraction pass has to look at.
 *
 * @c LOGGER_* messages are deliberately excluded: they are diagnostic
 * text for whoever is running or debugging the window manager, not
 * something an end user reads, so they stay as plain literals at their
 * call sites. The handful of literals in @c main.c (command-line usage
 * text) are excluded the same way, since they serve the same
 * diagnostic-audience role @c LOGGER_* messages do.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

#ifndef DEFS_UISTR_H
#define DEFS_UISTR_H


/* Root menu (src/menu/context/rootmenu.c): the fixed footer entries
 * every root menu gets, below whatever 'menus.json' configures */
#define STR_ROOTMENU_RELOAD_CONFIG   ("Reload configuration")
#define STR_ROOTMENU_REDRAW_ALL      ("Redraw all windows")
#define STR_ROOTMENU_EXIT            ("Exit")

/* Per-window context menu (src/menu/context/wincmenu.c) */
#define STR_WINCMENU_SEND_TO_DESKTOP       ("Send to desktop")
#define STR_WINCMENU_LAYER                 ("Layer")
#define STR_WINCMENU_LAYER_ALWAYS_ON_TOP    ("Always on top")
#define STR_WINCMENU_LAYER_NORMAL           ("Normal")
#define STR_WINCMENU_LAYER_ALWAYS_ON_BOTTOM ("Always on bottom")
#define STR_WINCMENU_ALL_DESKTOPS_PIN       ("All desktops (pin)")
#define STR_WINCMENU_THIS_DESKTOP_UNPIN     ("This desktop only (unpin)")
#define STR_WINCMENU_RESTORE         ("Restore")
#define STR_WINCMENU_MOVE            ("Move")
#define STR_WINCMENU_RESIZE          ("Resize")
#define STR_WINCMENU_ICONIFY         ("Iconify")
#define STR_WINCMENU_HIDE            ("Hide")
#define STR_WINCMENU_MAXIMIZE        ("Maximize")
#define STR_WINCMENU_FULLSCREEN_ENTER ("Fullscreen")
#define STR_WINCMENU_FULLSCREEN_EXIT  ("Exit Fullscreen")
#define STR_WINCMENU_SHADE            ("Shade")
#define STR_WINCMENU_UNSHADE          ("Unshade")
#define STR_WINCMENU_DECORATE         ("Decorate")
#define STR_WINCMENU_UNDECORATE       ("Undecorate")
#define STR_WINCMENU_CLOSE           ("Close")

/* All-desktops window list (src/menu/context/winlist.c) */
#define STR_WINLIST_GO_THERE         ("Go there...")

/* Quit-confirmation dialog (src/menu/dialog/quit.c); the prompt is a
 * format string taking the window manager's own display name (see
 * 'WM_EWMH_NAME' in defs/ewmh.h) */
#define STR_DIALOG_QUIT_PROMPT_FMT   ("Are you sure you want to exit %s?")
#define STR_DIALOG_QUIT_CANCEL       ("[ Cancel ]")
#define STR_DIALOG_QUIT_EXIT         ("[ Exit ]")


#endif  /* ! DEFS_UISTR_H */
