#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

typedef struct {
    uint8_t r, g, b;
} rgb8_t;

typedef struct {
    uint8_t r, g, b, a;
} rgba8_t;

char* read_file(const char* filepath);

#endif // !UTIL_H