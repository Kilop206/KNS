#pragma once

#include "imgui.h"
#include <array>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace kns {
    class Topology;
}

namespace gui {
    struct CableLane {
        int from, to;
        float offset;
    };

    /// One shared polyline for drawing, hit testing, arrows and packet animation.
    struct CablePath {
        static constexpr int SEGMENTS = 24;
        int from, to;
        std::array<ImVec2, SEGMENTS + 1> points;
        ImVec2 point(float progress) const noexcept;
        float distance(ImVec2 position) const noexcept;
    };

    using CableLanes = std::map<std::uint64_t, CableLane>;
    using CablePaths = std::map<std::uint64_t, CablePath>;

    /// Group unordered endpoint pairs, then order lanes by stable link identity.
    /// Reversed endpoint declarations occupy distinct lanes on the same bundle.
    CableLanes layoutCableLanes(const kns::Topology& topology);
    CablePaths buildCablePaths(const CableLanes& lanes,
                              const std::vector<std::pair<float, float>>& positions, float zoom);
}
