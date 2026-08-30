Contributing to IcoWM
=====================

IcoWM keeps one style throughout, in every one of its source files.
That consistency is deliberate and is worth more than any individual
preference, this author's included, so a patch that follows it is easier
to accept than one that argues with it.

What follows is that style, written down.  Nothing here is a matter of
taste to be relitigated; it is a description of what the tree already
does, so that a patch may look like the code around it.

Before anything else
--------------------

The whole of it must build clean and pass its tests:

```sh
make                    # gcc, no warnings, no errors
make CC=clang           # clang, likewise
make analyze            # the static analysis pass
```

Every warning is an error, under a deliberately unforgiving set of
flags.  A patch that introduces one will not build at all, which is the
intent.  The two compilers each carry warnings the other has not got, so
both are worth running.

Language
--------

C99 and POSIX.1-2001, and nothing else.  No compiler extension of any
kind: no `__attribute__`, no `typeof`, no `#pragma`, no statement
expressions.  The tree contains none, and `-pedantic-errors` is on to
keep it that way.

Anything outside POSIX.1-2001 needs a fallback, or it does not go in.
The few places that read Linux-specific files do so through a named
constant, and carry on unharmed when the file is not there; that is the
pattern to follow for anything else of the kind.

Layout
------

Four spaces of indentation, never a tab.  There is not a single tab at
the start of a line anywhere in `src/`.

Braces follow K&R, with the one traditional exception for functions:
a function's opening brace sits alone in column zero, and every other
opening brace sits at the end of the line that introduces it.

```c
static bool s_thing_is_ready(const thing_td *thing)
{
    if (thing == NULL) {
        return false;
    }

    for (int i = 0; i < thing->count; ++i) {
        ...
    }

    return true;
}
```

`else` shares a line with the brace that closed the branch before it:
`} else if (...) {`.

A continuation line is indented eight spaces, twice the ordinary indent,
so that it cannot be mistaken for a nested block:

```c
    place_icon_apply(client, desktop, policy, icon_dim, screen_dim,
            &anchor, &icon_pos);
```

Code lines stop at 75-78 columns.  Comment lines stop at 72 if possible, and following the Vim format `:set tw=72 cpo+=J fo+=rj1np1`.  Rare exceptions are used, for example if it prevents new line by just one character, or in a line that cannot be broken.

One space in a declaration or an assignment, never several to line
something up.  A ternary has its condition in parentheses even when that
condition is a single word: `(a) ? b : c`.

Declarations
------------

Variables are declared at the start of the block that needs them:
a function, a `for`, an `if`, a `while`.  Never in the middle of
a function.

A loop counter used nowhere outside its loop is declared inside it:

```c
    for (int i = 0; i < n; ++i) {
```

About compound blocks, no bare `{ ... }` block opened purely to hold a
declaration partway through a function.  The only place such a block is
admitted is inside a `case`, and only where it cannot be avoided.

No forward declarations unless there's no other way.  A static function is
defined before the first function that calls it, which means a file reads
bottom-up, from its smallest pieces to its public ones.

A `.c` file is ordered: its includes, then any type it declares for
itself, then its file-scope variables, then its static functions, then
the public ones it implements.  A type or a variable never sits partway
down among the functions, however near the one place using it.

Every parameter is examined for whether it should be `const`, and
carries it wherever the function does not write through it.

Any structure introduced is checked for internal padding: order the
fields so that no hole falls between two of them.

Comments
--------

Comments are in United States English, wrapped at 72 columns, with two
spaces between sentences.  A comment of a single sentence carries no
full stop; two or more sentences are punctuated normally.

They explain **why**, not what.  The code already says what it does.

### Doxygen, and where it is required

No function goes undocumented.  Specifically:

  - A prototype in a header carries a full Doxygen block.
  - A static function in an implementation file carries a full Doxygen
    block as well.
  - A public function's definition in the `.c` file carries an ordinary
    comment above it, holding the text of its `@brief` and nothing more,
    so the two files are not made to disagree.

A full block means brief, description where one is warranted, every
`@param`, the `@return` or `@retval` set, the notes, and last of all the
complexity note.

```c
/**
 * @brief Choose where one client's icon goes, from scratch
 *
 * Everything a fresh icon position needs, in one place: the monitor
 * @p client sits on rather than the whole combined screen, and the
 * placement policy in force.
 *
 * @param client   Client whose icon is being placed
 * @param icon_dim Icon width/height, in pixels
 * @param out_pos  Receives the position, in root coordinates
 *
 * @return @c true when a position was chosen
 *
 * @note Answers @c false only for a client with no configuration
 *       attached, which has no placement policy to apply
 * @note Complexity: @e O(n), where @e n is the number of clients on the
 *       desktop
 *
 * @see Client type @c client_td and its lifecycle with @a client_init
 *      and @a client_destroy
 */
```

### Rules the notes follow

  - Only the long description ends in a full stop.  `@brief`, `@note`,
    `@return`, `@retval`, `@see` and every other tag do not.
  - One sentence per `@note`.  Two sentences mean two notes.
  - The complexity note comes last.  Nothing goes below it.
  - A note never says "as above", nor refers to another note by where it
    sits.  A note that cannot stand alone is a note in the wrong place.
  - A continuation line is aligned under where the tag's text begins:
    eight columns in for `@brief`, seven for `@note`.
  - A tag never ends a line separated from the word it marks.  Wrap
    before the tag, never between the tag and its word.

### Naming things inside a comment

In an ordinary C comment, an identifier goes in 'single quotes':

```c
    /* Paired with the 'client_unhide' that 's_map_finish' does */
```

In Doxygen, the tags do that work and no quoting is added around them:
`@c` for an identifier or a literal, `@p` for a parameter of the
function being documented, `@a` for a function named elsewhere, and `@e`
for ordinary emphasis.

### Characters

No Unicode in a comment.  Not an em dash, not an ellipsis character, not
an arrow.  The one exception is `§`, and only to cite a section of
a specification, as in ICCCM §4.1.2.3.

Naming
------

**Files** are one lowercase word, with no separator of any kind and no
capital letter: `map.c`, `winlist.c`, `outline.h`, `visibility.c`.  Not
one of the files in the tree should depart from this.  Where a single
word will not do, the directory carries the rest of the meaning, which
is why there is `policy/placement/window.c` rather than
`placement_window.c`, and four different files named `ewmh.h` in four
different directories, each with a distinct remit.  Names run to eight
o nine characters if possible; that brevity is deliberate, and in the
same spirit as the rest of the project.

**Functions** read anchor, then object, then action:

    place_manual_enqueue      place  + manual  + enqueue
    ccmd_client_iconify       ccmd   + client  + iconify
    ctxmenu_tree_handle_click ctxmenu + tree   + handle_click
    wm_startup_randr_init     wm_startup + randr + init

The anchor comes from the path, but contracted to whatever identifies
the module rather than joined together mechanically.  `cmds/client/`
gives `ccmd_client_`, not `cmds_client_`; `input/mouse/drag/` gives
`drag_`, not `input_mouse_drag_`; `policy/placement/manual.h` gives
`place_manual_`.  Read the anchor already in use in the directory being
worked in and follow it, and where the right one for something new is
not obvious, ask before writing it rather than after.

`_init` is a suffix and only ever a suffix.  It is
`wm_startup_randr_init`, never `wm_startup_init_randr`.  The same goes
for `_destroy`.

**Everything file-local takes an `s_` prefix**, not functions alone:
a static function, a static variable, and a type declared in a `.c` file
and used nowhere else.  Two hundred and fourteen of the tree's two
hundred and sixteen file-scope variables do, and the two that do not are
older than the convention.

    static void s_place_window_finalize(...)
    static struct position_s s_cascade_last = { 0, 0 };
    struct s_place_ctx_s { ... };
    enum s_place_result_e { ... };

**Types carry a suffix saying what they are**: `_s` on a struct tag,
`_e` on an enumeration tag, `_td` on a typedef, and `_fn` on a function
pointer type.

    struct client_layout_s      enum config_gravity_e
    client_td                   place_manual_done_fn

Identifiers are descriptive and anchored.  A boolean reads as
a predicate and begins with a verb: `is_iconified`, `has_pending`,
`was_decorated_fullscreen`.

Constants that configure something live in `include/defs/`, one header
per domain, not scattered as literals through the code.

Header files
------------

Every header compiles alone.  All of them do (or should), and a patch is
expected to keep it so:

```sh
printf '#include <client/icccm.h>\nint main(void){return 0;}\n' > t.c
gcc -std=c99 -Wall -Wextra -Werror -I include -fsyntax-only t.c
```

A header that leans on its includer having included something first is
a header that will break the day somebody includes it somewhere new.

Include guards take two blank lines after the `#ifndef`/`#define` pair,
and two before the `#endif`, which carries the guard name in a trailing
comment.

Includes are grouped, each group behind a short comment saying what it
is: system, XCB, types, defaults, then the project's.

Documentation
-------------

A change in behavior is a change in `doc/`.  The manual pages under
`doc/man/` and the guides under `doc/` are not an afterthought; they are
the interface most people will meet.

A comment that no longer describes what its code does is worse than no
comment, because it is believed.  When behavior moves, the comment above
it moves with it, in the same patch.

Commit messages
---------------

A title of at most 52 characters, in United States English, in the
imperative, with no full stop.

A body wrapped at 70 columns, in prose paragraphs.  Not a bullet list.
Identifiers in the body go in `backticks`.

The body says what was wrong and why the change is the right answer, not
what the diff already shows.  A reader a year hence has the diff; what
they have not got is the reason.

Reporting a problem
-------------------

Say what was expected, what happened, and how to get there from a fresh
start.  Include the output of `icowm -v`, which carries the version and
the build, and the relevant part of the log.

A configuration small enough to reproduce the problem is worth far more
than a full one.
