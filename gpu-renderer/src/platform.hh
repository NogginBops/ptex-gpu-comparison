#ifndef PLATFORM_H
#define PLATFORM_H

bool change_directory(const char* path);

const char* get_current_directory();

bool create_directory(const char* dirname);

#endif // !PLATFORM_H
