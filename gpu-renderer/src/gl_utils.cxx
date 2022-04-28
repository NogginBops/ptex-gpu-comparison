
#include "gl_utils.hh"

#include <stdlib.h>
#include <stdio.h>
#include "util.hh"

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


GLuint create_color_attachment_texture(color_attachment_desc desc, int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_TEXTURE, texture, -1, desc.name);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, desc.internal_format, width, height, 0, desc.format, desc.type, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, desc.warp_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, desc.wrap_t);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, desc.mag_filter);

    return texture;
}

GLuint create_depth_attachment_texture(depth_attachment_desc desc, int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_TEXTURE, texture, -1, desc.name);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, desc.internal_format, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, desc.warp_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, desc.wrap_t);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, desc.mag_filter);

    return texture;
}

framebuffer_t create_framebuffer(framebuffer_desc desc, int width, int height)
{
    framebuffer_t framebuffer = { 0 };

    framebuffer.width = width;
    framebuffer.height = height;

    glGenFramebuffers(1, &framebuffer.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.framebuffer);

    if (has_KHR_debug)
    {
        glObjectLabel(GL_FRAMEBUFFER, framebuffer.framebuffer, -1, desc.name);
    }

    if (desc.depth_attachment)
    {
        framebuffer.depth_attachment = create_depth_attachment_texture(*desc.depth_attachment, width, height);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer.depth_attachment, 0);
    }

    framebuffer.color_attachments = new GLuint[desc.n_color_attachments];
    framebuffer.n_color_attachments = desc.n_color_attachments;
    for (int i = 0; i < desc.n_color_attachments; i++)
    {
        framebuffer.color_attachments[i] = create_color_attachment_texture(desc.color_attachments[i], width, height);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, framebuffer.color_attachments[i], 0);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Framebuffer not complete! Code: %d\n", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return framebuffer;
}

void recreate_framebuffer(framebuffer_t* framebuffer, framebuffer_desc desc, int width, int height)
{
    glDeleteTextures(framebuffer->n_color_attachments, framebuffer->color_attachments);
    glDeleteTextures(1, &framebuffer->depth_attachment);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->framebuffer);

    if (desc.depth_attachment)
    {
        framebuffer->depth_attachment = create_depth_attachment_texture(*desc.depth_attachment, width, height);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer->depth_attachment, 0);
    }

    if (desc.n_color_attachments != framebuffer->n_color_attachments)
    {
        delete[] framebuffer->color_attachments;
        framebuffer->color_attachments = new GLuint[desc.n_color_attachments];
    }

    framebuffer->n_color_attachments = desc.n_color_attachments;
    for (int i = 0; i < desc.n_color_attachments; i++)
    {
        framebuffer->color_attachments[i] = create_color_attachment_texture(desc.color_attachments[i], width, height);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, GL_TEXTURE_2D, framebuffer->color_attachments[i], 0);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        printf("Framebuffer not complete! Code: %d\n", status);
    }

    framebuffer->width = width;
    framebuffer->height = height;

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