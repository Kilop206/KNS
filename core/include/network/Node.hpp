#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace kns {

    class Link;

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

        /// Whether this node is considered active. A node that has been
        /// removed from the topology is marked inactive but its ID is not
        /// recycled (preserving referential integrity of in-flight packets).
        bool isActive() const noexcept { return active_; }
        void setActive(bool active) noexcept { active_ = active; }

    private:
        int id_;
        std::string label_;
        bool active_ = true;
    };

} // namespace kns
