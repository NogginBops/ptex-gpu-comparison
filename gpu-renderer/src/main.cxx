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

framebuffer_desc g_output_framebuffer_desc;
framebuffer_t g_output_framebuffer;

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
    recreate_framebuffer(&g_output_framebuffer, g_output_framebuffer_desc, width, height);
    glViewport(0, 0, width, height);
}

bool takeScreenshot = false;

void GLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ENTER && action == GLFW_PRESS)
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

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

#define R8UI 1
#define RG8UI 2
#define RGB8UI 3
#define RGBA8UI 4

#define R16UI 5
#define RG16UI 6
#define RGB16UI 7
#define RGBA16UI 8

#define R32F 9
#define RG32F 10
#define RGB32F 11
#define RGBA32F 12

int img_pixel_size_from_format(int format)
{
    int pixel_size = -1;
    switch (format) {
    case R8UI:     pixel_size = 1 * sizeof(int8_t);    break;
    case RG8UI:    pixel_size = 2 * sizeof(int8_t);    break;
    case RGB8UI:   pixel_size = 3 * sizeof(int8_t);    break;
    case RGBA8UI:  pixel_size = 4 * sizeof(int8_t);    break;
    case R16UI:    pixel_size = 1 * sizeof(int16_t);   break;
    case RG16UI:   pixel_size = 2 * sizeof(int16_t);   break;
    case RGB16UI:  pixel_size = 3 * sizeof(int16_t);   break;
    case RGBA16UI: pixel_size = 4 * sizeof(int16_t);   break;
    case R32F:     pixel_size = 1 * sizeof(float);     break;
    case RG32F:    pixel_size = 2 * sizeof(float);     break;
    case RGB32F:   pixel_size = 3 * sizeof(float);     break;
    case RGBA32F:  pixel_size = 4 * sizeof(float);     break;
    default: assert(false && "Unknown image format."); break;
    }
    return pixel_size;
}

void img_write(const char* filename, int width, int height, int format, const void* data)
{
    FILE* file;
    errno_t err = fopen_s(&file, filename, "wb");
    assert(err == 0 && "Could not open file!");
    
    fwrite("img", 3, 1, file);

    int32_t header[] = { width, height, format };
    fwrite(header, sizeof(header), 1, file);
    
    int pixel_size = img_pixel_size_from_format(format);

    int data_size = width * height * pixel_size;

    fwrite(data, data_size, 1, file);

    fclose(file);
}

void* img_read(const char* filename, int* width, int* height, int* format)
{
    FILE* file;
    errno_t err = fopen_s(&file, filename, "wb");
    assert(err == 0 && "Could not open file");

    char magic[3];
    fread(magic, 3, 1, file);
    
    fread(width, 4, 1, file);
    fread(height, 4, 1, file);
    fread(format, 4, 1, file);

    assert(width >= 0 && "Width cannot be negative");
    assert(height >= 0 && "Height cannot be negative");

    int pixel_size = img_pixel_size_from_format(*format);
    
    int data_size = *width * *height * pixel_size;

    void* data = malloc(data_size);
    assert(data !=  NULL);

    fread(data, data_size, 1, file);

    fclose(file);

    return data;
}

typedef struct {
    uint8_t r, g, b;
} rgb8_t;

uint8_t convert_float_uint(float f)
{
    //return (uint8_t)((f * 255.0f) - 0.0f);
    return (uint8_t)roundf(f * 255.0f);
}

rgb8_t vec3_to_rgb8(vec3_t vec)
{
    return { convert_float_uint(vec.x), convert_float_uint(vec.y), convert_float_uint(vec.z) };
}

void* calculate_image_cpu(int width, int height, uint16_t* faceID_buffer, vec2_t* uv_buffer, vec4_t* uv_deriv_buffer, vec3_t background_color)
{
    rgb8_t* cpu_data = (rgb8_t*)malloc(width * height * sizeof(rgb8_t));
    assert(cpu_data != NULL);

    rgb8_t bg = { (uint8_t)(background_color.x * 255), (uint8_t)(background_color.y * 255), (uint8_t)(background_color.z * 255) };

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++) {

            int i = y * width + x;

            int16_t id = faceID_buffer[i] - 1;
            
            if (id == -1)
            {
                cpu_data[i] = bg;
                continue;
            }


            vec2_t uv = uv_buffer[i];

            vec3_t color = { uv.x, uv.y, 1 - uv.x - uv.y };

            //const rgb8_t colors[] = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255} };

            cpu_data[i] = vec3_to_rgb8(color);
        }
    }

    return cpu_data;
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
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_FALSE);

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
    {
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

        color_attachment_desc UV_deriv_desc = {
            GL_RGBA32F,
            GL_RGBA,
            GL_FLOAT,
            GL_REPEAT, GL_REPEAT,
            GL_LINEAR, GL_LINEAR
        };

        color_attachment_desc* color_descriptions = new color_attachment_desc[3];
        color_descriptions[0] = faceID_desc;
        color_descriptions[1] = UV_desc;
        color_descriptions[2] = UV_deriv_desc;

        depth_attachment_desc depth_desc = {
            GL_DEPTH_COMPONENT32F,
            GL_REPEAT, GL_REPEAT,
            GL_LINEAR, GL_LINEAR
        };

        depth_attachment_desc* depth_descriptions = new depth_attachment_desc[1];
        depth_descriptions[0] = depth_desc;

        g_framebuffer_desc = {
            3,
            color_descriptions,
            depth_descriptions,
        };

        g_frambuffer = create_framebuffer(g_framebuffer_desc, width, height);
    }

    {
        color_attachment_desc color_desc = {
            GL_RGB32F,
            GL_RGB,
            GL_FLOAT,
            GL_REPEAT, GL_REPEAT,
            GL_LINEAR, GL_LINEAR
        };

        color_attachment_desc* color_descriptions = new color_attachment_desc[1];
        color_descriptions[0] = color_desc;

        depth_attachment_desc depth_desc = {
            GL_DEPTH_COMPONENT32F,
            GL_REPEAT, GL_REPEAT,
            GL_LINEAR, GL_LINEAR
        };

        depth_attachment_desc* depth_descriptions = new depth_attachment_desc[1];
        depth_descriptions[0] = depth_desc;

        g_output_framebuffer_desc = {
            1,
            color_descriptions,
            depth_descriptions
        };

        g_output_framebuffer = create_framebuffer(g_output_framebuffer_desc, width, height);
    }
    


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
        //"   const vec3 colors[3] = vec3[]( vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0) );\n"
        "   FragColor = vec4(fColor, 1.0);\n"
        "}\0";

    const char* outputFragShaderSource = "#version 330 core\n"
        "in vec3 fColor;\n"
        "layout (location = 0) out uint faceID;\n"
        "layout (location = 1) out vec2 uv;\n"
        "layout (location = 2) out vec4 uv_deriv;\n"

        "void main()\n"
        "{\n"
        "   faceID = uint(gl_PrimitiveID + 1);\n"
        "   uv = fColor.xy;\n"
        "   uv_deriv.xy = dFdx(fColor.xy);\n"
        "   uv_deriv.zw = dFdy(fColor.xy);\n"
        "}\0";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkShaderError(vertexShader);


    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkShaderError(fragmentShader);

    GLuint outputFragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(outputFragShader, 1, &outputFragShaderSource, NULL);
    glCompileShader(outputFragShader);
    checkShaderError(outputFragShader);

    GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    checkLinkError(program);

    GLuint outputProgram = glCreateProgram();
    glAttachShader(outputProgram, vertexShader);
    glAttachShader(outputProgram, outputFragShader);
    glLinkProgram(outputProgram);
    checkLinkError(outputProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteShader(outputFragShader);

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

    glUseProgram(program);

    int vp_location = glGetUniformLocation(program, "vp");
    glUniformMatrix4fv(vp_location, 1, GL_FALSE, (float*)&mvp);

    glUseProgram(outputProgram);

    vp_location = glGetUniformLocation(program, "vp");
    glUniformMatrix4fv(vp_location, 1, GL_FALSE, (float*)&mvp);

    glEnable(GL_DEPTH_TEST);

    vec3_t bg_color = { 1.0f, 0.5f, 0.8f };

    while (glfwWindowShouldClose(window) == false)
    {
        glfwPollEvents();

        // FIXME: Take the screenshot after rendering this frame?
        if (takeScreenshot)
        {
            // We need to render out:
            // FaceID, UV
            // UW1, VW2, UW2, VW2
            // 

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            
            glBindFramebuffer(GL_FRAMEBUFFER, g_frambuffer.framebuffer);

            glReadBuffer(GL_COLOR_ATTACHMENT0);
            void* faceID_buffer = malloc(width * height * sizeof(uint16_t));
            glReadPixels(0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_SHORT, faceID_buffer);

            glReadBuffer(GL_COLOR_ATTACHMENT1);
            void* uv_buffer = malloc(width * height * sizeof(vec2_t));
            glReadPixels(0, 0, width, height, GL_RG, GL_FLOAT, uv_buffer);

            glReadBuffer(GL_COLOR_ATTACHMENT2);
            void* uv_deriv_buffer = malloc(width * height * sizeof(vec4_t));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, uv_deriv_buffer);

            img_write("text_faceID.img", width, height, R16UI, faceID_buffer);
            img_write("test_uv.img", width, height, RG32F, uv_buffer);
            img_write("test_uv_deriv.img", width, height, RGBA32F, uv_deriv_buffer);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            void* reference_buffer = malloc(width * height * 3 * sizeof(uint8_t));
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, reference_buffer);

            stbi_flip_vertically_on_write(true);
            stbi_write_png("test_ref.png", width, height, 3, reference_buffer, width * 3 * sizeof(uint8_t));
            img_write("test_ref.img", width, height, RGB8UI, reference_buffer);

            void* cpu_buffer = calculate_image_cpu(width, height, (uint16_t*)faceID_buffer, (vec2_t*)uv_buffer, (vec4_t*)uv_deriv_buffer, bg_color);

            stbi_write_png("test_cpu.png", width, height, 3, cpu_buffer, width * 3);
            img_write("test_cpu.img", width, height, RGB8UI, cpu_buffer);

            free(faceID_buffer);
            free(uv_buffer);
            free(uv_deriv_buffer);
            free(reference_buffer);
            free(cpu_buffer);
            takeScreenshot = false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, g_frambuffer.framebuffer);

        GLenum drawBuffers[] = { 
            GL_COLOR_ATTACHMENT0, 
            GL_COLOR_ATTACHMENT1, 
            GL_COLOR_ATTACHMENT2,
        };
        glDrawBuffers(3, drawBuffers);

        uint32_t faceClearValue[] = { 0, 0, 0, 0 };
        float uvClearValue[] = { 0, 0, 0, 0 };
        float depthClearValue = 1.0f;
        glClearBufferuiv(GL_COLOR, 0, faceClearValue);
        glClearBufferfv(GL_COLOR, 1, uvClearValue);
        glClearBufferfv(GL_DEPTH, 0, &depthClearValue);

        glUseProgram(outputProgram);

        glDrawArrays(GL_TRIANGLES, 0, mesh->num_faces * 3);
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(program);

        glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLES, 0, mesh->num_faces * 3);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return EXIT_SUCCESS;
}