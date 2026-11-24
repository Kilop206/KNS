#include "CableGeometry.hpp"
#include "network/Topology.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace gui {
    ImVec2 CablePath::point(float progress) const noexcept {
        const float scaled = std::clamp(progress, 0.0f, 1.0f) * SEGMENTS;
        const auto segment = std::min(static_cast<int>(scaled), SEGMENTS - 1);
        const float t = scaled - segment;
        const auto a = points[segment], b = points[segment + 1];
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    }

    float CablePath::distance(ImVec2 position) const noexcept {
        float nearest = std::numeric_limits<float>::infinity();
        for (int i = 0; i < SEGMENTS; ++i) {
            const auto a = points[i], b = points[i + 1];
            const float dx = b.x - a.x, dy = b.y - a.y, length = dx * dx + dy * dy;
            const float t = length > 0
                ? std::clamp(((position.x - a.x) * dx + (position.y - a.y) * dy) / length,
                             0.0f, 1.0f) : 0;
            nearest = std::min(nearest,
                std::hypot(position.x - a.x - dx * t, position.y - a.y - dy * t));
        }
        return nearest;
    }

    CableLanes layoutCableLanes(const kns::Topology& topology) {
        std::map<std::pair<int, int>, std::vector<kns::Topology::LinkPtr>> bundles;
        for (const auto& link : topology.getLinks())
            bundles[std::minmax(link->getA(), link->getB())].push_back(link);
        CableLanes result;
        for (auto& [endpoints, links] : bundles) {
            std::sort(links.begin(), links.end(), [](const auto& a, const auto& b) {
                return a->getId() < b->getId();
            });
            for (std::size_t i = 0; i < links.size(); ++i) {
                const auto& link = *links[i];
                const float lane = static_cast<float>(i) - (links.size() - 1) * 0.5f;
                result.emplace(link.getId(), CableLane{link.getA(), link.getB(),
                    link.getA() == endpoints.first ? lane : -lane});
            }
        }
        return result;
    }

    CablePaths buildCablePaths(const CableLanes& lanes,
                              const std::vector<std::pair<float, float>>& positions, float zoom) {
        CablePaths result;
        const float spacing = std::clamp(28 * zoom, 16.0f, 70.0f);
        for (const auto& [id, lane] : lanes) {
            if (lane.from < 0 || lane.to < 0 ||
                static_cast<std::size_t>(std::max(lane.from, lane.to)) >= positions.size())
                continue;
            const auto [ax, ay] = positions[lane.from];
            const auto [bx, by] = positions[lane.to];
            const float dx = bx - ax, dy = by - ay, length = std::hypot(dx, dy);
            const float offset = 2 * spacing * lane.offset;
            const ImVec2 control{(ax + bx) * 0.5f - (length > 0 ? dy / length : 0) * offset,
                                  (ay + by) * 0.5f + (length > 0 ? dx / length : 1) * offset};
            CablePath path{lane.from, lane.to, {}};
            for (int i = 0; i <= CablePath::SEGMENTS; ++i) {
                const float t = static_cast<float>(i) / CablePath::SEGMENTS, u = 1 - t;
                path.points[i] = {u * u * ax + 2 * u * t * control.x + t * t * bx,
                                  u * u * ay + 2 * u * t * control.y + t * t * by};
            }
            result.emplace(id, path);
        }
        return result;
    }
}
