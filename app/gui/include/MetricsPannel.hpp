#pragma once

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "../../../core/include/engine/core/Stats.hpp"
#include "LatencyChart.hpp"

namespace interface {
    class MetricsPannel {
    public:
        void render(const kns::Stats& stats, const CircularBuffer& buffer);
    };
}
