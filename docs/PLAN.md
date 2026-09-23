# The tantu plan

tantu helps students build fast, secure websites for free. Every finished site is plain HTML, so it runs on any shared hosting plan or free subdomain such as GitHub Pages or Cloudflare Pages.

Items marked **[done]** are in v0.1.0. Everything else is planned.

## Releases

| Version | What it includes |
|---|---|
| **v0.1** | Command line tool, Markdown to HTML, blog and portfolio themes, full SEO, Open Graph, schema, sitemap, feed, security headers **[done]** |
| v0.5 | Dashboard in the browser, editor, media library, one click publishing over FTP or SFTP, six themes, SEO panel |
| v1.0 | Ten themes, site search, automatic social images, install wizard, full documentation |

## 1. Install and setup

- [done] Build from source with one `make` command, no other libraries needed
- [done] `tantu new`, `tantu build`, `tantu serve`
- [done] Works offline
- One download for Windows, macOS and Linux, no install needed
- Smaller than 10 MB and runs on old laptops
- Double click to open the dashboard
- First run wizard: pick a name, theme and language, and see a site in three minutes
- Portable mode to run from a USB drive in computer labs
- One click update that keeps your site safe

## 2. Dashboard

- Overview: number of pages and posts, last publish, SEO issues
- Editor with Markdown and visual modes, live preview and autosave
- Pages and posts with drafts, categories, tags and dates
- Media library with drag and drop, automatic resizing and alt text
- Menu builder with drag and drop
- Theme switcher with color and font options
- SEO panel with a score and a list of fixes for every page
- Publishing: FTP or SFTP (only changed files), GitHub Pages push, ZIP export
- Backup and restore in one file
- Settings for site details, social links, analytics and verification codes
- Dark mode and keyboard shortcuts

## 3. SEO

Per page:
- [done] Title and meta description
- [done] Clean URLs and custom slugs
- [done] Canonical URL
- [done] Robots meta (index or noindex)
- Length counters and warnings in the dashboard
- Focus keyword check

Site wide:
- [done] sitemap.xml and robots.txt
- [done] Atom feed
- [done] 404 page with noindex
- [done] Search Console and Bing verification tags
- RSS 2.0 feed
- Redirects for `.htaccess` and `_redirects`
- hreflang for sites in more than one language
- IndexNow ping when the site changes
- llms.txt for AI search tools

Schema (JSON-LD):
- [done] WebSite, WebPage, Person, Organization
- [done] BlogPosting, CreativeWork, CollectionPage, ProfilePage, AboutPage, ContactPage
- [done] BreadcrumbList
- FAQPage, HowTo, Event, Course
- LocalBusiness, ScholarlyArticle

Checks in the dashboard:
- Missing image alt text
- Heading order
- Broken links
- Internal link suggestions
- Duplicate titles or descriptions
- Very short pages

## 4. Open Graph and social

- [done] og:title, og:description, og:image, og:url, og:type, og:locale
- [done] article:published_time, article:modified_time, article:tag
- [done] Twitter/X cards
- Automatic 1200 by 630 social images for every post in the theme colors
- Preview of how a shared link will look
- Share buttons without tracking

## 5. Speed

- [done] No JavaScript in themes, system fonts, lazy loaded images
- [done] Compression and caching rules in `.htaccess`
- Minified HTML and CSS
- Images converted to WebP and AVIF in several sizes
- Page weight report
- Target: 90 or more on Google PageSpeed

## 6. Security

Dashboard:
- [done] Preview server on 127.0.0.1 only
- Optional password with Argon2 hashing
- CSRF tokens and session timeout
- Upload checks for file type and size
- FTP passwords stored encrypted, SFTP preferred

Finished site:
- [done] Static files only, nothing to hack on the server
- [done] All content escaped, unsafe links removed
- [done] Content-Security-Policy and other security headers
- [done] No secrets are ever written to the output folder
- Subresource Integrity for any outside script

tantu itself:
- [done] Tests under AddressSanitizer and UndefinedBehaviorSanitizer
- [done] Security policy in SECURITY.md
- Fuzz testing for the Markdown and template parsers
- Signed releases with checksums

## 7. Features on the finished site

- [done] Responsive layouts and dark mode
- [done] Skip links, visible focus and good contrast
- [done] Print friendly styles
- Site search that runs in the browser
- Table of contents and code highlighting
- Contact forms through free services such as Formspree or Web3Forms
- Comments through Giscus (free, uses GitHub Discussions)
- Privacy friendly analytics such as GoatCounter
- Sites in more than one language

## 8. Ten themes for students

| Theme | For | Highlights |
|---|---|---|
| Blog [done] | Writers and students | Tags, reading time, feed |
| Portfolio [done] | Designers and developers | Project grid, case studies |
| Resume | Job seekers | One page CV that prints well |
| Research | Thesis and research students | Publications, citations, ScholarlyArticle schema |
| Club | University clubs | Members, activities, notices |
| Event | Hackathons and meetups | Schedule, speakers, Event schema |
| Docs | Open source projects | Sidebar, search, versions |
| Course Notes | Teachers and tutors | Chapters, code blocks, Course schema |
| Small Business | Shops and freelancers | Services, prices, map link, LocalBusiness schema |
| Gallery | Photographers and artists | Image grid, lightbox |

## 9. Framework modules

These C modules are built along the way and can be used on their own.

| Module | Job | Status |
|---|---|---|
| util | Strings, files, folders, maps | done |
| markdown | Safe Markdown to HTML | done |
| template | Themes with variables, conditions, loops and partials | done |
| seo | Meta tags, schema, sitemap, feed, headers | done |
| serve | Local preview server | done |
| config | TOML settings | planned |
| router | URL routing for the dashboard | planned |
| json | JSON reading and writing | planned |
| watch | Rebuild when files change | planned |
| image | Resizing, WebP and AVIF, social images | planned |
| minify | Smaller HTML and CSS | planned |
| search | Search index for the browser | planned |
| crypto | Password hashing and checksums | planned |
| upload | Safe file uploads | planned |
| publish | FTP, SFTP, ZIP and GitHub | planned |
| i18n | Multiple languages | planned |

## 10. Docs and community

- A five minute first site guide with screenshots
- A demo site for every theme
- Video tutorials
- A free hosting guide
- A showcase of student sites
- A "Built with tantu" badge
- Discussions for questions and "good first issue" labels for new contributors
