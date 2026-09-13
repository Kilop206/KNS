#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "../../../core/include/engine/core/SimulationEngine.hpp"

namespace gui {

    class TranslationService;

    class TcpCongestionPanel {
    public:
        void render(
            const kns::SimulationEngine& engine,
            TranslationService& translations
        );
    };

}
