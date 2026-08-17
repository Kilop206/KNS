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

    std::vector<Topology::LinkPtr>& Topology::getLinksFromNode(int node)
    {
        return adjacency_list_[node];
    }

    const std::vector<Topology::LinkPtr>& Topology::getLinksFromNode(int node) const
    {
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
}