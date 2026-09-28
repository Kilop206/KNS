#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <utility>
#include <optional>

#include "network/DeviceType.hpp"

namespace kns {

    class Link;

    struct NodePosition {
        double x = 0.0;
        double y = 0.0;
        bool operator==(const NodePosition&) const = default;
    };

    struct DeviceInfo {
        DeviceType type = DeviceType::Unknown;
        std::string external_id;
        std::vector<std::string> addresses;
        std::string mac;
        std::string evidence;

        bool operator==(const DeviceInfo&) const = default;
    };

    /// Represents a network node with a stable integer ID, an optional human-
    /// readable label, and the list of Link pointers attached to it.
    ///
    /// In the current architecture the Topology owns the canonical adjacency
    /// list; Node is a lightweight façade that lets other subsystems (GUI,
    /// congestion experiments, stats) refer to a node by value rather than by
    /// a raw index.
    class Node {
    public:
        /// Construct a node with a given ID. The ID must match the node's
        /// position in Topology::adjacency_list_.
        explicit Node(int id, std::string label = "") noexcept
            : id_(id), label_(std::move(label)) {}

        /// Stable numeric identity (index in the Topology adjacency list).
        int getId() const noexcept { return id_; }

        /// Optional human-readable name for display in the GUI / logs.
        const std::string& getLabel() const noexcept { return label_; }
        void setLabel(std::string label) { label_ = std::move(label); }

        const DeviceInfo& getDeviceInfo() const noexcept { return device_; }
        void setDeviceInfo(DeviceInfo device) { device_ = std::move(device); }

        const std::optional<NodePosition>& getPosition() const noexcept { return position_; }
        void setPosition(NodePosition position) noexcept { position_ = position; }

        /// Whether this node is considered active. A node that has been
        /// removed from the topology is marked inactive but its ID is not
        /// recycled (preserving referential integrity of in-flight packets).
        bool isActive() const noexcept { return active_; }
        void setActive(bool active) noexcept { active_ = active; }

    private:
        int id_;
        std::string label_;
        bool active_ = true;
        DeviceInfo device_;
        std::optional<NodePosition> position_;
    };

} // namespace kns
