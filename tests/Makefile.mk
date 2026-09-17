# tests/Makefile.mk
#
# Included from the project's own root Makefile (see 'include
# tests/Makefile.mk' there), not meant to be invoked on its own: every
# variable used below ('CC', 'CCSTD', 'I_DIR', 'S_DIR', 'O_DIR',
# 'TESTS_DIR', 'JSON_CFLAGS', 'JSON_LFLAGS', 'XCB_CFLAGS',
# 'XCB_LFLAGS') is defined in that root Makefile, already in scope by
# the time this file is processed.  Kept as its own file purely to
# keep the root Makefile itself short and focused on building IcoWM
# proper; nothing here changes how 'make test' behaves from the
# project root.

.PHONY: test

## Tests
#
# Each 'tests/<dir>/test_<name>.c' is its own standalone binary,
# compiled and linked directly against exactly the source files it
# actually exercises (never through '$(TARGET)' itself): always under
# 'AddressSanitizer' and 'UndefinedBehaviorSanitizer', since a test
# binary's own job is finding exactly the class of bug those catch, not
# just checking return values.  Extend 'TEST_BINS' with one more line,
# and its own explicit recipe alongside the others below, for each new
# test file; no automatic discovery, so a new test always has to be
# wired in on purpose, not silently picked up (or silently skipped) by
# a glob.
TEST_CCFLAGS = -std=$(CCSTD) -D _POSIX_C_SOURCE=200112L -g -O0 \
    -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I $(I_DIR) -I $(TESTS_DIR) $(JSON_CFLAGS) $(XCB_CFLAGS)
TEST_LDFLAGS = -fsanitize=address,undefined

TEST_BINS = $(O_DIR)/tests/adt/test_cdlist \
    $(O_DIR)/tests/adt/test_list \
    $(O_DIR)/tests/adt/test_ohtbl \
    $(O_DIR)/tests/test_logger \
    $(O_DIR)/tests/test_lookup \
    $(O_DIR)/tests/policy/test_focus \
    $(O_DIR)/tests/test_scratchpad \
    $(O_DIR)/tests/ipc/test_args \
    $(O_DIR)/tests/ipc/test_response \
    $(O_DIR)/tests/ipc/test_resolve \
    $(O_DIR)/tests/ipc/test_dispatch \
    $(O_DIR)/tests/ipc/test_commands \
    $(O_DIR)/tests/ipc/test_readable \
    $(O_DIR)/tests/wm/test_clients \
    $(O_DIR)/tests/policy/test_urgency \
    $(O_DIR)/tests/test_rules \
    $(O_DIR)/tests/input/mouse/test_bounds \
    $(O_DIR)/tests/input/mouse/test_resolve \
    $(O_DIR)/tests/input/kbd/test_resolve \
    $(O_DIR)/tests/input/kbd/test_modal \
    $(O_DIR)/tests/test_memguard \
    $(O_DIR)/tests/systray/test_text \
    $(O_DIR)/tests/systray/test_battery \
    $(O_DIR)/tests/render/test_stage \
    $(O_DIR)/tests/client/test_state \
    $(O_DIR)/tests/client/test_gravity \
    $(O_DIR)/tests/client/test_geom \
    $(O_DIR)/tests/client/test_props \
    $(O_DIR)/tests/test_client \
    $(O_DIR)/tests/test_stage_lifecycle \
    $(O_DIR)/tests/cmds/client/test_flags \
    $(O_DIR)/tests/cmds/client/test_state \
    $(O_DIR)/tests/cmds/client/test_layer \
    $(O_DIR)/tests/cmds/client/test_visibility \
    $(O_DIR)/tests/cmds/client/test_workarea \
    $(O_DIR)/tests/cmds/client/test_maximize \
    $(O_DIR)/tests/cmds/client/test_transient \
    $(O_DIR)/tests/input/mouse/drag/test_resist \
    $(O_DIR)/tests/input/mouse/drag/test_snap \
    $(O_DIR)/tests/input/mouse/drag/test_warp \
    $(O_DIR)/tests/input/mouse/drag/test_pan \
    $(O_DIR)/tests/input/mouse/drag/test_background \
    $(O_DIR)/tests/input/mouse/test_viewport_edge \
    $(O_DIR)/tests/rules/test_apply \
    $(O_DIR)/tests/policy/test_placement \
    $(O_DIR)/tests/policy/test_tiling \
    $(O_DIR)/tests/stage/test_desktop_grid \
    $(O_DIR)/tests/stage/test_monitor_direction \
    $(O_DIR)/tests/stage/test_desktop_add_remove \
    $(O_DIR)/tests/stage/test_pinned_transfer \
    $(O_DIR)/tests/stage/test_viewport \
    $(O_DIR)/tests/render/viewport/test_mesh \
    $(O_DIR)/tests/enact/test_send_to_desktop \
    $(O_DIR)/tests/desktop/test_workarea \
    $(O_DIR)/tests/menu/context/ctxmenu/test_layout \
    $(O_DIR)/tests/menu/context/test_winlist \
    $(O_DIR)/tests/input/test_modifier \
    $(O_DIR)/tests/utils/test_geom \
    $(O_DIR)/tests/utils/test_sysmem \
    $(O_DIR)/tests/utils/safe/test_safeflg \
    $(O_DIR)/tests/utils/safe/test_safemem \
    $(O_DIR)/tests/utils/safe/test_safestr \
    $(O_DIR)/tests/utils/hash/test_murmurhash \
    $(O_DIR)/tests/utils/config/test_path \
    $(O_DIR)/tests/utils/config/test_json \
    $(O_DIR)/tests/config/test_randr \
    $(O_DIR)/tests/config/test_a11y \
    $(O_DIR)/tests/config/test_bindings \
    $(O_DIR)/tests/config/test_theme \
    $(O_DIR)/tests/config/test_memguard \
    $(O_DIR)/tests/config/test_lint \
    $(O_DIR)/tests/config/test_base \
    $(O_DIR)/tests/config/test_config \
    $(O_DIR)/tests/rules/test_match \
    $(O_DIR)/tests/cmds/client/test_grab \
    $(O_DIR)/tests/cmds/client/test_icon \
    $(O_DIR)/tests/cmds/client/test_focus \
    $(O_DIR)/tests/cmds/client/test_move \
    $(O_DIR)/tests/cmds/client/test_resize \
    $(O_DIR)/tests/cmds/client/test_ewmh \
    $(O_DIR)/tests/cmds/test_stage_desktop_switch \
    $(O_DIR)/tests/cmds/test_stage_viewport_pan \
    $(O_DIR)/tests/stage/test_workareas \
    $(O_DIR)/tests/stage/actions/test_randr \
    $(O_DIR)/tests/policy/test_ping \
    $(O_DIR)/tests/enact/test_stage \
    $(O_DIR)/tests/ipc/actions/client/test_flags \
    $(O_DIR)/tests/ipc/actions/client/test_focus \
    $(O_DIR)/tests/ipc/actions/client/test_geom \
    $(O_DIR)/tests/ipc/actions/client/test_layer \
    $(O_DIR)/tests/ipc/actions/client/test_meta \
    $(O_DIR)/tests/ipc/actions/client/test_state \
    $(O_DIR)/tests/ipc/actions/client/test_visibility \
    $(O_DIR)/tests/ipc/actions/test_wm \
    $(O_DIR)/tests/ipc/actions/test_scratchpad \
    $(O_DIR)/tests/ipc/actions/test_stage_action \
    $(O_DIR)/tests/ipc/actions/test_desktop \
    $(O_DIR)/tests/ipc/actions/test_query \
    $(O_DIR)/tests/enact/test_client \
    $(O_DIR)/tests/menu/context/ctxmenu/test_tree \
    $(O_DIR)/tests/menu/dialog/test_confirm \
    $(O_DIR)/tests/desktop/test_dclient \
    $(O_DIR)/tests/wm/test_actions \
    $(O_DIR)/tests/systray/test_layout \
    $(O_DIR)/tests/menu/test_search \
    $(O_DIR)/tests/cmds/client/test_meta \
    $(O_DIR)/tests/input/kbd/test_bind \
    $(O_DIR)/tests/input/kbd/test_event \
    $(O_DIR)/tests/input/kbd/test_execute \
    $(O_DIR)/tests/input/kbd/test_interact \
    $(O_DIR)/tests/input/kbd/test_intercept \
    $(O_DIR)/tests/input/mouse/drag/test_drag \
    $(O_DIR)/tests/input/mouse/drag/test_icon \
    $(O_DIR)/tests/input/mouse/drag/test_outline \
    $(O_DIR)/tests/input/mouse/drag/test_overlay \
    $(O_DIR)/tests/input/mouse/event/test_enter \
    $(O_DIR)/tests/input/mouse/event/test_overlay \
    $(O_DIR)/tests/input/mouse/event/test_press \
    $(O_DIR)/tests/input/mouse/event/test_release \
    $(O_DIR)/tests/input/mouse/event/test_scroll \
    $(O_DIR)/tests/input/mouse/event/test_titlebar \
    $(O_DIR)/tests/input/mouse/test_bind \
    $(O_DIR)/tests/input/mouse/test_cursor \
    $(O_DIR)/tests/input/mouse/test_hover \
    $(O_DIR)/tests/menu/context/ctxmenu/test_handle \
    $(O_DIR)/tests/menu/context/ctxmenu/test_redraw \
    $(O_DIR)/tests/menu/context/ctxmenu/test_select \
    $(O_DIR)/tests/menu/context/test_ctxmenu \
    $(O_DIR)/tests/menu/context/test_menujson \
    $(O_DIR)/tests/menu/context/test_rootmenu \
    $(O_DIR)/tests/menu/context/test_wincmenu \
    $(O_DIR)/tests/menu/context/submenu/test_desktop \
    $(O_DIR)/tests/menu/context/submenu/test_monitor \
    $(O_DIR)/tests/menu/context/submenu/test_page \
    $(O_DIR)/tests/menu/cycle/test_draw \
    $(O_DIR)/tests/menu/dialog/test_fortune \
    $(O_DIR)/tests/menu/dialog/test_info \
    $(O_DIR)/tests/menu/dialog/test_inspect \
    $(O_DIR)/tests/menu/dialog/test_message \
    $(O_DIR)/tests/menu/dialog/test_quit \
    $(O_DIR)/tests/menu/dialog/test_rrsafe \
    $(O_DIR)/tests/menu/dialog/test_run \
    $(O_DIR)/tests/menu/dialog/test_shortcuts \
    $(O_DIR)/tests/menu/notify/test_desktop \
    $(O_DIR)/tests/menu/test_cycle \
    $(O_DIR)/tests/menu/test_dialog \
    $(O_DIR)/tests/menu/test_draw \
    $(O_DIR)/tests/menu/test_notify \
    $(O_DIR)/tests/menu/test_popup \
    $(O_DIR)/tests/systray/test_protocol \
    $(O_DIR)/tests/test_enact \
    $(O_DIR)/tests/test_ipc \
    $(O_DIR)/tests/test_systray \
    $(O_DIR)/tests/utils/test_cursor \
    $(O_DIR)/tests/utils/test_spawn \
    $(O_DIR)/tests/utils/xcb/test_pixmap \
    $(O_DIR)/tests/utils/xcb/test_selection \
    $(O_DIR)/tests/utils/xcb/test_wait \
    $(O_DIR)/tests/wm/test_ewmh \
    $(O_DIR)/tests/wm/test_lifecycle \
    $(O_DIR)/tests/wm/test_shutdown \
    $(O_DIR)/tests/wm/test_startup \
    $(O_DIR)/tests/cctl/test_adopt \
    $(O_DIR)/tests/cctl/test_kill \
    $(O_DIR)/tests/cctl/test_launch \
    $(O_DIR)/tests/cctl/test_sn \
    $(O_DIR)/tests/handler/test_colormap \
    $(O_DIR)/tests/handler/test_configure \
    $(O_DIR)/tests/handler/test_crossing \
    $(O_DIR)/tests/handler/test_error \
    $(O_DIR)/tests/handler/test_ewmh \
    $(O_DIR)/tests/handler/test_expose \
    $(O_DIR)/tests/handler/test_focus \
    $(O_DIR)/tests/handler/test_map \
    $(O_DIR)/tests/handler/test_message \
    $(O_DIR)/tests/handler/test_randr \
    $(O_DIR)/tests/handler/test_selection \
    $(O_DIR)/tests/handler/test_sync \
    $(O_DIR)/tests/loop/event/test_input \
    $(O_DIR)/tests/loop/event/test_motion \
    $(O_DIR)/tests/loop/test_context \
    $(O_DIR)/tests/loop/test_dispatch \
    $(O_DIR)/tests/loop/test_loop \
    $(O_DIR)/tests/loop/test_pollset \
    $(O_DIR)/tests/loop/test_refresh \
    $(O_DIR)/tests/loop/test_signals \
    $(O_DIR)/tests/loop/test_timers \
    $(O_DIR)/tests/render/client/test_decoration \
    $(O_DIR)/tests/render/client/test_titlebar \
    $(O_DIR)/tests/render/desktop/test_background \
    $(O_DIR)/tests/render/test_desktop \
    $(O_DIR)/tests/render/test_glyph \
    $(O_DIR)/tests/render/test_icon \
    $(O_DIR)/tests/render/test_text \
    $(O_DIR)/tests/render/test_wmicon \
    $(O_DIR)/tests/test_main \
    $(O_DIR)/tests/test_session \
    $(O_DIR)/tests/test_xsettings \
    $(O_DIR)/tests/wm/startup/test_handle \
    $(O_DIR)/tests/wm/startup/test_install \
    $(O_DIR)/tests/wm/startup/test_selection \
    $(O_DIR)/tests/wm/startup/test_subscribe

test: $(TEST_BINS)
	@status=0; \
	for t in $(TEST_BINS); do \
		echo "== $$t =="; \
		"$$t" || status=1; \
		echo; \
	done; \
	exit $$status

$(O_DIR)/tests/adt/test_cdlist: $(TESTS_DIR)/adt/test_cdlist.c \
		$(S_DIR)/adt/cdlist.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/adt/test_list: $(TESTS_DIR)/adt/test_list.c \
		$(S_DIR)/adt/list.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/adt/test_ohtbl: $(TESTS_DIR)/adt/test_ohtbl.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/test_logger: $(TESTS_DIR)/test_logger.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/test_lookup: $(TESTS_DIR)/test_lookup.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/policy/test_focus: $(TESTS_DIR)/policy/test_focus.c \
		$(S_DIR)/policy/focus.c \
		$(S_DIR)/adt/cdlist.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/test_scratchpad: $(TESTS_DIR)/test_scratchpad.c \
		$(S_DIR)/scratchpad.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/utils/safe/safeflg.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/ipc/test_args: $(TESTS_DIR)/ipc/test_args.c \
		$(S_DIR)/ipc/args.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/test_response: $(TESTS_DIR)/ipc/test_response.c \
		$(S_DIR)/ipc/response.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/test_resolve: $(TESTS_DIR)/ipc/test_resolve.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/test_dispatch: $(TESTS_DIR)/ipc/test_dispatch.c \
		$(S_DIR)/ipc/dispatch.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/test_commands: $(TESTS_DIR)/ipc/test_commands.c \
		$(S_DIR)/ipc/commands.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/test_readable: $(TESTS_DIR)/ipc/test_readable.c \
		$(S_DIR)/ipc.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/config/path.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/wm/test_clients: $(TESTS_DIR)/wm/test_clients.c \
		$(S_DIR)/stage/desktops.c \
		$(S_DIR)/wm/client.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/cmds/client/ewmh.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/utils/xcb/connection.c \
		$(S_DIR)/utils/xcb/window.c \
		$(S_DIR)/utils/xcb/atom.c \
		$(S_DIR)/utils/xcb/reply.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/policy/test_urgency: $(TESTS_DIR)/policy/test_urgency.c \
		$(S_DIR)/utils/xcb/connection.c \
		$(S_DIR)/policy/urgency.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lxcb

$(O_DIR)/tests/test_rules: $(TESTS_DIR)/test_rules.c \
		$(S_DIR)/rules.c \
		$(S_DIR)/rules/match.c \
		$(S_DIR)/config.c \
		$(S_DIR)/config/base/parse.c \
		$(S_DIR)/config/base/desktops.c \
		$(S_DIR)/config/base/defaults.c \
		$(S_DIR)/config/base/systray.c \
		$(S_DIR)/config/base/load.c \
		$(S_DIR)/config/theme.c \
		$(S_DIR)/config/bindings.c \
		$(S_DIR)/config/randr.c \
		$(S_DIR)/config/a11y.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/config/path.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/input/mouse/test_bounds: \
		$(TESTS_DIR)/input/mouse/test_bounds.c \
		$(S_DIR)/input/mouse/bounds.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

# Both resolvers are pure over their own binding table, so each test
# supplies that table itself rather than linking the 'bind.c' that
# only ever fills one from a live X connection.
$(O_DIR)/tests/input/mouse/test_resolve: \
		$(TESTS_DIR)/input/mouse/test_resolve.c \
		$(S_DIR)/input/mouse/resolve.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/kbd/test_resolve: \
		$(TESTS_DIR)/input/kbd/test_resolve.c \
		$(S_DIR)/input/kbd/resolve.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/kbd/test_modal: \
		$(TESTS_DIR)/input/kbd/test_modal.c \
		$(S_DIR)/input/kbd/modal.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/test_memguard: $(TESTS_DIR)/test_memguard.c \
		$(S_DIR)/memguard.c \
		$(S_DIR)/utils/time/clock.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/systray/test_text: $(TESTS_DIR)/systray/test_text.c \
		$(S_DIR)/utils/xcb/connection.c \
		$(S_DIR)/systray/text.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/systray/test_battery: $(TESTS_DIR)/systray/test_battery.c \
		$(S_DIR)/systray/battery.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/test_stage: $(TESTS_DIR)/render/test_stage.c \
		$(S_DIR)/utils/xcb/connection.c \
		$(S_DIR)/render/stage.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lxcb -lpthread

$(O_DIR)/tests/client/test_state: \
		$(TESTS_DIR)/client/test_state.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/client/test_gravity: \
		$(TESTS_DIR)/client/test_gravity.c \
		$(S_DIR)/client/state.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/client/test_geom: \
		$(TESTS_DIR)/client/test_geom.c \
		$(S_DIR)/client/geom.c \
		$(S_DIR)/utils/safe/safemem.c \
		$(S_DIR)/utils/safe/safeflg.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/client/test_props: \
		$(TESTS_DIR)/client/test_props.c \
		$(S_DIR)/client/props.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/test_client: \
		$(TESTS_DIR)/test_client.c \
		$(S_DIR)/client.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/test_stage_lifecycle: \
		$(TESTS_DIR)/test_stage_lifecycle.c \
		$(S_DIR)/stage.c \
		$(S_DIR)/adt/cdlist.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cmds/client/test_flags: \
		$(TESTS_DIR)/cmds/client/test_flags.c \
		$(S_DIR)/cmds/client/flags.c \
		$(S_DIR)/utils/safe/safeflg.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/cmds/client/test_state: \
		$(TESTS_DIR)/cmds/client/test_state.c \
		$(S_DIR)/cmds/client/state.c \
		$(S_DIR)/utils/safe/safeflg.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/cmds/client/test_layer: \
		$(TESTS_DIR)/cmds/client/test_layer.c \
		$(S_DIR)/cmds/client/layer.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cmds/client/test_visibility: \
		$(TESTS_DIR)/cmds/client/test_visibility.c \
		$(S_DIR)/cmds/client/visibility.c \
		$(S_DIR)/utils/safe/safeflg.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/cmds/client/test_workarea: \
		$(TESTS_DIR)/cmds/client/test_workarea.c \
		$(S_DIR)/cmds/client/workarea.c \
		$(S_DIR)/cmds/client/screen.c \
		$(S_DIR)/adt/list.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cmds/client/test_maximize: \
		$(TESTS_DIR)/cmds/client/test_maximize.c \
		$(S_DIR)/cmds/client/maximize.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cmds/client/test_transient: \
		$(TESTS_DIR)/cmds/client/test_transient.c \
		$(S_DIR)/cmds/client/transient.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/utils/safe/safeflg.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/drag/test_resist: \
		$(TESTS_DIR)/input/mouse/drag/test_resist.c \
		$(S_DIR)/input/mouse/drag/resist.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/drag/test_snap: \
		$(TESTS_DIR)/input/mouse/drag/test_snap.c \
		$(S_DIR)/input/mouse/drag/snap.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/drag/test_warp: \
		$(TESTS_DIR)/input/mouse/drag/test_warp.c \
		$(S_DIR)/input/mouse/drag/warp.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/input/mouse/drag/test_pan: \
		$(TESTS_DIR)/input/mouse/drag/test_pan.c \
		$(S_DIR)/input/mouse/drag/pan.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/input/mouse/drag/test_background: \
		$(TESTS_DIR)/input/mouse/drag/test_background.c \
		$(S_DIR)/input/mouse/drag/background.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/input/mouse/test_viewport_edge: \
		$(TESTS_DIR)/input/mouse/test_viewport_edge.c \
		$(S_DIR)/input/mouse/viewport/edge.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/rules/test_apply: \
		$(TESTS_DIR)/rules/test_apply.c \
		$(S_DIR)/rules/apply.c \
		$(S_DIR)/rules/match.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/policy/test_placement: $(TESTS_DIR)/policy/test_placement.c \
		$(S_DIR)/utils/xcb/window.c \
		$(S_DIR)/policy/placement/window.c \
		$(S_DIR)/utils/xcb/connection.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/policy/placement/monitor.c \
		$(S_DIR)/policy/placement/rect.c \
		$(S_DIR)/policy/placement/smart.c \
		$(S_DIR)/policy/placement/manual.c \
		$(S_DIR)/policy/placement/score.c \
		$(S_DIR)/render/outline.c \
		$(S_DIR)/utils/time/clock.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS) -lpthread

$(O_DIR)/tests/policy/test_tiling: $(TESTS_DIR)/policy/test_tiling.c \
		$(S_DIR)/policy/placement/icon.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/utils/hash/murmurhash.c \
		$(S_DIR)/policy/placement/score.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/stage/test_desktop_grid: \
		$(TESTS_DIR)/stage/test_desktop_grid.c \
		$(S_DIR)/stage/desktops.c \
		$(S_DIR)/adt/cdlist.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

# Unlike every other test binary above, this one links the genuine
# libxcb-randr ($(XCB_LFLAGS), defined in the root Makefile) rather
# than a hand-written stand-in for its own functions: stage/
# monitors.c as a whole (the only actual dependency of the one
# function this file tests, stage_monitor_direction) also compiles
# stage_monitor_refresh_all alongside it in the same translation unit,
# and that one genuinely calls into RandR.  Its own reply structs are
# XCB-protocol-generated, not something safe to reconstruct a stand-in
# for by hand the way this project's own, much simpler functions
# (like atom_name, stood in for below) are; linking the real library
# instead is the safer choice, even though this test itself never
# actually calls stage_monitor_refresh_all, or triggers a real RandR
# round trip, at all.
$(O_DIR)/tests/stage/test_monitor_direction: \
		$(TESTS_DIR)/stage/test_monitor_direction.c \
		$(S_DIR)/utils/xcb/connection.c \
		$(S_DIR)/stage/monitors.c \
		$(S_DIR)/utils/xcb/reply.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/stage/test_desktop_add_remove: \
		$(TESTS_DIR)/stage/test_desktop_add_remove.c \
		$(S_DIR)/utils/xcb/connection.c \
		$(S_DIR)/stage/switch.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/stage/desktops.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/stage/test_pinned_transfer: \
		$(TESTS_DIR)/stage/test_pinned_transfer.c \
		$(S_DIR)/stage/actions/client.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/stage/test_viewport: \
		$(TESTS_DIR)/stage/test_viewport.c \
		$(S_DIR)/stage/viewport.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/viewport/test_mesh: \
		$(TESTS_DIR)/render/viewport/test_mesh.c \
		$(S_DIR)/render/viewport/mesh.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

# See this test file's own top-of-file comment: 'enact/desktop.c'
# compiles as a single translation unit, so every one of its own
# public functions besides 'enact_desktop_client_send' pulls in its
# own further dependencies regardless of which functions this test
# actually calls; every one of those is stood in for link-only in
# the test file itself, none genuinely reached at runtime, except
# 'adt/cdlist.c' itself (real, exercised by
# 'enact_desktop_client_rearrange_all', a function this test never
# calls but whose own dependencies still need resolving) and
# 'logger.c'/'safestr.c'
# (real, for the same reason 'test_desktop_add_remove' above already
# needs them).
$(O_DIR)/tests/enact/test_send_to_desktop: \
		$(TESTS_DIR)/enact/test_send_to_desktop.c \
		$(S_DIR)/utils/xcb/window.c \
		$(S_DIR)/enact/desktop.c \
		$(S_DIR)/utils/xcb/connection.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/desktop/test_workarea: \
		$(TESTS_DIR)/desktop/test_workarea.c \
		$(S_DIR)/desktop.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/utils/hash/murmurhash.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/menu/context/ctxmenu/test_layout: \
		$(TESTS_DIR)/menu/context/ctxmenu/test_layout.c \
		$(S_DIR)/menu/context/ctxmenu/layout.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/context/test_winlist: \
		$(TESTS_DIR)/menu/context/test_winlist.c \
		$(S_DIR)/menu/context/winlist.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/test_modifier: \
		$(TESTS_DIR)/input/test_modifier.c \
		$(S_DIR)/input/modifier.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/test_geom: $(TESTS_DIR)/utils/test_geom.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/test_sysmem: $(TESTS_DIR)/utils/test_sysmem.c \
		$(S_DIR)/utils/sysmem.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/safe/test_safeflg: \
		$(TESTS_DIR)/utils/safe/test_safeflg.c \
		$(S_DIR)/utils/safe/safeflg.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/safe/test_safemem: \
		$(TESTS_DIR)/utils/safe/test_safemem.c \
		$(S_DIR)/utils/safe/safemem.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/safe/test_safestr: \
		$(TESTS_DIR)/utils/safe/test_safestr.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/hash/test_murmurhash: \
		$(TESTS_DIR)/utils/hash/test_murmurhash.c \
		$(S_DIR)/utils/hash/murmurhash.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/config/test_path: \
		$(TESTS_DIR)/utils/config/test_path.c \
		$(S_DIR)/utils/config/path.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/config/test_json: \
		$(TESTS_DIR)/utils/config/test_json.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/config/test_randr: \
		$(TESTS_DIR)/config/test_randr.c \
		$(S_DIR)/config/randr.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/config/test_a11y: \
		$(TESTS_DIR)/config/test_a11y.c \
		$(S_DIR)/config/a11y.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/config/test_bindings: \
		$(TESTS_DIR)/config/test_bindings.c \
		$(S_DIR)/config/bindings.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/config/test_theme: \
		$(TESTS_DIR)/config/test_theme.c \
		$(S_DIR)/config/theme.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/config/test_memguard: \
		$(TESTS_DIR)/config/test_memguard.c \
		$(S_DIR)/config/memguard.c \
		$(S_DIR)/config/memguard/theme.c \
		$(S_DIR)/config/memguard/load.c \
		$(S_DIR)/config/memguard/defaults.c \
		$(S_DIR)/config/base/parse.c \
		$(S_DIR)/config/base/desktops.c \
		$(S_DIR)/config/base/defaults.c \
		$(S_DIR)/config/base/systray.c \
		$(S_DIR)/config/base/load.c \
		$(S_DIR)/config.c \
		$(S_DIR)/config/theme.c \
		$(S_DIR)/config/bindings.c \
		$(S_DIR)/config/randr.c \
		$(S_DIR)/config/a11y.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/config/path.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/config/test_lint: \
		$(TESTS_DIR)/config/test_lint.c \
		$(S_DIR)/config/lint.c \
		$(S_DIR)/config/lint/a11y.c \
		$(S_DIR)/config/lint/bindings.c \
		$(S_DIR)/config/lint/common.c \
		$(S_DIR)/config/lint/config.c \
		$(S_DIR)/config/lint/memguard.c \
		$(S_DIR)/config/lint/misc.c \
		$(S_DIR)/config/lint/theme.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/config/test_base: \
		$(TESTS_DIR)/config/test_base.c \
		$(S_DIR)/config/base/parse.c \
		$(S_DIR)/config/base/desktops.c \
		$(S_DIR)/config/base/defaults.c \
		$(S_DIR)/config/base/systray.c \
		$(S_DIR)/config/base/load.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/config/test_config: \
		$(TESTS_DIR)/config/test_config.c \
		$(S_DIR)/config.c \
		$(S_DIR)/config/base/parse.c \
		$(S_DIR)/config/base/desktops.c \
		$(S_DIR)/config/base/defaults.c \
		$(S_DIR)/config/base/systray.c \
		$(S_DIR)/config/base/load.c \
		$(S_DIR)/config/theme.c \
		$(S_DIR)/config/bindings.c \
		$(S_DIR)/config/randr.c \
		$(S_DIR)/config/a11y.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/config/path.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/rules/test_match: \
		$(TESTS_DIR)/rules/test_match.c \
		$(S_DIR)/rules/match.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/cmds/client/test_grab: \
		$(TESTS_DIR)/cmds/client/test_grab.c \
		$(S_DIR)/cmds/client/grab.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cmds/client/test_icon: \
		$(TESTS_DIR)/cmds/client/test_icon.c \
		$(S_DIR)/cmds/client/icon.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/desktop/dfind.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cmds/client/test_focus: \
		$(TESTS_DIR)/cmds/client/test_focus.c \
		$(S_DIR)/cmds/client/focus.c \
		$(S_DIR)/utils/safe/safeflg.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cmds/client/test_move: \
		$(TESTS_DIR)/cmds/client/test_move.c \
		$(S_DIR)/cmds/client/move.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/cmds/client/test_resize: \
		$(TESTS_DIR)/cmds/client/test_resize.c \
		$(S_DIR)/cmds/client/resize.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/cmds/client/test_ewmh: \
		$(TESTS_DIR)/cmds/client/test_ewmh.c \
		$(S_DIR)/cmds/client/ewmh.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/cmds/test_stage_desktop_switch: \
		$(TESTS_DIR)/cmds/test_stage_desktop_switch.c \
		$(S_DIR)/cmds/stage.c \
		$(S_DIR)/stage/viewport.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/cmds/test_stage_viewport_pan: \
		$(TESTS_DIR)/cmds/test_stage_viewport_pan.c \
		$(S_DIR)/cmds/stage.c \
		$(S_DIR)/stage/viewport.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/stage/test_workareas: \
		$(TESTS_DIR)/stage/test_workareas.c \
		$(S_DIR)/stage/workareas.c \
		$(S_DIR)/adt/cdlist.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/stage/actions/test_randr: \
		$(TESTS_DIR)/stage/actions/test_randr.c \
		$(S_DIR)/stage/actions/randr.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/policy/test_ping: \
		$(TESTS_DIR)/policy/test_ping.c \
		$(S_DIR)/policy/ping.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/utils/time/clock.c \
		$(S_DIR)/utils/safe/safeflg.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/enact/test_stage: \
		$(TESTS_DIR)/enact/test_stage.c \
		$(S_DIR)/enact/stage.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/actions/client/test_flags: \
		$(TESTS_DIR)/ipc/actions/client/test_flags.c \
		$(S_DIR)/ipc/actions/client/flags.c \
		$(S_DIR)/ipc/dispatch.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/actions/client/test_focus: \
		$(TESTS_DIR)/ipc/actions/client/test_focus.c \
		$(S_DIR)/ipc/actions/client/focus.c \
		$(S_DIR)/ipc/dispatch.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/actions/client/test_geom: \
		$(TESTS_DIR)/ipc/actions/client/test_geom.c \
		$(S_DIR)/ipc/actions/client/geom.c \
		$(S_DIR)/ipc/dispatch.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/client/state.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/actions/client/test_layer: \
		$(TESTS_DIR)/ipc/actions/client/test_layer.c \
		$(S_DIR)/ipc/actions/client/layer.c \
		$(S_DIR)/ipc/dispatch.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/actions/client/test_meta: \
		$(TESTS_DIR)/ipc/actions/client/test_meta.c \
		$(S_DIR)/ipc/actions/client/meta.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/actions/client/test_state: \
		$(TESTS_DIR)/ipc/actions/client/test_state.c \
		$(S_DIR)/ipc/actions/client/state.c \
		$(S_DIR)/ipc/dispatch.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/actions/client/test_visibility: \
		$(TESTS_DIR)/ipc/actions/client/test_visibility.c \
		$(S_DIR)/ipc/actions/client/visibility.c \
		$(S_DIR)/ipc/dispatch.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/actions/test_wm: \
		$(TESTS_DIR)/ipc/actions/test_wm.c \
		$(S_DIR)/ipc/actions/wm.c \
		$(S_DIR)/ipc/response.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/ipc/actions/test_scratchpad: \
		$(TESTS_DIR)/ipc/actions/test_scratchpad.c \
		$(S_DIR)/ipc/actions/scratchpad.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/ipc/actions/test_stage_action: \
		$(TESTS_DIR)/ipc/actions/test_stage_action.c \
		$(S_DIR)/ipc/actions/stage.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/ipc/actions/test_desktop: \
		$(TESTS_DIR)/ipc/actions/test_desktop.c \
		$(S_DIR)/ipc/actions/desktop.c \
		$(S_DIR)/ipc/resolve.c \
		$(S_DIR)/ipc/args.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/adt/cdlist.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/ipc/actions/test_query: \
		$(TESTS_DIR)/ipc/actions/test_query.c \
		$(S_DIR)/ipc/actions/query.c \
		$(S_DIR)/ipc/response.c \
		$(S_DIR)/lookup.c \
		$(S_DIR)/stage/desktops.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/enact/test_client: \
		$(TESTS_DIR)/enact/test_client.c \
		$(S_DIR)/enact/client.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/menu/context/ctxmenu/test_tree: \
		$(TESTS_DIR)/menu/context/ctxmenu/test_tree.c \
		$(S_DIR)/menu/context/ctxmenu/tree.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/dialog/test_confirm: \
		$(TESTS_DIR)/menu/dialog/test_confirm.c \
		$(S_DIR)/menu/dialog/confirm.c \
		$(S_DIR)/menu/dialog/defer.c \
		$(S_DIR)/utils/time/clock.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/desktop/test_dclient: \
		$(TESTS_DIR)/desktop/test_dclient.c \
		$(S_DIR)/desktop/dclient.c \
		$(S_DIR)/policy/stacking.c \
		$(S_DIR)/desktop/dfind.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/utils/hash/murmurhash.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/wm/test_actions: \
		$(TESTS_DIR)/wm/test_actions.c \
		$(S_DIR)/wm/actions.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/systray/test_layout: \
		$(TESTS_DIR)/systray/test_layout.c \
		$(S_DIR)/systray/layout.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/adt/list.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/test_search: \
		$(TESTS_DIR)/menu/test_search.c \
		$(S_DIR)/menu/search.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cmds/client/test_meta: \
		$(TESTS_DIR)/cmds/client/test_meta.c \
		$(S_DIR)/cmds/client/meta.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/kbd/test_bind: \
		$(TESTS_DIR)/input/kbd/test_bind.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/input/kbd/bind.c \
		$(S_DIR)/input/modifier.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/utils/xcb/connection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/kbd/test_event: \
		$(TESTS_DIR)/input/kbd/test_event.c \
		$(S_DIR)/input/kbd/event.c \
		$(S_DIR)/utils/xcb/connection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/kbd/test_execute: \
		$(TESTS_DIR)/input/kbd/test_execute.c \
		$(S_DIR)/input/kbd/execute.c \
		$(S_DIR)/utils/xcb/connection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/kbd/test_interact: \
		$(TESTS_DIR)/input/kbd/test_interact.c \
		$(S_DIR)/input/kbd/interact.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/kbd/test_intercept: \
		$(TESTS_DIR)/input/kbd/test_intercept.c \
		$(S_DIR)/input/kbd/bind.c \
		$(S_DIR)/input/kbd/intercept.c \
		$(S_DIR)/input/modifier.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/utils/xcb/connection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/drag/test_drag: \
		$(TESTS_DIR)/input/mouse/drag/test_drag.c \
		$(S_DIR)/input/mouse/drag.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/drag/test_icon: \
		$(TESTS_DIR)/input/mouse/drag/test_icon.c \
		$(S_DIR)/input/mouse/drag/icon.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/drag/test_outline: \
		$(TESTS_DIR)/input/mouse/drag/test_outline.c \
		$(S_DIR)/input/mouse/drag/outline.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/drag/test_overlay: \
		$(TESTS_DIR)/input/mouse/drag/test_overlay.c \
		$(S_DIR)/input/mouse/drag/overlay.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/input/mouse/event/test_enter: \
		$(TESTS_DIR)/input/mouse/event/test_enter.c \
		$(S_DIR)/input/mouse/event/enter.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/event/test_overlay: \
		$(TESTS_DIR)/input/mouse/event/test_overlay.c \
		$(S_DIR)/input/mouse/event/overlay.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/event/test_press: \
		$(TESTS_DIR)/input/mouse/event/test_press.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/input/mouse/event/press.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/input/mouse/event/test_release: \
		$(TESTS_DIR)/input/mouse/event/test_release.c \
		$(S_DIR)/input/mouse/event/release.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/event/test_scroll: \
		$(TESTS_DIR)/input/mouse/event/test_scroll.c \
		$(S_DIR)/input/mouse/event/scroll.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/event/test_titlebar: \
		$(TESTS_DIR)/input/mouse/event/test_titlebar.c \
		$(S_DIR)/input/mouse/event/titlebar.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/test_bind: \
		$(TESTS_DIR)/input/mouse/test_bind.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/input/modifier.c \
		$(S_DIR)/input/mouse/bind.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/test_cursor: \
		$(TESTS_DIR)/input/mouse/test_cursor.c \
		$(S_DIR)/input/mouse/bounds.c \
		$(S_DIR)/input/mouse/cursor.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/input/mouse/test_hover: \
		$(TESTS_DIR)/input/mouse/test_hover.c \
		$(S_DIR)/input/mouse/hover.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/context/ctxmenu/test_handle: \
		$(TESTS_DIR)/menu/context/ctxmenu/test_handle.c \
		$(S_DIR)/menu/context/ctxmenu/handle.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/context/ctxmenu/test_redraw: \
		$(TESTS_DIR)/menu/context/ctxmenu/test_redraw.c \
		$(S_DIR)/menu/context/ctxmenu/redraw.c \
		$(S_DIR)/utils/xcb/connection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/menu/context/ctxmenu/test_select: \
		$(TESTS_DIR)/menu/context/ctxmenu/test_select.c \
		$(S_DIR)/menu/context/ctxmenu/select.c \
		$(S_DIR)/utils/xcb/connection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/context/test_ctxmenu: \
		$(TESTS_DIR)/menu/context/test_ctxmenu.c \
		$(S_DIR)/menu/context/ctxmenu.c \
		$(S_DIR)/utils/xcb/connection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/menu/context/test_menujson: \
		$(TESTS_DIR)/menu/context/test_menujson.c \
		$(S_DIR)/menu/context/menujson.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/menu/context/test_rootmenu: \
		$(TESTS_DIR)/menu/context/test_rootmenu.c \
		$(S_DIR)/menu/context/rootmenu.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/context/test_wincmenu: \
		$(TESTS_DIR)/menu/context/test_wincmenu.c \
		$(S_DIR)/menu/context/wincmenu.c \
		$(S_DIR)/menu/context/submenu/desktop.c \
		$(S_DIR)/menu/context/submenu/monitor.c \
		$(S_DIR)/menu/context/submenu/page.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/menu/context/submenu/test_desktop: \
		$(TESTS_DIR)/menu/context/submenu/test_desktop.c \
		$(S_DIR)/menu/context/submenu/desktop.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/context/submenu/test_monitor: \
		$(TESTS_DIR)/menu/context/submenu/test_monitor.c \
		$(S_DIR)/menu/context/submenu/monitor.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/context/submenu/test_page: \
		$(TESTS_DIR)/menu/context/submenu/test_page.c \
		$(S_DIR)/menu/context/submenu/page.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/cycle/test_draw: \
		$(TESTS_DIR)/menu/cycle/test_draw.c \
		$(S_DIR)/menu/cycle/draw.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/dialog/test_fortune: \
		$(TESTS_DIR)/menu/dialog/test_fortune.c \
		$(S_DIR)/menu/dialog/fortune.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/dialog/test_info: \
		$(TESTS_DIR)/menu/dialog/test_info.c \
		$(S_DIR)/menu/dialog/info.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/dialog/test_inspect: \
		$(TESTS_DIR)/menu/dialog/test_inspect.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/menu/dialog/inspect.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/dialog/test_message: \
		$(TESTS_DIR)/menu/dialog/test_message.c \
		$(S_DIR)/menu/dialog/defer.c \
		$(S_DIR)/menu/dialog/message.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/dialog/test_quit: \
		$(TESTS_DIR)/menu/dialog/test_quit.c \
		$(S_DIR)/menu/dialog/quit.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/dialog/test_rrsafe: \
		$(TESTS_DIR)/menu/dialog/test_rrsafe.c \
		$(S_DIR)/menu/dialog/rrsafe.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/dialog/test_run: \
		$(TESTS_DIR)/menu/dialog/test_run.c \
		$(S_DIR)/menu/dialog/run.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/dialog/test_shortcuts: \
		$(TESTS_DIR)/menu/dialog/test_shortcuts.c \
		$(S_DIR)/config/bindings.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/menu/dialog/shortcuts.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/menu/notify/test_desktop: \
		$(TESTS_DIR)/menu/notify/test_desktop.c \
		$(S_DIR)/menu/draw.c \
		$(S_DIR)/menu/notify.c \
		$(S_DIR)/menu/notify/desktop.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/test_cycle: \
		$(TESTS_DIR)/menu/test_cycle.c \
		$(S_DIR)/menu/cycle.c \
		$(S_DIR)/menu/draw.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/test_dialog: \
		$(TESTS_DIR)/menu/test_dialog.c \
		$(S_DIR)/menu/dialog.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/test_draw: \
		$(TESTS_DIR)/menu/test_draw.c \
		$(S_DIR)/menu/draw.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/test_notify: \
		$(TESTS_DIR)/menu/test_notify.c \
		$(S_DIR)/menu/draw.c \
		$(S_DIR)/menu/notify.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/menu/test_popup: \
		$(TESTS_DIR)/menu/test_popup.c \
		$(S_DIR)/menu/draw.c \
		$(S_DIR)/menu/popup.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/systray/test_protocol: \
		$(TESTS_DIR)/systray/test_protocol.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/systray/protocol.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/utils/xcb/reply.c \
		$(S_DIR)/utils/xcb/selection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/test_enact: \
		$(TESTS_DIR)/test_enact.c \
		$(S_DIR)/enact.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/test_ipc: \
		$(TESTS_DIR)/test_ipc.c \
		$(S_DIR)/ipc.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/config/path.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/test_systray: \
		$(TESTS_DIR)/test_systray.c \
		$(S_DIR)/systray.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/test_cursor: \
		$(TESTS_DIR)/utils/test_cursor.c \
		$(S_DIR)/utils/cursor.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/test_spawn: \
		$(TESTS_DIR)/utils/test_spawn.c \
		$(S_DIR)/utils/spawn.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/xcb/test_pixmap: \
		$(TESTS_DIR)/utils/xcb/test_pixmap.c \
		$(S_DIR)/utils/xcb/pixmap.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/xcb/test_selection: \
		$(TESTS_DIR)/utils/xcb/test_selection.c \
		$(S_DIR)/utils/xcb/reply.c \
		$(S_DIR)/utils/xcb/selection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/utils/xcb/test_wait: \
		$(TESTS_DIR)/utils/xcb/test_wait.c \
		$(S_DIR)/utils/xcb/wait.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/wm/test_ewmh: \
		$(TESTS_DIR)/wm/test_ewmh.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/wm/ewmh.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/wm/test_lifecycle: \
		$(TESTS_DIR)/wm/test_lifecycle.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/wm.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/wm/test_shutdown: \
		$(TESTS_DIR)/wm/test_shutdown.c \
		$(S_DIR)/utils/time/clock.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/wm/shutdown.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/wm/test_startup: \
		$(TESTS_DIR)/wm/test_startup.c \
		$(S_DIR)/utils/xcb/reply.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/wm/startup.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/cctl/test_adopt: \
		$(TESTS_DIR)/cctl/test_adopt.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/cctl/adopt.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cctl/test_kill: \
		$(TESTS_DIR)/cctl/test_kill.c \
		$(S_DIR)/cctl/kill.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cctl/test_launch: \
		$(TESTS_DIR)/cctl/test_launch.c \
		$(S_DIR)/cctl/launch.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/cctl/test_sn: \
		$(TESTS_DIR)/cctl/test_sn.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/cctl/sn.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_colormap: \
		$(TESTS_DIR)/handler/test_colormap.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/handler/colormap.c \
		$(S_DIR)/stage/desktops.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_configure: \
		$(TESTS_DIR)/handler/test_configure.c \
		$(S_DIR)/client/state.c \
		$(S_DIR)/handler/configure.c \
		$(S_DIR)/utils/time/clock.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_crossing: \
		$(TESTS_DIR)/handler/test_crossing.c \
		$(S_DIR)/handler/crossing.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_error: \
		$(TESTS_DIR)/handler/test_error.c \
		$(S_DIR)/handler/error.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_ewmh: \
		$(TESTS_DIR)/handler/test_ewmh.c \
		$(S_DIR)/handler/ewmh.c \
		$(S_DIR)/utils/safe/safeflg.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_expose: \
		$(TESTS_DIR)/handler/test_expose.c \
		$(S_DIR)/handler/expose.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_focus: \
		$(TESTS_DIR)/handler/test_focus.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/handler/focus.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_map: \
		$(TESTS_DIR)/handler/test_map.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/handler/map.c \
		$(S_DIR)/utils/safe/safeflg.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/handler/test_message: \
		$(TESTS_DIR)/handler/test_message.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/handler/message.c \
		$(S_DIR)/utils/safe/safeflg.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_randr: \
		$(TESTS_DIR)/handler/test_randr.c \
		$(S_DIR)/handler/randr.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_selection: \
		$(TESTS_DIR)/handler/test_selection.c \
		$(S_DIR)/handler/selection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/handler/test_sync: \
		$(TESTS_DIR)/handler/test_sync.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/handler/sync.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/loop/event/test_input: \
		$(TESTS_DIR)/loop/event/test_input.c \
		$(S_DIR)/loop/event/input.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/loop/event/test_motion: \
		$(TESTS_DIR)/loop/event/test_motion.c \
		$(S_DIR)/loop/event/motion.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/loop/test_context: \
		$(TESTS_DIR)/loop/test_context.c \
		$(S_DIR)/loop/context.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/loop/test_dispatch: \
		$(TESTS_DIR)/loop/test_dispatch.c \
		$(S_DIR)/loop/dispatch.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/loop/test_loop: \
		$(TESTS_DIR)/loop/test_loop.c \
		$(S_DIR)/loop.c \
		$(S_DIR)/wm/instance.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/loop/test_pollset: \
		$(TESTS_DIR)/loop/test_pollset.c \
		$(S_DIR)/loop/pollset.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/loop/test_refresh: \
		$(TESTS_DIR)/loop/test_refresh.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/loop/refresh.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/loop/test_signals: \
		$(TESTS_DIR)/loop/test_signals.c \
		$(S_DIR)/loop/signals.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/loop/test_timers: \
		$(TESTS_DIR)/loop/test_timers.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/loop/timers.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/client/test_decoration: \
		$(TESTS_DIR)/render/client/test_decoration.c \
		$(S_DIR)/render/client/decoration.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/client/test_titlebar: \
		$(TESTS_DIR)/render/client/test_titlebar.c \
		$(S_DIR)/render/client/titlebar.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/desktop/test_background: \
		$(TESTS_DIR)/render/desktop/test_background.c \
		$(S_DIR)/render/desktop/background.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/test_desktop: \
		$(TESTS_DIR)/render/test_desktop.c \
		$(S_DIR)/render/desktop.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/test_glyph: \
		$(TESTS_DIR)/render/test_glyph.c \
		$(S_DIR)/render/glyph.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $(FONT_CFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS) $(FONT_LFLAGS)

$(O_DIR)/tests/render/test_icon: \
		$(TESTS_DIR)/render/test_icon.c \
		$(S_DIR)/render/icon.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/test_text: \
		$(TESTS_DIR)/render/test_text.c \
		$(S_DIR)/render/text.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/test_wmicon: \
		$(TESTS_DIR)/render/test_wmicon.c \
		$(S_DIR)/render/wmicon.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

# 'main.c' declares its own real 'main', which would collide with this
# test file's own TAP-driving 'main'; built here in three steps
# instead of the single-invocation pattern every other test uses:
# 'test_main.c' compiled plainly first, then 'main.c' compiled on its
# own with '-Dmain=icowm_main' (a preprocessor rename applied only to
# that one translation unit, without editing 'src/main.c' itself) plus
# the project-identity macros it needs that only ever come from the
# root 'GNUmakefile' during a real build, and finally both objects
# linked together with the remaining plain dependencies
$(O_DIR)/tests/test_main: \
		$(TESTS_DIR)/test_main.c \
		$(S_DIR)/main.c \
		$(S_DIR)/i18n.c \
		$(S_DIR)/utils/safe/safemem.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) -D I18N_DOMAIN=\"icowm\" -D I18N_LOCALE_DIR=\"/tmp\" \
		-c $(TESTS_DIR)/test_main.c -o $(O_DIR)/tests/test_main_file.o
	$(CC) $(TEST_CCFLAGS) -D I18N_DOMAIN=\"icowm\" -D I18N_LOCALE_DIR=\"/tmp\" \
		-Dmain=icowm_main \
		-D BUILD_NUMBER=1 -D BUILD_TIMESTAMP=\"20260101T0000\" \
		-D PROJECT_NAME_LONG=\"$(PROJECT_NAME_LONG)\" \
		-D PROJECT_NAME_SHORT=\"$(PROJECT_NAME_SHORT)\" \
		-D PROJECT_NAME_PROG=\"$(PROJECT_NAME_PROG)\" \
		-D PROJECT_VERSION=\"$(PROJECT_VERSION)\" \
		-D PROJECT_VERSION_CODENAME=\"ovelya\" \
		-D AUTHOR=\"$(AUTHOR)\" -D COPYRIGHT=\"$(COPYRIGHT)\" \
		-D LICENSE=\"$(LICENSE)\" -D RELEASE_DATE=\"$(RELEASE_DATE)\" \
		-c $(S_DIR)/main.c -o $(O_DIR)/tests/main_renamed.o
	$(CC) $(TEST_CCFLAGS) -D I18N_DOMAIN=\"icowm\" -D I18N_LOCALE_DIR=\"/tmp\" \
		$(O_DIR)/tests/test_main_file.o $(O_DIR)/tests/main_renamed.o \
		$(S_DIR)/i18n.c $(S_DIR)/utils/safe/safemem.c \
		$(S_DIR)/utils/safe/safestr.c \
		-o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/test_session: \
		$(TESTS_DIR)/test_session.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/session.c \
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/test_xsettings: \
		$(TESTS_DIR)/test_xsettings.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/xsettings.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/wm/startup/test_handle: \
		$(TESTS_DIR)/wm/startup/test_handle.c \
		$(S_DIR)/wm/startup/handle.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/wm/startup/test_install: \
		$(TESTS_DIR)/wm/startup/test_install.c \
		$(S_DIR)/wm/startup/handle.c \
		$(S_DIR)/wm/startup/install.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/wm/startup/test_selection: \
		$(TESTS_DIR)/wm/startup/test_selection.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/wm/startup/selection.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/wm/startup/test_subscribe: \
		$(TESTS_DIR)/wm/startup/test_subscribe.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/wm/startup/subscribe.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)
