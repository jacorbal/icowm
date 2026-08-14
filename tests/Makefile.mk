# tests/Makefile.mk
#
# Included from the project's own root Makefile (see 'include
# tests/Makefile.mk' there), not meant to be invoked on its own:
# every variable used below (CC, CCSTD, I_DIR, S_DIR, O_DIR,
# TESTS_DIR, JSON_CFLAGS, JSON_LFLAGS, XCB_CFLAGS) is defined in
# that root Makefile, already in scope by the time this file is
# processed.  Kept as its own file purely to keep the root Makefile
# itself short and focused on building IcoWM proper; nothing here
# changes how 'make test' behaves from the project root.

.PHONY: test

## Tests
#
# Each 'tests/<dir>/test_<name>.c' is its own standalone binary,
# compiled and linked directly against exactly the source files it
# actually exercises (never through $(TARGET) itself): always under
# AddressSanitizer and UndefinedBehaviorSanitizer, since a test
# binary's own job is finding exactly the class of bug those catch,
# not just checking return values.  Extend TEST_BINS with one more
# line, and its own explicit recipe alongside the others below, for
# each new test file; no automatic discovery, so a new test always
# has to be wired in on purpose, not silently picked up (or silently
# skipped) by a glob.
TEST_CCFLAGS = -std=$(CCSTD) -D _POSIX_C_SOURCE=200112L -g -O0 \
    -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I $(I_DIR) -I $(TESTS_DIR) $(JSON_CFLAGS) $(XCB_CFLAGS)
TEST_LDFLAGS = -fsanitize=address,undefined

TEST_BINS = $(O_DIR)/tests/adt/test_cdlist \
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
		$(S_DIR)/config/base.c \
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
		$(S_DIR)/utils/config/json.c \
		$(S_DIR)/utils/safe/safestr.c \
		$(S_DIR)/logger.c
	@mkdir -p $(@D)
	$(CC) $(TEST_CCFLAGS) $^ -o $@ $(TEST_LDFLAGS) $(JSON_LFLAGS) -lpthread

$(O_DIR)/tests/config/test_base: \
		$(TESTS_DIR)/config/test_base.c \
		$(S_DIR)/config/base.c \
		$(S_DIR)/utils/config/json.c \
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
