#pragma once
#include "imgui.h"
#include <algorithm>

namespace gui {
struct CanvasView {
    ImVec2 pan{60, 60};
    float zoom = 1.0f;
    ImVec2 toScreen(ImVec2 world, ImVec2 origin) const {
        return {origin.x + pan.x + world.x * zoom, origin.y + pan.y + world.y * zoom};
    }
    ImVec2 toWorld(ImVec2 screen, ImVec2 origin) const {
        return {(screen.x-origin.x-pan.x)/zoom, (screen.y-origin.y-pan.y)/zoom};
    }
    void zoomAt(float requested, ImVec2 anchor, ImVec2 origin) {
        const auto world = toWorld(anchor, origin);
        zoom = std::clamp(requested, 0.1f, 2.5f);
        pan = {anchor.x-origin.x-world.x*zoom, anchor.y-origin.y-world.y*zoom};
    }
};
}
