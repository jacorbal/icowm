# tests/Makefile.mk
#
# Included from the project's own root Makefile (see 'include
# tests/Makefile.mk' there), not meant to be invoked on its own: every
# variable used below ('CC', 'CCSTD', 'I_DIR', 'S_DIR', 'O_DIR',
# 'TESTS_DIR', 'JSON_CFLAGS', 'JSON_LFLAGS', 'XCB_CFLAGS') is defined in
# that root Makefile, already in scope by the time this file is
# processed.  Kept as its own file purely to keep the root Makefile
# itself short and focused on building IcoWM proper; nothing here
# changes how 'make test' behaves from the project root.

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
    $(O_DIR)/tests/wm/test_clients \
    $(O_DIR)/tests/policy/test_urgency \
    $(O_DIR)/tests/test_rules \
    $(O_DIR)/tests/input/mouse/test_bounds \
    $(O_DIR)/tests/input/mouse/test_resolve \
    $(O_DIR)/tests/input/kbd/test_resolve \
    $(O_DIR)/tests/test_memguard \
    $(O_DIR)/tests/systray/test_text \
    $(O_DIR)/tests/systray/test_battery \
    $(O_DIR)/tests/render/test_surface \
    $(O_DIR)/tests/client/test_state \
    $(O_DIR)/tests/policy/test_placement \
    $(O_DIR)/tests/policy/test_tiling \
    $(O_DIR)/tests/surface/test_desktop_grid \
    $(O_DIR)/tests/surface/test_monitor_direction \
    $(O_DIR)/tests/surface/test_desktop_add_remove \
    $(O_DIR)/tests/enact/test_send_to_desktop \
    $(O_DIR)/tests/desktop/test_workarea \
    $(O_DIR)/tests/menu/context/ctxmenu/test_layout \
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
    $(O_DIR)/tests/rules/test_match

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

$(O_DIR)/tests/wm/test_clients: $(TESTS_DIR)/wm/test_clients.c \
		$(S_DIR)/wm/clients.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/list.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/policy/test_urgency: $(TESTS_DIR)/policy/test_urgency.c \
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

$(O_DIR)/tests/test_memguard: $(TESTS_DIR)/test_memguard.c \
		$(S_DIR)/memguard.c \
		$(S_DIR)/utils/time/clock.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/systray/test_text: $(TESTS_DIR)/systray/test_text.c \
		$(S_DIR)/systray/text.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/systray/test_battery: $(TESTS_DIR)/systray/test_battery.c \
		$(S_DIR)/systray/battery.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

$(O_DIR)/tests/render/test_surface: $(TESTS_DIR)/render/test_surface.c \
		$(S_DIR)/render/surface.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lxcb -lpthread

$(O_DIR)/tests/client/test_state: \
		$(TESTS_DIR)/client/test_state.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/policy/test_placement: $(TESTS_DIR)/policy/test_placement.c \
		$(S_DIR)/policy/placement/window.c \
		$(S_DIR)/policy/placement/monitor.c \
		$(S_DIR)/policy/placement/rect.c \
		$(S_DIR)/policy/placement/smart.c \
		$(S_DIR)/policy/placement/score.c \
		$(S_DIR)/wm/instance.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/adt/ohtbl.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/policy/test_tiling: $(TESTS_DIR)/policy/test_tiling.c \
		$(S_DIR)/policy/placement/icon.c \
		$(S_DIR)/policy/placement/score.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) -lpthread

$(O_DIR)/tests/surface/test_desktop_grid: \
		$(TESTS_DIR)/surface/test_desktop_grid.c \
		$(S_DIR)/surface/desktops.c \
		$(S_DIR)/adt/cdlist.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

# Unlike every other test binary above, this one links the genuine
# libxcb-randr ($(XCB_LFLAGS), defined in the root Makefile) rather
# than a hand-written stand-in for its own functions: surface/
# monitors.c as a whole (the only actual dependency of the one
# function this file tests, surface_monitor_direction) also compiles
# surface_refresh_monitors alongside it in the same translation unit,
# and that one genuinely calls into RandR.  Its own reply structs are
# XCB-protocol-generated, not something safe to reconstruct a stand-in
# for by hand the way this project's own, much simpler functions
# (like atom_name, stood in for below) are; linking the real library
# instead is the safer choice, even though this test itself never
# actually calls surface_refresh_monitors, or triggers a real RandR
# round trip, at all.
$(O_DIR)/tests/surface/test_monitor_direction: \
		$(TESTS_DIR)/surface/test_monitor_direction.c \
		$(S_DIR)/surface/monitors.c \
		$(S_DIR)/utils/xcb/reply.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(XCB_LFLAGS)

$(O_DIR)/tests/surface/test_desktop_add_remove: \
		$(TESTS_DIR)/surface/test_desktop_add_remove.c \
		$(S_DIR)/surface/switch.c \
		$(S_DIR)/surface/desktops.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS)

# See this test file's own top-of-file comment: 'enact/desktop.c'
# compiles as a single translation unit, so every one of its own
# public functions besides 'enact_desktop_client_send' pulls in its
# own further dependencies regardless of which functions this test
# actually calls; every one of those is stood in for link-only in
# the test file itself, none genuinely reached at runtime, except
# 'adt/cdlist.c' itself (real, exercised by 'enact_desktop_clients_
# rearrange', a function this test never calls but whose own
# dependencies still need resolving) and 'logger.c'/'safestr.c'
# (real, for the same reason 'test_desktop_add_remove' above already
# needs them).
$(O_DIR)/tests/enact/test_send_to_desktop: \
		$(TESTS_DIR)/enact/test_send_to_desktop.c \
		$(S_DIR)/enact/desktop.c \
		$(S_DIR)/adt/cdlist.c \
		$(S_DIR)/logger.c \
		$(S_DIR)/utils/safe/safestr.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS)

$(O_DIR)/tests/desktop/test_workarea: \
		$(TESTS_DIR)/desktop/test_workarea.c \
		$(S_DIR)/desktop.c \
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
