# Tantu C Framework

**Build fast, secure websites for free. Written in C.**

Website: https://mmrahmanbappi.github.io/tantu-c-framework/

Tantu C Framework (tantu for short) turns simple Markdown files into a complete website with SEO, schema, Open Graph and security headers already done for you. The result is plain HTML, so you can host it for free on GitHub Pages or Cloudflare Pages, or upload it to any shared hosting plan.

tantu is made for students and anyone who wants a good website without paying for it. The name is an old Sanskrit word for thread or fiber: the framework is woven from small modules, one thread at a time.

> **Status: v1.0.0.** Dashboard, ten themes, site search, automatic social images, responsive images and FTP publishing, in one download for Windows, macOS and Linux. See [the full plan](docs/PLAN.md), or read all the guides on the [documentation site](https://mmrahmanbappi.github.io/tantu-c-framework/docs/).

## What works today

- **Dashboard in your browser:** write posts and pages with live preview and autosave, upload images, switch themes, edit settings and download the finished site as a ZIP
- **SEO score for every page** with plain advice, such as a missing description or image text
- **Ten themes:** blog, portfolio, resume, research, club, event, docs, course, business and gallery
- **Publish from the dashboard over FTP.** Only changed files are uploaded, and your password is never saved
- **Search inside your site,** with no server needed
- **A social image for every page,** drawn automatically in your theme colors
- **Responsive images:** large photos get 480, 960 and 1600 pixel versions, so phones download less
- **Google Analytics, Search Console and Bing** codes go in Settings, and tantu adds them to every page
- **Full SEO:** title, meta description, canonical, robots, sitemap.xml, robots.txt, Atom feed
- **Open Graph and Twitter/X cards** on every page
- **Schema (JSON-LD)** that fits each theme: BlogPosting, ScholarlyArticle, Event, Course, LearningResource, TechArticle, LocalBusiness, Service, VisualArtwork, ProfilePage, BreadcrumbList and more
- **Security:** content is escaped, unsafe links are removed, and `.htaccess` plus `_headers` files add security headers. The dashboard only listens on your own computer and needs a secret token
- **One file, nothing to install.** Themes are built into the program
- Works on a subdomain or in a sub-folder, for example `yourname.github.io/my-site`

## Quick start

### Download (Windows, macOS, Linux)

Download from the [releases page](https://github.com/mmrahmanbappi/tantu-c-framework/releases):

- **Windows:** `tantu.exe`. Put it in a folder and double click it.
- **macOS and Linux:** `tantu`. Open a terminal in that folder and run `chmod +x tantu && ./tantu`.

The first time, tantu creates a sample site in a folder called `mysite` and opens the dashboard in your browser. Keep the window open while you work.

On macOS, if you see a warning that the app cannot be checked, run `xattr -d com.apple.quarantine tantu` once. On Windows, if SmartScreen appears, choose More info, then Run anyway. The program is open source and not signed yet.

### Build from source

You need a C compiler and `make`.

```sh
git clone https://github.com/mmrahmanbappi/tantu-c-framework.git
cd tantu
make
./tantu new mysite --theme blog
./tantu dashboard mysite
```

## Your site folder

```
mysite/
  site.conf          site name, address, menu and SEO settings
  content/
    index.md         home page text
    pages/           pages such as about.md
    posts/           blog posts or projects
  static/            images and files copied as they are
  themes/            the theme your site uses
  public/            the finished website (created by tantu build)
```

## Writing a post

Create a file in `content/posts`, for example `2026-09-23-my-first-post.md`:

```
---
title: My first post
description: A short summary for search results and social previews.
tags: study, c
image: /images/cover.png
image_alt: What the cover image shows
---

Write your post here in **Markdown**.
```

The date comes from the file name. Add `draft: true` to keep a post unpublished.

## Publishing

| Where | How |
|---|---|
| Shared hosting | In the dashboard, open Publish and fill in your FTP details. Or download the ZIP and upload it with your hosting file manager |
| GitHub Pages | Upload the contents of `public` to a repository and turn on Pages |
| Cloudflare Pages | Create a Pages project and drag in the `public` folder |

Set `base_url` in `site.conf` (or Settings) to your real address before you publish.

Plain FTP does not encrypt your password. Use it on a network you trust, or use the ZIP and your host's file manager, which works over HTTPS.

## Useful settings

| Setting | What it does |
|---|---|
| `base_url` | Your site's address, used for links, the sitemap and social previews |
| `google_verification` | Google Search Console verification code |
| `bing_verification` | Bing Webmaster Tools verification code |
| `google_analytics` | Google Analytics 4 measurement ID, like `G-XXXXXXXXXX` |
| `search` | `false` turns off the search page |
| `auto_social_images` | `false` turns off the drawn social images |
| `ftp_host`, `ftp_port`, `ftp_user`, `ftp_dir` | FTP details for `tantu publish` (the password is asked each time) |

If you use Google Analytics and have visitors from the EU or UK, you may need a cookie consent notice.

## Commands

```
tantu                                   open the dashboard (creates mysite the first time)
tantu new <folder> [--theme NAME]       start a new site
tantu dashboard [folder] [--port 8080]  edit your site in the browser
tantu build [folder]                    build the site into public/
tantu serve [folder] [--port 8000]      preview the built site
tantu publish [folder] [--all]          upload changed files over FTP
tantu themes                            list the ten themes
```

## Project layout

| Folder | What is inside |
|---|---|
| `src/` | The C source: markdown, template, seo, serve, dashboard, zip and util modules |
| `ui/` | The dashboard page (HTML, CSS and JavaScript) |
| `tools/` | Small build helpers, such as the file embedder |
| `themes/` | The ten built-in themes |
| `starters/` | Sample content used by `tantu new` |
| `tests/` | Tests, run with `make test` under AddressSanitizer and UBSan |
| `docs/` | The guides as Markdown, plus the web pages made from them |

## Contributing

Help of every kind is welcome: code, themes, docs, testing and design ideas. Read [CONTRIBUTING.md](CONTRIBUTING.md) to get started, and see [SECURITY.md](SECURITY.md) to report a security problem.

## License

[MIT](LICENSE)
