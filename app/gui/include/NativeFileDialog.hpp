#pragma once

#include <optional>
#include <string>

struct GLFWwindow;

namespace gui {
// Returns no path on cancellation; throws on a platform/dialog error.
std::optional<std::string> chooseTopologyFile(GLFWwindow* owner, bool save,
    const std::string& title);
}
