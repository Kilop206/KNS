#include <filesystem>

#include "../include/Window.hpp"
#include "../include/ThemeManager.hpp"

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

        ImFontConfig config;
        config.SizePixels = 16.0f;

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        const std::filesystem::path assetDir = KNS_ASSET_DIR;
        const std::string regularFont = (assetDir / "fonts" / "Orbit" / "Orbit-Regular.ttf").string();
        const std::string boldFont    = (assetDir / "fonts" / "Orbit Bold" / "Orbit Bold.ttf").string();
        const std::string monoFont    = (assetDir / "fonts" / "JetBrains Mono" / "JetBrainsMono-Regular.ttf").string();

        gui::ThemeManager::apply(
            regularFont.c_str(),
            boldFont.c_str(),
            monoFont.c_str()
        );

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 130");

        return window;
    }
}