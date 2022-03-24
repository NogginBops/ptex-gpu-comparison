cmake_minimum_required(VERSION 3.10)

project(GpuRenderer)

add_executable(GpuRenderer src/main.cxx)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

