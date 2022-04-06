cmake_minimum_required(VERSION 3.10)

project(ImageComparer)

# specify the C++ standard
set(CMAKE_C_STANDARD 17)

set(HEADERS  
	lib/stb/stb_image.h
	lib/stb/tinydir.h
)

set(SOURCES 
	src/main.c
)

set(TARGET ImageComparer)

add_executable(${TARGET} ${HEADERS} ${SOURCES} win32.manifest)

######## STB ########

include_directories(lib/stb)