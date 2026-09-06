/**
 * @file defs/uistr.h
 *
 * @brief Every user-facing text string in the window manager, in one
 *        place
 *
 * Menu labels, dialog prompts, and button labels live here as plain
 * macros, not scattered as literals through the source files that use
 * them as though this were code from the pre-coding-standards era.
 * This is groundwork for @c gettext.  Once wired in, each of these
 * becomes an argument to a translation lookup (e.g., @c _(STR_FOO))
 * instead of a raw literal, and this file becomes the single place
 * a translator's @c .pot extraction pass has to look at.
 *
 * @c LOGGER_* messages are deliberately excluded, for they are
 * diagnostic text for whoever is running or debugging the window
 * manager, not something an end user reads, so they stay as plain
 * literals at their call sites.  The handful of literals in @c main.c
 * (command-line usage text) are excluded the same way, since they serve
 * the same diagnostic-audience role @c LOGGER_* messages do.
 *
 * @ingroup defs
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


/* Message dialog ('src/menu/dialog/message.c').  Its single dismiss
 * button, shared by every caller (an alert-level message, the
 * keyboard-shortcuts list, the fortune easter egg) */
#define STR_DIALOG_MSG_LABEL_OK "[ OK ]"

/* Window inspector dialog ('src/menu/dialog/inspect.c') */
/** @{ */
#define STR_INSPECT_GROUP_IDENTITY  "Identity"
#define STR_INSPECT_GROUP_PLACEMENT "Placement"
#define STR_INSPECT_GROUP_STATE     "State"
#define STR_INSPECT_GROUP_SIZE      "Size hints"
#define STR_INSPECT_GROUP_RELATIONS "Relations"
#define STR_INSPECT_GROUP_PROTOCOLS "Protocols"
#define STR_INSPECT_GROUP_WINDOWS   "Windows"

#define STR_INSPECT_TITLE       "Title"
#define STR_INSPECT_CLASS       "Class"
#define STR_INSPECT_ROLE        "Role"
#define STR_INSPECT_TYPE        "Type"
#define STR_INSPECT_PROCESS     "Process"
#define STR_INSPECT_DESKTOP     "Desktop"
#define STR_INSPECT_SCREEN      "Screen"
#define STR_INSPECT_MONITOR     "Monitor"
#define STR_INSPECT_GEOMETRY    "Geometry"
#define STR_INSPECT_FRAME_EXT   "Frame extents"
#define STR_INSPECT_LAYER       "Layer"
#define STR_INSPECT_IS          "Is"
#define STR_INSPECT_IS_NOT      "Is not"
#define STR_INSPECT_MINIMUM     "Minimum"
#define STR_INSPECT_MAXIMUM     "Maximum"
#define STR_INSPECT_TRANSIENT   "Transient for"
#define STR_INSPECT_TRANSIENTS  "Transients"
#define STR_INSPECT_GROUP_LEAD  "Group leader"
#define STR_INSPECT_SUPPORTS    "Supports"
#define STR_INSPECT_CLIENT_WIN  "Client"
#define STR_INSPECT_FRAME_WIN   "Frame"
#define STR_INSPECT_TITLEBAR    "Titlebar"
#define STR_INSPECT_ICON_WIN    "Icon window"

/** Shown where a property the inspector lists is absent */
#define STR_INSPECT_NONE        "(none)"
/** Shown where a client sets no upper bound on its own size */
#define STR_INSPECT_UNLIMITED   "unlimited"
/** Appended to the desktop of a client pinned to every one of them */
#define STR_INSPECT_PINNED_ALL  "pinned to all"
/** @} */

/** How to reach the lines a message dialog had no room to show, given
 *  beside the count of the ones it did */
#define STR_DIALOG_MSG_SCROLL_HINT \
    "Up/Down; PgUp/PgDown; scroll wheel"

/* Root menu ('src/menu/context/rootmenu.c').  The fixed footer entries
 * every root menu gets, below whatever 'menus.json' configures */
#define STR_ROOTMENU_STRUTLESS_MAXIMIZATION "Strutless maximization"
#define STR_ROOTMENU_STRUTTED_MAXIMIZATION "Strutted maximization"
#define STR_ROOTMENU_REARRANGE "Rearrange windows"
#define STR_ROOTMENU_RELOAD_CONFIG "Reload configuration"
#define STR_ROOTMENU_REDRAW_ALL "Redraw all windows"
#define STR_ROOTMENU_EXIT "Exit"

/* Per-window context menu ('src/menu/context/wincmenu.c') */
#define STR_WINCMENU_SEND_TO_DESKTOP "Send to desktop"
#define STR_WINCMENU_SEND_TO_MONITOR "Send to monitor"
#define STR_WINCMENU_SEND_TO_PAGE "Send to page"
#define STR_WINCMENU_PAGE "Page %u,%u"
#define STR_WINCMENU_STICK "Sticky"
#define STR_WINCMENU_UNSTICK "Unsticky"
#define STR_WINCMENU_LAYER "Layer"
#define STR_WINCMENU_LAYER_ALWAYS_ON_TOP "Always on top"
#define STR_WINCMENU_LAYER_NORMAL "Normal"
#define STR_WINCMENU_LAYER_ALWAYS_ON_BOTTOM "Always on bottom"
#define STR_WINCMENU_ALL_DESKTOPS_PIN "All desktops (pin)"
#define STR_WINCMENU_THIS_DESKTOP_UNPIN "This desktop only (unpin)"
#define STR_WINCMENU_RESTORE "Restore"
#define STR_WINCMENU_MOVE "Move"
#define STR_WINCMENU_RESIZE "Resize"
#define STR_WINCMENU_ICONIFY "Iconify"
#define STR_WINCMENU_HIDE "Hide"
#define STR_WINCMENU_MAXIMIZE "Maximize"
#define STR_WINCMENU_FULLSCREEN_ENTER "Fullscreen"
#define STR_WINCMENU_UNFULLSCREEN "Unfullscreen"
#define STR_WINCMENU_SHADE "Shade"
#define STR_WINCMENU_UNSHADE "Unshade"
#define STR_WINCMENU_DECORATE "Decorate"
#define STR_WINCMENU_UNDECORATE "Undecorate"
/** Window menu entry opening the property inspector */
#define STR_WINCMENU_INSPECT "Inspect..."

#define STR_WINCMENU_CLOSE "Close"

/* All-desktops window list ('src/menu/context/winlist.c') */
#define STR_WINLIST_GO_THERE "Go there..."
#define STR_WINLIST_DESKTOP_ADD "Add new desktop"
#define STR_WINLIST_DESKTOP_REMOVE "Remove last desktop"
#define STR_WINLIST_NO_WINDOWS "(no windows)"
#define STR_WINLIST_UNRESPONSIVE_SUFFIX " (not responding)"

/* Quit-confirmation dialog ('src/menu/dialog/quit.c'); the prompt is
 * a format string taking the window manager's display name (see
 * 'WM_EWMH_NAME' in 'defs/ewmh.h') */
#define STR_DIALOG_QUIT_PROMPT_FMT "Are you sure you want to exit %s?"
#define STR_DIALOG_QUIT_CANCEL "[ Cancel ]"
#define STR_DIALOG_QUIT_EXIT "[ Exit ]"

/* Generic confirm dialog's optional countdown line
 * ('src/menu/dialog/confirm.c'), shown under the prompt whenever
 * a timeout was given to 'menu_confirm_dialog_show'.  Names the cancel
 * button's label specifically (never assumed to literally read
 * "Cancel"; see, e.g., 'STR_DIALOG_RANDR_CONFIRM_CANCEL' below), since
 * the countdown always takes that path once it elapses regardless of
 * which button a user may have tabbed the visible selection to in the
 * meantime (a safety timeout has to fall back to the one path that
 * needs no working display to have been chosen deliberately).  A format
 * string taking the cancel button's label, then the whole seconds
 * remaining, updated once a second as it counts down.  The '%s'
 * precision is capped at 255 ((DIALOG_TEXT_MAX_LEN - 1), the most
 * a label can ever actually hold) explicitly, in the format string
 * itself rather than left to be inferred from the caller's buffer:
 * GCC's '-Wformat-truncation' cannot prove that a label reached
 * through a struct pointer is null-terminated within its declared
 * array bound rather than somewhere later in the struct, so without
 * this it assumes the width of every field after it too. */
#define STR_DIALOG_CONFIRM_TIMEOUT_FMT \
    "Automatically selecting '%.255s' in %d second(s)"

/* RandR output-profile confirm dialog ('src/menu/dialog/ rrsafe.c'),
 * shown after 'surface_action_apply_randr_profiles' is called from
 * a configuration reload (see 'wm_action_config_reload'), never at
 * startup or on a hotplug 'OUTPUT_CHANGE' */
#define STR_DIALOG_RANDR_CONFIRM_PROMPT \
    "The 'randr.json' configuration has been applied.  Keep it, or " \
    "revert to the previous one?"
#define STR_DIALOG_RANDR_CONFIRM_CANCEL "[ Revert ]"
#define STR_DIALOG_RANDR_CONFIRM_OK "[ Keep ]"

/* Keyboard-shortcuts list dialog ('src/menu/dialog/shortcuts.c').  One
 * section header, and one label per action, each paired at runtime with
 * that action's configured key combo.  Deliberately the bare
 * section name alone, without the surrounding brackets that appear
 * around it in the dialog itself: those are fixed, structural
 * formatting the code itself applies (see 's_append_line''s
 * "[%s]" format string, menu/dialog/shortcuts.c), not part of the
 * translatable content, so a locale only ever needs to translate the
 * word itself, never remember to also carry the brackets along with
 * it. */
#define STR_SHORTCUTS_HEADER_WM "Window Manager"
#define STR_SHORTCUTS_HEADER_DESKTOP "Desktop"
#define STR_SHORTCUTS_HEADER_LAUNCH "Launch"
#define STR_SHORTCUTS_HEADER_WINDOW "Window"
#define STR_SHORTCUTS_HEADER_CYCLE "Cycle"
#define STR_SHORTCUTS_HEADER_VIEWPORT "Viewport"

#define STR_SHORTCUTS_ROOT_MENU "Root menu"
#define STR_SHORTCUTS_WINDOWS_MENU "Windows menu"
#define STR_SHORTCUTS_SEARCH_WINDOWS "Search windows"
#define STR_SHORTCUTS_SHOW_DESKTOP "Show desktop"
#define STR_SHORTCUTS_REDRAW "Redraw"
#define STR_SHORTCUTS_RELOAD_CONFIG "Reload configuration"
#define STR_SHORTCUTS_QUIT "Quit"
#define STR_SHORTCUTS_THIS_LIST "This list"
#define STR_SHORTCUTS_FORTUNE "Fortune"
#define STR_SHORTCUTS_SCRATCHPAD "Scratchpad"
#define STR_SHORTCUTS_DESKTOP_ADD "Add desktop"
#define STR_SHORTCUTS_DESKTOP_REMOVE "Remove desktop"
#define STR_SHORTCUTS_TOGGLE_STRUTLESS_MAXIMIZE \
    "Toggle strutless maximization"
/* Whole, fixed line on its own (no combo of its own to pair with; the
 * combo is itself hardcoded, not user-configurable) */
#define STR_SHORTCUTS_EMERGENCY_EXIT "Emergency exit: Ctrl+Mod1+Backspace"

#define STR_SHORTCUTS_TERMINAL "Terminal"
#define STR_SHORTCUTS_LAUNCHER "Launcher"
#define STR_SHORTCUTS_FILE_MANAGER "File manager"
#define STR_SHORTCUTS_WEB_BROWSER "Web browser"
#define STR_SHORTCUTS_EDITOR "Editor"

#define STR_SHORTCUTS_CLOSE "Close"
#define STR_SHORTCUTS_KILL "Kill"
#define STR_SHORTCUTS_DECORATE "Decorate"
#define STR_SHORTCUTS_FULLSCREEN "Fullscreen"
#define STR_SHORTCUTS_HIDE "Hide"
#define STR_SHORTCUTS_ICONIFY "Iconify"
#define STR_SHORTCUTS_ICONIFY_ALL "Iconify all"
#define STR_SHORTCUTS_DEICONIFY_ALL "Deiconify all"
#define STR_SHORTCUTS_ARRANGE "Arrange"
#define STR_SHORTCUTS_INFO "Info"
#define STR_SHORTCUTS_INSPECT "Inspect window"
#define STR_SHORTCUTS_LAYER "Layer"
#define STR_SHORTCUTS_MAXIMIZE "Maximize"
#define STR_SHORTCUTS_MONITOR_NORTH "Send to monitor to the north"
#define STR_SHORTCUTS_MONITOR_SOUTH "Send to monitor to the south"
#define STR_SHORTCUTS_MONITOR_EAST "Send to monitor to the east"
#define STR_SHORTCUTS_MONITOR_WEST "Send to monitor to the west"
#define STR_SHORTCUTS_SEND_TO_DESKTOP_NORTH "Send to desktop to the north"
#define STR_SHORTCUTS_SEND_TO_DESKTOP_SOUTH "Send to desktop to the south"
#define STR_SHORTCUTS_SEND_TO_DESKTOP_EAST "Send to desktop to the east"
#define STR_SHORTCUTS_SEND_TO_DESKTOP_WEST "Send to desktop to the west"
#define STR_SHORTCUTS_PIN "Pin"
/** Not to be confused with @c STR_SHORTCUTS_PIN above; see
 *  @c CLIENT_FLAG_STICKY's comment in @c client/state.h for the full
 *  distinction between the two */
#define STR_SHORTCUTS_STICKY "Sticky"
#define STR_SHORTCUTS_SHADE "Shade"

/* Group labels; each pairs with a short direction/position name below,
 * joined at runtime as, e.g., "Right=<combo>, Left=<combo>" */
#define STR_SHORTCUTS_MOVE_RELATIVE "Move (relative)"
#define STR_SHORTCUTS_MOVE_ABSOLUTE "Move (absolute)"
#define STR_SHORTCUTS_RESIZE "Resize"
#define STR_SHORTCUTS_DESKTOPS "Desktops"
#define STR_SHORTCUTS_ICONS "Icons"
#define STR_SHORTCUTS_WINDOWS "Windows"
#define STR_SHORTCUTS_VIEWPORT_PAN "Pan"
#define STR_SHORTCUTS_VIEWPORT_PAGE "Page"

/* Deliberately NOT translated, unlike every label above: 'Right',
 * 'Left', 'Up', 'Down', 'Center', 'TopLeft', 'TopRight', 'BotLeft',
 * 'BotRight', 'prev', and 'next' each sit directly beside the literal,
 * never-translated key combo they name (e.g., 'Right=mod1+ Right'), so
 * they stay as plain literals at their call site in 'shortcuts.c'
 * instead of living here; translating only one half of that pairing
 * would read as more inconsistent than helpful. */

/* TRANSLATION: '%.*s' and '<0-9>' together spell out a shared key combo
 * prefix followed by a literal digit placeholder; '%u' and the second
 * '%s' are a desktop's index and its combo.  Keep every
 * placeholder, in this exact order, in translation. */
#define STR_SHORTCUTS_GOTO_DESKTOP_RANGE "Go to desktop 0-9"
#define STR_SHORTCUTS_GOTO_DESKTOP_FMT "Go to desktop %u"

/* TRANSLATION: same placeholder rules as the desktop pair above,
 * just for the viewport's own page range instead of a desktop's */
#define STR_SHORTCUTS_GOTO_VIEWPORT_RANGE "Go to viewport page 0-9"
#define STR_SHORTCUTS_GOTO_VIEWPORT_FMT "Go to viewport page %u"

/* Cross-desktop urgency notification ('src/desktop/dclient.c',
 * 'desktop_action_recompute_urgent'): shown, via the shared message
 * dialog ('menu/dialog/message.h') at 'MENU_MSG_LEVEL_INFO', when
 * a client becomes urgent on a desktop other than the one currently
 * visible on its own surface (see 'desktops.notify-activity' in
 * config.json).
 *
 * TRANSLATION: keep every '%u' (a desktop's index, or, only in
 * the surface-suffix variant, a surface's index) */
#define STR_DESKTOP_ACTIVITY_UNNAMED_FMT \
    "Detected activity on desktop [%u]"

/* Appended right after the message above, only when the desktop that
 * had activity actually has a name of its own set; kept as its
 * separate, tiny translatable string instead of a second, almost
 * entirely duplicate whole-sentence one, the same reasoning already
 * applied to the surface-suffix variant right below it.
 *
 * TRANSLATION: keep the '%s' (a desktop's name) */
#define STR_DESKTOP_ACTIVITY_NAME_SUFFIX_FMT " -- %s"

/* Appended after the base message above and, if the desktop that had
 * activity has a name of its own, the name suffix right above this
 * one too; only when more than one surface is managed (a
 * single-surface setup, by far the common case, has nothing
 * to disambiguate) */
#define STR_DESKTOP_ACTIVITY_SURFACE_SUFFIX_FMT " (on surface %u)"

/* Battery indicator ('src/systray/battery.c') */
#define STR_BATTERY_NOT_AVAILABLE "N/A"
#define STR_BATTERY_FULL_AC "Full, AC"
#define STR_BATTERY_FULL "Full"

/* The percentage itself, formatted on its own before being embedded
 * into any of the longer strings below (e.g., before 'STR_BATTERY_AC'
 * to form "34% AC"): kept as its translatable format string,
 * separate from where it gets used, since whether the '%' sign sits
 * flush against the number or has a space before it is a per-language
 * typographic convention, not something a single hardcoded "%u%%" can
 * get right for every locale at once. */
#define STR_BATTERY_PERCENT "%u%%"

/* Standalone, appended after a percentage (e.g., "34% AC").  Kept as
 * its own string, separate from 'STR_BATTERY_FULL_AC' above, since
 * a translation cannot derive one from the other by substring; some
 * languages place the qualifier before the percentage, or use an
 * entirely different word (or word order) for "on AC power" versus
 * "fully charged, on AC power" */
#define STR_BATTERY_AC "AC"

/* Restricted-memory mode announcement, shown once at startup
 * ('src/wm.c') */
#define STR_WM_RESTRICTED_MEMORY_MODE_ANNOUNCE \
    "IcoWM is running in restricted-memory mode.  In this mode: " \
    "application icons are shown without their own picture, text is " \
    "drawn with simpler fonts, and there is a limit on how many " \
    "windows can be open at once.  All of this trades some visual " \
    "polish for keeping memory use low and predictable."

/* Configuration-file syntax-error dialog ('src/wm.c',
 * 'wm_json_syntax_errors_warn'), one file or several.
 *
 * TRANSLATION: keep the single '%s' in the first, and the trailing
 * '%s' in the second, after which a comma-separated filename list is
 * appended */
#define STR_WM_JSON_SYNTAX_ERROR_SINGLE_FMT \
    "Error parsing '%s'; possible syntax error.  Reverted to " \
    "default values."
#define STR_WM_JSON_SYNTAX_ERROR_MULTIPLE_FMT \
    "Error parsing the following file(s); possible syntax " \
    "error(s).  Reverted to default values for each: '%s'"

/* Appended to either message above when the theme file itself was also
 * missing.
 *
 * TRANSLATION: keep the single '%s' (the theme filename), and
 * 'config.json' unchanged (a real filename, not prose) */
#define STR_WM_MISSING_THEME_FMT \
    "  Additionally, the theme file '%s' named by 'config.json' was " \
    "not found; using the built-in default theme instead."

/* Restricted-memory mode's two warning dialogs ('src/memguard.c').
 *
 * TRANSLATION: keep every '%u' (a MiB count, or a window count) and the
 * literal '-M' (the command-line option's name, unchanged in every
 * language) */
#define STR_MEMGUARD_CEILING_REACHED_FMT \
    "IcoWM has reached its configured memory ceiling: using %u " \
    "MiB of the %u MiB allowed (see the '-M' command-line " \
    "option).  Each window IcoWM manages adds to its own memory " \
    "use, regardless of that window's own application.  Close a " \
    "window before opening more."
#define STR_MEMGUARD_CLIENT_CAP_REACHED_FMT \
    "IcoWM is running in restricted-memory mode and will not manage " \
    "more than %u window(s) at once (see the '-M' command-line " \
    "option).  Close a window before opening another."

/* Fuzzy window-search widget ('src/menu/search.c').  Shown instead of
 * opening the widget itself when there is nothing to search for */
#define STR_SEARCH_NO_WINDOWS \
    "There are no open windows in this session at the moment."

/* Fuzzy window-search widget: the desktop label shown for a pinned
 * result instead of any one specific desktop's name or number,
 * since a pinned client is not really on any one of them in
 * particular.  Deliberately distinct from the desktop label being
 * left blank entirely, the way it already is whenever a session has
 * only a single desktop: shown for a pinned client on a session with
 * more than one, so the two cases ("nothing to disambiguate" and
 * "this one client is pinned across all of them") never look
 * identical (a blank space) to someone reading the search results. */
#define STR_SEARCH_ALL_DESKTOPS "On all desktops"

/* Built-in run-box ('src/menu/dialog/run.c').  The prompt preceding its
 * own text field.  Kept short and distinct from 'STR_SEARCH_NO_WINDOWS'
 * above so the two widgets, easy to confuse at a glance since both are
 * a single centered text field, never look alike. */
#define STR_RUN_PROMPT "Run:"

/* Built-in run-box: shown (as an informational dialog, never a blocking
 * warning or error) when the entered command could not be found or
 * executed.
 *
 * TRANSLATION: keep the single '%s' (the command as typed) */
#define STR_RUN_COMMAND_NOT_FOUND_FMT \
    "Command '%s' not found."

/* 'cctl_launch_dispatch' (src/cctl/launch.c).  Shown as a blocking
 * warning dialog, unlike 'STR_RUN_COMMAND_NOT_FOUND_FMT' above, when
 * a keybind-triggered program (e.g., 'programs.terminal') could not be
 * found or executed.  Unlike the run-box, where a bad command is a
 * one-off typo the user just made, this always means the very same
 * configured program will keep failing every single time that same
 * keybind is pressed again until 'config.json' itself is fixed, which
 * is worth calling more attention to.
 *
 * TRANSLATION: keep the single '%s' (the configured command) */
#define STR_LAUNCH_COMMAND_NOT_FOUND_FMT \
    "Failed to execute '%s': command not found."

/* Fortune easter egg ('src/menu/dialog/fortune.c').  Shown instead when
 * the configured 'fortune.command' is missing or produces no output;
 * deliberately overwrought and archaic, per its whole point being
 * a small joke rather than a plain error message */
#define STR_FORTUNE_FALLBACK \
    "Alack!  The oracle 'fortune' abideth not upon this machine, " \
    "wherefore no wisdom of the ancients may this day be divined.  " \
    "Prithee, entreat thy package steward with an incantation such " \
    "as 'sudo apt install fortune-mod' (or whate'er charm thy " \
    "distribution demandeth), that the sages of yore might once " \
    "more speak through this humble dialog."


#endif  /* ! DEFS_UISTR_H */
