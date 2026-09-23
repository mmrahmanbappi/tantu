#!/bin/sh
# tantu tests. Usage: sh tests/run.sh path/to/tantu
set -e
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
ROOT=$(pwd)
TMP="$ROOT/tests/tmp"
rm -rf "$TMP"
mkdir -p "$TMP"
fail() { echo "FAIL: $1"; exit 1; }
has() { grep -q -- "$2" "$1" || fail "$1 should contain: $2"; }
hasnt() { if grep -q -- "$2" "$1"; then fail "$1 should not contain: $2"; fi; }

for theme in blog portfolio resume research club event docs course business gallery; do
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
  if command -v python3 > /dev/null; then
    python3 - "$PWD/public" << 'PYCHK' || fail "$theme: invalid JSON-LD"
import json, re, sys, pathlib
for f in pathlib.Path(sys.argv[1]).rglob("*.html"):
    for j in re.findall(r'<script type="application/ld\+json">(.*?)</script>', f.read_text(), re.S):
        json.loads(j)
PYCHK
  fi
  echo "ok: $theme theme builds"
done

# Base path support (site in a sub-folder)
cd "$TMP/site-blog"
sed -i.bak 's#^base_url = .*#base_url = https://example.com/sub#' site.conf
"$BIN" build > /dev/null
has public/index.html 'href="/sub/assets/style.css"'
has public/sitemap.xml '<loc>https://example.com/sub/blog/'
echo "ok: base path"

# Social images, search and analytics
[ -f public/og/home.png ] || fail "home social image missing"
head -c 8 public/og/home.png | od -An -c | grep -q "P   N   G" || fail "social image is not a PNG"
has public/index.html 'og:image:width'
[ -f public/search-index.json ] || fail "search index missing"
[ -f public/search/index.html ] || fail "search page missing"
has public/search/index.html 'noindex'
hasnt public/sitemap.xml '/search/'
printf 'google_analytics = G-ABC123XYZ\n' >> site.conf
"$BIN" build > /dev/null
has public/index.html 'googletagmanager.com/gtag/js?id=G-ABC123XYZ'
has public/.htaccess 'https://www.googletagmanager.com'
[ -f public/assets/tantu-analytics.js ] || fail "analytics loader missing"
printf 'google_analytics = <script>\n' >> site.conf
"$BIN" build > /dev/null 2>&1
hasnt public/index.html '<script>alert'
echo "ok: social images, search, analytics"

# Responsive images (needs Python with Pillow to make a test photo)
if python3 -c "import PIL" 2> /dev/null; then
  python3 -c "from PIL import Image; Image.new('RGB',(2000,1000),'#336699').save('static/images/wide.jpg')"
  printf -- '---\ntitle: Photo test\ndate: 2026-01-02\n---\n![A blue test photo](/images/wide.jpg)\n' > content/posts/photo.md
  "$BIN" build > /dev/null
  [ -f public/images/w/wide-960.jpg ] || fail "resized image missing"
  has public/blog/photo/index.html 'srcset="/sub/images/w/wide-480.jpg 480w'
  has public/blog/photo/index.html 'width="2000" height="1000"'
  echo "ok: responsive images"
fi

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

# Dashboard API security
cd "$TMP/site-blog"
sed -i.bak 's#^base_url = .*#base_url = https://example.com#' site.conf
PORT=18765
"$BIN" dashboard --port $PORT --no-browser > dash.log 2>&1 &
DPID=$!
trap 'kill $DPID 2>/dev/null || true' EXIT
for i in 1 2 3 4 5 6 7 8 9 10; do grep -q "t=" dash.log && break; sleep 0.5; done
TOK=$(grep -o 't=[0-9a-f]*' dash.log | head -1 | cut -d= -f2)
[ -n "$TOK" ] || fail "dashboard did not start"
U="http://127.0.0.1:$PORT/_tantu/api"
code() { curl -s -o /dev/null -w "%{http_code}" "$@"; }
[ "$(code "$U/state")" = 403 ] || fail "API must refuse requests without the token"
[ "$(code -H "X-Tantu-Token: $TOK" -H "Host: evil.example" "$U/state")" = 403 ] || fail "API must refuse other hosts"
[ "$(code -H "X-Tantu-Token: $TOK" "$U/state")" = 200 ] || fail "state should work with the token"
[ "$(code -H "X-Tantu-Token: $TOK" "$U/file?path=site.conf")" = 400 ] || fail "must not read site.conf through the file API"
[ "$(code -H "X-Tantu-Token: $TOK" "$U/file?path=content/../../etc/passwd.md")" = 400 ] || fail "path traversal must be refused"
[ "$(code -H "X-Tantu-Token: $TOK" -X POST --data 'x' "$U/file?path=content/posts/a/b.md")" = 400 ] || fail "nested paths must be refused"
[ "$(code -H "X-Tantu-Token: $TOK" -X POST --data 'not an image' "$U/upload?name=evil.png")" = 400 ] || fail "fake images must be refused"
[ "$(code -H "X-Tantu-Token: $TOK" -X POST --data '<svg/>' "$U/upload?name=a.svg")" = 400 ] || fail "SVG uploads must be refused"
printf '\211PNG\r\n\032\n0000000000' > tiny.png
[ "$(code -H "X-Tantu-Token: $TOK" -X POST --data-binary @tiny.png "$U/upload?name=My%20Photo.png")" = 200 ] || fail "real PNG upload failed"
[ -f static/images/my-photo.png ] || fail "upload was not saved with a clean name"
R=$(curl -s -H "X-Tantu-Token: $TOK" -X POST "$U/new?kind=post&title=Hello%20dashboard")
echo "$R" | grep -q 'content/posts/' || fail "new post failed: $R"
curl -s -H "X-Tantu-Token: $TOK" -o site.zip "$U/export"
python3 -c "import zipfile,sys; z=zipfile.ZipFile('site.zip'); n=z.namelist(); assert 'index.html' in n and '.htaccess' in n, n" || fail "export ZIP is broken"
[ "$(code "http://127.0.0.1:$PORT/_tantu/")" = 200 ] || fail "dashboard page should load"
kill $DPID 2>/dev/null || true
echo "ok: dashboard API and security"

# FTP upload (needs Python with pyftpdlib)
if python3 -c "import pyftpdlib" 2> /dev/null; then
  mkdir -p "$TMP/ftproot/www"
  python3 - "$TMP/ftproot" > /dev/null 2>&1 << 'PYFTP' &
import sys
from pyftpdlib.authorizers import DummyAuthorizer
from pyftpdlib.handlers import FTPHandler
from pyftpdlib.servers import FTPServer
a = DummyAuthorizer(); a.add_user("u", "p", sys.argv[1], perm="elradfmwMT")
FTPHandler.authorizer = a
FTPServer(("127.0.0.1", 18766), FTPHandler).serve_forever()
PYFTP
  FPID=$!
  sleep 1
  cd "$TMP/site-portfolio"
  printf 'ftp_host = localhost\nftp_port = 18766\nftp_user = u\nftp_dir = /www\n' >> site.conf
  TANTU_FTP_PASSWORD=p "$BIN" publish > ftp.log 2>&1 || { cat ftp.log; kill $FPID; fail "FTP upload failed"; }
  [ -f "$TMP/ftproot/www/index.html" ] || { kill $FPID; fail "FTP did not upload index.html"; }
  [ -f "$TMP/ftproot/www/.htaccess" ] || { kill $FPID; fail "FTP did not upload .htaccess"; }
  TANTU_FTP_PASSWORD=p "$BIN" publish 2>&1 | grep -q "Nothing changed" || { kill $FPID; fail "second upload should send nothing"; }
  kill $FPID 2>/dev/null || true
  echo "ok: FTP upload"
fi

echo "All tests passed."
