#pragma once
#include "imgui.h"
#include "network/DeviceType.hpp"

namespace gui {
const char* deviceDisplayName(kns::DeviceType type);
ImU32 deviceColor(kns::DeviceType type);
// Vector silhouettes remain crisp at every canvas zoom and need no image assets.
void drawDeviceIcon(ImDrawList* draw, kns::DeviceType type, ImVec2 center, float size);
}
