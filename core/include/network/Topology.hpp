#pragma once

#include <memory>
#include <string>
#include <vector>

#include "network/Link.hpp"

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

    private:
        std::vector<LinkPtr> links_;
        std::vector<std::vector<LinkPtr>> adjacency_list_;
        std::string name_;
    };

}
