#include <stdint.h>
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

#include "profiler.hh"

bool g_show_imgui;

Methods::Methods current_rendering_method = Methods::Methods::nvidia;
Methods::Methods prev_rendering_method;

texture_t g_tex_test;

int current_mesh = 0;
Ptex::PtexFilter* current_filter;

custom_arrays::array_t<const char*> mesh_names(10);
custom_arrays::array_t<ptex_mesh_t*> meshes(10);
custom_arrays::array_t<GLuint> mesh_vaos(10);
custom_arrays::array_t<Ptex::PtexTexture*> ptexTextures(10);
custom_arrays::array_t<gl_ptex_data> texturesGLData(10);
custom_arrays::array_t<mat4_t> mesh_model_matrix(10);

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

struct saved_viewpoint {
    char name[128];
    camera_t camera;
};

custom_arrays::array_t<const char*> viewpoint_names(0);
custom_arrays::array_t<saved_viewpoint> viewpoints(0);

void load_viewpoints_file(const char* path) {
    FILE* file = fopen(path, "rb");
    assert(file != NULL);

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (size == 0)
    {
        // This viewpoints file is uninitialized
        viewpoints = custom_arrays::array_t<saved_viewpoint>(10);
        fclose(file);
        return;
    }

    size_t read_size;

    // Read the number of viewpoints this file contains.
    int viewpoint_count;
    read_size = fread(&viewpoint_count, sizeof(int), 1, file);
    assert(read_size == 1);
    assert(viewpoint_count >= 0);

    // Allocate memory for that number of viewpoints
    saved_viewpoint* views = (saved_viewpoint*)malloc(viewpoint_count * sizeof(saved_viewpoint));
    assert(views != NULL);

    read_size = fread(views, sizeof(saved_viewpoint), viewpoint_count, file);
    assert(read_size == viewpoint_count);

    fclose(file);

    viewpoints = custom_arrays::array_t<saved_viewpoint>(views, viewpoint_count);
}

void save_viewpoints_file(const char* path) {
    FILE* file = fopen(path, "wb");
    assert(file != NULL);

    int* count = (int*)alloca(sizeof(int));
    *count = viewpoints.size;

    int elements_written;
    elements_written = fwrite(count, sizeof(int), 1, file);
    assert(elements_written == 1);
    elements_written = fwrite(viewpoints.arr, sizeof(saved_viewpoint), viewpoints.size, file);
    assert(elements_written == viewpoints.size);

    fflush(file);
    fclose(file);
}

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
    Methods::hybrid.resize_buffers(width, height);

    g_camera.aspect = width / (float)height;

    printf("Resize!\n");
}

bool takeScreenshot = false;

void GLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (ImGui::GetIO().WantCaptureKeyboard) return;

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

        ImGui::GetIO().WantCaptureKeyboard = false;
        ImGui::GetIO().WantCaptureMouse = false;

    }
}

bool control_camera = false;
bool translate_camera = false;
void GLFWMouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (ImGui::GetIO().WantCaptureMouse) return;

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
    if (ImGui::GetIO().WantCaptureMouse) return;

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
    if (ImGui::GetIO().WantCaptureMouse) return;

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

vec3_t rgb_to_vec3(int r, int g, int b)
{
    return { r / 255.0f, g / 255.0f, b / 255.0f };
}

void* download_framebuffer(framebuffer_t* framebuffer, GLenum attachment) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->framebuffer);

    int pixels = framebuffer->width * framebuffer->height;

    glReadBuffer(attachment);
    void* buffer = malloc(pixels * sizeof(float) * 3);
    glReadPixels(0, 0, framebuffer->width, framebuffer->height, GL_RGB, GL_FLOAT, buffer);

    return buffer;
}

#ifdef __APPLE__
    #define ASSETS_PATH "../assets"
#else
    #define ASSETS_PATH "../../../assets"
#endif

#define VIEWPOINTS_FILE "viewpoints.vp"

void add_model(const char* name, const char* model_path, const char* ptex_path, mat4_t model_mat)
{
    Ptex::String error_str;
    Ptex::PtexTexture* ptex = PtexTexture::open(ptex_path, error_str);
    if (error_str.empty() == false)
    {
        printf("Ptex Error at model %s! %s\n", name, error_str.c_str());
    }

    ptex_mesh_t* mesh = load_ptex_mesh(model_path);

    GLuint mesh_vao;
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

        mesh_vao = create_vao(&vao_desc, mesh->vertices, sizeof(ptex_vertex_t), mesh->num_vertices);

        delete[] attribs;
    }

    mesh_names.add(name);
    meshes.add(mesh);
    mesh_vaos.add(mesh_vao);
    ptexTextures.add(ptex);
    texturesGLData.add(create_gl_texture_arrays(extract_textures(ptex), GL_LINEAR, GL_LINEAR));
    mesh_model_matrix.add(model_mat);
}

int main(int argv, char** argc)
{
    // FIXME: Either make this work for all platforms,
    // or find a better solution for this.
    change_directory(ASSETS_PATH);

    load_viewpoints_file(VIEWPOINTS_FILE);
    
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
#if RELEASE
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
#else
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
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
#if !(RELEASE)
        glDebugMessageCallback(GLDebugCallback, NULL);
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
    }

    // Load models and textures
    {
        g_current_filter_type = PtexFilter::FilterType::f_bilinear;
        add_model("ground plane", "models/ground_plane/ground_plane.obj", "models/ground_plane/ground_plane.ptx", mat4_scale(1.0f, 1.0f, 1.0f));
        add_model("teapot", "models/teapot/teapot.obj", "models/teapot/teapot.ptx", mat4_scale(1.0f, 1.0f, 1.0f));
        add_model("sphere", "models/mud_sphere/mud_sphere.obj", "models/mud_sphere/mud_sphere.ptx", mat4_scale(0.01f, 0.01f, 0.01f));

        current_filter = PtexFilter::getFilter(ptexTextures[current_mesh], PtexFilter::Options{ g_current_filter_type, false, 0, false });
    }
    
    glDisable(GL_SCISSOR_TEST);

    Methods::init_methods(
        width, height, 
        GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR,
        16.0f);

    ImGuiContext* imctx = ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(NULL);

    texture_desc tex_test_desc = {
        GL_REPEAT, GL_REPEAT,
        GL_LINEAR, GL_LINEAR,
        false,
    };
    g_tex_test = create_texture("textures/uv_space.png", tex_test_desc);

    g_camera.center = meshes[current_mesh]->center;

    //GLuint program = compile_shader("shaders/default.vert", "shaders/default.frag");
    //GLuint outputProgram = compile_shader("shaders/default.vert", "shaders/output.frag");

    //GLuint ptex_program_intel = compile_shader("ptex_program", "shaders/ptex.vert", "shaders/ptex_intel.frag");
    
    GLuint cpu_stream_program = compile_shader("cpu_stream_program", "shaders/fullscreen.vert", "shaders/fullscreen.frag");

    mat4_t view = calc_view_matrix(g_camera);
    mat4_t proj = mat4_perspective(g_camera.fovy, width / (float)height, 0.01f, 1000.0f);
    
    view = mat4_transpose(view);
    proj = mat4_transpose(proj);

    mat4_t vp = mat4_mul_mat4(view, proj);

    float scale = 1.0;
    float angle_x = 0;
    float angle_y = 0;
    float angle_z = 0;
    
    mat4_t model = mesh_model_matrix[current_mesh];
    model = mat4_mul_mat4(model, mat4_rotate_x(angle_x));
    model = mat4_mul_mat4(model, mat4_rotate_y(angle_y));
    model = mat4_mul_mat4(model, mat4_rotate_z(angle_z));
    //model = mat4_scale(0.01f, 0.01f, 0.01f);
    
    mat4_t mvp = mat4_mul_mat4(model, vp);

    glEnable(GL_DEPTH_TEST);

    vec3_t bg_color = rgb_to_vec3(100, 149, 237);

    while (glfwWindowShouldClose(window) == false)
    {
        profiler::new_frame();
        #ifndef __APPLE__
            profiler::push_span("Frame", 0);
        #endif

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

                if (ImGui::CollapsingHeader("Model Transform")) {
                    ImGui::SliderAngle("Angle X", &angle_x);
                    ImGui::SliderAngle("Angle Y", &angle_y);
                    ImGui::SliderAngle("Angle Z", &angle_z);
                }

                if (ImGui::BeginCombo("Model", mesh_names[current_mesh]))
                {
                    for (int i = 0; i < mesh_names.size; i++)
                    {
                        bool is_selected = current_mesh == i;
                        if (ImGui::Selectable(mesh_names[i], is_selected))
                        {
                            current_mesh = i;
                            if (current_filter) current_filter->release();
                            current_filter = PtexFilter::getFilter(ptexTextures[current_mesh], PtexFilter::Options{ g_current_filter_type, false, 0, false });
                            g_camera.center = meshes[current_mesh]->center;
                        }
                        if (is_selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
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
                                if (current_filter) current_filter->release();
                                current_filter = PtexFilter::getFilter(ptexTextures[current_mesh], PtexFilter::Options{g_current_filter_type, false, 0, false});
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

                    if (ImGui::SliderInt("MSAA", &Methods::nvidia.framebuffer_desc.samples, 1, 16))
                    {
                        recreate_framebuffer(
                            &Methods::nvidia.framebuffer,
                            Methods::nvidia.framebuffer_desc,
                            Methods::nvidia.framebuffer.width,
                            Methods::nvidia.framebuffer.height);
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

                    if (ImGui::SliderInt("MSAA", &Methods::intel.ms_color_framebuffer_desc.samples, 1, 16))
                    {
                        // Recreate the framebuffer with the new number of samples
                        recreate_framebuffer(
                            &Methods::intel.ms_color_framebuffer, 
                            Methods::intel.ms_color_framebuffer_desc,
                            Methods::intel.ms_color_framebuffer.width, 
                            Methods::intel.ms_color_framebuffer.height);
                    }
                    break;
                }
                case Methods::Methods::hybrid:
                {
                    int curr_aniso = Methods::hybrid.border_sampler.desc.max_anisotropy;
                    if (ImGui::SliderInt("Max Anisotropy", &curr_aniso, 1, 16))
                    {
                        glSamplerParameterf(Methods::hybrid.border_sampler.sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, curr_aniso);
                        glSamplerParameterf(Methods::hybrid.clamp_sampler.sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, curr_aniso);
                        Methods::hybrid.border_sampler.desc.max_anisotropy = curr_aniso;
                        Methods::hybrid.clamp_sampler.desc.max_anisotropy = curr_aniso;
                    }

                    if (ImGui::SliderInt("MSAA", &Methods::hybrid.ms_framebuffer_desc.samples, 1, 16))
                    {
                        // Recreate the framebuffer with the new number of samples
                        recreate_framebuffer(
                            &Methods::hybrid.ms_framebuffer,
                            Methods::hybrid.ms_framebuffer_desc,
                            Methods::hybrid.ms_framebuffer.width,
                            Methods::hybrid.ms_framebuffer.height);
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

                if (ImGui::CollapsingHeader("Viewpoints", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    static char viewpoint_name[128];

                    static int current_viewpoint = 0;

                    // This list needs to be recreated every time we need it
                    // The underlying memory might have moved.
                    viewpoint_names.clear();
                    for (int i = 0; i < viewpoints.size; i++)
                    {
                        viewpoint_names.add(viewpoints[i].name);
                    }

                    if (ImGui::BeginListBox("Viewpoint"))
                    {
                        static bool editing_name = false;
                        int remove_index = -1;
                        for (size_t i = 0; i < viewpoint_names.size; i++)
                        {
                            bool is_selected = current_viewpoint == i;

                            if (is_selected && editing_name == true)
                            {
                                if (ImGui::InputText("###edit", viewpoints[i].name, sizeof(viewpoints[i].name), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue))
                                {
                                    editing_name = false;
                                }
                            }
                            else if (ImGui::Selectable(viewpoint_names[i], is_selected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowItemOverlap))
                            {
                                if (ImGui::IsMouseDoubleClicked(0)) editing_name = true;
                                else editing_name = false;

                                current_viewpoint = i;

                                // Copy over all camera data except center and aspect.
                                auto* cam = &viewpoints[current_viewpoint].camera;
                                g_camera.fovy = cam->fovy;
                                g_camera.near_plane = cam->near_plane;
                                g_camera.far_plane = cam->far_plane;
                                g_camera.offset = cam->offset;
                                g_camera.distance = cam->distance;
                                g_camera.distance_t = cam->distance_t;
                                g_camera.x_axis_rot = cam->x_axis_rot;
                                g_camera.y_axis_rot = cam->y_axis_rot;
                                g_camera.quaternion = cam->quaternion;
                            }

                            ImGui::SameLine(ImGui::GetColumnWidth(0) - 10);
                            char label[100];
                            sprintf(label, "x###%zd", i);
                            if (ImGui::SmallButton(label)) {
                                // Remove this from the list
                                remove_index = i;
                            }

                            if (is_selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndListBox();

                        // Check if we should remove one of the items
                        if (remove_index != -1)
                        {
                            // HACK: Quick and diry remove code
                            for (size_t i = remove_index; i < viewpoints.size-1; i++)
                            {
                                viewpoints[i] = viewpoints[i + 1];
                            }
                            viewpoints.size -= 1;
                        }
                    }

                    static bool save_modal_open = false;
                    if (ImGui::Button("Save viewpoint"))
                    {
                        ImGui::OpenPopup("Save viewpoint");
                        memset(viewpoint_name, 0, sizeof(viewpoint_name));

                        save_modal_open = true;
                    }

                    if (ImGui::BeginPopupModal("Save viewpoint", &save_modal_open))
                    {
                        ImGui::InputText("Viewpoint name", viewpoint_name, sizeof(viewpoint_name));
                        ImGui::SetItemDefaultFocus();

                        size_t length = strnlen(viewpoint_name, sizeof(viewpoint_name));
                        bool disable = length == 0;

                        ImGui::BeginDisabled(disable);
                        if (ImGui::Button("Save")) {
                            saved_viewpoint point;
                            memcpy(point.name, viewpoint_name, sizeof(viewpoint_name));
                            point.camera = g_camera;
                            viewpoints.add(point);
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndDisabled();
                        ImGui::EndPopup();
                    }
                }
            }

            profiler::show_profiler();

            ImGui::End();
        }
        
        view = calc_view_matrix(g_camera);
        view = mat4_transpose(view);

        proj = mat4_perspective(g_camera.fovy, g_camera.aspect, g_camera.near_plane, g_camera.far_plane);
        proj = mat4_transpose(proj);

        model = mesh_model_matrix[current_mesh];
        model = mat4_mul_mat4(model, mat4_rotate_x(angle_x));
        model = mat4_mul_mat4(model, mat4_rotate_y(angle_y));
        model = mat4_mul_mat4(model, mat4_rotate_z(angle_z));

        mat4_t vp = mat4_mul_mat4(view, proj);
        mat4_t mvp = mat4_mul_mat4(model, vp);

        if (takeScreenshot)
        {
            // Render using all methods.
            Methods::nvidia.render(mesh_vaos[current_mesh], meshes[current_mesh]->num_vertices, texturesGLData[current_mesh], mvp, bg_color);

            Methods::intel.render(mesh_vaos[current_mesh], meshes[current_mesh]->num_vertices, texturesGLData[current_mesh], mvp, bg_color);
            // resolve intel MS buffer
            glBindFramebuffer(GL_READ_FRAMEBUFFER, Methods::intel.ms_color_framebuffer.framebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Methods::intel.resolve_color_framebuffer.framebuffer);
            glBlitFramebuffer(
                0, 0, Methods::intel.ms_color_framebuffer.width, Methods::intel.ms_color_framebuffer.height,
                0, 0, Methods::intel.resolve_color_framebuffer.width, Methods::intel.resolve_color_framebuffer.height,
                GL_COLOR_BUFFER_BIT, GL_NEAREST);

            Methods::hybrid.render(mesh_vaos[current_mesh], meshes[current_mesh]->num_vertices, texturesGLData[current_mesh], mvp, bg_color);
            // resolve hybrid MS buffer
            glBindFramebuffer(GL_READ_FRAMEBUFFER, Methods::hybrid.ms_framebuffer.framebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Methods::hybrid.resolve_framebuffer.framebuffer);
            glBlitFramebuffer(
                0, 0, Methods::hybrid.ms_framebuffer.width, Methods::hybrid.ms_framebuffer.height,
                0, 0, Methods::hybrid.resolve_framebuffer.width, Methods::hybrid.resolve_framebuffer.height,
                GL_COLOR_BUFFER_BIT, GL_NEAREST);

            Methods::cpu.render(mesh_vaos[current_mesh], meshes[current_mesh]->num_vertices, ptexTextures[current_mesh], current_filter, mvp, bg_color);

            // Then we will download all of the final pictures.
            // We also need to resolve the intel MS framebuffer.
            
            vec3_t* nvidia_data = (vec3_t*)download_framebuffer(&Methods::nvidia.framebuffer, GL_COLOR_ATTACHMENT0);
            vec3_t* intel_data = (vec3_t*)download_framebuffer(&Methods::intel.resolve_color_framebuffer, GL_COLOR_ATTACHMENT0);
            vec3_t* hybrid_data = (vec3_t*)download_framebuffer(&Methods::hybrid.resolve_framebuffer, GL_COLOR_ATTACHMENT0);
            vec3_t* cpu_data = (vec3_t*)download_framebuffer(&Methods::cpu.cpu_result_framebuffer, GL_COLOR_ATTACHMENT0);

            stbi_flip_vertically_on_write(true);

            rgb8_t* nvidia_rgb8_data = vec3_buffer_to_rgb8(nvidia_data, Methods::nvidia.framebuffer.width, Methods::nvidia.framebuffer.height);
            rgb8_t* intel_rgb8_data = vec3_buffer_to_rgb8(intel_data, Methods::intel.resolve_color_framebuffer.width, Methods::intel.resolve_color_framebuffer.height);
            rgb8_t* hybrid_rgb8_data = vec3_buffer_to_rgb8(hybrid_data, Methods::hybrid.resolve_framebuffer.width, Methods::hybrid.resolve_framebuffer.height);
            rgb8_t* cpu_rgb8_data = vec3_buffer_to_rgb8(cpu_data, Methods::cpu.cpu_result_framebuffer.width, Methods::cpu.cpu_result_framebuffer.height);

            // FIXME: Add scene name to file name...
            stbi_write_png("nvidia.png", Methods::nvidia.framebuffer.width, Methods::nvidia.framebuffer.height, 3, nvidia_rgb8_data, Methods::nvidia.framebuffer.width * 3);
            stbi_write_png("intel.png", Methods::intel.resolve_color_framebuffer.width, Methods::intel.resolve_color_framebuffer.height, 3, intel_rgb8_data, Methods::intel.resolve_color_framebuffer.width * 3);
            stbi_write_png("hybrid.png", Methods::hybrid.resolve_framebuffer.width, Methods::hybrid.resolve_framebuffer.height, 3, hybrid_rgb8_data, Methods::hybrid.resolve_framebuffer.width * 3);
            stbi_write_png("cpu.png", Methods::cpu.cpu_result_framebuffer.width, Methods::cpu.cpu_result_framebuffer.height, 3, cpu_rgb8_data, Methods::cpu.cpu_result_framebuffer.width * 3);

            free(nvidia_data);
            free(intel_data);
            free(hybrid_data);
            free(cpu_data);

            free(nvidia_rgb8_data);
            free(intel_rgb8_data);
            free(hybrid_rgb8_data);
            free(cpu_rgb8_data);

            takeScreenshot = false;
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        const char* pass_name = Methods::method_names[(int)current_rendering_method];

        //Methods::nvidia.render(ptex_vao, g_teapot_mesh->num_vertices, mvp, bg_color);
        if (has_KHR_debug)
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, pass_name);

        #ifndef __APPLE__
            profiler::push_span("method", 1);
        #endif

        switch (current_rendering_method)
        {
        case Methods::Methods::cpu:
            Methods::cpu.render(mesh_vaos[current_mesh], meshes[current_mesh]->num_vertices, ptexTextures[current_mesh], current_filter, mvp, bg_color);
            break;

        case Methods::Methods::nvidia:
            Methods::nvidia.render(mesh_vaos[current_mesh], meshes[current_mesh]->num_vertices, texturesGLData[current_mesh], mvp, bg_color);
            break;

        case Methods::Methods::intel:
            Methods::intel.render(mesh_vaos[current_mesh], meshes[current_mesh]->num_vertices, texturesGLData[current_mesh], mvp, bg_color);
            break;

        case Methods::Methods::hybrid:
            Methods::hybrid.render(mesh_vaos[current_mesh], meshes[current_mesh]->num_vertices, texturesGLData[current_mesh], mvp, bg_color);
            break;

        default:
            assert(false); break;
        }

        #ifndef __APPLE__
            profiler::pop_span(1); // pop "method"
        #endif

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

        case Methods::Methods::hybrid:
            color_output_framebuffer = Methods::hybrid.ms_framebuffer;
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

        #ifndef __APPLE__
            profiler::pop_span(0); // pop "Frame"
        #endif

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(imctx);

    current_filter->release();

    save_viewpoints_file(VIEWPOINTS_FILE);

    glfwTerminate();
    return EXIT_SUCCESS;
}