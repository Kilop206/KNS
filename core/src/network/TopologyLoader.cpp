#include "network/TopologyLoader.hpp"
#include "network/Topology.hpp"
#include "network/Link.hpp"
#include "enums/LinkMode.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace kns {

    namespace {
        bool integerInRange(const json& value, int minimum, int maximum) {
            if (!value.is_number_integer()) {
                return false;
            }
            // JSON can hold unsigned values above INT64_MAX. Keep their original
            // width and signedness so a wrapped threshold cannot pass validation.
            if (value.is_number_unsigned()) {
                const auto number = value.get<std::uint64_t>();
                return std::cmp_greater_equal(number, minimum) &&
                       std::cmp_less_equal(number, maximum);
            }
            const auto number = value.get<std::int64_t>();
            return minimum <= number && number <= maximum;
        }

        std::map<int, int> denseNodeIndices(const json& nodes) {
            std::map<int, int> indices;
            for (const auto& node : nodes) {
                if (!node.is_object() || !node.contains("id") ||
                    !integerInRange(node.at("id"), 0, std::numeric_limits<int>::max())) {
                    throw std::invalid_argument("Node IDs must be nonnegative 32-bit integers");
                }
                if (!indices.emplace(node.at("id").get<int>(), 0).second) {
                    throw std::invalid_argument("Duplicate node ID");
                }
            }

            // Routing tables use dense indices. Assign them in wire-ID order so
            // reordering the node array leaves the simulator's indices stable.
            int next_index = 0;
            for (auto& entry : indices) {
                entry.second = next_index++;
            }
            return indices;
        }
    } // namespace

    Topology TopologyLoader::load_topology(const std::string& filename) {
        std::ifstream file(std::filesystem::path(std::u8string(filename.begin(), filename.end())),
                           std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open topology file: " + filename);
        }

        json j;
        file >> j;

        return fromJson(j);
    }

    Topology TopologyLoader::fromJson(const json& document) {
        const std::string filename = "topology snapshot";

        if (!document.is_object()) {
            throw std::invalid_argument("Topology JSON is not an object: " + filename);
        }

        if (document.contains("schema_version") && document.at("schema_version") != "1.0") {
            throw std::invalid_argument("Unsupported topology schema_version");
        }
        const auto nodes = document.find("nodes");
        const bool has_typed_nodes = nodes != document.end() && nodes->is_array();
        int count = 0;
        if (has_typed_nodes) {
            if (nodes->size() > Topology::MAX_NODES) {
                throw std::invalid_argument("Too many nodes (maximum " +
                                            std::to_string(Topology::MAX_NODES) + ")");
            }
            count = static_cast<int>(nodes->size());
        } else if (nodes != document.end()) {
            if (!integerInRange(*nodes, 0, Topology::MAX_NODES)) {
                throw std::invalid_argument("nodes must be an array or an integer in [0, " +
                                            std::to_string(Topology::MAX_NODES) + "]");
            }
            count = nodes->get<int>();
        }
        const auto node_indices = has_typed_nodes ? denseNodeIndices(*nodes) : std::map<int, int>{};
        Topology topology(count);
        if (has_typed_nodes) {
            for (const auto& node : *nodes) {
                const int id = node_indices.at(node.at("id").get<int>());
                DeviceInfo device;
                device.type = deviceTypeFromString(node.value("type", "unknown"));
                device.external_id = node.value("external_id", "");
                device.addresses = node.value("addresses", std::vector<std::string>{});
                device.mac = node.value("mac", "");
                device.evidence = node.value("evidence", "");
                topology.setNodeDeviceInfo(id, std::move(device));
                topology.setNodeLabel(id, node.value("label", ""));
                if (node.contains("services")) {
                    const auto& entries = node.at("services");
                    if (!entries.is_array() || entries.size() > 32)
                        throw std::invalid_argument(
                            "services must be an array with at most 32 entries");
                    std::vector<NetworkService> services;
                    for (const auto& entry : entries) {
                        NetworkService service;
                        service.name = entry.at("name").get<std::string>();
                        service.kind = parseServiceKind(entry.at("kind").get<std::string>());
                        const auto& port = entry.at("port");
                        if (!integerInRange(port, 1, 65535))
                            throw std::invalid_argument(
                                "Service port must be an integer in [1, 65535]");
                        service.port = port.get<int>();
                        service.enabled = entry.value("enabled", true);
                        service.delay_ms = entry.value("delay_ms", 0.0);
                        if (entry.contains("pages")) {
                            if (!entry.at("pages").is_object() || entry.at("pages").size() > 128)
                                throw std::invalid_argument(
                                    "pages must be an object with at most 128 entries");
                            for (const auto& [path, page] : entry.at("pages").items()) {
                                const auto& status = page.at("status");
                                if (!integerInRange(status, 200, 599))
                                    throw std::invalid_argument(
                                        "HTTP status must be an integer in [200, 599]");
                                service.pages.emplace(path,
                                                      HttpPage{status.get<int>(),
                                                               page.at("body").get<std::string>()});
                            }
                        }
                        if (entry.contains("records")) {
                            if (!entry.at("records").is_object() ||
                                entry.at("records").size() > 128)
                                throw std::invalid_argument(
                                    "records must be an object with at most 128 entries");
                            for (const auto& [name, address] : entry.at("records").items()) {
                                if (!service.records
                                         .emplace(normalizeDnsName(name),
                                                  address.get<std::string>())
                                         .second)
                                    throw std::invalid_argument(
                                        "Duplicate DNS name after normalization");
                            }
                        }
                        services.push_back(std::move(service));
                    }
                    topology.setNodeServices(id, std::move(services));
                }
                if (node.contains("position")) {
                    const auto& position = node.at("position");
                    if (!position.is_object() || !position.contains("x") ||
                        !position.contains("y") || !position.at("x").is_number() ||
                        !position.at("y").is_number()) {
                        throw std::invalid_argument(
                            "Node position must contain numeric x and y coordinates");
                    }
                    topology.setNodePosition(
                        id, {position.at("x").get<double>(), position.at("y").get<double>()});
                }
                if (!node.value("active", true))
                    topology.removeNode(id);
            }
        }

        if (!document.contains("links") || !document["links"].is_array()) {
            throw std::invalid_argument("Topology JSON is missing 'links' array: " + filename);
        }

        for (const auto& link_document : document["links"]) {
            if (!link_document.is_object())
                throw std::invalid_argument("Link must be an object");
            if (!link_document.contains("from") || !link_document.contains("to") ||
                !link_document.contains("bandwidth") || !link_document.contains("delay") ||
                !link_document.contains("loss")) {
                throw std::invalid_argument("Link is missing required fields in " + filename);
            }

            if (!integerInRange(link_document.at("from"), 0, std::numeric_limits<int>::max()) ||
                !integerInRange(link_document.at("to"), 0, std::numeric_limits<int>::max())) {
                throw std::invalid_argument("Link endpoints must be nonnegative 32-bit integers");
            }
            int from = link_document["from"].get<int>();
            int to = link_document["to"].get<int>();
            if (has_typed_nodes) {
                const auto source = node_indices.find(from);
                const auto target = node_indices.find(to);
                if (source == node_indices.end() || target == node_indices.end())
                    throw std::invalid_argument("Link references missing node");
                from = source->second;
                to = target->second;
            } else if (from >= Topology::MAX_NODES || to >= Topology::MAX_NODES) {
                throw std::invalid_argument("Link endpoints must be integers in [0, " +
                                            std::to_string(Topology::MAX_NODES) + ")");
            }
            double bandwidth = link_document["bandwidth"].get<double>();
            double delay = link_document["delay"].get<double>();
            double loss = link_document["loss"].get<double>();

            if (from < 0 || to < 0 || from == to) {
                throw std::invalid_argument("Invalid node indices in link in " + filename);
            }
            if (bandwidth <= 0.0) {
                throw std::invalid_argument("Bandwidth must be positive in " + filename);
            }
            if (delay < 0.0) {
                throw std::invalid_argument("Delay cannot be negative in " + filename);
            }
            if (loss < 0.0 || loss > 1.0) {
                throw std::invalid_argument("Loss probability must be in range [0, 1] in " +
                                            filename);
            }

            LinkMode mode = LinkMode::FULL_DUPLEX;
            if (link_document.contains("mode")) {
                std::string mode_str = link_document["mode"].get<std::string>();
                if (mode_str == "full_duplex") {
                    mode = LinkMode::FULL_DUPLEX;
                } else if (mode_str == "half_duplex") {
                    mode = LinkMode::HALF_DUPLEX;
                } else if (mode_str == "simplex") {
                    mode = LinkMode::SIMPLEX;
                } else {
                    throw std::invalid_argument("Unknown link mode in " + filename);
                }
            }

            int queue_capacity = 32;
            if (link_document.contains("queue_capacity")) {
                const auto& value = link_document.at("queue_capacity");
                if (!integerInRange(value, 1, std::numeric_limits<int>::max())) {
                    throw std::invalid_argument(
                        "Link queue_capacity must be a positive integer in " + filename);
                }
                queue_capacity = value.get<int>();
            }
            auto link = topology.addLinkPtr(from, to, bandwidth, delay, loss, mode, queue_capacity);

            const std::string queue_policy = link_document.value("queue_policy", "drop_tail");
            if (queue_policy == "red") {
                if (!link_document.contains("red_min_threshold") ||
                    !link_document.contains("red_max_threshold") ||
                    !link_document.contains("red_max_drop_probability")) {
                    throw std::invalid_argument(
                        "RED queue policy requires red_min_threshold, red_max_threshold, and red_max_drop_probability");
                }
                const auto& min_threshold = link_document.at("red_min_threshold");
                const auto& max_threshold = link_document.at("red_max_threshold");
                const auto& max_probability = link_document.at("red_max_drop_probability");
                const auto queue_weight = link_document.value("red_queue_weight", json(1.0));
                if (!integerInRange(min_threshold, 0, queue_capacity) ||
                    !integerInRange(max_threshold, 1, queue_capacity) ||
                    !max_probability.is_number() || !queue_weight.is_number() ||
                    min_threshold.get<int>() >= max_threshold.get<int>()) {
                    throw std::invalid_argument("Invalid RED queue configuration");
                }
                link->configureRed(min_threshold.get<int>(), max_threshold.get<int>(),
                                   max_probability.get<double>(), queue_weight.get<double>());
            } else if (queue_policy != "drop_tail") {
                throw std::invalid_argument("Unknown queue_policy in " + filename);
            }

            link->setUp(link_document.value("up", true));
            link->setDiscoveryMetadata(link_document.value("inferred", false),
                                       link_document.value("evidence", ""));
        }

        if (document.contains("name") && document["name"].is_string()) {
            topology.setName(document["name"].get<std::string>().c_str());
        }

        return topology;
    }

    json TopologyLoader::toJson(const Topology& topology) {
        json result = {{"schema_version", "1.0"},
                       {"name", topology.getName()},
                       {"nodes", json::array()},
                       {"links", json::array()}};
        for (int id = 0; id < topology.size(); ++id) {
            const auto& node = *topology.getNode(id);
            const auto& device = node.getDeviceInfo();
            result["nodes"].push_back({{"id", id},
                                       {"active", node.isActive()},
                                       {"label", node.getLabel()},
                                       {"type", toString(device.type)},
                                       {"external_id", device.external_id},
                                       {"addresses", device.addresses},
                                       {"mac", device.mac},
                                       {"evidence", device.evidence}});
            if (node.getPosition()) {
                result["nodes"].back()["position"] = {{"x", node.getPosition()->x},
                                                      {"y", node.getPosition()->y}};
            }
            if (node.hasServiceConfiguration()) {
                auto& services = result["nodes"].back()["services"] = json::array();
                for (const auto& service : node.getServices()) {
                    json entry = {{"name", service.name},
                                  {"kind", serviceKindName(service.kind)},
                                  {"port", service.port},
                                  {"enabled", service.enabled},
                                  {"delay_ms", service.delay_ms}};
                    if (service.kind == ServiceKind::Http) {
                        entry["pages"] = json::object();
                        for (const auto& [path, page] : service.pages)
                            entry["pages"][path] = {{"status", page.status}, {"body", page.body}};
                    } else
                        entry["records"] = service.records;
                    services.push_back(std::move(entry));
                }
            }
        }
        for (const auto& link : topology.getLinks()) {
            const auto mode =
                link->getMode() == LinkMode::SIMPLEX
                    ? "simplex"
                    : (link->getMode() == LinkMode::HALF_DUPLEX ? "half_duplex" : "full_duplex");
            json entry = {{"from", link->getA()},
                          {"to", link->getB()},
                          {"bandwidth", link->getBandwidthMbps()},
                          {"delay", link->getDelayMs()},
                          {"loss", link->getLossProb()},
                          {"mode", mode},
                          {"up", link->isUp()},
                          {"queue_capacity", link->getQueueCapacity()},
                          {"queue_policy", link->getQueueDiscipline() == Link::QueueDiscipline::RED
                                               ? "red"
                                               : "drop_tail"},
                          {"inferred", link->isInferred()},
                          {"evidence", link->getEvidence()}};
            if (link->getQueueDiscipline() == Link::QueueDiscipline::RED) {
                entry["red_min_threshold"] = link->getRedMinThreshold();
                entry["red_max_threshold"] = link->getRedMaxThreshold();
                entry["red_max_drop_probability"] = link->getRedMaxDropProbability();
                entry["red_queue_weight"] = link->getRedQueueWeight();
            }
            result["links"].push_back(std::move(entry));
        }
        return result;
    }
}
