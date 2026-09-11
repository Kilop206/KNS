#pragma once

#include <cstdint>
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

        /// Returns a reference to the list of links connected to the given node.
        /// Throws std::out_of_range if node < 0 or node >= size().
        std::vector<LinkPtr>& getLinksFromNode(int node);

        /// Returns a const reference to the list of links connected to the given node.
        /// Throws std::out_of_range if node < 0 or node >= size().
        const std::vector<LinkPtr>& getLinksFromNode(int node) const;

        const std::vector<LinkPtr>& getLinks() const noexcept { return links_; }

        int size() const noexcept;

        /// Monotonically increasing version of all routing-relevant state.
        /// Copies share this counter, so Link mutations remain observable by
        /// every engine that references the topology.
        std::uint64_t getRoutingRevision() const noexcept;

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
    /// 5. Every routing-relevant topology or Link mutation increments a shared
    ///    revision. SimulationEngine APIs rebuild synchronously, while direct
    ///    mutations through getTopology() are rebuilt on the next getNextHop()
    ///    or getRoutingTable() call. Consequently, newly forwarded packets
    ///    never use a stale route. Packets already scheduled continue hop by
    ///    hop and consult getNextHop() again when they arrive at a router.

    // Node/link management for GUI
        int addNode();
        bool removeNode(int id);

        /// Remove the only link between two endpoints. Returns false when the
        /// pair is missing or ambiguous; use removeLinkById for parallel links.
        bool removeLink(int a, int b);

        /// Remove one specific link by its stable identity.
        bool removeLinkById(std::uint64_t link_id);

        /// Change the state of the only link between two endpoints. Returns
        /// false when the pair is missing or ambiguous.
        bool setLinkUp(int a, int b, bool up);

        /// Change the state of one specific link by its stable identity.
        bool setLinkUpById(std::uint64_t link_id, bool up);

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
        std::shared_ptr<std::uint64_t> routing_revision_;

        void markRoutingChanged() noexcept;
    };

}
