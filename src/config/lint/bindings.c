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
    {"root", NULL, 0u,
        0, NULL, 0u, NULL},
    {"windows", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_go_to[] = {
    {"desktop0", NULL, 0u,
        0, NULL, 0u, NULL}, {"desktop1", NULL, 0u,
        0, NULL, 0u, NULL},
    {"desktop2", NULL, 0u,
        0, NULL, 0u, NULL}, {"desktop3", NULL, 0u,
        0, NULL, 0u, NULL},
    {"desktop4", NULL, 0u,
        0, NULL, 0u, NULL}, {"desktop5", NULL, 0u,
        0, NULL, 0u, NULL},
    {"desktop6", NULL, 0u,
        0, NULL, 0u, NULL}, {"desktop7", NULL, 0u,
        0, NULL, 0u, NULL},
    {"desktop8", NULL, 0u,
        0, NULL, 0u, NULL}, {"desktop9", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_kb_wm[] = {
    {"menus", s_schema_wm_menus,
        sizeof(s_schema_wm_menus) / sizeof(s_schema_wm_menus[0]),
        0, NULL, 0u, NULL},
    {"search", NULL, 0u,
        0, NULL, 0u, NULL},
    {"scratchpad", NULL, 0u,
        0, NULL, 0u, NULL},
    {"redraw", NULL, 0u,
        0, NULL, 0u, NULL},
    {"reload", NULL, 0u,
        0, NULL, 0u, NULL},
    {"quit", NULL, 0u,
        0, NULL, 0u, NULL},
    {"shortcuts", NULL, 0u,
        0, NULL, 0u, NULL},
    {"fortune", NULL, 0u,
        0, NULL, 0u, NULL},
    {"toggle-strutless-maximization", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_kb_desktop[] = {
    {"add", NULL, 0u,
        0, NULL, 0u, NULL},
    {"remove", NULL, 0u,
        0, NULL, 0u, NULL},
    {"show", NULL, 0u,
        0, NULL, 0u, NULL},
    {"go-to", s_schema_go_to,
        sizeof(s_schema_go_to) / sizeof(s_schema_go_to[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_kb_launch[] = {
    {"terminal", NULL, 0u,
        0, NULL, 0u, NULL},
    {"launcher", NULL, 0u,
        0, NULL, 0u, NULL},
    {"file-manager", NULL, 0u,
        0, NULL, 0u, NULL},
    {"web-browser", NULL, 0u,
        0, NULL, 0u, NULL},
    {"editor", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_move_relative[] = {
    {"right", NULL, 0u,
        0, NULL, 0u, NULL}, {"left", NULL, 0u,
        0, NULL, 0u, NULL},
    {"up", NULL, 0u,
        0, NULL, 0u, NULL}, {"down", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_move_absolute[] = {
    {"center", NULL, 0u,
        0, NULL, 0u, NULL},
    {"top-left", NULL, 0u,
        0, NULL, 0u, NULL}, {"top-right", NULL, 0u,
        0, NULL, 0u, NULL},
    {"bottom-left", NULL, 0u,
        0, NULL, 0u, NULL}, {"bottom-right", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_window_move[] = {
    {"relative", s_schema_move_relative,
        sizeof(s_schema_move_relative) / sizeof(s_schema_move_relative[0]),
        0, NULL, 0u, NULL},
    {"absolute", s_schema_move_absolute,
        sizeof(s_schema_move_absolute) / sizeof(s_schema_move_absolute[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_prev_next[] = {
    {"prev", NULL, 0u,
        0, NULL, 0u, NULL},
    {"next", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_compass[] = {
    {"north", NULL, 0u,
        0, NULL, 0u, NULL},
    {"south", NULL, 0u,
        0, NULL, 0u, NULL},
    {"east", NULL, 0u,
        0, NULL, 0u, NULL},
    {"west", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_window_send_to[] = {
    {"desktop", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0]),
        0, NULL, 0u, NULL},
    {"monitor", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_kb_window[] = {
    {"arrange", NULL, 0u,
        0, NULL, 0u, NULL},
    {"close", NULL, 0u,
        0, NULL, 0u, NULL},
    {"decorate", NULL, 0u,
        0, NULL, 0u, NULL},
    {"deiconify-all", NULL, 0u,
        0, NULL, 0u, NULL},
    {"fullscreen", NULL, 0u,
        0, NULL, 0u, NULL},
    {"hide", NULL, 0u,
        0, NULL, 0u, NULL},
    {"iconify", NULL, 0u,
        0, NULL, 0u, NULL},
    {"iconify-all", NULL, 0u,
        0, NULL, 0u, NULL},
    {"info", NULL, 0u,
        0, NULL, 0u, NULL},
    {"inspect", NULL, 0u,
        0, NULL, 0u, NULL},
    {"kill", NULL, 0u,
        0, NULL, 0u, NULL},
    {"layer", NULL, 0u,
        0, NULL, 0u, NULL},
    {"maximize", NULL, 0u,
        0, NULL, 0u, NULL},
    {"pin", NULL, 0u,
        0, NULL, 0u, NULL},
    {"sticky", NULL, 0u,
        0, NULL, 0u, NULL},
    {"shade", NULL, 0u,
        0, NULL, 0u, NULL},
    {"move", s_schema_window_move,
        sizeof(s_schema_window_move) / sizeof(s_schema_window_move[0]),
        0, NULL, 0u, NULL},
    {"resize", s_schema_move_relative,
        sizeof(s_schema_move_relative) / sizeof(s_schema_move_relative[0]),
        0, NULL, 0u, NULL},
    {"send-to", s_schema_window_send_to,
        sizeof(s_schema_window_send_to) /
            sizeof(s_schema_window_send_to[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_kb_cycle[] = {
    {"desktop", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0]),
        0, NULL, 0u, NULL},
    {"icon", s_schema_prev_next,
        sizeof(s_schema_prev_next) / sizeof(s_schema_prev_next[0]),
        0, NULL, 0u, NULL},
    {"window", s_schema_prev_next,
        sizeof(s_schema_prev_next) / sizeof(s_schema_prev_next[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_viewport_go_to[] = {
    {"page0", NULL, 0u,
        0, NULL, 0u, NULL}, {"page1", NULL, 0u,
        0, NULL, 0u, NULL},
    {"page2", NULL, 0u,
        0, NULL, 0u, NULL}, {"page3", NULL, 0u,
        0, NULL, 0u, NULL},
    {"page4", NULL, 0u,
        0, NULL, 0u, NULL}, {"page5", NULL, 0u,
        0, NULL, 0u, NULL},
    {"page6", NULL, 0u,
        0, NULL, 0u, NULL}, {"page7", NULL, 0u,
        0, NULL, 0u, NULL},
    {"page8", NULL, 0u,
        0, NULL, 0u, NULL}, {"page9", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_kb_viewport[] = {
    {"pan", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0]),
        0, NULL, 0u, NULL},
    {"go-to", s_schema_viewport_go_to,
        sizeof(s_schema_viewport_go_to) /
            sizeof(s_schema_viewport_go_to[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_keyboard[] = {
    {"wm", s_schema_kb_wm,
        sizeof(s_schema_kb_wm) / sizeof(s_schema_kb_wm[0]),
        0, NULL, 0u, NULL},
    {"desktop", s_schema_kb_desktop,
        sizeof(s_schema_kb_desktop) / sizeof(s_schema_kb_desktop[0]),
        0, NULL, 0u, NULL},
    {"launch", s_schema_kb_launch,
        sizeof(s_schema_kb_launch) / sizeof(s_schema_kb_launch[0]),
        0, NULL, 0u, NULL},
    {"window", s_schema_kb_window,
        sizeof(s_schema_kb_window) / sizeof(s_schema_kb_window[0]),
        0, NULL, 0u, NULL},
    {"cycle", s_schema_kb_cycle,
        sizeof(s_schema_kb_cycle) / sizeof(s_schema_kb_cycle[0]),
        0, NULL, 0u, NULL},
    {"viewport", s_schema_kb_viewport,
        sizeof(s_schema_kb_viewport) /
            sizeof(s_schema_kb_viewport[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_mouse_window[] = {
    {"move", NULL, 0u,
        0, NULL, 0u, NULL},
    {"lower", NULL, 0u,
        0, NULL, 0u, NULL},
    {"resize", NULL, 0u,
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_mouse_cycle[] = {
    {"desktop", s_schema_compass,
        sizeof(s_schema_compass) / sizeof(s_schema_compass[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_mouse[] = {
    {"window", s_schema_mouse_window,
        sizeof(s_schema_mouse_window) / sizeof(s_schema_mouse_window[0]),
        0, NULL, 0u, NULL},
    {"cycle", s_schema_mouse_cycle,
        sizeof(s_schema_mouse_cycle) / sizeof(s_schema_mouse_cycle[0]),
        0, NULL, 0u, NULL}
};

static const config_lint_key_td s_schema_modifiers[] = {
    {"modc", NULL, 0u,
        0, NULL, 0u, NULL}, {"mods", NULL, 0u,
        0, NULL, 0u, NULL}, {"modl", NULL, 0u,
        0, NULL, 0u, NULL},
    {"mod1", NULL, 0u,
        0, NULL, 0u, NULL}, {"mod2", NULL, 0u,
        0, NULL, 0u, NULL}, {"mod3", NULL, 0u,
        0, NULL, 0u, NULL},
    {"mod4", NULL, 0u,
        0, NULL, 0u, NULL}, {"mod5", NULL, 0u,
        0, NULL, 0u, NULL}
};

const config_lint_key_td s_schema_bindings[] = {
    {"modifiers", s_schema_modifiers,
        sizeof(s_schema_modifiers) / sizeof(s_schema_modifiers[0]),
        0, NULL, 0u, NULL},
    {"keyboard", s_schema_keyboard,
        sizeof(s_schema_keyboard) / sizeof(s_schema_keyboard[0]),
        0, NULL, 0u, NULL},
    {"mouse", s_schema_mouse,
        sizeof(s_schema_mouse) / sizeof(s_schema_mouse[0]),
        0, NULL, 0u, NULL}
};


/* Size check; see 'config/lint/internal.h' for why */
typedef char config_lint_bindings_size_check
    [(sizeof(s_schema_bindings) /
      sizeof(s_schema_bindings[0]) == CONFIG_LINT_BINDINGS_KEYS)
     ? 1 : -1];
