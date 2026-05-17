#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <cstdlib>

namespace interface {
    class Window {
        public:
            GLFWwindow* generate_window();
    };
}