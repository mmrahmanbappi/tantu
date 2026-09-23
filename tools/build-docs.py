#!/usr/bin/env python3
"""Turn the guides in docs/*.md into web pages under docs/ on the GitHub Pages site.

Run from the repo root after editing a guide:
    python3 -m pip install --user markdown
    python3 tools/build-docs.py

The .md files stay the source. The .html files are generated, so edit the .md, not the .html.
"""
import html
import json
import re
from pathlib import Path

import markdown

ROOT = Path(__file__).resolve().parent.parent
BASE = "https://mmrahmanbappi.github.io/tantu-c-framework/"
REPO = "https://github.com/mmrahmanbappi/tantu-c-framework"
DATE = "2026-09-23"

GUIDES = [
    {
        "src": "DEVELOPING.md", "slug": "developing",
        "title": "How to Build and Change Tantu C Framework",
        "short": "Developing tantu",
        "desc": "How to build, test and change Tantu C Framework: make commands, how a build works, where each source file lives, the code rules and how a release is made.",
        "blurb": "Build and test the code, learn how a site build works, find each source file and follow the code rules.",
    },
    {
        "src": "THEMES.md", "slug": "themes",
        "title": "How to Make a Theme for Tantu C Framework",
        "short": "Making a theme",
        "desc": "Make your own theme for Tantu C Framework: the theme folder, theme.conf settings, the template language, the variables you can use and the rules to follow.",
        "blurb": "Copy a starter theme, set up theme.conf, and use the template language and variables to build your own look.",
    },
    {
        "src": "PLAN.md", "slug": "plan",
        "title": "The Tantu C Framework Plan: Releases and Features",
        "short": "The tantu plan",
        "desc": "The full plan for Tantu C Framework: three releases covering setup, the dashboard, SEO, Open Graph, speed, security, site features, ten student themes and framework modules.",
        "blurb": "Every release and feature on the way: setup, dashboard, SEO, social cards, speed, security, themes and modules.",
    },
]

CSS = """:root{--ink:#1F2A5C;--text:#2B2F3F;--muted:#565C74;--line:#D5D9E5;--paper:#FFFFFF;--soft:#F2F4F9;--madder:#B23A3A;--turmeric:#C8912B;--indigo:#2E3F8F;
--display:"Bricolage Grotesque","Segoe UI",system-ui,sans-serif;--body:"Literata",Georgia,"Times New Roman",serif;--mono:ui-monospace,"Cascadia Code",Menlo,Consolas,monospace}
*{box-sizing:border-box}html{scroll-behavior:smooth}
body{margin:0;background:var(--paper);color:var(--text);font:1.125rem/1.75 var(--body);-webkit-font-smoothing:antialiased}
a{color:var(--indigo);text-underline-offset:3px}a:hover{color:var(--madder)}
:focus-visible{outline:3px solid var(--turmeric);outline-offset:3px;border-radius:2px}
.skip{position:absolute;left:-999px;top:0;background:var(--ink);color:#fff;padding:.6rem 1rem}.skip:focus{left:1rem;top:1rem}
.wrap{max-width:1080px;margin:0 auto;padding:0 1.5rem}
h1,h2,h3{font-family:var(--display);color:var(--ink);line-height:1.15;margin:0 0 .75rem}
h1{font-size:clamp(2.1rem,4.5vw,3.2rem);font-weight:700;letter-spacing:-.02em}
h2{font-size:clamp(1.5rem,2.6vw,1.9rem);font-weight:700;margin-top:2.6rem;scroll-margin-top:1rem}
h3{font-size:1.2rem;font-weight:500;margin-top:1.8rem}
p{margin:0 0 1.1rem}
.top{border-bottom:1px solid var(--line)}
.top .wrap{display:flex;align-items:center;justify-content:space-between;gap:1rem;min-height:4.25rem;flex-wrap:wrap}
.brand{font-family:var(--display);font-weight:700;font-size:1.5rem;color:var(--ink);text-decoration:none;letter-spacing:-.02em}
.top nav{display:flex;gap:1.4rem;flex-wrap:wrap;font-family:var(--display);font-size:1rem}
.top nav a{color:var(--text);text-decoration:none}.top nav a:hover,.top nav a[aria-current]{color:var(--madder)}
.crumbs{font-family:var(--display);font-size:.95rem;color:var(--muted);margin:2rem 0 1rem}.crumbs a{color:var(--muted)}
.layout{display:grid;grid-template-columns:15rem 1fr;gap:3rem;padding-bottom:4rem}
.side{position:sticky;top:1.5rem;align-self:start;font-family:var(--display);font-size:.98rem}
.side h2{font-size:1rem;margin:0 0 .6rem;color:var(--muted);text-transform:uppercase;letter-spacing:.06em}
.side ul{list-style:none;margin:0 0 1.6rem;padding:0}.side li{margin:.35rem 0}
.side a{color:var(--text);text-decoration:none}.side a:hover,.side a[aria-current]{color:var(--madder)}
.prose{max-width:44rem;min-width:0}
.lead{font-size:1.2rem;color:var(--muted)}
.prose ul,.prose ol{padding-left:1.4rem;margin:0 0 1.1rem}.prose li{margin-bottom:.35rem}
code{font-family:var(--mono);font-size:.93em;background:var(--soft);padding:.1rem .35rem;border-radius:4px}
pre{background:var(--ink);color:#E9ECF5;padding:1.2rem 1.4rem;border-radius:8px;overflow-x:auto;line-height:1.6;margin:0 0 1.3rem}
pre code{background:none;padding:0;color:inherit;font-size:.93rem}
table{border-collapse:collapse;width:100%;margin:0 0 1.3rem;font-size:1rem;display:block;overflow-x:auto}
th,td{border-bottom:1px solid var(--line);padding:.55rem .7rem;text-align:left;vertical-align:top}th{font-family:var(--display);color:var(--ink)}
.cards{list-style:none;padding:0;margin:2rem 0;display:grid;gap:1.4rem}
.cards li{border:1px solid var(--line);border-top:4px solid var(--turmeric);border-radius:8px;padding:1.3rem 1.4rem}
.cards h2{margin:0 0 .4rem;font-size:1.35rem}.cards p{margin:0;color:var(--muted)}
.edit{font-family:var(--display);font-size:.95rem;color:var(--muted);border-top:1px solid var(--line);padding-top:1.2rem;margin-top:2.5rem}
.pager{display:flex;justify-content:space-between;gap:1rem;flex-wrap:wrap;font-family:var(--display);margin-top:1.5rem}
footer{border-top:1px solid var(--line);padding:2.5rem 0;font-size:1rem;color:var(--muted)}
footer .wrap{display:flex;justify-content:space-between;gap:1rem;flex-wrap:wrap}
@media(max-width:820px){.layout{grid-template-columns:1fr;gap:1rem}.side{position:static;border-bottom:1px solid var(--line);padding-bottom:.5rem}}"""


def head(title, desc, url, schema):
    t, d = html.escape(title), html.escape(desc)
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{t}</title>
<meta name="description" content="{d}">
<meta name="robots" content="index, follow, max-image-preview:large">
<meta name="author" content="mmrahmanbappi">
<link rel="canonical" href="{url}">
<meta name="theme-color" content="#1F2A5C">
<meta property="og:type" content="article">
<meta property="og:site_name" content="Tantu C Framework">
<meta property="og:title" content="{t}">
<meta property="og:description" content="{d}">
<meta property="og:url" content="{url}">
<meta property="og:image" content="{BASE}og-image.png">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta name="twitter:card" content="summary_large_image">
<meta name="twitter:title" content="{t}">
<meta name="twitter:description" content="{d}">
<meta name="twitter:image" content="{BASE}og-image.png">
<link rel="icon" href="../favicon.svg" type="image/svg+xml">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,500;12..96,700&family=Literata:opsz,wght@7..72,400;7..72,600&display=swap" rel="stylesheet">
<script type="application/ld+json">{json.dumps(schema, ensure_ascii=False)}</script>
<style>{CSS}</style>
</head>
<body>
<a class="skip" href="#main">Skip to content</a>
<header class="top">
  <div class="wrap">
    <a class="brand" href="../">Tantu C Framework</a>
    <nav aria-label="Main">
      <a href="../#start">Get started</a>
      <a href="../#features">Features</a>
      <a href="./" aria-current="page">Docs</a>
      <a href="../#faq">FAQ</a>
      <a href="{REPO}">GitHub</a>
    </nav>
  </div>
</header>
"""


FOOT = f"""<footer>
  <div class="wrap">
    <p>tantu is open source under the <a href="{REPO}/blob/main/LICENSE">MIT license</a>.</p>
    <p>&copy; 2026 tantu contributors. <a href="{REPO}">Source on GitHub</a></p>
  </div>
</footer>
</body>
</html>
"""

PUBLISHER = {"@type": "Person", "name": "mmrahmanbappi", "url": "https://github.com/mmrahmanbappi"}
SITE = {"@id": BASE + "#website"}


def crumbs(items):
    return {"@type": "BreadcrumbList", "itemListElement": [
        {"@type": "ListItem", "position": i + 1, "name": n, "item": u} for i, (n, u) in enumerate(items)]}


def side(current, toc):
    cur = ' aria-current="page"'
    guides = "".join(
        f'<li><a href="{g["slug"]}.html"{cur if g["slug"] == current else ""}>{g["short"]}</a></li>' for g in GUIDES)
    on_page = "".join(f'<li><a href="#{i}">{html.escape(t)}</a></li>' for i, t in toc)
    block = f'<h2>On this page</h2><ul>{on_page}</ul>' if on_page else ""
    return f'<nav class="side" aria-label="Docs"><h2>Guides</h2><ul><li><a href="./"{cur if current == "index" else ""}>All docs</a></li>{guides}</ul>{block}</nav>'


def build_guide(i, g):
    src = (ROOT / "docs" / g["src"]).read_text(encoding="utf-8")
    md = markdown.Markdown(extensions=["fenced_code", "tables", "toc"], extension_configs={"toc": {"toc_depth": "2"}})
    body = md.convert(src)
    body = re.sub(r"<h1[^>]*>.*?</h1>\s*", "", body, count=1, flags=re.S)
    # links between guides should stay on the site
    for other in GUIDES:
        body = body.replace(f'href="{other["src"]}', f'href="{other["slug"]}.html').replace(f'href="docs/{other["src"]}', f'href="{other["slug"]}.html')
    toc = [(t["id"], t["name"]) for t in md.toc_tokens if t["level"] == 2]
    h1 = re.search(r"^# (.+)$", src, re.M).group(1)
    url = f"{BASE}docs/{g['slug']}.html"
    schema = {"@context": "https://schema.org", "@graph": [
        {"@type": "TechArticle", "@id": url + "#article", "headline": g["title"], "description": g["desc"], "url": url,
         "inLanguage": "en", "datePublished": DATE, "dateModified": DATE, "author": PUBLISHER, "publisher": PUBLISHER,
         "isPartOf": SITE, "image": BASE + "og-image.png", "about": {"@type": "SoftwareApplication", "name": "Tantu C Framework",
         "applicationCategory": "DeveloperApplication", "operatingSystem": "Linux, macOS, Windows"}},
        crumbs([("Home", BASE), ("Docs", BASE + "docs/"), (g["short"], url)])]}
    prev_g, next_g = (GUIDES[i - 1] if i else None), (GUIDES[i + 1] if i + 1 < len(GUIDES) else None)
    pager = '<nav class="pager" aria-label="More guides">' + \
        (f'<a href="{prev_g["slug"]}.html">Previous: {prev_g["short"]}</a>' if prev_g else '<span></span>') + \
        (f'<a href="{next_g["slug"]}.html">Next: {next_g["short"]}</a>' if next_g else '<span></span>') + '</nav>'
    page = head(g["title"], g["desc"], url, schema) + f"""<div class="wrap">
<p class="crumbs"><a href="../">Home</a> / <a href="./">Docs</a> / {html.escape(g["short"])}</p>
<div class="layout">
{side(g["slug"], toc)}
<main id="main" class="prose">
<h1>{html.escape(h1)}</h1>
{body}
<p class="edit">This page is made from <a href="{REPO}/blob/main/docs/{g["src"]}">docs/{g["src"]}</a>. Spotted a mistake? Open an issue or send a pull request on GitHub.</p>
{pager}
</main>
</div>
</div>
""" + FOOT
    (ROOT / "docs" / f"{g['slug']}.html").write_text(page, encoding="utf-8")
    return url


def build_index():
    url = BASE + "docs/"
    title = "Tantu C Framework Documentation: Guides for Building and Themes"
    desc = "Documentation for Tantu C Framework, the free website builder written in C. Guides for building and changing the code, making themes, and the full release plan."
    schema = {"@context": "https://schema.org", "@graph": [
        {"@type": "CollectionPage", "@id": url, "url": url, "name": title, "description": desc, "inLanguage": "en", "isPartOf": SITE,
         "mainEntity": {"@type": "ItemList", "itemListElement": [
             {"@type": "ListItem", "position": i + 1, "url": f"{BASE}docs/{g['slug']}.html", "name": g["short"]} for i, g in enumerate(GUIDES)]}},
        crumbs([("Home", BASE), ("Docs", url)])]}
    cards = "".join(f'<li><h2><a href="{g["slug"]}.html">{g["short"]}</a></h2><p>{g["blurb"]}</p></li>' for g in GUIDES)
    page = head(title, desc, url, schema) + f"""<div class="wrap">
<p class="crumbs"><a href="../">Home</a> / Docs</p>
<div class="layout">
{side("index", [])}
<main id="main" class="prose">
<h1>Tantu documentation</h1>
<p class="lead">Everything you need to build tantu from source, change how it works, make your own theme, and see what is coming next.</p>
<p>New to tantu? Start on the <a href="../#start">home page</a> to make your first site in a few minutes, then come back here when you want to go further.</p>
<ul class="cards">{cards}</ul>
<p class="edit">These guides are made from the Markdown files in the <a href="{REPO}/tree/main/docs">docs folder on GitHub</a>, so they always match the code.</p>
</main>
</div>
</div>
""" + FOOT
    (ROOT / "docs" / "index.html").write_text(page, encoding="utf-8")
    return url


def update_sitemap(urls):
    path = ROOT / "sitemap.xml"
    text = path.read_text(encoding="utf-8")
    for u in urls:
        if f"<loc>{u}</loc>" not in text:
            text = text.replace("</urlset>", f"  <url>\n    <loc>{u}</loc>\n    <lastmod>{DATE}</lastmod>\n  </url>\n</urlset>")
    path.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    urls = [build_index()] + [build_guide(i, g) for i, g in enumerate(GUIDES)]
    update_sitemap(urls)
    print("Built", len(urls), "docs pages")
