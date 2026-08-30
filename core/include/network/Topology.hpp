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

    /// Dynamic topology change policy (issue #95)
    /// ============================================
    /// The simulation allows topology mutations during a run (GUI edits,
    /// LinkFailureEvent, deleteNode/deleteLink). The following invariants
    /// define how in-flight packets and scheduled events are affected:
    ///
    /// 1. Packets already in transit (in packets_in_transit and scheduled as
    ///    PacketReceivedEvent) CONTINUE to their destination. They carry a
    ///    stable link_id that was valid when sendPacket() was called; on
    ///    arrival the release step simply becomes a no-op if the link has
    ///    since been removed.
    ///
    /// 2. A packet arriving at a node that has been removed is processed
    ///    normally until the TCP session lookup. hasTCPSession() guards
    ///    against use-after-free; unrecognised packets are silently dropped.
    ///
    /// 3. A packet mid-route (not yet at destination) whose next hop
    ///    no longer exists is counted as lost (sendPacketThroughTopology
    ///    returns false, stats.packets_lost incremented).
    ///
    /// 4. TCP sessions are NOT automatically torn down when a topology
    ///    mutation makes their path unreachable. The session stays open until
    ///    the SYN handshake timeout exhausts retries and declares it failed.
    ///
    /// 5. Routing tables are rebuilt synchronously on every topology mutation
    ///    (rebuildRoutingTables) so that newly scheduled packets use the
    ///    updated topology. Packets already scheduled use their pre-computed
    ///    route (hop-by-hop via getNextHop at arrival time).

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
