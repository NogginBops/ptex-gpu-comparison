
#include "platform.hh"

#include <assert.h>
#include <stdlib.h>

#if WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>

bool change_directory(const char* path)
{
#if WIN32
	BOOL success = SetCurrentDirectory(path);
	return success != 0;
#else
	int success = chdir(path);
	return success == 0;
#endif
}

const char* get_current_directory()
{
#if WIN32
	int required_length = GetCurrentDirectory(0, NULL);
	char* str = (char*)malloc(required_length);
	int success = GetCurrentDirectory(required_length, str);
	assert(success != 0);
	return str;
#else
	assert(false);
#endif
}
