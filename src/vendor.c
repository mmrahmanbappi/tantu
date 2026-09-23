/* Implementation unit for the stb libraries in src/vendor.
 * Compiled with warnings off, since this is third-party code. */
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 16384
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"

#define STBIR_NO_SIMD
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "vendor/stb_image_resize2.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "vendor/stb_truetype.h"
