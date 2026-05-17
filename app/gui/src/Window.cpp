#include "../include/Window.hpp"

namespace interface {
    GLFWwindow* Window::generate_window() {
        glfwInit();
        GLFWwindow* window = glfwCreateWindow(1280, 720, "KNS", NULL, NULL);
        glfwMakeContextCurrent(window);
           
        if (window == nullptr) exit(EXIT_FAILURE);

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
          
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        return window;
    }
}