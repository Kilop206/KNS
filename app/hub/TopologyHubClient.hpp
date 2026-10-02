#pragma once

#include <cstddef>
#include <string>

#include "network/Topology.hpp"

namespace kns::app::hub {

struct TopologyHubClientConfig {
    std::string base_url = "http://localhost:3001";
    int connection_timeout_seconds = 5;
    int read_timeout_seconds = 15;
    std::size_t max_response_bytes = 1024 * 1024;
};

class TopologyHubClient {
public:
    explicit TopologyHubClient(TopologyHubClientConfig config = {});

    [[nodiscard]] Topology fetchPublicTopology(const std::string& topology_id) const;

private:
    TopologyHubClientConfig config_;
};

} // namespace kns::app::hub
