#include "network/Topology.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

namespace kns {

bool Topology::synchronizeFrom(const Topology& snapshot)
{
    if (this == &snapshot) return false;
    const auto initial_revision = getRoutingRevision();
    std::map<std::string, int> identities;
    for (const auto& node : nodes_) {
        if (node.isActive() && !node.getDeviceInfo().external_id.empty()) {
            identities.emplace(node.getDeviceInfo().external_id, node.getId());
        }
    }

    std::vector<int> mapping(static_cast<std::size_t>(snapshot.size()), -1);
    std::set<int> retained_nodes;
    int next_id = size();
    for (int source = 0; source < snapshot.size(); ++source) {
        const auto& incoming = *snapshot.getNode(source);
        if (!incoming.isActive()) continue;
        int target = -1;
        const auto& identity = incoming.getDeviceInfo().external_id;
        if (!identity.empty()) {
            const auto existing = identities.find(identity);
            if (existing != identities.end()) target = existing->second;
        } else {
            const auto* existing = getNode(source);
            if (existing && existing->isActive() && existing->getDeviceInfo().external_id.empty()) target = source;
        }
        if (target == -1) target = next_id++;
        mapping[static_cast<std::size_t>(source)] = target;
        if (!retained_nodes.insert(target).second) throw std::invalid_argument("Duplicate snapshot identity");
    }
    // Bound accumulated tombstones as well as a single snapshot. No mutation has
    // happened if this limit is reached; loading afresh starts a new ID space.
    if (next_id > 4096) throw std::invalid_argument("Live topology reached 4096 node slots; reload to start a new simulation");

    struct PlannedLink {
        LinkPtr incoming;
        LinkPtr existing;
        int from;
        int to;
    };
    std::vector<PlannedLink> plan;
    std::set<std::uint64_t> retained_links;
    for (const auto& incoming : snapshot.getLinks()) {
        const int from = mapping.at(static_cast<std::size_t>(incoming->getA()));
        const int to = mapping.at(static_cast<std::size_t>(incoming->getB()));
        if (from < 0 || to < 0) throw std::invalid_argument("Snapshot links to an inactive node");
        LinkPtr match;
        for (const auto& existing : links_) {
            const bool endpoints = (existing->getA() == from && existing->getB() == to) ||
                (incoming->getMode() != LinkMode::SIMPLEX && existing->getA() == to && existing->getB() == from);
            if (endpoints && existing->getMode() == incoming->getMode() &&
                existing->getQueueCapacity() == incoming->getQueueCapacity() &&
                !retained_links.contains(existing->getId())) {
                match = existing;
                retained_links.insert(existing->getId());
                break;
            }
        }
        plan.push_back({incoming, match, from, to});
    }

    while (size() < next_id) addNode();
    for (int source = 0; source < snapshot.size(); ++source) {
        const int target = mapping[static_cast<std::size_t>(source)];
        if (target == -1) continue;
        const auto& incoming = *snapshot.getNode(source);
        setNodeLabel(target, incoming.getLabel());
        setNodeDeviceInfo(target, incoming.getDeviceInfo());
    }
    std::vector<std::uint64_t> removed_links;
    for (const auto& link : links_) {
        if (!retained_links.contains(link->getId())) removed_links.push_back(link->getId());
    }
    for (auto id : removed_links) removeLinkById(id);
    for (int id = 0; id < size(); ++id) {
        if (getNode(id)->isActive() && !retained_nodes.contains(id)) removeNode(id);
    }
    for (const auto& item : plan) {
        const auto& incoming = item.incoming;
        auto link = item.existing;
        if (!link) {
            link = addLinkPtr(item.from, item.to, incoming->getBandwidthMbps(), incoming->getDelayMs(),
                incoming->getLossProb(), incoming->getMode(), static_cast<int>(incoming->getQueueCapacity()));
        } else {
            if (link->getBandwidthMbps() != incoming->getBandwidthMbps()) link->setBandwidthMbps(incoming->getBandwidthMbps());
            if (link->getDelayMs() != incoming->getDelayMs()) link->setDelayMs(incoming->getDelayMs());
            if (link->getLossProb() != incoming->getLossProb()) link->setLossProb(incoming->getLossProb());
        }
        if (link->isUp() != incoming->isUp()) link->setUp(incoming->isUp());
        link->setDiscoveryMetadata(incoming->isInferred(), incoming->getEvidence());
    }
    setName(snapshot.getName());
    return getRoutingRevision() != initial_revision;
}

} // namespace kns
