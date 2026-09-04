#include "network/Topology.hpp"

#include <algorithm>
#include <stdexcept>

namespace kns {

    Topology::Topology(int nodes) {
        if (nodes < 0) {
            throw std::invalid_argument("Node count cannot be negative");
        }
        if (nodes > 0) {
            adjacency_list_.resize(static_cast<std::size_t>(nodes));
            nodes_.reserve(static_cast<std::size_t>(nodes));
            for (int i = 0; i < nodes; ++i) {
                nodes_.emplace_back(i);
            }
        }
    }

    Topology::LinkPtr Topology::addLinkPtr(
        int a,
        int b,
        double bandwidth_mbps,
        double delay_ms,
        double loss_prob,
        LinkMode mode
    ) {
        if (a < 0 || b < 0) {
            throw std::invalid_argument("Node indices cannot be negative");
        }
        if (a == b) {
            throw std::invalid_argument("Self-loops are not supported");
        }
        if (bandwidth_mbps <= 0.0) {
            throw std::invalid_argument("Bandwidth must be positive");
        }
        if (delay_ms < 0.0) {
            throw std::invalid_argument("Delay cannot be negative");
        }
        if (loss_prob < 0.0 || loss_prob > 1.0) {
            throw std::invalid_argument("Loss probability must be between 0.0 and 1.0");
        }

        auto ptr = std::make_shared<Link>(a, b, bandwidth_mbps, delay_ms, loss_prob, mode);

        const int max_node = std::max(a, b);
        if (max_node >= static_cast<int>(adjacency_list_.size())) {
            const std::size_t old_size = adjacency_list_.size();
            adjacency_list_.resize(static_cast<std::size_t>(max_node + 1));
            for (std::size_t i = old_size; i <= static_cast<std::size_t>(max_node); ++i) {
                nodes_.emplace_back(static_cast<int>(i));
            }
        }

        links_.push_back(ptr);
        adjacency_list_[static_cast<std::size_t>(a)].push_back(ptr);
        adjacency_list_[static_cast<std::size_t>(b)].push_back(ptr);

        interfaces_.emplace_back(a, ptr->getId());
        interfaces_.emplace_back(b, ptr->getId());

        return ptr;
    }

    void Topology::addLink(const Link& link) {
        addLinkPtr(
            link.getA(),
            link.getB(),
            link.getBandwidthMbps(),
            link.getDelayMs(),
            link.getLossProb(),
            link.getMode()
        );
    }

    void Topology::addLink(
        int a,
        int b,
        double bandwidth_mbps,
        double delay_ms,
        double loss_prob,
        LinkMode mode
    ) {
        addLinkPtr(a, b, bandwidth_mbps, delay_ms, loss_prob, mode);
    }



    std::vector<Topology::LinkPtr>& Topology::getLinksFromNode(int node) {
        if (node < 0 || static_cast<std::size_t>(node) >= adjacency_list_.size()) {
            throw std::out_of_range("Node index out of bounds");
        }
        return adjacency_list_[node];
    }
    
    const std::vector<Topology::LinkPtr>& Topology::getLinksFromNode(int node) const {
        if (node < 0 || static_cast<std::size_t>(node) >= adjacency_list_.size()) {
            throw std::out_of_range("Node index out of bounds");
        }
        return adjacency_list_[node];
    }

    int Topology::size() const noexcept {
        return static_cast<int>(adjacency_list_.size());
    }

    void Topology::setGlobalLossProb(double value) {
        if (value < 0.0 || value > 1.0) {
            throw std::invalid_argument("Loss probability must be between 0.0 and 1.0");
        }
        for (auto& link : links_) {
            link->setLossProb(value);
        }
    }

    const std::string& Topology::getName() const noexcept {
        return name_;
    }

    void Topology::setName(std::string name) {
        name_ = std::move(name);
    }

    int Topology::addNode() {
        const int id = static_cast<int>(adjacency_list_.size());
        adjacency_list_.push_back({});
        nodes_.emplace_back(id);
        return id;
    }

    bool Topology::removeNode(int id) {
        if (id < 0 || static_cast<std::size_t>(id) >= adjacency_list_.size()) {
            return false;
        }

        // Remove all links that reference this node (and their interfaces)
        std::vector<LinkPtr> to_remove;
        for (auto& link : links_) {
            if (link->getA() == id || link->getB() == id) {
                to_remove.push_back(link);
            }
        }

        for (auto& link : to_remove) {
            const int a = link->getA();
            const int b = link->getB();

            // remove from links_
            links_.erase(std::remove(links_.begin(), links_.end(), link), links_.end());

            // remove from adjacency lists
            if (a >= 0 && static_cast<std::size_t>(a) < adjacency_list_.size()) {
                auto& vec = adjacency_list_[static_cast<std::size_t>(a)];
                vec.erase(std::remove(vec.begin(), vec.end(), link), vec.end());
            }
            if (b >= 0 && static_cast<std::size_t>(b) < adjacency_list_.size()) {
                auto& vec = adjacency_list_[static_cast<std::size_t>(b)];
                vec.erase(std::remove(vec.begin(), vec.end(), link), vec.end());
            }

            // Remove interfaces for this link's endpoints
            const std::uint64_t lid = link->getId();
            interfaces_.erase(
                std::remove_if(interfaces_.begin(), interfaces_.end(),
                               [lid](const Interface& iface) { return iface.getLinkId() == lid; }),
                interfaces_.end());
        }

        // Clear adjacency entry for the node (leave hole to preserve indices)
        adjacency_list_[static_cast<std::size_t>(id)].clear();

        // Mark the Node inactive (preserve index slot for referential integrity)
        if (static_cast<std::size_t>(id) < nodes_.size()) {
            nodes_[static_cast<std::size_t>(id)].setActive(false);
        }
        return true;
    }

    bool Topology::removeLink(int a, int b) {
        for (auto it = links_.begin(); it != links_.end(); ++it) {
            const auto& link = *it;
            if ((link->getA() == a && link->getB() == b) || (link->getA() == b && link->getB() == a)) {
                // remove from adjacency lists
                if (link->getA() >= 0 && static_cast<std::size_t>(link->getA()) < adjacency_list_.size()) {
                    auto& vec = adjacency_list_[static_cast<std::size_t>(link->getA())];
                    vec.erase(std::remove(vec.begin(), vec.end(), link), vec.end());
                }
                if (link->getB() >= 0 && static_cast<std::size_t>(link->getB()) < adjacency_list_.size()) {
                    auto& vec = adjacency_list_[static_cast<std::size_t>(link->getB())];
                    vec.erase(std::remove(vec.begin(), vec.end(), link), vec.end());
                }

                // Remove the two Interface objects for this link
                const std::uint64_t lid = link->getId();
                interfaces_.erase(
                    std::remove_if(interfaces_.begin(), interfaces_.end(),
                                   [lid](const Interface& iface) { return iface.getLinkId() == lid; }),
                    interfaces_.end());

                links_.erase(it);
                return true;
            }
        }
        return false;
    }

    bool Topology::setLinkUp(int a, int b, bool up) {
        for (auto& link : links_) {
            if ((link->getA() == a && link->getB() == b) || (link->getA() == b && link->getB() == a)) {
                link->setUp(up);
                return true;
            }
        }
        return false;
    }

    const Node* Topology::getNode(int id) const noexcept {
        if (id < 0 || static_cast<std::size_t>(id) >= nodes_.size()) return nullptr;
        return &nodes_[static_cast<std::size_t>(id)];
    }

    Node* Topology::getNode(int id) noexcept {
        if (id < 0 || static_cast<std::size_t>(id) >= nodes_.size()) return nullptr;
        return &nodes_[static_cast<std::size_t>(id)];
    }

}
