
#include "img.hh"

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int img_pixel_size_from_format(int format)
{
    int pixel_size = -1;
    switch (format) {
    case R8UI:     pixel_size = 1 * sizeof(int8_t);    break;
    case RG8UI:    pixel_size = 2 * sizeof(int8_t);    break;
    case RGB8UI:   pixel_size = 3 * sizeof(int8_t);    break;
    case RGBA8UI:  pixel_size = 4 * sizeof(int8_t);    break;
    case R16UI:    pixel_size = 1 * sizeof(int16_t);   break;
    case RG16UI:   pixel_size = 2 * sizeof(int16_t);   break;
    case RGB16UI:  pixel_size = 3 * sizeof(int16_t);   break;
    case RGBA16UI: pixel_size = 4 * sizeof(int16_t);   break;
    case R32F:     pixel_size = 1 * sizeof(float);     break;
    case RG32F:    pixel_size = 2 * sizeof(float);     break;
    case RGB32F:   pixel_size = 3 * sizeof(float);     break;
    case RGBA32F:  pixel_size = 4 * sizeof(float);     break;
    default: assert(false && "Unknown image format."); break;
    }
    return pixel_size;
}

void img_write(const char* filename, int width, int height, int format, const void* data)
{
    FILE* file;
#if defined(_MSC_VER) && _MSC_VER >= 1400
    errno_t err = fopen_s(&file, filename, "wb");
    assert(err == 0 && "Could not open file!");
#else
    file = fopen(filename, "wb");
#endif
    fwrite("img", 3, 1, file);

    int32_t header[] = { width, height, format };
    fwrite(header, sizeof(header), 1, file);

    int pixel_size = img_pixel_size_from_format(format);

    int data_size = width * height * pixel_size;

    fwrite(data, data_size, 1, file);

    fclose(file);
}

void* img_read(const char* filename, int* width, int* height, int* format)
{
    FILE* file;
#if defined(_MSC_VER) && _MSC_VER >= 1400
    errno_t err = fopen_s(&file, filename, "wb");
    assert(err == 0 && "Could not open file");
#else
    file = fopen(filename, "wb");
#endif
    char magic[3];
    fread(magic, 3, 1, file);

    fread(width, 4, 1, file);
    fread(height, 4, 1, file);
    fread(format, 4, 1, file);

    assert(*width >= 0 && "Width cannot be negative");
    assert(*height >= 0 && "Height cannot be negative");

    int pixel_size = img_pixel_size_from_format(*format);

    int data_size = *width * *height * pixel_size;

    void* data = malloc(data_size);
    assert(data != NULL);

    fread(data, data_size, 1, file);

    fclose(file);

    return data;
}
