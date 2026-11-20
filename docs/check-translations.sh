#!/bin/sh
#
# Report the state of the Russian translations.
#
# The list of pairs lives in docs/translations.yaml; staleness is derived from
# git history: if the English page was committed after the Russian one, the
# translation is probably behind. A pair that changed together is current.
#
# This is a report, not a gate: stale translations never fail the build. The
# cost of writing prose is high enough without a red CI run on every English
# typo fix.
#
# Usage: docs/check-translations.sh [--verbose]

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
MANIFEST="$ROOT/docs/translations.yaml"

VERBOSE=0
case "${1:-}" in
    "")
        ;;
    --verbose)
        VERBOSE=1
        ;;
    -h|--help)
        sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'
        exit 0
        ;;
    *)
        echo "docs/check-translations.sh: unknown argument: $1" >&2
        exit 2
        ;;
esac

if [ ! -f "$MANIFEST" ]; then
    echo "docs/check-translations.sh: no manifest at $MANIFEST" >&2
    exit 1
fi

if ! git -C "$ROOT" rev-parse --git-dir >/dev/null 2>&1; then
    echo "docs/check-translations.sh: not a git checkout, nothing to compare" >&2
    exit 0
fi

last_commit_time()
{
    stamp=$(git -C "$ROOT" log -1 --format=%ct -- "$1" 2>/dev/null | head -1 || true)
    if [ -z "$stamp" ]; then
        # Not committed yet, or never touched: treat as "same age".
        stamp=0
    fi
    echo "$stamp"
}

pairs=$(mktemp)
trap 'rm -f "$pairs"' EXIT INT TERM

# The manifest has a fixed shape: "- en: <path>" followed by "ru: <path>".
# Parsing it with awk keeps the script dependency-free.
awk '
    /^[[:space:]]*- en: / { sub(/^[[:space:]]*- en: /, ""); en = $0; next }
    /^[[:space:]]*ru: /    { sub(/^[[:space:]]*ru: /, "");    ru = $0; printf "%s\t%s\n", en, ru }
' "$MANIFEST" > "$pairs"

pair_count=0
stale=""
missing=""

while IFS='	' read -r en ru; do
    [ -n "$en" ] || continue
    pair_count=$((pair_count + 1))

    if [ ! -f "$ROOT/docs/$ru" ]; then
        missing="$missing
  docs/$ru (declared for docs/$en)"
        continue
    fi

    en_time=$(last_commit_time "docs/$en")
    ru_time=$(last_commit_time "docs/$ru")

    if [ "$en_time" -gt "$ru_time" ]; then
        # git log needs a revision, not the timestamp used for the comparison.
        ru_commit=$(git -C "$ROOT" log -1 --format=%H -- "docs/$ru")
        behind=$(git -C "$ROOT" rev-list --count "$ru_commit..HEAD" -- "docs/$en" 2>/dev/null || echo '?')
        stale="$stale
  docs/$ru  (English changed in $behind commit(s) after the translation)"
    elif [ "$VERBOSE" = 1 ]; then
        echo "ok      docs/$ru"
    fi
done < "$pairs"

# English pages with neither a pair nor an explicit "untranslated" entry.
untracked=""
for page in $(cd "$ROOT/docs" && find en -name '*.md' | sort); do
    if grep -q -- "en: $page$" "$MANIFEST"; then
        continue
    fi
    if grep -q -- "- $page$" "$MANIFEST"; then
        continue
    fi
    untracked="$untracked
  docs/$page"
done

echo "translation pairs: $pair_count"

if [ -n "$stale" ]; then
    echo "possibly stale:$stale"
fi

if [ -n "$missing" ]; then
    echo "declared but missing:$missing"
fi

if [ -n "$untracked" ]; then
    echo "English pages not tracked in translations.yaml:$untracked"
fi

if [ -z "$stale" ] && [ -z "$missing" ] && [ -z "$untracked" ]; then
    echo "all pairs are current"
fi
