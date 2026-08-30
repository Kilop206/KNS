#pragma once

#include <memory>
#include <string>
#include <vector>

#include "network/Link.hpp"
#include "network/Node.hpp"
#include "network/Interface.hpp"

namespace kns {

    class Topology {
    public:
        using LinkPtr = std::shared_ptr<Link>;

        explicit Topology(int nodes = 0);

        void addLink(const Link& link);
        LinkPtr addLinkPtr(
            int a,
            int b,
            double bandwidth_mbps,
            double delay_ms,
            double loss_prob = 0.0,
            LinkMode mode = LinkMode::FULL_DUPLEX
        );

        void addLink(
            int a,
            int b,
            double bandwidth_mbps,
            double delay_ms,
            double loss_prob = 0.0,
            LinkMode mode = LinkMode::FULL_DUPLEX
        );

        std::vector<LinkPtr>& getLinksFromNode(int);
        const std::vector<LinkPtr>& getLinksFromNode(int) const;

        const std::vector<LinkPtr>& getLinks() const noexcept { return links_; }

        int size() const noexcept;
        void setGlobalLossProb(double value);

        const std::string& getName() const noexcept;
        void setName(std::string name);

        // Node/link management for GUI
        int addNode();
        bool removeNode(int id);

        bool removeLink(int a, int b);
        bool setLinkUp(int a, int b, bool up);

        /// Access the Node object for a given id. Returns nullptr if out of range.
        const Node* getNode(int id) const noexcept;
        Node* getNode(int id) noexcept;

        /// All interface objects (one per link endpoint on a node).
        const std::vector<Interface>& getInterfaces() const noexcept { return interfaces_; }

    private:
        std::vector<LinkPtr> links_;
        std::vector<std::vector<LinkPtr>> adjacency_list_;
        std::vector<Node> nodes_;           ///< One Node per adjacency_list_ slot.
        std::vector<Interface> interfaces_; ///< Interface objects for every link endpoint.
        std::string name_;
    };

}
