#!/bin/sh
#
# po2mo.sh -- compile every '<lang>/LC_MESSAGES/*.po' under here into
#             its own '*.mo', right beside it, one language directory
#             at a time.
#
# Run from inside 'locale/' itself (the same directory 'default.pot'
# lives in):
#
#   cd locale
#   ./po2mo.sh
#
# Requires 'msgfmt' (part of GNU gettext).

if ! command -v msgfmt >/dev/null 2>&1; then
    echo "po2mo.sh: msgfmt not found (install gettext)" >&2
    exit 1
fi

total=0
failed=0

for dir in */LC_MESSAGES; do
    [ -d "$dir" ] || continue
    echo "$dir:"
    for po in "$dir"/*.po; do
        [ -f "$po" ] || continue
        total=$((total + 1))
        mo=${po%.po}.mo
        if msgfmt -o "$mo" "$po"; then
            echo "  OK   ${po##*/} -> ${mo##*/}"
        else
            echo "  FAIL ${po##*/}" >&2
            failed=$((failed + 1))
        fi
    done
done

if [ "$total" -eq 0 ]; then
    echo "po2mo.sh: no .po files found under */LC_MESSAGES/" >&2
    exit 1
fi

echo ""
echo "$((total - failed))/$total compiled"
[ "$failed" -eq 0 ]
