#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mesh.hh"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>


typedef struct {
    GLint internal_format;
    GLenum format;
    GLenum type;
    GLenum warp_s, wrap_t;
    GLenum mag_filter, min_filter;
} color_attachment_desc;

typedef struct {
    GLint internal_format;
    GLenum warp_s, wrap_t;
    GLenum mag_filter, min_filter;
} depth_attachment_desc;

typedef struct {
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


framebuffer_desc g_framebuffer_desc;
framebuffer_t g_frambuffer;

void recreate_framebuffer(framebuffer_t* framebuffer, framebuffer_desc desc, int width, int height);

void GLFWErrorCallback(int error_code, const char* description)
{
    printf("%d: %s\n", error_code, description);
}

void GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    printf("%s\n", message);
}

void GLFWFrambufferSizeCallback(GLFWwindow* window, int width, int height)
{
    recreate_framebuffer(&g_frambuffer, g_framebuffer_desc, width, height);
    glViewport(0, 0, width, height);
}

bool takeScreenshot = false;

void GLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ENTER)
    {
        takeScreenshot = true;
    }
}

void checkShaderError(int shader) 
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

void checkLinkError(int program)
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

GLuint create_color_attachment_texture(color_attachment_desc desc, int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

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
    glTexImage2D(GL_TEXTURE_2D, 0, desc.internal_format, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, desc.warp_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, desc.wrap_t);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, desc.min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, desc.mag_filter);

    return texture;
}

framebuffer_t create_framebuffer(framebuffer_desc desc, int width, int height) 
{
    framebuffer_t framebuffer = {0};

    glGenFramebuffers(1, &framebuffer.framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.framebuffer);

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

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, GL_TEXTURE_2D, framebuffer.color_attachments[i], 0);
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

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int main(int argv, char** argc)
{
    printf("Hello, world!\n");

    glfwSetErrorCallback(GLFWErrorCallback);

    if (glfwInit() == GLFW_FALSE)
    {
        printf("Failed to initialize GLFW\n");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(600, 600, "Test title", NULL, NULL);
    if (window == NULL)
    {
        printf("Failed to create GLFW window\n");
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);

    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == false)
    {
        printf("Failed to initialize GLAD\n");
        return EXIT_FAILURE;
    }

    glfwShowWindow(window);

    glfwSetFramebufferSizeCallback(window, GLFWFrambufferSizeCallback);
    glfwSetKeyCallback(window, GLFWKeyCallback);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);


    const char* version = (char*)glGetString(GL_VERSION);
    glfwSetWindowTitle(window, version);
    
    glDebugMessageCallback(GLDebugCallback, NULL);
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f, 0.5f, 0.0f,
    };

    color_attachment_desc faceID_desc = {
        GL_R16UI,
        GL_RED_INTEGER,
        GL_UNSIGNED_INT,
        GL_REPEAT, GL_REPEAT,
        GL_LINEAR, GL_LINEAR
    };

    color_attachment_desc UV_desc = {
        GL_RG32F,
        GL_RG,
        GL_FLOAT,
        GL_REPEAT, GL_REPEAT,
        GL_LINEAR, GL_LINEAR
    };

    color_attachment_desc* color_descriptions = new color_attachment_desc[2];
    color_descriptions[0] = faceID_desc;
    color_descriptions[1] = UV_desc;

    depth_attachment_desc depth_desc = {
        GL_DEPTH_COMPONENT32F,
        GL_REPEAT, GL_REPEAT,
        GL_LINEAR, GL_LINEAR
    };

    depth_attachment_desc* depth_descriptions = new depth_attachment_desc[1];
    depth_descriptions[0] = depth_desc;

    g_framebuffer_desc = {
        2,
        color_descriptions,
        depth_descriptions,
    };

    g_frambuffer = create_framebuffer(g_framebuffer_desc, width, height);

    mesh_t* mesh = load_obj("susanne.obj");

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_t) * mesh->num_faces * 3, mesh->vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void*)offsetof(vertex_t, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_TRUE, sizeof(vertex_t), (void*)offsetof(vertex_t, texcoord));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (void*)offsetof(vertex_t, normal));
    glEnableVertexAttribArray(2);

    const char* vertexShaderSource = "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        //"layout (location = 1) in vec2 aUV;\n"
        "out vec3 fColor;\n"
        "uniform mat4 vp;"
        "void main()\n"
        "{\n"
        "   const vec3 colors[3] = vec3[]( vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0) );\n"
        "   fColor = colors[gl_VertexID%3];//vec3(aUV, 0);\n"
        "   gl_Position = vp * vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
        "}\0";

    const char* fragmentShaderSource = "#version 330 core\n"
        "in vec3 fColor;\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "   FragColor = vec4(fColor, 1.0);\n"
        "}\0";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkShaderError(vertexShader);


    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkShaderError(fragmentShader);

    GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    checkLinkError(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(program);

    mat4_t view = 
    { {
        {1, 0, -0, -0},
        {0, 1, 4.37114e-08, -0},
        {0, -4.37114e-08, 1, -1.5f},
        {0, 0, 0, 1}
    } };

    mat4_t proj =
    { {
        {1.73205, 0, 0, 0},
        {0, 1.73205, 0, 0},
        {0, 0, -1.00002, -0.200002},
        {0, 0, -1, 0}
    } };
    
    view = mat4_transpose(view);
    proj = mat4_transpose(proj);

    mat4_t vp = mat4_mul_mat4(view, proj);

    mat4_t model = mat4_scale(0.5f, 0.5f, 0.5f);

    mat4_t mvp = mat4_mul_mat4(model, vp);

    int vp_location = glGetUniformLocation(program, "vp");
    glUniformMatrix4fv(vp_location, 1, GL_FALSE, (float*) &mvp);

    glEnable(GL_DEPTH_TEST);

    while (glfwWindowShouldClose(window) == false)
    {
        glfwPollEvents();

        if (takeScreenshot)
        {
            // We need to render out:
            // FaceID, UV
            // UW1, VW2, UW2, VW2
            // 


            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            void* data = malloc(width * height * 3);

            //glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
            
            stbi_flip_vertically_on_write(true);

            stbi_write_png("test.png", width, height, 3, data, width*3);

            free(data);
            takeScreenshot = false;
        }

        glClearColor(1.0f, 0.5f, 0.8f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        
        glDrawArrays(GL_TRIANGLES, 0, mesh->num_faces * 3);
        
        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return EXIT_SUCCESS;
}