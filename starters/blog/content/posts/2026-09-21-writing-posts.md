---
title: How to write a post in Markdown
date: 2026-09-21
tags: guide, markdown
---

Every post is a text file in the `content/posts` folder. The part between the two `---` lines at the top is called front matter. It holds the title, date and tags.

## Formatting basics

Write **bold** with two stars and *italic* with one. Make a link like this: [tantu on GitHub](https://github.com/mmrahmanbappi/tantu-c-framework).

> A quote starts with a greater than sign.

Code goes between three backticks:

```c
#include <stdio.h>

int main(void) {
    printf("Hello, world\n");
    return 0;
}
```

## Drafts

Add `draft: true` to the front matter and the post will not be published until you remove it.
