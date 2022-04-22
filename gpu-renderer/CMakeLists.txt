cmake_minimum_required(VERSION 3.10)

project(GpuRenderer)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(HEADERS  
	src/mesh.hh
	src/maths.hh
    src/mesh_loading.hh
    src/gl_utils.hh
    src/util.hh
)

set(SOURCES 
	src/main.cxx
	src/mesh.cxx
	src/maths.cxx
    src/mesh_loading.cxx
    src/gl_utils.cxx
    src/util.cxx
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

######## tinyobj ########

include_directories(lib/tinyobj)

######## PTEX ########

set(PTEX_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(lib/ptex)
target_link_libraries(${TARGET} Ptex)

######## Copy asset folder ########

set(ASSETS_SRC "${CMAKE_SOURCE_DIR}/assets")
set(ASSETS_DST "$<TARGET_FILE_DIR:${TARGET}>/assets")

add_custom_command(
    TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ASSETS_SRC}" "${ASSETS_DST}"
    COMMENT "Copying assets from ${ASSETS_SRC} to ${ASSETS_DST}"
)

add_custom_command(
    TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying assets from ${ASSETS_SRC} to ${ASSETS_DST}"
)