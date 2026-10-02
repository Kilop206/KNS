#include "hub/TopologyHubClient.hpp"

#include <cctype>
#include <stdexcept>
#include <utility>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "network/TopologyLoader.hpp"

namespace kns::app::hub {

namespace {

bool validTopologyId(const std::string& id)
{
    if (id.empty() || id.size() > 128) {
        return false;
    }

    for (const unsigned char character : id) {
        if (!std::isalnum(character) && character != '-' && character != '_') {
            return false;
        }
    }

    return true;
}

} // namespace

TopologyHubClient::TopologyHubClient(TopologyHubClientConfig config)
    : config_(std::move(config))
{
    if (config_.base_url.empty()) {
        throw std::invalid_argument("Topology Hub base URL cannot be empty");
    }
    if (config_.connection_timeout_seconds <= 0 || config_.read_timeout_seconds <= 0) {
        throw std::invalid_argument("Topology Hub timeouts must be positive");
    }
    if (config_.max_response_bytes == 0) {
        throw std::invalid_argument("Topology Hub response limit must be positive");
    }
}

Topology TopologyHubClient::fetchPublicTopology(const std::string& topology_id) const
{
    if (!validTopologyId(topology_id)) {
        throw std::invalid_argument("Invalid Topology Hub topology ID");
    }

    httplib::Client client(config_.base_url);
    client.set_connection_timeout(config_.connection_timeout_seconds);
    client.set_read_timeout(config_.read_timeout_seconds);
    client.set_write_timeout(config_.connection_timeout_seconds);

    const auto endpoint = "/api/topologies/" + topology_id + "/download";
    const httplib::Headers headers{
        {"Accept", "application/json"},
        {"User-Agent", "KNS/1.0"}
    };

    const auto result = client.Get(endpoint, headers);
    if (!result) {
        throw std::runtime_error(
            "Could not connect to Topology Hub: " + httplib::to_string(result.error())
        );
    }
    if (result->status < 200 || result->status >= 300) {
        throw std::runtime_error(
            "Topology Hub returned HTTP " + std::to_string(result->status)
        );
    }
    if (result->body.size() > config_.max_response_bytes) {
        throw std::runtime_error("Topology Hub topology exceeds response size limit");
    }

    try {
        return TopologyLoader::fromJson(nlohmann::json::parse(result->body));
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error(
            std::string("Topology Hub returned invalid JSON: ") + error.what()
        );
    }
}

} // namespace kns::app::hub
