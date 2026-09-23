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
