
#include "gl_utils.hh"

#include <stdlib.h>
#include <stdio.h>
#include "util.hh"
#include <stb_image.h>

bool has_KHR_debug = false;

void check_shader_error(int shader)
{
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == 0)
    {
        int log_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        char* log = (char*)malloc(log_length * sizeof(char));
        glGetShaderInfoLog(shader, log_length, NULL, log);

        fprintf(stderr, "Error: %s\n", log);

        free(log);
    }
}

void check_link_error(int program)
{
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success == 0)
    {
        int log_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        char* log = (char*)malloc(log_length * sizeof(char));
        glGetProgramInfoLog(program, log_length, NULL, log);

        fprintf(stderr, "Link error: %s\n", log);

        free(log);
    }
}

GLuint compile_shader_source(const char* name, const char* vertex_name, const char* vertex_source, const char* fragment_name, const char* fragment_source)
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertex_source, NULL);
    glCompileShader(vertexShader);
    check_shader_error(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragment_source, NULL);
    glCompileShader(fragmentShader);
    check_shader_error(fragmentShader);

    GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    check_link_error(program);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_SHADER, vertexShader, -1, vertex_name);
        glObjectLabel(GL_SHADER, fragmentShader, -1, fragment_name);

        glObjectLabel(GL_PROGRAM, program, -1, name);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

GLuint compile_shader(const char* name, const char* vertex_filename, const char* fragment_filename)
{
    const char* vertex_source = read_file(vertex_filename);
    const char* fragment_source = read_file(fragment_filename);

    return compile_shader_source(name, vertex_filename, vertex_source, fragment_filename, fragment_source);
}


GLuint create_color_attachment_texture(color_attachment_desc desc, int width, int height, int samples) {
    GLuint texture;
    glGenTextures(1, &texture);
    
    bool multisample = samples > 1;
    GLenum target = multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(target, texture);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_TEXTURE, texture, -1, desc.name);
    }

    if (multisample)
    {
        glTexImage2DMultisample(target, samples, desc.internal_format, width, height, true);
    }
    else
    {
        glTexImage2D(target, 0, desc.internal_format, width, height, 0, desc.format, desc.type, NULL);

        glTexParameteri(target, GL_TEXTURE_WRAP_S, desc.warp_s);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, desc.wrap_t);

        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, desc.min_filter);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, desc.mag_filter);

    }

    return texture;
}

GLuint create_depth_attachment_texture(depth_attachment_desc desc, int width, int height, int samples) {
    GLuint texture;
    glGenTextures(1, &texture);

    bool multisample = samples > 1;
    GLenum target = multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(target, texture);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_TEXTURE, texture, -1, desc.name);
    }

    if (multisample)
    {
        glTexImage2DMultisample(target, samples, desc.internal_format, width, height, true);
    }
    else
    {
        glTexImage2D(target, 0, desc.internal_format, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

        glTexParameteri(target, GL_TEXTURE_WRAP_S, desc.warp_s);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, desc.wrap_t);

        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, desc.min_filter);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, desc.mag_filter);
    }
    
    return texture;
}

framebuffer_t create_framebuffer(framebuffer_desc desc, int width, int height)
{
    framebuffer_t framebuffer = { 0 };

    framebuffer.width = width;
    framebuffer.height = height;

    framebuffer.samples = desc.samples;
    bool multisample = desc.samples > 1;
    GLenum texture_target = multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    glGenFramebuffers(1, &framebuffer.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.framebuffer);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_FRAMEBUFFER, framebuffer.framebuffer, -1, desc.name);
    }

    if (desc.depth_attachment)
    {
        framebuffer.depth_attachment = create_depth_attachment_texture(*desc.depth_attachment, width, height, desc.samples);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target, framebuffer.depth_attachment, 0);
    }

    framebuffer.color_attachments = new GLuint[desc.n_color_attachments];
    framebuffer.n_color_attachments = desc.n_color_attachments;
    for (int i = 0; i < desc.n_color_attachments; i++)
    {
        framebuffer.color_attachments[i] = create_color_attachment_texture(desc.color_attachments[i], width, height, desc.samples);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture_target, framebuffer.color_attachments[i], 0);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        switch (status)
        {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            printf("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            printf("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            printf("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            printf("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            printf("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE\n");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            printf("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER\n");
            break;
        default:
            break;
        }

        printf("Framebuffer not complete! Code: %d\n", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return framebuffer;
}

void recreate_framebuffer(framebuffer_t* framebuffer, framebuffer_desc desc, int width, int height)
{
    //glDeleteFramebuffers(1, &framebuffer->framebuffer);
    glDeleteTextures(framebuffer->n_color_attachments, framebuffer->color_attachments);
    glDeleteTextures(1, &framebuffer->depth_attachment);

    //glGenFramebuffers(1, &framebuffer->framebuffer);

    //if (has_KHR_debug)
    //{
    //    glObjectLabel(GL_FRAMEBUFFER, framebuffer->framebuffer, -1, desc.name);
    //}

    bool multisample = desc.samples > 1;
    GLenum texture_target = multisample ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);

    if (desc.depth_attachment)
    {
        framebuffer->depth_attachment = create_depth_attachment_texture(*desc.depth_attachment, width, height, desc.samples);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target, framebuffer->depth_attachment, 0);
    }

    if (desc.n_color_attachments != framebuffer->n_color_attachments)
    {
        delete[] framebuffer->color_attachments;
        framebuffer->color_attachments = new GLuint[desc.n_color_attachments];
    }

    framebuffer->n_color_attachments = desc.n_color_attachments;
    for (int i = 0; i < desc.n_color_attachments; i++)
    {
        framebuffer->color_attachments[i] = create_color_attachment_texture(desc.color_attachments[i], width, height, desc.samples);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, texture_target, framebuffer->color_attachments[i], 0);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Framebuffer not complete! Code: %d\n", status);
    }

    framebuffer->width = width;
    framebuffer->height = height;

    framebuffer->samples = desc.samples;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


GLuint create_vao(const vao_desc* desc, void* vertex_data, int vertex_size, int vertex_count)
{
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_VERTEX_ARRAY, vao, -1, desc->name);
    }

    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, vertex_size * vertex_count, vertex_data, GL_STATIC_DRAW);

    for (int i = 0; i < desc->num_attribs; i++)
    {
        attribute_desc attrib = desc->attribs[i];
        if (attrib.is_integer_attrib)
        {
            glVertexAttribIPointer(i, attrib.size, attrib.type, vertex_size, (void*)attrib.offset);
        }
        else
        {
            glVertexAttribPointer(i, attrib.size, attrib.type, attrib.normalized, vertex_size, (void*)attrib.offset);
        }
        glEnableVertexAttribArray(i);
    }

    glBindVertexArray(0);

    return vao;
}


texture_t create_texture(const char* filepath, texture_desc desc) {
    int channels;
    texture_t tex;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc* img = stbi_load(filepath, &tex.width, &tex.height, &channels, 4);

    glGenTextures(1, &tex.texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex.texture);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_TEXTURE, tex.texture, -1, filepath);
    }

    GLenum internal_format = GL_RGBA8;
    if (desc.is_sRGB) internal_format = GL_SRGB8_ALPHA8;

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, tex.width, tex.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);

    tex.data = img;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, desc.wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, desc.wrap_t);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, desc.mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.min_filter);

    tex.wrap_s = desc.wrap_s;
    tex.wrap_t = desc.wrap_t;

    tex.mag_filter = desc.mag_filter;
    tex.min_filter = desc.min_filter;

    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

texture_t create_empty_texture(texture_desc desc, int width, int height, GLenum internal_format, const char* name)
{
    int channels;
    texture_t tex;

    glGenTextures(1, &tex.texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex.texture);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_TEXTURE, tex.texture, -1, name);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    tex.data = NULL;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, desc.wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, desc.wrap_t);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, desc.mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.min_filter);

    tex.wrap_s = desc.wrap_s;
    tex.wrap_t = desc.wrap_t;

    tex.mag_filter = desc.mag_filter;
    tex.min_filter = desc.min_filter;

    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

void update_texture(texture_t* tex, GLenum internal_format, GLenum pixel_format, GLenum pixel_type, int width, int height, void* data)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex->texture);

    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, pixel_format, pixel_type, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tex->wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tex->wrap_t);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tex->mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tex->min_filter);

    glBindTexture(GL_TEXTURE_2D, 0);
}

sampler_t create_sampler(const char* name, sampler_desc desc) {
    GLuint sampler;
    glGenSamplers(1, &sampler);

    glBindSampler(0, sampler);
    glBindSampler(0, 0);

    if (has_KHR_debug)
    {
        char label[256];
        sprintf(label, "Sampler: %s", name);
        glObjectLabel(GL_SAMPLER, sampler, -1, label);
    }

    glSamplerParameterf(sampler, GL_TEXTURE_WRAP_S, desc.wrap_s);
    glSamplerParameterf(sampler, GL_TEXTURE_WRAP_T, desc.wrap_t);

    glSamplerParameterf(sampler, GL_TEXTURE_MAG_FILTER, desc.mag_filter);
    glSamplerParameterf(sampler, GL_TEXTURE_MIN_FILTER, desc.min_filter);

    glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, desc.max_anisotropy);

    glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, (float*)&desc.border_color);

    sampler_t result;
    result.sampler = sampler;
    result.desc = desc;

    return result;
}


void uniform_1i(GLuint program, const char* name, int value)
{
    glUseProgram(program);
    GLint location = glGetUniformLocation(program, name);
    glUniform1i(location, value);
}

void uniform_mat4(GLuint program, const char* name, mat4_t* mat)
{
    glUseProgram(program);
    GLint location = glGetUniformLocation(program, name);
    glUniformMatrix4fv(location, 1, GL_FALSE, (GLfloat*)mat);
}