#!/bin/sh
#
# Regenerate the site root: index.html (version chooser) and 404.html
# (redirect map for URLs published before the versioned layout).
#
# Scanning the directory instead of hardcoding a version list keeps the chooser
# honest: it lists exactly the version directories that exist, so it can be
# re-run inside the gh-pages checkout after a new version has been copied in.
#
# Usage: docs/site-index.sh <site-directory>

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ $# -ne 1 ]; then
    echo "docs/site-index.sh: expected exactly one argument: the site directory" >&2
    exit 2
fi

SITE=$1
if [ ! -d "$SITE" ]; then
    echo "docs/site-index.sh: no such directory: $SITE" >&2
    exit 1
fi

links=""
for dir in "$SITE"/*; do
    [ -d "$dir" ] || continue
    name=$(basename "$dir")
    case "$name" in
        latest)
            continue
            ;;
        [0-9]*.[0-9]*|[0-9]*.[0-9]*.[0-9]*)
            links="$links    <li><a href=\"$name/\">avcpp $name</a> &middot; <a href=\"$name/ru/\">русский</a></li>
"
            ;;
    esac
done

if [ -d "$SITE/latest" ]; then
    links="    <li><a href=\"latest/\">latest</a> &middot; <a href=\"latest/ru/\">русский</a></li>
$links"
fi

cat > "$SITE/index.html" <<EOF
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>avcpp documentation</title>
<style>
  :root { color-scheme: light dark; }
  body { font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
         max-width: 40rem; margin: 4rem auto; padding: 0 1.5rem; line-height: 1.5; }
  h1 { font-size: 1.6rem; margin-bottom: 0.25rem; }
  p.sub { margin-top: 0; opacity: 0.7; }
  ul { padding-left: 1.2rem; }
  li { margin: 0.35rem 0; }
</style>
</head>
<body>
<h1>avcpp documentation</h1>
<p class="sub">Wrapper for FFmpeg that simplifies its use from C++ projects.</p>
<ul>
$links</ul>
<p>
  <a href="https://github.com/h4tr3d/avcpp">Source code</a> &middot;
  <a href="https://github.com/h4tr3d/avcpp/issues">Issues</a>
</p>
</body>
</html>
EOF

cp "$ROOT/docs/theme/404.html" "$SITE/404.html"

echo "index.html and 404.html written to $SITE"
