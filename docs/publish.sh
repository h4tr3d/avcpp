#!/bin/sh
#
# Publish a built documentation site to the gh-pages branch.
#
# The script owns the whole flow: it prepares a worktree of the branch (or an
# orphan one on the first publish), removes the flat 2.x layout once, copies the
# version being published, keeps `latest` pointing at the newest release,
# regenerates the version chooser and pushes.
#
# Usage: docs/publish.sh [--site DIR] [--remote NAME] [--branch NAME] [--no-push]
#
# Environment:
#   PUBLISH_AUTHOR_NAME   author name for the commit (default: github-actions[bot])
#   PUBLISH_AUTHOR_EMAIL  author email for the commit
#
# The site directory must contain <major.minor>/ produced by docs/build.sh.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
VERSION=$(tr -d '[:space:]' < "$ROOT/VERSION.txt")
DOC_VERSION=${VERSION%.*}

SITE="$ROOT/build/docs/site"
REMOTE=origin
BRANCH=gh-pages
PUSH=1

AUTHOR_NAME=${PUBLISH_AUTHOR_NAME:-github-actions[bot]}
AUTHOR_EMAIL=${PUBLISH_AUTHOR_EMAIL:-41898282+github-actions[bot]@users.noreply.github.com}

usage()
{
    sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --site)
            [ $# -ge 2 ] || { echo "docs/publish.sh: --site needs a value" >&2; exit 2; }
            SITE=$2
            shift 2
            ;;
        --remote)
            [ $# -ge 2 ] || { echo "docs/publish.sh: --remote needs a value" >&2; exit 2; }
            REMOTE=$2
            shift 2
            ;;
        --branch)
            [ $# -ge 2 ] || { echo "docs/publish.sh: --branch needs a value" >&2; exit 2; }
            BRANCH=$2
            shift 2
            ;;
        --no-push)
            PUSH=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "docs/publish.sh: unknown option: $1" >&2
            exit 2
            ;;
    esac
done

case "$SITE" in
    /*) ;;
    *) SITE="$PWD/$SITE" ;;
esac

# The push runs from a worktree in a temporary directory, where a relative
# remote URL would resolve against the wrong path. Make it absolute once.
REMOTE_URL=$(git -C "$ROOT" remote get-url "$REMOTE")
case "$REMOTE_URL" in
    /*|*:*)
        ;;
    *)
        REMOTE_URL="$ROOT/$REMOTE_URL"
        ;;
esac

if [ ! -d "$SITE/$DOC_VERSION" ]; then
    echo "docs/publish.sh: no built site at $SITE/$DOC_VERSION" >&2
    echo "run docs/build.sh first" >&2
    exit 1
fi

WORK=$(mktemp -d)
PAGES="$WORK/pages"
WORKTREE_ADDED=0

cleanup()
{
    if [ "$WORKTREE_ADDED" = 1 ]; then
        git -C "$ROOT" worktree remove --force "$PAGES" >/dev/null 2>&1 || true
    fi
    rm -rf "$WORK"
}
trap cleanup EXIT INT TERM

# The branch may already exist on the remote. actions/checkout fetches only the
# ref being built, so it has to be fetched explicitly before it can be checked
# out; `git ls-remote` only asks the server and creates nothing locally.
if git -C "$ROOT" ls-remote --exit-code --heads "$REMOTE" "$BRANCH" >/dev/null 2>&1; then
    git -C "$ROOT" fetch --no-tags --depth=1 "$REMOTE" \
        "+refs/heads/$BRANCH:refs/remotes/$REMOTE/$BRANCH"
    git -C "$ROOT" worktree add --detach "$PAGES" "$REMOTE/$BRANCH"
    WORKTREE_ADDED=1
else
    git -C "$ROOT" worktree add --detach "$PAGES"
    WORKTREE_ADDED=1
    git -C "$PAGES" checkout --orphan "$BRANCH"
    git -C "$PAGES" rm -rf . >/dev/null 2>&1 || true
fi

cd "$PAGES"

# One-time migration away from the flat 2.x site. GitHub Pages serves an
# existing file instead of returning 404, so while those pages sit in the root
# the redirect in 404.html can never fire for the URLs people bookmarked.
if [ -n "$(find . -maxdepth 1 -name 'classav_1_1*.html' -print -quit)" ]; then
    echo "removing the legacy flat site from the branch root"
    for entry in * .*; do
        case "$entry" in
            .|..|.git|.nojekyll|index.html|404.html|CNAME|latest)
                continue
                ;;
        esac
        if [ -d "$entry" ]; then
            case "$entry" in
                [0-9]*.[0-9]*)
                    continue
                    ;;
            esac
        fi
        rm -rf -- "$entry"
    done
fi

# Only the version being published is replaced; older version directories are
# left alone, so the archive survives.
rm -rf "$DOC_VERSION"
cp -a "$SITE/$DOC_VERSION" "$DOC_VERSION"

# `latest` points at the newest release: republishing an older tag must not move
# it backwards.
if [ -f latest/version.txt ]; then
    newest=$(cat latest/version.txt)
    if [ "$(printf '%s\n%s\n' "$newest" "$VERSION" | sort -V | tail -1)" != "$VERSION" ]; then
        echo "keeping latest at $newest, publishing $VERSION"
        update_latest=0
    else
        update_latest=1
    fi
else
    update_latest=1
fi

if [ "$update_latest" = 1 ]; then
    rm -rf latest
    cp -a "$DOC_VERSION" latest
fi

"$ROOT/docs/site-index.sh" .

git add -A
if git diff --cached --quiet; then
    echo "documentation is unchanged, nothing to publish"
    exit 0
fi

git -c "user.name=$AUTHOR_NAME" -c "user.email=$AUTHOR_EMAIL" \
    commit -m "docs: publish $VERSION"

if [ "$PUSH" = 1 ]; then
    git push "$REMOTE_URL" "HEAD:$BRANCH"
else
    echo "not pushing (--no-push); the commit is in $PAGES"
fi
