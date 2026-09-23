# tantu

**Build fast, secure websites for free. Written in C.**

Website: https://mmrahmanbappi.github.io/tantu/

tantu turns simple Markdown files into a complete website with SEO, schema, Open Graph and security headers already done for you. The result is plain HTML, so you can host it for free on GitHub Pages or Cloudflare Pages, or upload it to any shared hosting plan.

tantu is made for students and anyone who wants a good website without paying for it. The name is an old Sanskrit word for thread or fiber: the framework is woven from small modules, one thread at a time.

> **Status: v0.1.0, early release.** The command line tool works today with two themes. The dashboard and more themes are planned. See [the full plan](docs/PLAN.md).

## What works today

- Markdown to HTML for pages and posts, with drafts and front matter
- Two themes: **blog** and **portfolio**, both responsive, accessible and with dark mode
- Full SEO: title, meta description, canonical, robots, sitemap.xml, robots.txt, Atom feed
- Open Graph and Twitter/X cards on every page
- JSON-LD schema: WebSite, Person or Organization, WebPage, BlogPosting, CollectionPage, ProfilePage, BreadcrumbList and more
- Security: all content is HTML escaped, unsafe links are removed, and `.htaccess` plus `_headers` files add security headers on shared hosting, Cloudflare Pages and Netlify
- Works in a sub-folder, for example `yourname.github.io/my-site`
- A local preview server that only listens on your own computer
- Builds a small site in a few milliseconds

## Quick start

### Option 1: download (Linux)

Download `tantu-0.1.0-linux-x86_64.tar.gz` from the [releases page](https://github.com/mmrahmanbappi/tantu/releases), then:

```sh
tar xzf tantu-0.1.0-linux-x86_64.tar.gz
cd tantu-0.1.0-linux-x86_64
./tantu new mysite
cd mysite
../tantu serve
```

### Option 2: build from source (Linux, macOS, Windows with WSL)

You need a C compiler and `make`. On Linux and macOS they are usually installed already. On Windows, use WSL for now.

```sh
git clone https://github.com/mmrahmanbappi/tantu.git
cd tantu
make

./tantu new mysite --theme blog
cd mysite
../tantu serve
```

Open http://127.0.0.1:8000 in your browser. Edit `site.conf` and the files in `content/`, then refresh.

When you are happy, run `tantu build` and upload the `public` folder to your host.

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

## Publishing for free

| Where | How |
|---|---|
| GitHub Pages | Upload the contents of `public` to a repository and turn on Pages |
| Cloudflare Pages | Create a Pages project and drag in the `public` folder |
| Shared hosting | Upload the contents of `public` into `public_html` with FTP or the file manager |

Set `base_url` in `site.conf` to your real address before you build.

## Commands

```
tantu new <folder> [--theme blog|portfolio]
tantu build [folder]
tantu serve [folder] [--port 8000]
tantu version
```

## Project layout

| Folder | What is inside |
|---|---|
| `src/` | The C source: markdown, template, seo, serve and util modules |
| `themes/` | Built-in themes |
| `starters/` | Sample content used by `tantu new` |
| `tests/` | Tests, run with `make test` under AddressSanitizer and UBSan |
| `docs/` | The full plan and other documents |

## Contributing

Help of every kind is welcome: code, themes, docs, testing and design ideas. Read [CONTRIBUTING.md](CONTRIBUTING.md) to get started, and see [SECURITY.md](SECURITY.md) to report a security problem.

## License

[MIT](LICENSE)
