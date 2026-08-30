#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace kns {

    class Link;

    /// Represents one network interface on a node — the attachment point
    /// between a Node and a Link.
    ///
    /// Currently a thin value type.  Future work (congestion control, per-
    /// interface queues, MTU, per-port stats) will add fields here rather
    /// than to the Link or Topology, keeping responsibilities separated.
    class Interface {
    public:
        /// @param node_id   ID of the owning node.
        /// @param link_id   ID of the Link this interface connects to.
        /// @param name      Optional label (e.g. "eth0", "lo").
        Interface(int node_id, std::uint64_t link_id, std::string name = "") noexcept
            : node_id_(node_id), link_id_(link_id), name_(std::move(name)) {}

        int getNodeId() const noexcept { return node_id_; }
        std::uint64_t getLinkId() const noexcept { return link_id_; }

        const std::string& getName() const noexcept { return name_; }
        void setName(std::string name) { name_ = std::move(name); }

        /// MTU in bytes. 0 means "use the link default / unlimited".
        std::size_t getMtu() const noexcept { return mtu_; }
        void setMtu(std::size_t mtu) noexcept { mtu_ = mtu; }

        /// Whether this interface is administratively up.
        bool isUp() const noexcept { return up_; }
        void setUp(bool up) noexcept { up_ = up; }

    private:
        int node_id_;
        std::uint64_t link_id_;
        std::string name_;
        std::size_t mtu_ = 0;
        bool up_ = true;
    };

} // namespace kns
