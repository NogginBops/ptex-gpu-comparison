﻿#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "mesh.hh"
#include "mesh_loading.hh"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>

#include <stb_image.h>
#include <stb_image_write.h>

#include <Ptexture.h>
#include <iostream>

#include "util.hh"
#include "gl_utils.hh"
#include "ptex_utils.hh"

#include "platform.hh"

#include "array.hh"

#include "img.hh"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>


#include "cpu_renderer.hh"

#include "methods/Methods.hh"

bool g_show_imgui;

Methods::Methods current_rendering_method = Methods::Methods::nvidia;
Methods::Methods prev_rendering_method;

texture_t g_tex_test;

ptex_mesh_t* g_mesh;

typedef struct {
    float fovy;
    float aspect;
    float near_plane;
    float far_plane;

    vec3_t center, offset;
    float distance, distance_t;
    float x_axis_rot, y_axis_rot;

    quat_t quaternion;
} camera_t;

camera_t g_camera;

const float MOUSE_SPEED_X = -0.01f;
const float MOUSE_SPEED_Y = -0.01f;

const float ZOOM_SPEED = 0.01f;

const float MOUSE_TRANSLATION_SPEED_X = -0.001f;
const float MOUSE_TRANSLATION_SPEED_Y = -0.001f;

const float CAMERA_MIN_Y = -85.0f;
const float CAMERA_MAX_Y = +85.0f;

const float CAMERA_MIN_DIST = 0.2f;
const float CAMERA_MAX_DIST = 100.0f;


mat4_t calc_view_matrix(camera_t camera) 
{
    vec3_t center = vec3_add(camera.center, camera.offset);

    mat4_t center_offset = mat4_translate(center.x, center.y, center.z);
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

void reset_camera(camera_t* camera)
{
    camera->offset = { 0 };
    camera->x_axis_rot = 0;
    camera->y_axis_rot = 0;
    camera->quaternion = quat_identity();
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
    
    // Ignore "Buffer detailed info:" messages.
    if (id == 131185)
        return;

    // This is a message we sent, with glPushDebugGroup
    if (id == 0)
    {
        assert(severity == GL_DEBUG_SEVERITY_NOTIFICATION);
        return;
    }

    printf("%u: %s\n", id, message);

    if (severity == GL_DEBUG_SEVERITY_HIGH)
    {
        printf("Error!\n");
    }
}

void GLFWFrambufferSizeCallback(GLFWwindow* window, int width, int height)
{
    if (width == 0 || height == 0) return;
    
    // FIXME: Resize buffers of all methods
    Methods::cpu.resize_buffers(width, height);
    Methods::nvidia.resize_buffers(width, height);
    Methods::intel.resize_buffers(width, height);

    g_camera.aspect = width / (float)height;

    printf("Resize!\n");
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
        if (current_rendering_method == Methods::Methods::cpu)
        {
            current_rendering_method = prev_rendering_method;
        }
        else
        {
            prev_rendering_method = current_rendering_method;
            current_rendering_method = Methods::Methods::cpu;
            
        }
    }

    if (key == GLFW_KEY_BACKSPACE && action == GLFW_PRESS)
    {
        reset_camera(&g_camera);
    }

    if (key == GLFW_KEY_I && action == GLFW_PRESS)
    {
        g_show_imgui = !g_show_imgui;
    }
}

bool control_camera = false;
bool translate_camera = false;
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

    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
    {
        translate_camera = true;
    }
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
    {
        translate_camera = false;
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

    if (translate_camera)
    {
        // Get the plane of the current camera
        mat4_t rot = mat4_from_quat(g_camera.quaternion);
        
        vec3_t up = vec3_from_vec4(mat4_mul_vec4(rot, { 0, -1, 0, 0 }));
        vec3_t right = vec3_from_vec4(mat4_mul_vec4(rot, { 1, 0, 0, 0 }));

        float speed = float_eerp(0.5f, 150.0f, g_camera.distance_t);

        g_camera.offset = vec3_add(g_camera.offset, vec3_add(vec3_mul(up, ydiff * MOUSE_TRANSLATION_SPEED_Y * speed), vec3_mul(right, xdiff * MOUSE_TRANSLATION_SPEED_X * speed)));
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

int main(int argv, char** argc)
{
    // FIXME: Either make this work for all platforms,
    // or find a better solution for this.
    change_directory("../../../assets");
    
    g_mesh = load_ptex_mesh("models/ground_plane/ground_plane.obj");
    //g_mesh = load_ptex_mesh("models/teapot/teapot.obj");
    //g_mesh = load_ptex_mesh("models/mud_sphere/mud_sphere.obj");

    Ptex::String error_str;
    g_ptex_texture = PtexTexture::open("models/ground_plane/ground_plane.ptx", error_str);
    //g_ptex_texture = PtexTexture::open("models/teapot/teapot.ptx", error_str);
    //g_ptex_texture = PtexTexture::open("models/mud_sphere/mud_sphere.ptx", error_str);

    if (error_str.empty() == false)
    {
        printf("Ptex Error! %s\n", error_str.c_str());
    }

    /*
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
    */

    g_current_filter_type = PtexFilter::FilterType::f_bilinear;
    g_ptex_filter = Ptex::PtexFilter::getFilter(g_ptex_texture, PtexFilter::Options{ g_current_filter_type, false, 0, false });

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

    GLFWwindow* window = glfwCreateWindow(500, 500, "Test title", NULL, NULL);
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

    g_camera = {
       TO_RADIANS(80), // fovy
       width / (float)height, // aspect
       0.01f, 1000.0f, // near, far

       { 0, 1, 0 }, { 0, 0, 0 }, // center, offset
       float_eerp(CAMERA_MIN_DIST, CAMERA_MAX_DIST, 0.3f), 0.3f, // distance, distance_t
       0, 0,

       { 0, 0, 0, 1 } // quat
    };

    const char* version = (char*)glGetString(GL_VERSION);
    glfwSetWindowTitle(window, version);
    
    // Check for GL_KHR_debug
    {
        int num_extensions;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

        for (size_t i = 0; i < num_extensions; i++)
        {
            const char* ext = (char*)glGetStringi(GL_EXTENSIONS, i);

            if (strcmp(ext, "GL_KHR_debug") == 0)
            {
                has_KHR_debug = true;
            }
        }
    }

    if (has_KHR_debug)
    {
        glDebugMessageCallback(GLDebugCallback, NULL);
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }

    glDisable(GL_SCISSOR_TEST);

    Methods::init_methods(
        width, height, 
        g_ptex_texture, g_ptex_filter, 
        GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR,
        16.0f);

    ImGuiContext* imctx = ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(NULL);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f, 0.5f, 0.0f,
    };

    texture_desc tex_test_desc = {
        GL_REPEAT, GL_REPEAT,
        GL_LINEAR, GL_LINEAR,
        false,
    };
    g_tex_test = create_texture("textures/uv_space.png", tex_test_desc);

    mesh_t* mesh = load_obj("models/susanne.obj");

    GLuint vao;
    {
        attribute_desc* attribs = new attribute_desc[3];
        attribs[0] = {
            "Position", 3, GL_FLOAT, GL_FALSE, offsetof(vertex_t, position), false,
        };
        attribs[1] = {
            "UV", 2, GL_FLOAT, GL_TRUE, offsetof(vertex_t, texcoord), false,
        };
        attribs[2] = {
            "Normal", 3, GL_FLOAT, GL_FALSE, offsetof(vertex_t, normal), false,
        };

        vao_desc vao_desc = {
            "Susanne",
            3,
            attribs,
        };

        vao = create_vao(&vao_desc, mesh->vertices, sizeof(vertex_t), mesh->num_faces * 3);

        delete[] attribs;
    }

    GLuint ptex_vao;
    {
        attribute_desc* attribs = new attribute_desc[4];
        attribs[0] = {
            "Position", 3, GL_FLOAT, GL_FALSE, offsetof(ptex_vertex_t, position), false,
        };
        attribs[1] = {
            "Normal", 3, GL_FLOAT, GL_FALSE, offsetof(ptex_vertex_t, normal), false,
        };
        attribs[2] = {
            "UV", 2, GL_FLOAT, GL_TRUE, offsetof(ptex_vertex_t, uv), false,
        };
        attribs[3] = {
            "FaceID", 1, GL_INT, GL_FALSE, offsetof(ptex_vertex_t, face_id), true,
        };

        vao_desc vao_desc = {
            "ptex model",
            4,
            attribs,
        };

        ptex_vao = create_vao(&vao_desc, g_mesh->vertices, sizeof(ptex_vertex_t), g_mesh->num_vertices);

        delete[] attribs;
    }

    g_camera.center = g_mesh->center;

    //GLuint program = compile_shader("shaders/default.vert", "shaders/default.frag");
    //GLuint outputProgram = compile_shader("shaders/default.vert", "shaders/output.frag");

    //GLuint ptex_program_intel = compile_shader("ptex_program", "shaders/ptex.vert", "shaders/ptex_intel.frag");
    
    GLuint cpu_stream_program = compile_shader("cpu_stream_program", "shaders/fullscreen.vert", "shaders/fullscreen.frag");

    mat4_t view = calc_view_matrix(g_camera);
    mat4_t proj = mat4_perspective(g_camera.fovy, width / (float)height, 0.01f, 1000.0f);
    
    view = mat4_transpose(view);
    proj = mat4_transpose(proj);

    mat4_t vp = mat4_mul_mat4(view, proj);

    mat4_t model = mat4_scale(0.5f, 0.5f, 0.5f);
    model = mat4_scale(1.0f, 1.f, 1.f);
    model = mat4_mul_mat4(model, mat4_rotate_x(TO_RADIANS(15)));
    //model = mat4_scale(0.01f, 0.01f, 0.01f);
    
    mat4_t mvp = mat4_mul_mat4(model, vp);

    glEnable(GL_DEPTH_TEST);

    vec3_t bg_color = { 1.0f, 0.5f, 0.8f };

    glBindVertexArray(vao);
    glBindVertexArray(ptex_vao);

    while (glfwWindowShouldClose(window) == false)
    {
        glfwPollEvents();

        if (g_show_imgui)
        {
            ImGui_ImplGlfw_NewFrame();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();

            if (ImGui::Begin("Controls"))
            {
                if (ImGui::Button("Take screenshot"))
                {
                    takeScreenshot = true;
                }

                if (ImGui::BeginCombo("Ptex method", Methods::method_names[(int)current_rendering_method]))
                {
                    for (int i = 0; i < (int)Methods::Methods::last; i++)
                    {
                        bool is_selected = current_rendering_method == (Methods::Methods)i;
                        if (ImGui::Selectable(Methods::method_names[i], is_selected))
                            current_rendering_method = (Methods::Methods)i;
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                switch (current_rendering_method)
                {
                case Methods::Methods::cpu:
                {
                    const char* filter_names[] = {
                        "f_point",
                        "f_bilinear",
                        "f_box",
                        "f_gaussian",
                        "f_bicubic",
                        "f_bspline",
                        "f_catmullrom",
                        "f_mitchell"
                    };

                    if (ImGui::BeginCombo("Filter type", filter_names[g_current_filter_type]))
                    {
                        for (int i = 0; i < 8; i++)
                        {
                            bool is_selected = g_current_filter_type == (PtexFilter::FilterType)i;
                            if (ImGui::Selectable(filter_names[i], is_selected))
                            {
                                g_current_filter_type = (PtexFilter::FilterType)i;
                                g_ptex_filter->release();
                                g_ptex_filter = PtexFilter::getFilter(g_ptex_texture, PtexFilter::Options{ g_current_filter_type, false, 0, false });
                            }

                            if (is_selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    ImGui::Checkbox("Use dv/dx, du/dy", &use_cross_derivatives);
                    break;
                }
                case Methods::Methods::nvidia:
                {
                    int curr_aniso = Methods::nvidia.border_sampler.desc.max_anisotropy;
                    if (ImGui::SliderInt("Max Anisotropy", &curr_aniso, 1, 16))
                    {
                        glSamplerParameterf(Methods::nvidia.border_sampler.sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, curr_aniso);
                        Methods::nvidia.border_sampler.desc.max_anisotropy = curr_aniso;
                    }
                    break;
                }
                case Methods::Methods::intel:
                {
                    int curr_aniso = Methods::intel.border_sampler.desc.max_anisotropy;
                    if (ImGui::SliderInt("Max Anisotropy", &curr_aniso, 1, 16))
                    {
                        glSamplerParameterf(Methods::intel.border_sampler.sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, curr_aniso);
                        glSamplerParameterf(Methods::intel.clamp_sampler.sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, curr_aniso);
                        Methods::intel.border_sampler.desc.max_anisotropy = curr_aniso;
                        Methods::intel.clamp_sampler.desc.max_anisotropy = curr_aniso;
                    }
                    break;
                }
                default:
                    break;
                }

                if (ImGui::Button("Reset camera"))
                {
                    reset_camera(&g_camera);
                }
            }

            ImGui::End();
        }
        
        if (takeScreenshot)
        {



            takeScreenshot = false;
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        view = calc_view_matrix(g_camera);
        view = mat4_transpose(view);

        proj = mat4_perspective(g_camera.fovy, g_camera.aspect, g_camera.near_plane, g_camera.far_plane);
        proj = mat4_transpose(proj);

        mat4_t vp = mat4_mul_mat4(view, proj);
        mat4_t mvp = mat4_mul_mat4(model, vp);

        const char* pass_name = Methods::method_names[(int)current_rendering_method];

        //Methods::nvidia.render(ptex_vao, g_teapot_mesh->num_vertices, mvp, bg_color);
        if (has_KHR_debug)
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, pass_name);
            
        switch (current_rendering_method)
        {
        case Methods::Methods::cpu:
            Methods::cpu.render(ptex_vao, g_mesh->num_vertices, mvp, bg_color);
            break;

        case Methods::Methods::nvidia:
            Methods::nvidia.render(ptex_vao, g_mesh->num_vertices, mvp, bg_color);
            break;

        case Methods::Methods::intel:
            Methods::intel.render(ptex_vao, g_mesh->num_vertices, mvp, bg_color);
            break;

        default:
            assert(false); break;
        }

        if (has_KHR_debug)
            glPopDebugGroup();

        framebuffer_t color_output_framebuffer;
        switch (current_rendering_method) {
        case Methods::Methods::cpu:
            color_output_framebuffer = Methods::cpu.cpu_result_framebuffer;
            break;

        case Methods::Methods::nvidia:
            color_output_framebuffer = Methods::nvidia.framebuffer;
            break;

        case Methods::Methods::intel:
            color_output_framebuffer = Methods::intel.ms_color_framebuffer;
            break;

        default:
            assert(false);
            break;
        }

        glActiveTexture(GL_TEXTURE0);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, color_output_framebuffer.framebuffer);

        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glDrawBuffer(GL_BACK);

        glClearColor(bg_color.x, bg_color.y, bg_color.z, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        //printf("width: %d, framebuffer: %d\n", width, color_output_framebuffer.width);
        glBlitFramebuffer(0, 0, color_output_framebuffer.width, color_output_framebuffer.height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        GLenum error;
        while ((error = glGetError()) != GL_NO_ERROR)
        {
            printf("Error: %x", error);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        if (g_show_imgui)
        {
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
        
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(imctx);

    g_ptex_filter->release();
    g_ptex_texture->release();

    glfwTerminate();
    return EXIT_SUCCESS;
}