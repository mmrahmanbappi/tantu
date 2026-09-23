# Contributing to tantu

Thank you for your interest! tantu is at an early stage, so ideas and discussion are just as valuable as code.

## Ways to help

- **Discuss design:** open a thread in GitHub Discussions.
- **Report bugs or propose features:** open an Issue.
- **Write code:** pick an open Issue, comment that you are working on it, then open a pull request.
- **Improve docs:** typo fixes and clearer explanations are always welcome.

## Pull request guidelines

1. Fork the repo and create a branch from `main`.
2. Keep each pull request focused on one change.
3. Write C11. Avoid compiler-specific extensions unless they are guarded.
4. Add tests for new behavior.
5. Code must build without warnings using `-Wall -Wextra`. Run `make test`, which builds with AddressSanitizer and UndefinedBehaviorSanitizer and runs all tests.
6. Describe what your change does and why.

## Themes

New themes are very welcome. A theme is a folder with `home.html`, `page.html`, `post.html`, `list.html`, a `partials` folder and an `assets` folder. Copy `themes/blog` to start. Themes must work without JavaScript, pass basic accessibility checks and avoid loading files from other websites.

## Security

Please do not report security vulnerabilities in public issues. See [SECURITY.md](SECURITY.md).

## Be kind

Respect everyone, assume good intent, and help newcomers.
