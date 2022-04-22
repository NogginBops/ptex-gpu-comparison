
#include "util.hh"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

char* read_file(const char* filepath) {
    FILE* file = fopen(filepath, "rb");

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* file_buffer = (char*)malloc(size + 1);
    assert(file_buffer != NULL);

    file_buffer[size] = 0;
    fread(file_buffer, size, 1, file);

    fclose(file);

    return file_buffer;
}
