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
    int samples;
} framebuffer_desc;

typedef struct {
    GLuint framebuffer;
    int n_color_attachments;
    GLuint* color_attachments;
    GLuint depth_attachment;
    int width, height;
    int samples;
} framebuffer_t;

//void check_shader_error(int shader);
//void check_link_error(int program);
GLuint compile_shader_source(const char* name, const char* vertex_name, const char* vertex_source, const char* fragment_name, const char* fragment_source);
GLuint compile_shader(const char* name, const char* vertex_filename, const char* fragment_filename);

GLuint create_color_attachment_texture(color_attachment_desc desc, int width, int height, int samples);
GLuint create_depth_attachment_texture(depth_attachment_desc desc, int width, int height, int samples);
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

typedef struct {
    GLenum wrap_s, wrap_t;
    GLenum mag_filter, min_filter;
    bool is_sRGB;
} texture_desc;

typedef struct {
    int width;
    int height;
    GLuint texture;
    GLenum wrap_s, wrap_t;
    GLenum mag_filter, min_filter;
    bool is_sRGB;

    uint8_t* data;
} texture_t;

typedef struct {
    int width;
    int height;
    int slices;
    GLuint texture;
    GLenum wrap_s, wrap_t;
    GLenum mag_filter, min_filter;
    bool is_sRGB;
} array_texture_t;

typedef struct {
    GLenum wrap_s, wrap_t;
    GLenum mag_filter, min_filter;
    int max_anisotropy;
    vec4_t border_color;
} sampler_desc;

typedef struct {
    GLuint sampler;
    sampler_desc desc;
} sampler_t;

texture_t create_texture(const char* filepath, texture_desc desc);

texture_t create_empty_texture(texture_desc desc, int width, int height, GLenum internal_format, const char* name);

void update_texture(texture_t* tex, GLenum internal_format, GLenum pixel_format, GLenum pixel_type, int width, int height, void* data);

sampler_t create_sampler(const char* name, sampler_desc desc);

void uniform_1i(GLuint program, const char* name, int value);
void uniform_mat4(GLuint program, const char* name, mat4_t* mat);

#define QUERY_BUFFER_SIZE 4

typedef struct {
    const char* name;
    GLuint start_queries[QUERY_BUFFER_SIZE];
    GLuint end_queries[QUERY_BUFFER_SIZE];
    int current_query;
    int read_query;
} buffered_query_t;

buffered_query_t create_query(const char* name);

void begin_query(buffered_query_t* query);

void end_query(buffered_query_t* query);

bool is_query_ready(buffered_query_t* query);

long get_time_elapsed_query_result(buffered_query_t* query, bool wait_if_not_available);

#endif // GL_UTILS_H