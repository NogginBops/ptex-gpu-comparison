#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mesh.hh"
#include "mesh_loading.hh"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <Ptexture.h>
#include <iostream>

#include "util.hh"
#include "gl_utils.hh"

#include "platform.hh"

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

framebuffer_desc g_framebuffer_desc;
framebuffer_t g_frambuffer;

framebuffer_desc g_output_framebuffer_desc;
framebuffer_t g_output_framebuffer;

texture_t g_tex_test;

Ptex::PtexTexture* g_ptex_texture;
Ptex::PtexFilter* g_ptex_filter;

ptex_mesh_t* g_teapot_mesh;

bool stream_cpu_result = false;
texture_t g_cpu_stream_tex;

typedef struct {
    vec3_t center;
    float distance, distance_t;
    float x_axis_rot, y_axis_rot;

    quat_t quaternion;
} camera_t;

camera_t g_camera;

const float MOUSE_SPEED_X = -0.01f;
const float MOUSE_SPEED_Y = -0.01f;

const float ZOOM_SPEED = 0.01f;

const float CAMERA_MIN_Y = -85.0f;
const float CAMERA_MAX_Y = +85.0f;

const float CAMERA_MIN_DIST = 0.2f;
const float CAMERA_MAX_DIST = 100.0f;


mat4_t calc_view_matrix(camera_t camera) 
{
    mat4_t center_offset = mat4_translate(camera.center.x, camera.center.y, camera.center.z);
    mat4_t offset = mat4_translate(0, 0, camera.distance);
    // FIXME!
    mat4_t transform = mat4_identity();
    transform = mat4_mul_mat4(transform, center_offset);
    transform = mat4_mul_mat4(transform, mat4_from_quat(camera.quaternion));
    transform = mat4_mul_mat4(transform, offset);
    //transform = mat4_mul_mat4(transform, mat4_from_quat(camera.quaternion));
    //transform = mat4_mul_mat4(center_offset, transform);
    return mat4_inverse(transform);
}

void GLFWErrorCallback(int error_code, const char* description)
{
    printf("%d: %s\n", error_code, description);
}

void GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    // Ignore 'Pixel-path performance warning: Pixel transfer is synchronized with 3D rendering.'
    // as we are going to get a lot of those.
    if (id == 131154)
        return;

    printf("%u: %s\n", id, message);
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

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }

    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        stream_cpu_result = !stream_cpu_result;
    }
}

bool control_camera = false;
void GLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        control_camera = true;
    }
    else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        control_camera = false;
    }
}

void GLFWMouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    static double prev_xpos = 0, prev_ypos = 0;

    double xdiff = xpos - prev_xpos;
    double ydiff = ypos - prev_ypos;

    if (control_camera)
    {
        g_camera.x_axis_rot += xdiff * MOUSE_SPEED_X;
        g_camera.y_axis_rot += ydiff * MOUSE_SPEED_Y;

        g_camera.y_axis_rot = float_clamp(g_camera.y_axis_rot, TO_RADIANS(CAMERA_MIN_Y), TO_RADIANS(CAMERA_MAX_Y));
    }

    const vec3_t x_axis = { 1, 0, 0 };
    const vec3_t y_axis = { 0, 1, 0 };
    const vec3_t z_axis = { 0, 0, 1 };

    quat_t yrot = quat_from_axis_angle(y_axis, g_camera.x_axis_rot);
    quat_t xrot = quat_from_axis_angle(x_axis, g_camera.y_axis_rot);

    g_camera.quaternion = quat_mul_quat(yrot, xrot);

    prev_xpos = xpos;
    prev_ypos = ypos;
}

void GLFWScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    g_camera.distance_t += -yoffset * ZOOM_SPEED;
    g_camera.distance_t = float_clamp(g_camera.distance_t, 0, 1);

    g_camera.distance = float_eerp(CAMERA_MIN_DIST, CAMERA_MAX_DIST, g_camera.distance_t);
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
#if defined(_MSC_VER) && _MSC_VER >= 1400
    errno_t err = fopen_s(&file, filename, "wb");
    assert(err == 0 && "Could not open file!");
#else
    file = fopen(filename, "wb");
#endif
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
#if defined(_MSC_VER) && _MSC_VER >= 1400
    errno_t err = fopen_s(&file, filename, "wb");
    assert(err == 0 && "Could not open file");
#else
    file = fopen(filename, "wb");
#endif
    char magic[3];
    fread(magic, 3, 1, file);
    
    fread(width, 4, 1, file);
    fread(height, 4, 1, file);
    fread(format, 4, 1, file);

    assert(*width >= 0 && "Width cannot be negative");
    assert(*height >= 0 && "Height cannot be negative");

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

typedef struct {
    uint8_t r, g, b, a;
} rgba8_t;

uint8_t convert_float_uint(float f)
{
    //return (uint8_t)((f * 255.0f) + 0.5f);
    //if (f == 0.5) return 127;
    return (uint8_t)roundf(f * 255.0f);
}

rgb8_t vec3_to_rgb8(vec3_t vec)
{
    return { convert_float_uint(vec.x), convert_float_uint(vec.y), convert_float_uint(vec.z) };
}

rgb8_t* vec3_buffer_to_rgb8(vec3_t* buffer, int width, int height)
{
    rgb8_t* rgb_buffer = (rgb8_t*) malloc(width * height * sizeof(rgb8_t));
    assert(rgb_buffer != NULL);

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int index = y * width + x;

            rgb_buffer[index] = vec3_to_rgb8(buffer[index]);
        }
    }

    return rgb_buffer;
}

void check_u8(const char* label, uint8_t ref, uint8_t res, float original)
{
    if (ref != res)
    {
        bool round = ((uint8_t)roundf(original * 255.0f)) == ref;
        bool floor = ((uint8_t)floorf(original * 255.0f)) == ref;
        bool ceil = ((uint8_t)ceilf(original * 255.0f)) == ref;
        bool cast = ((uint8_t)(original * 255.0f)) == ref;
        bool castAddHalf = ((uint8_t)((original * 255.0f) + 0.5f)) == ref;
        
        printf("%s: Ref: %3u, Res: %3u, Orig: %g, Round: %u, Floor: %u, Ceil: %u, Cast: %u, Cast+0.5: %u\n", label, ref, res, original * 255.0f, round, floor, ceil, cast, castAddHalf);
    }
}

void vec3_buffer_to_rgb8_check(vec3_t* buffer, rgb8_t* reference, int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int index = y * width + x;

            rgb8_t rgb8 = vec3_to_rgb8(buffer[index]);

            rgb8_t ref = reference[index];

            check_u8("red  ", ref.r, rgb8.r, buffer[index].x);
            check_u8("green", ref.g, rgb8.g, buffer[index].y);
            check_u8("blue ", ref.b, rgb8.b, buffer[index].z);
        }
    }
}

float sRGB_to_linear(float c)
{
    if (c <= 0.04045)
    {
        return c / 12.92f;
    }
    else 
    {
        return powf((c + 0.055f) / 1.055f, 2.4f);
    }
}

float tex_coord_wrap(int coord, int size, GLenum mode) 
{
    switch (mode)
    {
    case GL_CLAMP_TO_EDGE:
        if (coord < 0) return 0;
        else if (coord >= size) return size - 1;
        else return coord;
    case GL_REPEAT:
        return ((coord % size) + size) % size;
    default:
        assert(false);
    }
}

vec3_t sample_texel(const texture_t* tex, int x, int y) 
{
    int index = (y * tex->width + x) * 4;

    vec3_t color;
    color.x = (tex->data[index + 0] / 255.0f);
    color.y = (tex->data[index + 1] / 255.0f);
    color.z = (tex->data[index + 2] / 255.0f);

    if (tex->is_sRGB && false)
    {
        color.x = sRGB_to_linear(color.x);
        color.y = sRGB_to_linear(color.y);
        color.z = sRGB_to_linear(color.z);
    }

    return color;
}

vec3_t sample_texture(const texture_t* tex, vec2_t uv, vec4_t uv_derivatives)
{
    float u = uv.x * tex->width;
    float v = uv.y * tex->height;

    vec3_t color = {0};
    if (tex->mag_filter == GL_NEAREST && tex->min_filter == GL_NEAREST)
    {
        int x = (int)tex_coord_wrap(u, tex->width, tex->wrap_s);
        int y = (int)tex_coord_wrap(v, tex->height, tex->wrap_t);

        assert(x >= 0 && x < tex->width);
        assert(y >= 0 && y < tex->height);

        int index = (y * tex->width + x) * 4;

        color = sample_texel(tex, x, y);
    }
    else
    {
        // See 8.14 in: https://www.khronos.org/registry/OpenGL/specs/gl/glspec45.core.pdf

        /*
        float dudx = uv_derivatives.x;
        float dvdx = uv_derivatives.y;

        float dudy = uv_derivatives.z;
        float dvdy = uv_derivatives.w;

        float rho1 = sqrtf(dudx * dudx + dvdx * dvdx);
        float rho2 = sqrtf(dudy * dudy + dvdy * dvdy);
        float rho = rho1 > rho2 ? rho1 : rho2;

        float λ_base = log2f(rho);
        */

        float uMHalf = u - 0.5f;
        float vMHalf = v - 0.5f;

        int i0 = tex_coord_wrap((int)floorf(uMHalf), tex->width, tex->wrap_s);
        int j0 = tex_coord_wrap((int)floorf(vMHalf), tex->height, tex->wrap_t);

        int i1 = tex_coord_wrap((int)floorf(uMHalf) + 1, tex->width, tex->wrap_s);
        int j1 = tex_coord_wrap((int)floorf(vMHalf) + 1, tex->height, tex->wrap_t);

        assert(i0 >= 0 && i0 < tex->width);
        assert(i1 >= 0 && i0 < tex->width);
        assert(j0 >= 0 && i0 < tex->height);
        assert(j1 >= 0 && i0 < tex->height);

        //float int_part;
        //float α = modff(uMHalf, &int_part);
        //float β = modff(vMHalf, &int_part);
        float α = uMHalf - floorf(uMHalf);
        float β = vMHalf - floorf(vMHalf);

        assert(α >= 0 && α <= 1.0f);
        assert(β >= 0 && β <= 1.0f);

        vec3_t τ_i0j0 = sample_texel(tex, i0, j0);
        vec3_t τ_i0j1 = sample_texel(tex, i0, j1);
        vec3_t τ_i1j0 = sample_texel(tex, i1, j0);
        vec3_t τ_i1j1 = sample_texel(tex, i1, j1);

        vec3_t τ_i01j0 = vec3_lerp(τ_i0j0, τ_i1j0, α);
        vec3_t τ_i01j1 = vec3_lerp(τ_i0j1, τ_i1j1, α);

        vec3_t τ = vec3_lerp(τ_i01j0, τ_i01j1, β);

        /*
        vec3_t τ_i01j0 = vec3_lerp(τ_i0j0, τ_i0j1, β);
        vec3_t τ_i01j1 = vec3_lerp(τ_i1j0, τ_i1j1, β);

        vec3_t τ = vec3_lerp(τ_i01j0, τ_i01j1, α);
        */

        color = τ;
    }

    return color;
}

vec3_t sample_ptex_texture(Ptex::PtexTexture* tex, Ptex::PtexFilter* filter, int faceID, vec2_t uv, vec4_t uv_derivatives)
{
    //assert(faceID < tex->numFaces());

    if (faceID >= tex->numFaces())
        return { 1.0f, 0.0f, 1.0f };

    if (tex->numChannels() == 1)
    {
        float gray;
        filter->eval(&gray, 0, 1, faceID, uv.x, uv.y, uv_derivatives.x, uv_derivatives.y, uv_derivatives.z, uv_derivatives.w);
        return { gray, gray, gray };
    }
    else if (tex->numChannels() == 3)
    {
        vec3_t color;
        filter->eval((float*) &color, 0, 3, faceID, uv.x, uv.y, uv_derivatives.x, uv_derivatives.y, uv_derivatives.z, uv_derivatives.w);
        return color;
    }
    else if (tex->numChannels() == 4)
    {
        vec3_t color;
        filter->eval((float*)&color, 0, 3, faceID, uv.x, uv.y, uv_derivatives.x, uv_derivatives.y, uv_derivatives.z, uv_derivatives.w);
        return color;
    }
    else assert(false);
}

vec3_t* calculate_image_cpu(int width, int height, uint16_t* faceID_buffer, vec3_t* uv_buffer, vec4_t* uv_deriv_buffer, vec3_t background_color)
{
    vec3_t* cpu_data = (vec3_t*)malloc(width * height * sizeof(vec3_t));
    assert(cpu_data != NULL);

    vec3_t bg = background_color;
    //{ (uint8_t)(background_color.x * 255), (uint8_t)(background_color.y * 255), (uint8_t)(background_color.z * 255) };

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

            vec3_t uv = uv_buffer[i];

            vec4_t uv_deriv = uv_deriv_buffer[i];
            
            vec3_t color = { uv.x, uv.y, uv.z };

            //vec3_t tex = sample_texture(&g_tex_test, { uv.x, uv.y }, uv_deriv);

            vec3_t ptex = sample_ptex_texture(g_ptex_texture, g_ptex_filter, id, { uv.x, uv.y }, uv_deriv);

            //const rgb8_t colors[] = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255} };

            cpu_data[i] = ptex;
            //cpu_data[i] = { uv.x, uv.y, 0 };
        }
    }

    return cpu_data;
}

float calculate_sme_f32(float v1, float v2)
{
    float diff = v1 - v2;
    return diff * diff;
}

float calculate_sme_u8(uint8_t v1, uint8_t v2) 
{
    float f1 = v1 / 255.0f;
    float f2 = v2 / 255.0f;

    float diff = f1 - f2;

    return diff * diff;
}

void compare_buffers_rgb32f(float* reference, float* result, int width, int height)
{
    float red_sme = 0, green_sme = 0, blue_sme = 0;
    float red_max = 0, green_max = 0, blue_max = 0;
    int diffs = 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int i = (y * width + x) * 3;

            float ref_red = reference[i + 0];
            float ref_green = reference[i + 1];
            float ref_blue = reference[i + 2];

            float res_red = result[i + 0];
            float res_green = result[i + 1];
            float res_blue = result[i + 2];

            float red_diff = ref_red - res_red;
            float green_diff = ref_green - res_green;
            float blue_diff = ref_blue - res_blue;

            red_sme += red_diff * red_diff;
            green_sme += green_diff * green_diff;
            blue_sme += blue_diff * blue_diff;

            if (red_diff > red_max) red_max = red_diff;
            if (green_diff > green_max) green_max = green_diff;
            if (blue_diff > blue_max) blue_max = blue_diff;

            if (red_diff != 0 || green_diff != 0 || blue_diff != 0) {

                //printf("Diff at (%d, %d). Reference: (%g, %g, %g), Result: (%g, %g, %g)\n", x, y, ref_red, ref_green, ref_blue, res_red, res_green, res_blue);

                diffs++;
            }
        }
    }

    int total_pixels = width * height;

    red_sme /= total_pixels;
    green_sme /= total_pixels;
    blue_sme /= total_pixels;

    printf("red SME: %g, green SME: %g, blue SME: %g\n", red_sme, green_sme, blue_sme);
    printf("red MAX: %g, green MAX: %g, blue MAX: %g\n", red_max * 255, green_max * 255, blue_max * 255);
    printf("%d non-matching pixels\n", diffs);
}

void compare_buffers_rgb8(uint8_t* reference, uint8_t* result, int width, int height)
{
    float red_sme = 0, green_sme = 0, blue_sme = 0;
    float red_max = 0, green_max = 0, blue_max = 0;
    int diffs = 0;

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++) 
        {
            int i = (y * width + x) * 3;

            float ref_red   = reference[i + 0] / 255.0f;
            float ref_green = reference[i + 1] / 255.0f;
            float ref_blue  = reference[i + 2] / 255.0f;
            
            float res_red   = result[i + 0] / 255.0f;
            float res_green = result[i + 1] / 255.0f;
            float res_blue  = result[i + 2] / 255.0f;

            float red_diff = ref_red - res_red;
            float green_diff = ref_green - res_green;
            float blue_diff = ref_blue - res_blue;

            red_sme += red_diff * red_diff;
            green_sme += green_diff * green_diff;
            blue_sme += blue_diff * blue_diff;

            if (red_diff > red_max) red_max = red_diff;
            if (green_diff > green_max) green_max = green_diff;
            if (blue_diff > blue_max) blue_max = blue_diff;

            if (red_diff != 0 || green_diff != 0 || blue_diff != 0) {

                //printf("Diff at (%d, %d). Reference: (%u, %u, %u), Result: (%u, %u, %u)\n", x, y, ref_red, ref_green, ref_blue, res_red, res_green, res_blue);

                diffs++;
            }
        }
    }

    int total_pixels = width * height;

    red_sme /= total_pixels;
    green_sme /= total_pixels;
    blue_sme /= total_pixels;

    printf("red SME: %g, green SME: %g, blue SME: %g\n", red_sme, green_sme, blue_sme);
    printf("red MAX: %g, green MAX: %g, blue MAX: %g\n", red_max * 255.0f, green_max * 255.0f, blue_max * 255.0f);
    printf("%d different red pixels\n", diffs);
}

texture_t create_texture(const char* filepath, texture_desc desc) {
    int channels;
    texture_t tex;
    stbi_set_flip_vertically_on_load(true);
    stbi_uc* img = stbi_load(filepath, &tex.width, &tex.height, &channels, 4);

    glGenTextures(1, &tex.texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex.texture);

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

texture_t create_empty_texture(texture_desc desc, int width, int height, GLenum internal_format) 
{
    int channels;
    texture_t tex;
    
    glGenTextures(1, &tex.texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex.texture);

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

int main(int argv, char** argc)
{
    // FIXME: Either make this work for all platforms,
    // or find a better solution for this.
    change_directory("../../../assets");
    
    Ptex::String error_str;
    //g_ptex_texture = PtexTexture::open("models/teapot/teapot.ptx", error_str);
    g_ptex_texture = PtexTexture::open("models/sphere/sphere_sorted_line.ptx", error_str);

    if (error_str.empty() == false)
    {
        printf("Ptex Error! %s\n", error_str.c_str());
    }

    char* meshTypeString[] = { "mt_triangle", "mt_quad" };
    char* dataTypeString[] = { "dt_uint8", "dt_uint16", "dt_half", "dt_float" };
    char* borderModeString[] = { "m_clamp", "m_black", "m_periodic" };
    char* edgeFilterModeString[] = { "efm_none", "efm_tanvec" };

    auto info = g_ptex_texture->getInfo();
    std::cout << "meshType: " << meshTypeString[info.meshType] << std::endl;
    std::cout << "dataType: " << dataTypeString[info.dataType] << std::endl;
    std::cout << "uBorderMode: " << borderModeString[info.uBorderMode] << std::endl;
    std::cout << "vBorderMode: " << borderModeString[info.vBorderMode] << std::endl;
    std::cout << "edgeFilterMode: " << edgeFilterModeString[info.edgeFilterMode] << std::endl;
    std::cout << "alphaChannel: " << info.alphaChannel << std::endl;
    std::cout << "numChannels: " << info.numChannels << std::endl;
    std::cout << "numFaces: " << info.numFaces << std::endl;

    char* metadataTypeString[] = { "mdt_string", "mdt_int8", "mdt_int16", "mdt_int32", "mdt_float", "mdt_double" };

    auto meta = g_ptex_texture->getMetaData();
    std::cout << "Metadata keys: " << meta->numKeys() << std::endl;
    for (size_t i = 0; i < meta->numKeys(); i++)
    {
        Ptex::MetaDataType metaData;
        const char* name;
        meta->getKey(i, name, metaData);
        printf("%zu %s: %s\n", i, name, metadataTypeString[metaData]);    
    }

    g_ptex_filter = Ptex::PtexFilter::getFilter(g_ptex_texture, PtexFilter::Options{ PtexFilter::FilterType::f_point, false, 0, false });

    g_camera = {
        { 0, 1, 0 }, // center
        float_eerp(CAMERA_MIN_DIST, CAMERA_MAX_DIST, 0.3f), 0.3f, // distance, distance_t
        0, 0,

        { 0, 0, 0, 1 } // quat
    };

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

    GLFWwindow* window = glfwCreateWindow(800, 800, "Test title", NULL, NULL);
    if (window == NULL)
    {
        printf("Failed to create GLFW window\n");
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);

    // Enable VSync
    glfwSwapInterval(1);

    if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == false)
    {
        printf("Failed to initialize GLAD\n");
        return EXIT_FAILURE;
    }

    glfwShowWindow(window);

    glfwSetFramebufferSizeCallback(window, GLFWFrambufferSizeCallback);
    glfwSetKeyCallback(window, GLFWKeyCallback);
    glfwSetCursorPosCallback(window, GLFWMouseCallback);
    glfwSetMouseButtonCallback(window, GLFWMouseButtonCallback);
    glfwSetScrollCallback(window, GLFWScrollCallback);

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

    // setup "to cpu" output buffer
    {
        color_attachment_desc faceID_desc = {
            GL_R16UI,
            GL_RED_INTEGER,
            GL_UNSIGNED_INT,
            GL_REPEAT, GL_REPEAT,
            GL_LINEAR, GL_LINEAR
        };

        color_attachment_desc UV_desc = {
            GL_RGB32F,
            GL_RGB,
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

    // setup output buffer
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
    
    texture_desc tex_test_desc = {
        GL_REPEAT, GL_REPEAT,
        GL_LINEAR, GL_LINEAR,
        false,
    };

    g_tex_test = create_texture("textures/uv_space.png", tex_test_desc);

    texture_desc cpu_stream_tex_desc = {
        GL_CLAMP, GL_CLAMP,
        GL_LINEAR, GL_LINEAR,
        true
    };
    g_cpu_stream_tex = create_empty_texture(cpu_stream_tex_desc, width, height, GL_RGB32F);

    mesh_t* mesh = load_obj("models/susanne.obj");

    GLuint vao;
    {
        attribute_desc* attribs = new attribute_desc[3];
        attribs[0] = {
            3, GL_FLOAT, GL_FALSE, offsetof(vertex_t, position), false,
        };
        attribs[1] = {
            2, GL_FLOAT, GL_TRUE, offsetof(vertex_t, texcoord), false,
        };
        attribs[2] = {
            3, GL_FLOAT, GL_FALSE, offsetof(vertex_t, normal), false,
        };

        vao_desc vao_desc;
        vao_desc.num_attribs = 3;
        vao_desc.attribs = attribs;

        vao = create_vao(&vao_desc, mesh->vertices, sizeof(vertex_t), mesh->num_faces * 3);

        delete[] attribs;
    }

    GLuint ptex_vao;
    {
        //g_teapot_mesh = load_ptex_mesh("models/teapot/teapot.obj");
        g_teapot_mesh = load_ptex_mesh("models/sphere/sphere.obj");

        attribute_desc* attribs = new attribute_desc[4];
        attribs[0] = {
            3, GL_FLOAT, GL_FALSE, offsetof(ptex_vertex_t, position), false,
        };
        attribs[1] = {
            3, GL_FLOAT, GL_FALSE, offsetof(ptex_vertex_t, normal), false,
        };
        attribs[2] = {
            2, GL_FLOAT, GL_TRUE, offsetof(ptex_vertex_t, uv), false,
        };
        attribs[3] = {
            1, GL_INT, GL_FALSE, offsetof(ptex_vertex_t, face_id), true,
        };

        vao_desc vao_desc;
        vao_desc.num_attribs = 4;
        vao_desc.attribs = attribs;

        ptex_vao = create_vao(&vao_desc, g_teapot_mesh->vertices, sizeof(ptex_vertex_t), g_teapot_mesh->num_vertices);

        delete[] attribs;
    }

    g_camera.center = g_teapot_mesh->center;

    //GLuint program = compile_shader("shaders/default.vert", "shaders/default.frag");
    //GLuint outputProgram = compile_shader("shaders/default.vert", "shaders/output.frag");

    GLuint ptex_program = compile_shader("shaders/ptex.vert", "shaders/ptex.frag");
    GLuint ptex_output_program = compile_shader("shaders/ptex.vert", "shaders/ptex_output.frag");

    GLuint cpu_stream_program = compile_shader("shaders/fullscreen.vert", "shaders/fullscreen.frag");

    mat4_t view = 
    { {
        {1, 0, -0, -0},
        {0, 1, 4.37114e-08f, -0.3},
        {0, -4.37114e-08f, 1, -1.5f},
        {0, 0, 0, 1}
    } };
    view = calc_view_matrix(g_camera);

    mat4_t proj =
    { {
        {1.73205f, 0, 0, 0},
        {0, 1.73205f, 0, 0},
        {0, 0, -1.00002f, -0.200002f},
        {0, 0, -1, 0}
    } };
    
    view = mat4_transpose(view);
    proj = mat4_transpose(proj);

    mat4_t vp = mat4_mul_mat4(view, proj);

    mat4_t model = mat4_scale(0.5f, 0.5f, 0.5f);
    model = mat4_scale(1.0f, 1.f, 1.f);

    mat4_t mvp = mat4_mul_mat4(model, vp);

    //glUseProgram(program);
    
    //int tex_location = glGetUniformLocation(program, "tex");
    //glUniform1i(tex_location, 0);

    //int vp_location = glGetUniformLocation(program, "mvp");
    //glUniformMatrix4fv(vp_location, 1, GL_FALSE, (float*)&mvp);

    
    uniform_mat4(ptex_program, "mvp", &mvp);
    uniform_mat4(ptex_output_program, "mvp", &mvp);

    glEnable(GL_DEPTH_TEST);

    vec3_t bg_color = { 1.0f, 0.5f, 0.8f };

    glBindVertexArray(vao);
    glBindVertexArray(ptex_vao);

    while (glfwWindowShouldClose(window) == false)
    {
        glfwPollEvents();


        {
            view = calc_view_matrix(g_camera);
            view = mat4_transpose(view);

            mat4_t vp = mat4_mul_mat4(view, proj);
            mat4_t mvp = mat4_mul_mat4(model, vp);

            uniform_mat4(ptex_program, "mvp", &mvp);
            uniform_mat4(ptex_output_program, "mvp", &mvp);
        }


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
            void* uv_buffer = malloc(width * height * sizeof(vec3_t));
            glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, uv_buffer);

            glReadBuffer(GL_COLOR_ATTACHMENT2);
            void* uv_deriv_buffer = malloc(width * height * sizeof(vec4_t));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, uv_deriv_buffer);

            img_write("text_faceID.img", width, height, R16UI, faceID_buffer);
            img_write("test_uv.img", width, height, RGB32F, uv_buffer);
            img_write("test_uv_deriv.img", width, height, RGBA32F, uv_deriv_buffer);

            glBindFramebuffer(GL_FRAMEBUFFER, g_output_framebuffer.framebuffer);

            vec3_t* reference_buffer = (vec3_t*)malloc(width * height * 3 * sizeof(float));
            glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, reference_buffer);

            stbi_flip_vertically_on_write(true);
            //stbi_write_png("test_ref.png", width, height, 3, reference_buffer, width * 3 * sizeof(uint8_t));
            img_write("test_ref.img", width, height, RGB32F, reference_buffer);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);


            rgb8_t* reference_rgb8_buffer = vec3_buffer_to_rgb8(reference_buffer, width, height);
            //rgb8_t* reference_rgb8_buffer = (rgb8_t*)malloc(width * height * 3 * sizeof(uint8_t));
            //glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, reference_rgb8_buffer);

            vec3_t* cpu_buffer = calculate_image_cpu(width, height, (uint16_t*)faceID_buffer, (vec3_t*)uv_buffer, (vec4_t*)uv_deriv_buffer, bg_color);

            //stbi_write_png("test_cpu.png", width, height, 3, cpu_buffer, width * 3);
            img_write("test_cpu.img", width, height, RGB32F, cpu_buffer);
            
            //vec3_buffer_to_rgb8_check(cpu_buffer, reference_rgb8_buffer, width, height);

            rgb8_t* cpu_rgb8_buffer = vec3_buffer_to_rgb8(cpu_buffer, width, height);

            stbi_write_png("test_ref.png", width, height, 3, reference_rgb8_buffer, width * 3);
            stbi_write_png("test_cpu.png", width, height, 3, cpu_rgb8_buffer, width * 3);

            if (memcmp(reference_buffer, cpu_buffer, width * height * 3) != 0)
                printf("Difference!!\n");

            compare_buffers_rgb32f((float*)reference_buffer, (float*)cpu_buffer, width, height);
            compare_buffers_rgb8((uint8_t*)reference_rgb8_buffer, (uint8_t*)cpu_rgb8_buffer, width, height);

            free(reference_rgb8_buffer);
            free(cpu_rgb8_buffer);

            free(faceID_buffer);
            free(uv_buffer);
            free(uv_deriv_buffer);
            free(reference_buffer);
            free(cpu_buffer);

            takeScreenshot = false;
        }

        if (stream_cpu_result)
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            glBindFramebuffer(GL_FRAMEBUFFER, g_frambuffer.framebuffer);

            glReadBuffer(GL_COLOR_ATTACHMENT0);
            void* faceID_buffer = malloc(width * height * sizeof(uint16_t));
            glReadPixels(0, 0, width, height, GL_RED_INTEGER, GL_UNSIGNED_SHORT, faceID_buffer);

            glReadBuffer(GL_COLOR_ATTACHMENT1);
            void* uv_buffer = malloc(width * height * sizeof(vec3_t));
            glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, uv_buffer);

            glReadBuffer(GL_COLOR_ATTACHMENT2);
            void* uv_deriv_buffer = malloc(width * height * sizeof(vec4_t));
            glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, uv_deriv_buffer);

            vec3_t* cpu_buffer = calculate_image_cpu(width, height, (uint16_t*)faceID_buffer, (vec3_t*)uv_buffer, (vec4_t*)uv_deriv_buffer, bg_color);

            rgb8_t* cpu_rgb8_buffer = vec3_buffer_to_rgb8(cpu_buffer, width, height);

            update_texture(&g_cpu_stream_tex, GL_RGB32F, GL_RGB, GL_FLOAT, width, height, cpu_buffer);

            free(cpu_rgb8_buffer);
            free(faceID_buffer);
            free(uv_buffer);
            free(uv_deriv_buffer);
            free(cpu_buffer);
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
        glClearBufferfv(GL_COLOR, 2, uvClearValue);
        glClearBufferfv(GL_DEPTH, 0, &depthClearValue);

        glUseProgram(ptex_output_program);

        glDrawArrays(GL_TRIANGLES, 0, g_teapot_mesh->num_vertices);
        
        glBindFramebuffer(GL_FRAMEBUFFER, g_output_framebuffer.framebuffer);

        glUseProgram(ptex_program);

        glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_tex_test.texture);

        glDrawArrays(GL_TRIANGLES, 0, g_teapot_mesh->num_vertices);

        glBindTexture(GL_TEXTURE_2D, 0);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_output_framebuffer.framebuffer);

        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        if (stream_cpu_result)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glUseProgram(cpu_stream_program);
            
            int tex_location = glGetUniformLocation(cpu_stream_program, "tex");
            glUniform1i(tex_location, 0);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_cpu_stream_tex.texture);

            glDisable(GL_DEPTH_TEST);
            
            glDrawArrays(GL_TRIANGLES, 0, 3);

            glEnable(GL_DEPTH_TEST);
        }

        glfwSwapBuffers(window);
    }

    g_ptex_filter->release();
    g_ptex_texture->release();

    glfwTerminate();
    return EXIT_SUCCESS;
}