# The tantu plan

tantu helps students build fast, secure websites for free. Every finished site is plain HTML, so it runs on any shared hosting plan or free subdomain such as GitHub Pages or Cloudflare Pages.

Items marked **[done]** are available now (v1.0.0). Everything else is planned.

## Releases

| Version | What it includes |
|---|---|
| **v0.1** | Command line tool, Markdown to HTML, blog and portfolio themes, full SEO, Open Graph, schema, sitemap, feed, security headers **[done]** |
| **v0.5** | Dashboard in the browser, editor with live preview, image uploads, SEO score per page, theme switcher, ZIP export, all ten themes, one download for Windows, macOS and Linux **[done]** |
| **v1.0** | FTP publishing of changed files, site search, automatic social images, responsive images, Google Analytics **[done]** |
| Next | FTPS and SFTP, WebP images, setup wizard, visual editor, backup and restore |

## 1. Install and setup

- [done] Build from source with one `make` command, no other libraries needed
- [done] `tantu new`, `tantu build`, `tantu serve`
- [done] Works offline
- [done] One download for Windows, macOS and Linux, no install needed
- [done] Smaller than 10 MB and runs on old laptops
- [done] Double click to open the dashboard
- [done] First run creates a sample site and opens the dashboard
- Full setup wizard with name, theme and language
- [done] Portable: runs from any folder, including a USB drive
- One click update that keeps your site safe

## 2. Dashboard

- [done] Overview: number of pages and posts, last publish, SEO issues
- [done] Editor with Markdown, live preview and autosave
- Visual editing mode
- [done] Pages and posts with drafts, tags and dates
- [done] Image uploads with type and size checks
- [done] Automatic resizing into 480, 960 and 1600 pixel versions
- Drag and drop
- Menu builder with drag and drop
- [done] Theme switcher with all ten themes
- Color and font options per theme
- [done] SEO panel with a score and a list of fixes for every page
- [done] Download the finished site as a ZIP file
- [done] Publishing over FTP, only changed files
- FTPS and SFTP, and GitHub Pages push
- Backup and restore in one file
- [done] Settings for site details, social links and verification codes
- [done] Dark mode and keyboard shortcuts

## 3. SEO

Per page:
- [done] Title and meta description
- [done] Clean URLs and custom slugs
- [done] Canonical URL
- [done] Robots meta (index or noindex)
- [done] Length counters and warnings in the dashboard
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
- [done] Event, Course, LearningResource, TechArticle, Service, VisualArtwork
- FAQPage, HowTo
- [done] LocalBusiness, ScholarlyArticle

Checks in the dashboard:
- [done] Missing image alt text
- Heading order
- Broken links
- Internal link suggestions
- [done] Duplicate titles or descriptions
- [done] Very short pages

## 4. Open Graph and social

- [done] og:title, og:description, og:image, og:url, og:type, og:locale
- [done] article:published_time, article:modified_time, article:tag
- [done] Twitter/X cards
- [done] Automatic 1200 by 630 social images for every page in the theme colors
- Preview of how a shared link will look
- Share buttons without tracking

## 5. Speed

- [done] No JavaScript in themes, system fonts, lazy loaded images
- [done] Compression and caching rules in `.htaccess`
- Minified HTML and CSS
- [done] Images in several sizes with srcset, width and height
- WebP and AVIF versions
- Page weight report
- Target: 90 or more on Google PageSpeed

## 6. Security

Dashboard:
- [done] Preview server on 127.0.0.1 only
- [done] A random security token for every dashboard session, which also blocks cross-site requests
- [done] Host header check against DNS rebinding
- [done] Upload checks for file type, content and size (SVG uploads refused)
- [done] File access limited to content, images and site.conf
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
- [done] Site search that runs in the browser
- Table of contents and code highlighting
- Contact forms through free services such as Formspree or Web3Forms
- Comments through Giscus (free, uses GitHub Discussions)
- [done] Google Analytics 4, with the security policy updated to allow it
- Privacy friendly analytics such as GoatCounter
- Sites in more than one language

## 8. Ten themes for students

| Theme | For | Highlights |
|---|---|---|
| Blog [done] | Writers and students | Tags, reading time, feed |
| Portfolio [done] | Designers and developers | Project grid, case studies |
| Resume [done] | Job seekers | One page CV that prints well |
| Research [done] | Thesis and research students | Publications, citations, ScholarlyArticle schema |
| Club [done] | University clubs | Members, activities, notices |
| Event [done] | Hackathons and meetups | Schedule, speakers, Event schema |
| Docs [done] | Open source projects | Sidebar, search, versions |
| Course Notes [done] | Teachers and tutors | Chapters, code blocks, Course schema |
| Small Business [done] | Shops and freelancers | Services, prices, map link, LocalBusiness schema |
| Gallery [done] | Photographers and artists | Image grid, lightbox |

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
| dashboard | Browser dashboard and its API | done |
| zip | ZIP export | done |
| embedded | Themes and starters built into the program | done |
| json | JSON reading and writing | planned |
| watch | Rebuild when files change | planned |
| image | Resizing and social images | done |
| minify | Smaller HTML and CSS | planned |
| search | Search index for the browser | done |
| crypto | Password hashing and checksums | planned |
| upload | Safe image uploads | done |
| publish | FTP and ZIP | done |
| i18n | Multiple languages | planned |

## 10. Docs and community

- A five minute first site guide with screenshots
- A demo site for every theme
- Video tutorials
- A free hosting guide
- A showcase of student sites
- A "Built with tantu" badge
- Discussions for questions and "good first issue" labels for new contributors
