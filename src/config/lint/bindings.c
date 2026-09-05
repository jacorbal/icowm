/**
 * @file config/lint/bindings.c
 *
 * @brief 'bindings.json' schema
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* Local includes */
#include <config/lint/internal.h>
#include <config/lint/bindings.h>


static const config_lint_key_td s_schema_wm_menus[] = {
    {"root", NULL, 0u},
    {"windows", NULL, 0u}
};

static const config_lint_key_td s_schema_go_to[] = {
    {"desktop0", NULL, 0u}, {"desktop1", NULL, 0u},
    {"desktop2", NULL, 0u}, {"desktop3", NULL, 0u},
    {"desktop4", NULL, 0u}, {"desktop5", NULL, 0u},
    {"desktop6", NULL, 0u}, {"desktop7", NULL, 0u},
    {"desktop8", NULL, 0u}, {"desktop9", NULL, 0u}
};

static const config_lint_key_td s_schema_kb_wm[] = {
    {"menus", s_schema_wm_menus,
        sizeof(s_schema_wm_menus) / sizeof(s_schema_wm_menus[0])},
    {"search", NULL, 0u},
    {"scratchpad", NULL, 0u},
    {"redraw", NULL, 0u},
    {"reload", NULL, 0u},
    {"quit", NULL, 0u},
    {"shortcuts", NULL, 0u},
    {"fortune", NULL, 0u},
    {"toggle-strutless-maximization", NULL, 0u}
};

static const config_lint_key_td s_schema_kb_desktop[] = {
    {"add", NULL, 0u},
    {"remove", NULL, 0u},
    {"show", NULL, 0u},
    {"go-to", s_schema_go_to,
        sizeof(s_schema_go_to) / sizeof(s_schema_go_to[0])}
};

static const config_lint_key_td s_schema_kb_launch[] = {
    {"terminal", NULL, 0u},
    {"launcher", NULL, 0u},
    {"file-manager", NULL, 0u},
    {"web-browser", NULL, 0u},
    {"editor", NULL, 0u}
};

static const config_lint_key_td s_schema_move_relative[] = {
    {"right", NULL, 0u}, {"left", NULL, 0u},
    {"up", NULL, 0u}, {"down", NULL, 0u}
};

static const config_lint_key_td s_schema_move_absolute[] = {
    {"center", NULL, 0u},
    {"top-left", NULL, 0u}, {"top-right", NULL, 0u},
    {"bottom-left", NULL, 0u}, {"bottom-right", NULL, 0u}
};

static const config_lint_key_td s_schema_window_move[] = {
    {"relative", s_schema_move_relative,
        sizeof(s_schema_move_relative) / sizeof(s_schema_move_relative[0])},
    {"absolute", s_schema_move_absolute,
        sizeof(s_schema_move_absolute) / sizeof(s_schema_move_absolute[0])}
};

static const config_lint_key_td s_schema_prev_next[] = {
    {"prev", NULL, 0u},
    {"next", NULL, 0u}
};

static const config_lint_key_td s_schema_compass[] = {
    {"north", NULL, 0u},
    {"south", NULL, 0u},
    {"east", NULL, 0u},
    {"west", NULL, 0u}
};

static const config_lint_key_td s_schema_window_send_to[] = {
    {"desktop", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0])},
    {"monitor", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0])}
};

static const config_lint_key_td s_schema_kb_window[] = {
    {"arrange", NULL, 0u},
    {"close", NULL, 0u},
    {"decorate", NULL, 0u},
    {"deiconify-all", NULL, 0u},
    {"fullscreen", NULL, 0u},
    {"hide", NULL, 0u},
    {"iconify", NULL, 0u},
    {"iconify-all", NULL, 0u},
    {"info", NULL, 0u},
    {"inspect", NULL, 0u},
    {"kill", NULL, 0u},
    {"layer", NULL, 0u},
    {"maximize", NULL, 0u},
    {"pin", NULL, 0u},
    {"shade", NULL, 0u},
    {"move", s_schema_window_move,
        sizeof(s_schema_window_move) / sizeof(s_schema_window_move[0])},
    {"resize", s_schema_move_relative,
        sizeof(s_schema_move_relative) / sizeof(s_schema_move_relative[0])},
    {"send-to", s_schema_window_send_to,
        sizeof(s_schema_window_send_to) /
            sizeof(s_schema_window_send_to[0])}
};

static const config_lint_key_td s_schema_kb_cycle[] = {
    {"desktop", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0])},
    {"icon", s_schema_prev_next,
        sizeof(s_schema_prev_next) / sizeof(s_schema_prev_next[0])},
    {"window", s_schema_prev_next,
        sizeof(s_schema_prev_next) / sizeof(s_schema_prev_next[0])}
};

static const config_lint_key_td s_schema_viewport_go_to[] = {
    {"page1", NULL, 0u}, {"page2", NULL, 0u}, {"page3", NULL, 0u},
    {"page4", NULL, 0u}, {"page5", NULL, 0u}, {"page6", NULL, 0u},
    {"page7", NULL, 0u}, {"page8", NULL, 0u}, {"page9", NULL, 0u}
};

static const config_lint_key_td s_schema_kb_viewport[] = {
    {"pan", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0])},
    {"go-to", s_schema_viewport_go_to,
        sizeof(s_schema_viewport_go_to) /
            sizeof(s_schema_viewport_go_to[0])}
};

static const config_lint_key_td s_schema_keyboard[] = {
    {"wm", s_schema_kb_wm,
        sizeof(s_schema_kb_wm) / sizeof(s_schema_kb_wm[0])},
    {"desktop", s_schema_kb_desktop,
        sizeof(s_schema_kb_desktop) / sizeof(s_schema_kb_desktop[0])},
    {"launch", s_schema_kb_launch,
        sizeof(s_schema_kb_launch) / sizeof(s_schema_kb_launch[0])},
    {"window", s_schema_kb_window,
        sizeof(s_schema_kb_window) / sizeof(s_schema_kb_window[0])},
    {"cycle", s_schema_kb_cycle,
        sizeof(s_schema_kb_cycle) / sizeof(s_schema_kb_cycle[0])},
    {"viewport", s_schema_kb_viewport,
        sizeof(s_schema_kb_viewport) /
            sizeof(s_schema_kb_viewport[0])}
};

static const config_lint_key_td s_schema_mouse_window[] = {
    {"move", NULL, 0u},
    {"lower", NULL, 0u},
    {"resize", NULL, 0u}
};

static const config_lint_key_td s_schema_mouse_cycle[] = {
    {"desktop", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0])}
};

static const config_lint_key_td s_schema_mouse[] = {
    {"window", s_schema_mouse_window,
        sizeof(s_schema_mouse_window) / sizeof(s_schema_mouse_window[0])},
    {"cycle", s_schema_mouse_cycle,
        sizeof(s_schema_mouse_cycle) / sizeof(s_schema_mouse_cycle[0])}
};

static const config_lint_key_td s_schema_modifiers[] = {
    {"modc", NULL, 0u}, {"mods", NULL, 0u}, {"modl", NULL, 0u},
    {"mod1", NULL, 0u}, {"mod2", NULL, 0u}, {"mod3", NULL, 0u},
    {"mod4", NULL, 0u}, {"mod5", NULL, 0u}
};

const config_lint_key_td s_schema_bindings[] = {
    {"modifiers", s_schema_modifiers,
        sizeof(s_schema_modifiers) / sizeof(s_schema_modifiers[0])},
    {"keyboard", s_schema_keyboard,
        sizeof(s_schema_keyboard) / sizeof(s_schema_keyboard[0])},
    {"mouse", s_schema_mouse,
        sizeof(s_schema_mouse) / sizeof(s_schema_mouse[0])}
};


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_bindings_size_check
    [(sizeof(s_schema_bindings) /
      sizeof(s_schema_bindings[0]) == CONFIG_LINT_BINDINGS_KEYS)
     ? 1 : -1];
