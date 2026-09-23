#!/bin/sh
# tantu tests. Usage: sh tests/run.sh path/to/tantu
set -e
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
ROOT=$(pwd)
TMP="$ROOT/tests/tmp"
rm -rf "$TMP"
mkdir -p "$TMP"
export TANTU_HOME="$ROOT"
fail() { echo "FAIL: $1"; exit 1; }
has() { grep -q -- "$2" "$1" || fail "$1 should contain: $2"; }
hasnt() { if grep -q -- "$2" "$1"; then fail "$1 should not contain: $2"; fi; }

for theme in blog portfolio; do
  cd "$TMP"
  "$BIN" new "site-$theme" --theme "$theme" > /dev/null
  cd "site-$theme"
  "$BIN" build > /dev/null
  for f in index.html 404.html sitemap.xml robots.txt feed.xml .htaccess _headers assets/style.css; do
    [ -f "public/$f" ] || fail "$theme: missing public/$f"
  done
  has public/index.html '<title>'
  has public/index.html 'rel="canonical"'
  has public/index.html 'og:image'
  has public/index.html 'application/ld+json'
  has public/404.html 'noindex'
  hasnt public/404.html 'rel="canonical"'
  echo "ok: $theme theme builds"
done

# Base path support (site in a sub-folder)
cd "$TMP/site-blog"
sed -i.bak 's#^base_url = .*#base_url = https://example.com/sub#' site.conf
"$BIN" build > /dev/null
has public/index.html 'href="/sub/assets/style.css"'
has public/sitemap.xml '<loc>https://example.com/sub/blog/'
echo "ok: base path"

# Security: raw HTML and javascript: links in Markdown must be neutralised
cat > content/posts/xss.md << 'MD'
---
title: <script>alert(1)</script>
date: 2026-01-01
---
<script>alert("x")</script>

[click](javascript:alert(1)) and ![img](data:text/html,bad)
MD
"$BIN" build > /dev/null
P=public/blog/xss/index.html
[ -f "$P" ] || fail "xss post not built"
hasnt "$P" '<script>alert'
hasnt "$P" 'javascript:'
hasnt "$P" 'data:text/html'
has "$P" '&lt;script&gt;'
echo "ok: html escaping and unsafe links"

# Drafts are skipped
printf -- '---\ntitle: Secret\ndraft: true\n---\nhidden\n' > content/posts/secret.md
"$BIN" build > /dev/null
[ ! -d public/blog/secret ] || fail "draft was published"
echo "ok: drafts"

# Refuses to delete a folder it did not create
cd "$TMP" && mkdir -p guard && cd guard
cp ../site-blog/site.conf . && cp -r ../site-blog/themes . && mkdir -p public && echo keep > public/important.txt
if "$BIN" build > /dev/null 2>&1; then fail "should refuse to overwrite foreign public/"; fi
[ -f public/important.txt ] || fail "foreign file was deleted"
echo "ok: output folder guard"

echo "All tests passed."
