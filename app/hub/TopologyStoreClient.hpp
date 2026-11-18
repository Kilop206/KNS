#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "network/Topology.hpp"

namespace kns::app::hub {

struct TopologyStoreClientConfig {
    std::string base_url = "http://localhost:3001";
    std::string bearer_token;
    int connection_timeout_seconds = 5;
    int read_timeout_seconds = 15;
    std::size_t max_response_bytes = 1024 * 1024;
};

struct StoredTopology {
    std::string id;
    std::string title;
    std::string description;
    std::string visibility;
    std::uint64_t version = 0;
    Topology topology;
};

class TopologyStoreClient {
public:
    explicit TopologyStoreClient(TopologyStoreClientConfig config = {});

    [[nodiscard]] Topology fetchPublicTopology(const std::string& topology_id) const;
    [[nodiscard]] StoredTopology fetchTopology(const std::string& topology_id) const;
    [[nodiscard]] StoredTopology createTopology(const StoredTopology& topology) const;
    [[nodiscard]] StoredTopology saveTopology(const StoredTopology& topology) const;

private:
    TopologyStoreClientConfig config_;
};

} // namespace kns::app::hub
