#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <utility>
#include <optional>

#include "network/DeviceType.hpp"
#include "network/services/NetworkService.hpp"

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
        void setDeviceInfo(DeviceInfo device) {
            if (static_cast<std::size_t>(device.type) >= deviceTypeNames.size())
                throw std::invalid_argument("Invalid device type");
            validateDeviceServices(device.type, services_);
            if (device_.type != device.type) {
                ++services_revision_;
                for (auto& [name, revision] : service_revisions_) revision = services_revision_;
            }
            device_ = std::move(device);
        }

        const std::optional<NodePosition>& getPosition() const noexcept { return position_; }
        void setPosition(NodePosition position) noexcept { position_ = position; }

        const std::vector<NetworkService>& getServices() const noexcept { return services_; }
        bool hasServiceConfiguration() const noexcept { return services_configured_; }
        std::uint64_t getServicesRevision() const noexcept { return services_revision_; }
        std::uint64_t getServiceRevision(const std::string& name) const noexcept {
            const auto found = service_revisions_.find(name);
            return found == service_revisions_.end() ? 0 : found->second;
        }
        void setServices(std::vector<NetworkService> services) {
            validateDeviceServices(device_.type, services);
            if (services != services_) ++services_revision_;
            std::map<std::string, std::uint64_t> revisions;
            for (const auto& service : services) {
                bool unchanged = false;
                for (const auto& previous : services_) if (previous == service) { unchanged = true; break; }
                revisions[service.name] = unchanged ? getServiceRevision(service.name) : services_revision_;
            }
            services_ = std::move(services);
            service_revisions_ = std::move(revisions);
            services_configured_ = true;
        }

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
        std::vector<NetworkService> services_;
        bool services_configured_ = false;
        std::uint64_t services_revision_ = 0;
        std::map<std::string, std::uint64_t> service_revisions_;
    };

} // namespace kns
