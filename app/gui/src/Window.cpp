#include "../include/Window.hpp"

namespace gui {

    GLFWwindow* Window::generate_window() {
        glfwInit();
        
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
        GLFWwindow* window = glfwCreateWindow(1280, 720, "KNS", NULL, NULL);
        if (window == nullptr) exit(EXIT_FAILURE);

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);
          
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGui::StyleColorsLight();

        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_WindowBg]      = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_ChildBg]       = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_PopupBg]       = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_DockingEmptyBg]= ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_Text]          = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);

        style.Colors[ImGuiCol_FrameBg]     = ImVec4(0.92f, 0.92f, 0.92f, 1.0f);
        style.Colors[ImGuiCol_TitleBg]     = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
        style.Colors[ImGuiCol_MenuBarBg]   = ImVec4(0.95f, 0.95f, 0.95f, 1.0f);
        style.Colors[ImGuiCol_Border]      = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        return window;
    }
}