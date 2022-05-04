
#pragma once

#define R8UI 1
#define RG8UI 2
#define RGB8UI 3
#define RGBA8UI 4

#define R16UI 5
#define RG16UI 6
#define RGB16UI 7
#define RGBA16UI 8

#define R32F 9
#define RG32F 10
#define RGB32F 11
#define RGBA32F 12

void img_write(const char* filename, int width, int height, int format, const void* data);

void* img_read(const char* filename, int* width, int* height, int* format);
