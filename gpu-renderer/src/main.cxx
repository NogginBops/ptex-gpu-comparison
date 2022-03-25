#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>

void GLFWErrorCallback(int error_code, const char* description)
{
    printf("%d: %s\n", error_code, description);
}

void GLFWFrambufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
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

    GLFWwindow* window = glfwCreateWindow(200, 200, "Test title", NULL, NULL);
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

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.0f, 0.5f, 0.0f,
    };

    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    const char* vertexShaderSource = "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "void main()\n"
        "{\n"
        "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
        "}\0";

    const char* fragmentShaderSource = "#version 330 core\n"
        "out vec4 FragColor;\n"
        "void main()\n"
        "{\n"
        "   FragColor = vec4(1.0, 0.5, 0.2, 1.0);\n"
        "}\0";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    GLuint program = glCreateProgram();

    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    glUseProgram(program);

    while (glfwWindowShouldClose(window) == false)
    {
        glfwPollEvents();

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return EXIT_SUCCESS;
}