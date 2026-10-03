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

httplib::Headers requestHeaders(
    const TopologyHubClientConfig& config,
    bool mutation = false
)
{
    httplib::Headers headers{
        {"Accept", "application/json"},
        {"User-Agent", "KNS/1.1"}
    };
    if (!config.bearer_token.empty()) {
        headers.emplace("Authorization", "Bearer " + config.bearer_token);
    }
    if (mutation) {
        headers.emplace("X-Hub-Request", "1");
    }
    return headers;
}

void configureClient(httplib::Client& client, const TopologyHubClientConfig& config)
{
    client.set_connection_timeout(config.connection_timeout_seconds);
    client.set_read_timeout(config.read_timeout_seconds);
    client.set_write_timeout(config.connection_timeout_seconds);
    client.set_payload_max_length(config.max_response_bytes);
}

std::runtime_error httpError(const std::string& operation, int status)
{
    if (status == 409) {
        return std::runtime_error(
            operation + " failed because the topology changed on the Hub; reload before saving"
        );
    }
    if (status == 401 || status == 403) {
        return std::runtime_error(operation + " is not authorized by Topology Hub");
    }
    if (status == 404) {
        return std::runtime_error(operation + " could not find an accessible topology");
    }
    return std::runtime_error(operation + " failed with HTTP " + std::to_string(status));
}

HubTopology parseDetail(
    const std::string& topology_id,
    const std::string& body
)
{
    try {
        const auto document = nlohmann::json::parse(body);
        const auto& summary = document.at("topology");
        HubTopology result;
        result.id = topology_id;
        result.title = summary.at("title").get<std::string>();
        result.description = summary.at("description").get<std::string>();
        result.visibility = summary.at("visibility").get<std::string>();
        result.version = summary.at("version").get<std::uint64_t>();
        result.topology = TopologyLoader::fromJson(document.at("graph"));
        return result;
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error(
            std::string("Topology Hub returned an invalid topology document: ") + error.what()
        );
    }
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
    configureClient(client, config_);

    const auto endpoint = "/api/topologies/" + topology_id + "/download";
    const auto result = client.Get(endpoint, requestHeaders(config_));
    if (!result) {
        throw std::runtime_error(
            "Could not connect to Topology Hub: " + httplib::to_string(result.error())
        );
    }
    if (result->status < 200 || result->status >= 300) {
        throw httpError("Topology download", result->status);
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

HubTopology TopologyHubClient::fetchTopology(const std::string& topology_id) const
{
    if (!validTopologyId(topology_id)) {
        throw std::invalid_argument("Invalid Topology Hub topology ID");
    }

    httplib::Client client(config_.base_url);
    configureClient(client, config_);
    const auto endpoint = "/api/topologies/" + topology_id;
    const auto result = client.Get(endpoint, requestHeaders(config_));
    if (!result) {
        throw std::runtime_error(
            "Could not connect to Topology Hub: " + httplib::to_string(result.error())
        );
    }
    if (result->status < 200 || result->status >= 300) {
        throw httpError("Topology load", result->status);
    }
    if (result->body.size() > config_.max_response_bytes) {
        throw std::runtime_error("Topology Hub response exceeds response size limit");
    }
    return parseDetail(topology_id, result->body);
}

HubTopology TopologyHubClient::saveTopology(const HubTopology& topology) const
{
    if (!validTopologyId(topology.id)) {
        throw std::invalid_argument("Invalid Topology Hub topology ID");
    }
    if (config_.bearer_token.empty()) {
        throw std::runtime_error("Saving to Topology Hub requires KNS_TOPOLOGY_HUB_TOKEN");
    }

    nlohmann::json body{
        {"title", topology.title},
        {"description", topology.description},
        {"visibility", topology.visibility},
        {"version", topology.version},
        {"graph", TopologyLoader::toJson(topology.topology)}
    };

    httplib::Client client(config_.base_url);
    configureClient(client, config_);
    const auto endpoint = "/api/topologies/" + topology.id;
    const auto result = client.Put(
        endpoint,
        requestHeaders(config_, true),
        body.dump(),
        "application/json"
    );
    if (!result) {
        throw std::runtime_error(
            "Could not connect to Topology Hub: " + httplib::to_string(result.error())
        );
    }
    if (result->status < 200 || result->status >= 300) {
        throw httpError("Topology save", result->status);
    }
    if (result->body.size() > config_.max_response_bytes) {
        throw std::runtime_error("Topology Hub response exceeds response size limit");
    }
    return parseDetail(topology.id, result->body);
}

} // namespace kns::app::hub
