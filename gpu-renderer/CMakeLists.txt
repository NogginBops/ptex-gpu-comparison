cmake_minimum_required(VERSION 3.10)

project(GpuRenderer)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(GpuRenderer src/main.cxx)

######## GLFW ########

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/glfw)
target_link_libraries(GpuRenderer glfw)

######## GLAD ########

include_directories(lib/glad/include)

target_sources(GpuRenderer PRIVATE lib/glad/src/glad.c)