#include "network/TopologyLoader.hpp"
#include "network/Topology.hpp"
#include "network/Link.hpp"
#include "enums/LinkMode.hpp"

#include <fstream>
#include <stdexcept>
#include <iostream>
#include <limits>
#include <set>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace kns {

    Topology TopologyLoader::load_topology(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open topology file: " + filename);
        }

        json j;
        file >> j;

        return fromJson(j);
    }

    Topology TopologyLoader::fromJson(const json& j) {
        const std::string filename = "topology snapshot";

        if (!j.is_object()) {
            throw std::invalid_argument("Topology JSON is not an object: " + filename);
        }

        if (j.contains("schema_version") && j.at("schema_version") != "1.0") {
            throw std::invalid_argument("Unsupported topology schema_version");
        }
        const bool typed = j.contains("nodes") && j.at("nodes").is_array();
        int count = 0;
        if (typed) {
            if (j.at("nodes").size() > 4096) throw std::invalid_argument("Too many nodes (maximum 4096)");
            count = static_cast<int>(j.at("nodes").size());
        } else if (j.contains("nodes")) {
            const auto& value = j.at("nodes");
            if (!value.is_number_integer() || value < 0 || value > 4096) {
                throw std::invalid_argument("nodes must be an array or an integer in [0, 4096]");
            }
            count = value.get<int>();
        }
        Topology topology(count);
        if (typed) {
            std::set<int> ids;
            for (const auto& node : j.at("nodes")) {
                if (!node.is_object() || !node.contains("id") || !node.at("id").is_number_integer() ||
                    node.at("id") < 0 || node.at("id") >= count) {
                    throw std::invalid_argument("Node IDs must be unique integers in [0, nodes.size())");
                }
                const int id = node.at("id").get<int>();
                if (!ids.insert(id).second) throw std::invalid_argument("Duplicate node ID");
                DeviceInfo device;
                device.type = deviceTypeFromString(node.value("type", "unknown"));
                device.external_id = node.value("external_id", "");
                device.addresses = node.value("addresses", std::vector<std::string>{});
                device.mac = node.value("mac", "");
                device.evidence = node.value("evidence", "");
                topology.setNodeDeviceInfo(id, std::move(device));
                topology.setNodeLabel(id, node.value("label", ""));
                if (!node.value("active", true)) topology.removeNode(id);
            }
        }

        if (!j.contains("links") || !j["links"].is_array()) {
            throw std::invalid_argument("Topology JSON is missing 'links' array: " + filename);
        }

        for (const auto& l : j["links"]) {
            if (!l.is_object()) throw std::invalid_argument("Link must be an object");
            if (!l.contains("from") || !l.contains("to") || 
                !l.contains("bandwidth") || !l.contains("delay") || !l.contains("loss")) {
                throw std::invalid_argument("Link is missing required fields in " + filename);
            }

            if (!l.at("from").is_number_integer() || !l.at("to").is_number_integer() ||
                l.at("from") < 0 || l.at("from") >= 4096 || l.at("to") < 0 || l.at("to") >= 4096) {
                throw std::invalid_argument("Link endpoints must be integers in [0, 4096)");
            }
            int from = l["from"].get<int>();
            int to = l["to"].get<int>();
            if (typed && (from >= count || to >= count)) throw std::invalid_argument("Link references missing node");
            double bandwidth = l["bandwidth"].get<double>();
            double delay = l["delay"].get<double>();
            double loss = l["loss"].get<double>();

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
                throw std::invalid_argument("Loss probability must be in range [0, 1] in " + filename);
            }

            LinkMode mode = LinkMode::FULL_DUPLEX;
            if (l.contains("mode")) {
                std::string mode_str = l["mode"].get<std::string>();
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
            if (l.contains("queue_capacity")) {
                const auto& value = l.at("queue_capacity");
                if (!value.is_number_integer() || value <= 0 ||
                    value > std::numeric_limits<int>::max()) {
                    throw std::invalid_argument("Link queue_capacity must be a positive integer in " + filename);
                }
                queue_capacity = value.get<int>();
            }
            auto link = topology.addLinkPtr(from, to, bandwidth, delay, loss, mode, queue_capacity);
            link->setUp(l.value("up", true));
            link->setDiscoveryMetadata(l.value("inferred", false), l.value("evidence", ""));
        }

        if (j.contains("name") && j["name"].is_string()) {
            topology.setName(j["name"].get<std::string>().c_str());
        }

        return topology;
    }

    json TopologyLoader::toJson(const Topology& topology) {
        json result = {{"schema_version", "1.0"}, {"name", topology.getName()},
                       {"nodes", json::array()}, {"links", json::array()}};
        for (int id = 0; id < topology.size(); ++id) {
            const auto& node = *topology.getNode(id);
            const auto& device = node.getDeviceInfo();
            result["nodes"].push_back({{"id", id}, {"active", node.isActive()},
                {"label", node.getLabel()}, {"type", toString(device.type)},
                {"external_id", device.external_id}, {"addresses", device.addresses},
                {"mac", device.mac}, {"evidence", device.evidence}});
        }
        for (const auto& link : topology.getLinks()) {
            const auto mode = link->getMode() == LinkMode::SIMPLEX ? "simplex" :
                (link->getMode() == LinkMode::HALF_DUPLEX ? "half_duplex" : "full_duplex");
            result["links"].push_back({{"from", link->getA()}, {"to", link->getB()},
                {"bandwidth", link->getBandwidthMbps()}, {"delay", link->getDelayMs()},
                {"loss", link->getLossProb()}, {"mode", mode}, {"up", link->isUp()},
                {"queue_capacity", link->getQueueCapacity()},
                {"inferred", link->isInferred()}, {"evidence", link->getEvidence()}});
        }
        return result;
    }
}
