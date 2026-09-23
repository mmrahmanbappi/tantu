# Security policy

## Reporting a problem

Please do not open a public issue for security problems. Use GitHub's private vulnerability reporting on this repository (Security tab, then "Report a vulnerability"), or contact the maintainer through GitHub.

Include what you found, how to reproduce it, and which version you used. You will get a reply as soon as possible.

## How tantu protects your site

- All text from Markdown and front matter is HTML escaped. Raw HTML in Markdown is shown as text, not run.
- Links and images using `javascript:`, `vbscript:` or `data:` URLs are removed.
- JSON-LD is escaped so content cannot close the script tag.
- The finished site is static files only, with no database or server code to attack.
- `.htaccess` and `_headers` files set Content-Security-Policy, X-Content-Type-Options, X-Frame-Options, Referrer-Policy and Permissions-Policy.
- The preview server listens on 127.0.0.1 only, accepts GET and HEAD, and refuses paths with `..`.
- `tantu build` only deletes an output folder that it created itself.
- Every change is tested with AddressSanitizer and UndefinedBehaviorSanitizer.

## The dashboard

- It listens on 127.0.0.1 only, so other computers on your network cannot reach it.
- Every session gets a new random token, printed in the terminal. The API refuses requests without it, which also stops other websites from sending requests to it.
- Requests must use the Host 127.0.0.1 or localhost, which blocks DNS rebinding.
- It can only read and write Markdown files in `content/posts`, `content/pages` and `content/index.md`, images in `static/images`, and `site.conf`. Paths with `..` are refused.
- Uploads must be real PNG, JPEG, GIF or WebP files up to 10 MB. SVG uploads are refused because SVG files can contain scripts.
- Deleted files are moved to `content/.trash`, not erased.
- Builds run in a separate process, so a broken file cannot crash the dashboard.
