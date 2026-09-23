---
title: How to put your site online for free
date: 2026-09-22
tags: guide, hosting
description: Three free ways to publish a tantu site: GitHub Pages, Cloudflare Pages and your own shared hosting.
---

When you run `tantu build`, your whole site is saved in the `public` folder. It is just HTML, CSS and images, so it works almost anywhere.

## GitHub Pages

Create a free GitHub account, make a repository, and upload the contents of `public`. Turn on Pages in the repository settings. Your site will be at `yourname.github.io`.

## Cloudflare Pages

Sign up for free, create a new Pages project, and drag the `public` folder into the upload box.

## Shared hosting

If you already have hosting, open your file manager or an FTP app and upload the contents of `public` into `public_html`. The included `.htaccess` file adds security headers and a proper 404 page.

Before you publish, set `base_url` in `site.conf` to your real address. This keeps links, the sitemap and social previews correct.
