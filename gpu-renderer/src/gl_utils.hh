#ifndef GL_UTILS_H
#define GL_UTILS_H

#include <glad/glad.h>
#include <stdint.h>

#include "maths.hh"

extern bool has_KHR_debug;

typedef struct {
    const char* name;
    GLint internal_format;
    GLenum format;
    GLenum type;
    GLenum warp_s, wrap_t;
    GLenum mag_filter, min_filter;
} color_attachment_desc;

typedef struct {
    const char* name;
    GLint internal_format;
    GLenum warp_s, wrap_t;
    GLenum mag_filter, min_filter;
} depth_attachment_desc;

typedef struct {
    const char* name;
    int n_color_attachments;
    color_attachment_desc* color_attachments;
    depth_attachment_desc* depth_attachment;
} framebuffer_desc;

typedef struct {
    GLuint framebuffer;
    int n_color_attachments;
    GLuint* color_attachments;
    GLuint depth_attachment;
} framebuffer_t;

//void check_shader_error(int shader);
//void check_link_error(int program);
GLuint compile_shader_source(const char* name, const char* vertex_name, const char* vertex_source, const char* fragment_name, const char* fragment_source);
GLuint compile_shader(const char* name, const char* vertex_filename, const char* fragment_filename);

GLuint create_color_attachment_texture(color_attachment_desc desc, int width, int height);
GLuint create_depth_attachment_texture(depth_attachment_desc desc, int width, int height);
framebuffer_t create_framebuffer(framebuffer_desc desc, int width, int height);
void recreate_framebuffer(framebuffer_t* framebuffer, framebuffer_desc desc, int width, int height);

typedef struct {
    const char* name;
    int size;
    GLenum type;
    GLboolean normalized;
    GLsizei offset;
    bool is_integer_attrib;
} attribute_desc;

typedef struct {
    const char* name;
    int num_attribs;
    attribute_desc* attribs;
} vao_desc;

GLuint create_vao(const vao_desc* desc, void* vertex_data, int vertex_size, int vertex_count);


void uniform_mat4(GLuint program, const char* name, mat4_t* mat);


#endif // GL_UTILS_H