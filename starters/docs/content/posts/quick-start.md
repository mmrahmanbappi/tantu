---
title: Quick start
order: 2
description: Resize your first image in five lines of code.
---

```c
#include "lumen.h"

int main(void) {
    lm_image *img = lm_load("photo.jpg");
    lm_resize(img, 800, 0);
    lm_save(img, "photo-small.jpg");
    lm_free(img);
}
```

Setting the height to 0 keeps the original proportions.
