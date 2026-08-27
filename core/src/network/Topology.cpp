#include "network/Topology.hpp"

#include <algorithm>
#include <stdexcept>

namespace kns {

    Topology::Topology(int nodes) {
        if (nodes > 0) {
            adjacency_list_.resize(static_cast<std::size_t>(nodes));
        }
    }

    void Topology::addLink(const Link& link) {
        auto ptr = std::make_shared<Link>(link);

        const int a = ptr->getA();
        const int b = ptr->getB();
        const int max_node = std::max(a, b);

        if (max_node >= static_cast<int>(adjacency_list_.size())) {
            adjacency_list_.resize(static_cast<std::size_t>(max_node + 1));
        }

        links_.push_back(ptr);
        adjacency_list_[static_cast<std::size_t>(a)].push_back(ptr);
        adjacency_list_[static_cast<std::size_t>(b)].push_back(ptr);
    }

    Topology::LinkPtr Topology::addLinkPtr(
        int a,
        int b,
        double bandwidth_mbps,
        double delay_ms,
        double loss_prob,
        LinkMode mode
    ) {
        auto ptr = std::make_shared<Link>(a, b, bandwidth_mbps, delay_ms, loss_prob, mode);

        const int max_node = std::max(a, b);
        if (max_node >= static_cast<int>(adjacency_list_.size())) {
            adjacency_list_.resize(static_cast<std::size_t>(max_node + 1));
        }

        links_.push_back(ptr);
        adjacency_list_[static_cast<std::size_t>(a)].push_back(ptr);
        adjacency_list_[static_cast<std::size_t>(b)].push_back(ptr);

        return ptr;
    }

    void Topology::addLink(
        int a,
        int b,
        double bandwidth_mbps,
        double delay_ms,
        double loss_prob,
        LinkMode mode
    ) {
        addLink(Link(a, b, bandwidth_mbps, delay_ms, loss_prob, mode));
    }



    std::vector<Topology::LinkPtr>& Topology::getLinksFromNode(int node) {
        if (node < 0 || static_cast<std::size_t>(node) >= adjacency_list_.size()) {
            throw std::out_of_range("Node index out of bounds");
        }
        return adjacency_list_[node];
    }
    
    const std::vector<Topology::LinkPtr>& Topology::getLinksFromNode(int node) const {
        if (node < 0 || static_cast<std::size_t>(node) >= adjacency_list_.size()) {
            static const std::vector<LinkPtr> empty;
            return empty;
        }
        return adjacency_list_[node];
    }

    int Topology::size() const noexcept {
        return static_cast<int>(adjacency_list_.size());
    }

    void Topology::setGlobalLossProb(double value) {
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
        adjacency_list_.push_back({});
        return static_cast<int>(adjacency_list_.size()) - 1;
    }

    bool Topology::removeNode(int id) {
        if (id < 0 || static_cast<std::size_t>(id) >= adjacency_list_.size()) {
            return false;
        }

        // Remove all links that reference this node
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
        }

        // Clear adjacency entry for the node (leave hole to preserve indices)
        adjacency_list_[static_cast<std::size_t>(id)].clear();
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

}
