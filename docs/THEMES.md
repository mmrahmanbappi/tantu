# Making a theme

A theme is a folder in `themes/`. Copy `themes/blog` to start.

```
themes/mytheme/
  theme.conf        id, name, description and social image colors
  home.html         the home page
  list.html         the list of posts
  post.html         one post
  page.html         one page (also used for search and 404)
  partials/         head.html, header.html, footer.html
  assets/style.css  copied to /assets/style.css
```

Add a matching sample site in `starters/mytheme/` (a `site.conf`, some `content/`, and `static/`).

## theme.conf

```
id = mytheme
name = My theme
description = One line that says who it is for
og_bg = #ffffff
og_fg = #1c1f24
og_accent = #1f2a5c
```

## Template language

| Syntax | Meaning |
|---|---|
| `{{ title }}` | A value, HTML escaped |
| `{{{ content }}}` | Raw HTML, only for `content` and `seo` |
| `{{#if image}} ... {{else}} ... {{/if}}` | Show something if a value is set |
| `{{#each posts}} ... {{/each}}` | Repeat for every post |
| `{{> header}}` | Insert `partials/header.html` |

## Variables

On every page: `site_title`, `site_description`, `site_author`, any `site_<key>` from site.conf, `base_path`, `year`, `list_url`, `search_url`, and the lists `menu`, `posts` and `recent`.

On posts and pages: `title`, `description`, `content`, `url`, `date`, `date_display`, `reading_time`, `tags`, `image`, `image_alt`, `prev_url`, `prev_title`, `next_url`, `next_title`, `position`, plus any front matter key.

In `{{#each menu}}`: `label`, `url`.

Always put `{{{seo}}}` in the head (the `head.html` partial does this) and prefix local links with `{{base_path}}`.

## Rules

- Work without JavaScript
- Responsive down to small phones, readable in dark mode
- Visible keyboard focus and good contrast
- No files from other websites (fonts, scripts, images), so the security policy stays strict
- Run `make test`, which builds every starter site and checks its schema
