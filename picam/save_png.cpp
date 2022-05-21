#include <stdio.h>

#include <libcamera/libcamera.h>
#include <png.h>

#include "checks.h"

void save_png(
    int width, int height, int stride, uint8_t const* image,
    std::string const& fn
) {
    struct Context {
        FILE* fp = nullptr;
        png_structp png = nullptr;
        png_infop info = nullptr;

        ~Context() {
            if (png || info) png_destroy_write_struct(&png, &info);
            if (fp) fclose(fp);
        }
    } cx;

    cx.fp = fopen(fn.c_str(), "wb");
    CHECK_RUNTIME(cx.fp, "Open failed: {}", fn);
    cx.png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    cx.info = png_create_info_struct(cx.png);

    CHECK_RUNTIME(!setjmp(png_jmpbuf(cx.png)), "PNG write failed: {}", fn);
    png_set_IHDR(
        cx.png, cx.info, width, height, 8,
        PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT
    );
    png_set_filter(cx.png, 0, PNG_FILTER_AVG);
    png_set_compression_level(cx.png, 1);
    png_init_io(cx.png, cx.fp);

    std::vector<png_bytep> rows;
    for (int y = 0; y < height; ++y)
        rows.push_back(const_cast<png_bytep>(image + y * stride));
    png_set_rows(cx.png, cx.info, rows.data());
    png_write_png(cx.png, cx.info, PNG_TRANSFORM_IDENTITY, 0);
}
