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
    src/ptex_utils.hh
    src/util.hh
    src/platform.hh
    src/array.hh
    src/cpu_renderer.hh
    src/img.hh
    src/methods/Methods.hh
    src/profiler.hh
)

set(SOURCES 
	src/main.cxx
	src/mesh.cxx
	src/maths.cxx
    src/mesh_loading.cxx
    src/gl_utils.cxx
    src/ptex_utils.cxx
    src/util.cxx
    src/platform.cxx
    src/cpu_renderer.cxx
    src/img.cxx
    src/stb.cxx
    src/methods/Methods.cxx
    src/methods/CpuMethod.cxx
    src/methods/NvidiaMethod.cxx
    src/methods/IntelMethod.cxx
    src/methods/ReducedTraverseMethod.cxx
    src/profiler.cxx
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

######## dear imgui ########

set(IMGUI_SOURCES
    lib/imgui/imgui.cpp
    lib/imgui/imgui_demo.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
)

set(IMGUI_HEADERS
    lib/imgui/imconfig.h
    lib/imgui/imgui.h
    lib/imgui/imgui_internal.h
    lib/imgui/imstb_rectpack.h
    lib/imgui/imstb_textedit.h
    lib/imgui/imstb_truetype.h
)

set(IMGUI_BACKEND
    lib/imgui/backends/imgui_impl_glfw.h
    lib/imgui/backends/imgui_impl_glfw.cpp

    lib/imgui/backends/imgui_impl_opengl3.h
    lib/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(${TARGET} PUBLIC lib/imgui lib/imgui/backends)

cmake_policy(SET CMP0076 NEW)
target_sources(${TARGET} PUBLIC ${IMGUI_HEADERS} PRIVATE ${IMGUI_SOURCES} ${IMGUI_BACKEND})

######## Copy asset folder ########

set(ASSETS_SRC "${CMAKE_SOURCE_DIR}/assets")
set(ASSETS_DST "$<TARGET_FILE_DIR:${TARGET}>/assets")

#${CMAKE_CURRENT_BINARY_DIR}

#add_custom_command(
#    #TARGET ${TARGET} POST_BUILD
#    OUTPUT ${MISSING_FILE}
#    COMMAND ${CMAKE_COMMAND} -E copy_directory
#    "${ASSETS_SRC}" "${ASSETS_DST}"
#    COMMENT "Copying assets from ${ASSETS_SRC} to ${ASSETS_DST}"
#)
#
#add_custom_command(
#    TARGET ${TARGET} POST_BUILD
#    #OUTPUT ${MISSING_FILE}
#    COMMAND ${CMAKE_COMMAND} -E echo "Copying assets from ${ASSETS_SRC} to ${ASSETS_DST}"
#)

#file(GLOB_RECURSE asset_files LIST_DIRECTORIES FALSE CONFIGURE_DEPENDS RELATIVE ${CMAKE_SOURCE_DIR} "assets/*")
#
#message(STATUS ${asset_files})
#
#add_custom_target(
#    copy_assets ALL POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_SRC}" "${ASSETS_DST}"
#    COMMENT "Copy assets!"
#)
#
#add_custom_target(
#    copy_assets_echo ALL POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E echo "Copy assets from ${ASSETS_SRC} to ${ASSETS_DST}"
#    COMMENT "Copy assets!"
#)
#add_dependencies(${TARGET} copy_assets)

#file(COPY ${ASSETS_SRC} DESTINATION .)