/**
 * @file tests/utils/config/test_path.c
 *
 * @brief Test battery for path normalization and XDG base directory
 *        resolution
 *
 * Weighted toward 'path_simplify''s own handling of '..': a real bug
 * found while writing this very test battery, not a hypothetical
 * one.  The original implementation only ever moved 'dst' back to
 * the slash right before the popped component, never removing that
 * component's own name, so e.g. "a/../b" simplified to "ab" instead
 * of "b" (the slash itself silently disappeared, and the component
 * it should have cancelled stayed behind).  Fixed to scan back to
 * where that component actually starts.  s_test_dotdot_regression
 * below is that exact reported case.
 */
/*
 * Copyright (c) 2026, J. A. Corbal.
 * All rights reserved.
 *
 * This file is licensed under the 'ISC License'.
 * Read the 'LICENSE' file in the root of this repository for details.
 */

/* System includes */
#include <stdlib.h>
#include <string.h>

/* Local includes */
#include <utils/config/path.h>
#include <harness/tap.h>


/**
 * @brief Run 'path_simplify' on a fresh copy of 'input' and assert
 *        the result matches 'expected'
 */
static void s_assert_simplify(const char *input, const char *expected,
        const char *desc)
{
    char buf[256];

    strcpy(buf, input);
    path_simplify(buf);
    TAP_EQ_STR(buf, expected, desc);
}


/* The exact bug this whole file was written to catch: 'a/../b' must
 * simplify to 'b', not 'ab' */
static void s_test_dotdot_regression(void)
{
    s_assert_simplify("a/../b", "b",
            "regression: 'a/../b' resolves to 'b', not 'ab'");
    s_assert_simplify("a/b/../c", "a/c",
            "regression: 'a/b/../c' resolves to 'a/c', not 'a/bc'");
}


/* Ordinary normalization: nothing to resolve, redundant slashes, and
 * '.' components */
static void s_test_ordinary_normalization(void)
{
    s_assert_simplify("a/b/c", "a/b/c", "already-simplified path");
    s_assert_simplify("a//b", "a/b", "one redundant slash collapsed");
    s_assert_simplify("a///b", "a/b", "several redundant slashes");
    s_assert_simplify("./a/b", "a/b", "leading './' dropped");
    s_assert_simplify("a/./b", "a/b", "mid-path './' dropped");
}


/* Multiple '..' components, including two in a row, correctly pop
 * more than one real component */
static void s_test_multiple_dotdot(void)
{
    s_assert_simplify("a/b/c/../../d", "a/d",
            "two '..' in a row pop two real components");
    s_assert_simplify("a/b/../../c", "c",
            "popping every component leaves just the last name");
}


/* A '..' with no real component behind it to cancel (a relative
 * path's own leading '..', or one right after another unresolved
 * '..') is kept literally, never silently dropped or miscomputed */
static void s_test_dotdot_with_nothing_to_cancel(void)
{
    s_assert_simplify("../a", "../a",
            "leading '..' with nothing behind it stays literal");
    s_assert_simplify("../../a", "../../a",
            "two leading '..' in a row both stay literal");
}


/* An absolute path's '..' pops correctly same as a relative one's,
 * but one already at the root is dropped outright: there is nothing
 * above root to name, so it is not kept literally the way a
 * relative path's unresolvable '..' is */
static void s_test_absolute_paths(void)
{
    s_assert_simplify("/a/b/../c", "/a/c",
            "absolute path pops a component the same way");
    s_assert_simplify("/../a", "/a",
            "'..' already at root is dropped, not kept literally");
    s_assert_simplify("/", "/", "root alone is left alone");
}


/* A relative path that resolves down to nothing at all becomes '.'
 * (the current directory), never an empty string: an empty path and
 * "the current directory" are not interchangeable to whatever uses
 * this result next */
static void s_test_empties_to_dot_not_empty_string(void)
{
    s_assert_simplify("a/../", ".",
            "a relative path popped down to nothing becomes '.'");
    s_assert_simplify("", ".", "an empty input also becomes '.'");
}


/* A single plain component, with nothing at all to simplify, passes
 * through completely unchanged */
static void s_test_single_component_unchanged(void)
{
    s_assert_simplify("a", "a", "single component, nothing to do");
    s_assert_simplify(".", ".", "a bare '.' stays as itself");
}


/* Known, deliberately out-of-scope limitation, documented in
 * path_simplify's own doc comment: '..' is only recognized
 * immediately followed by '/' (more path after it), not at the very
 * end of the string with nothing following.  This is the pre-
 * existing behavior this fix's own scope was kept to, not something
 * newly introduced. */
static void s_test_trailing_dotdot_without_slash_is_unresolved(void)
{
    s_assert_simplify("/a/..", "/a/..",
            "trailing '..' with no slash after it: left unresolved,"
            " matching the documented, pre-existing limitation");
}


/**
 * @brief Clear every environment variable 'xdg_resolve_dir' itself
 *        reads, so each test below starts from a known, empty
 *        environment rather than whatever this process happened to
 *        inherit
 */
static void s_clear_xdg_env(void)
{
    unsetenv("XDG_CONFIG_HOME");
    unsetenv("XDG_DATA_HOME");
    unsetenv("XDG_STATE_HOME");
    unsetenv("XDG_CACHE_HOME");
    unsetenv("XDG_RUNTIME_DIR");
    unsetenv("HOME");
}


/* The XDG environment variable, when set and non-empty, always wins
 * over every other tier, for every one of the five directory kinds */
static void s_test_xdg_env_var_wins(void)
{
    char out[256];

    s_clear_xdg_env();
    setenv("XDG_CONFIG_HOME", "/xdg/config", 1);
    xdg_resolve_dir(XDG_DIR_CONFIG, "/fallback", out, sizeof(out));
    TAP_EQ_STR(out, "/xdg/config/icowm",
            "XDG_CONFIG_HOME set: used verbatim, with 'icowm' appended");

    s_clear_xdg_env();
    setenv("XDG_RUNTIME_DIR", "/run/user/1000", 1);
    xdg_resolve_dir(XDG_DIR_RUNTIME, "/fallback", out, sizeof(out));
    TAP_EQ_STR(out, "/run/user/1000/icowm",
            "XDG_RUNTIME_DIR set: used verbatim, with 'icowm' appended");
}


/* With no XDG variable set, $HOME is the next tier, using this
 * project's own pre-XDG convention for XDG_DIR_CONFIG specifically
 * (a flat, dotted "~/.icowm", not "~/.config/icowm") and the XDG
 * specification's own documented default location for the other
 * three that have one */
static void s_test_home_fallback_tier(void)
{
    char out[256];

    s_clear_xdg_env();
    setenv("HOME", "/home/user", 1);
    xdg_resolve_dir(XDG_DIR_CONFIG, "/fallback", out, sizeof(out));
    TAP_EQ_STR(out, "/home/user/.icowm",
            "XDG_DIR_CONFIG falls back to the pre-XDG '~/.icowm'");

    s_clear_xdg_env();
    setenv("HOME", "/home/user", 1);
    xdg_resolve_dir(XDG_DIR_DATA, "/fallback", out, sizeof(out));
    TAP_EQ_STR(out, "/home/user/.local/share/icowm",
            "XDG_DIR_DATA falls back to the spec's own default");

    s_clear_xdg_env();
    setenv("HOME", "/home/user", 1);
    xdg_resolve_dir(XDG_DIR_STATE, "/fallback", out, sizeof(out));
    TAP_EQ_STR(out, "/home/user/.local/state/icowm",
            "XDG_DIR_STATE falls back to the spec's own default");

    s_clear_xdg_env();
    setenv("HOME", "/home/user", 1);
    xdg_resolve_dir(XDG_DIR_CACHE, "/fallback", out, sizeof(out));
    TAP_EQ_STR(out, "/home/user/.cache/icowm",
            "XDG_DIR_CACHE falls back to the spec's own default");
}


/* XDG_DIR_RUNTIME skips the $HOME tier entirely, even when $HOME is
 * set: an ordinary $HOME directory offers none of the guarantees
 * (mode 0700, cleared on logout) the specification requires of this
 * one kind, so falling back to it would be actively wrong */
static void s_test_runtime_skips_home_tier(void)
{
    char out[256];

    s_clear_xdg_env();
    setenv("HOME", "/home/user", 1);
    xdg_resolve_dir(XDG_DIR_RUNTIME, "/tmp/1000-icowm", out,
            sizeof(out));
    TAP_EQ_STR(out, "/tmp/1000-icowm",
            "XDG_DIR_RUNTIME ignores $HOME, going straight to the"
            " final fallback");
}


/* With neither the XDG variable nor (when applicable) $HOME set,
 * every kind falls all the way through to the caller's own
 * verbatim final_fallback */
static void s_test_final_fallback_tier(void)
{
    char out[256];

    s_clear_xdg_env();
    xdg_resolve_dir(XDG_DIR_CONFIG, "./icowm", out, sizeof(out));
    TAP_EQ_STR(out, "icowm",
            "no env at all: falls through to final_fallback"
            " (here simplified from './icowm' to 'icowm')");
}


/* An empty (but set) XDG variable is treated the same as unset, not
 * as a literal empty-string prefix */
static void s_test_empty_env_var_is_treated_as_unset(void)
{
    char out[256];

    s_clear_xdg_env();
    setenv("XDG_CONFIG_HOME", "", 1);
    setenv("HOME", "/home/user", 1);
    xdg_resolve_dir(XDG_DIR_CONFIG, "/fallback", out, sizeof(out));
    TAP_EQ_STR(out, "/home/user/.icowm",
            "empty XDG_CONFIG_HOME falls through to $HOME, not used"
            " as an empty prefix");
}


/* The resolved path is always run through path_simplify itself, so
 * a redundant slash reaching in from the environment does not leak
 * through unsimplified */
static void s_test_result_is_simplified(void)
{
    char out[256];

    s_clear_xdg_env();
    setenv("XDG_CONFIG_HOME", "/xdg//config/", 1);
    xdg_resolve_dir(XDG_DIR_CONFIG, "/fallback", out, sizeof(out));
    TAP_EQ_STR(out, "/xdg/config/icowm",
            "redundant slashes from the environment are simplified"
            " away in the result");
}


int main(void)
{
    TAP_PLAN(29);

    s_test_dotdot_regression();
    s_test_ordinary_normalization();
    s_test_multiple_dotdot();
    s_test_dotdot_with_nothing_to_cancel();
    s_test_absolute_paths();
    s_test_empties_to_dot_not_empty_string();
    s_test_single_component_unchanged();
    s_test_trailing_dotdot_without_slash_is_unresolved();
    s_test_xdg_env_var_wins();
    s_test_home_fallback_tier();
    s_test_runtime_skips_home_tier();
    s_test_final_fallback_tier();
    s_test_empty_env_var_is_treated_as_unset();
    s_test_result_is_simplified();

    return TAP_DONE();
}
