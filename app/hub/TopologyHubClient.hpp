#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "network/Topology.hpp"

namespace kns::app::hub {

struct TopologyHubClientConfig {
    std::string base_url = "http://localhost:3001";
    std::string bearer_token;
    int connection_timeout_seconds = 5;
    int read_timeout_seconds = 15;
    std::size_t max_response_bytes = 1024 * 1024;
};

struct HubTopology {
    std::string id;
    std::string title;
    std::string description;
    std::string visibility;
    std::uint64_t version = 0;
    Topology topology;
};

class TopologyHubClient {
public:
    explicit TopologyHubClient(TopologyHubClientConfig config = {});

    [[nodiscard]] Topology fetchPublicTopology(const std::string& topology_id) const;
    [[nodiscard]] HubTopology fetchTopology(const std::string& topology_id) const;
    [[nodiscard]] HubTopology saveTopology(const HubTopology& topology) const;

private:
    TopologyHubClientConfig config_;
};

} // namespace kns::app::hub
