cmake_minimum_required(VERSION 3.10)

project(GpuRenderer)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(HEADERS  
	src/mesh.hh
	src/maths.hh
)

set(SOURCES 
	src/main.cxx
	src/mesh.cxx
	src/maths.cxx
)

set(TARGET GpuRenderer)

add_executable(${TARGET} ${HEADERS} ${SOURCES})

######## GLFW ########

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/glfw)
target_link_libraries(${TARGET} glfw)

######## GLAD ########

include_directories(lib/glad/include)

target_sources(${TARGET} PRIVATE lib/glad/src/glad.c)

######## STB ########

include_directories(lib/stb)