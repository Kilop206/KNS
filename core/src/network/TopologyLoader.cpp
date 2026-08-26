#include "network/TopologyLoader.hpp"
#include "network/Topology.hpp"
#include "network/Link.hpp"
#include "enums/LinkMode.hpp"

#include <fstream>
#include <stdexcept>
#include <iostream>
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

        if (!j.is_object()) {
            throw std::invalid_argument("Topology JSON is not an object: " + filename);
        }

        Topology topology(j.contains("nodes") ? j["nodes"].get<int>() : 0);

        if (!j.contains("links") || !j["links"].is_array()) {
            throw std::invalid_argument("Topology JSON is missing 'links' array: " + filename);
        }

        for (const auto& l : j["links"]) {
            if (!l.contains("from") || !l.contains("to") || 
                !l.contains("bandwidth") || !l.contains("delay") || !l.contains("loss")) {
                throw std::invalid_argument("Link is missing required fields in " + filename);
            }

            int from = l["from"].get<int>();
            int to = l["to"].get<int>();
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

            topology.addLink(Link(from, to, bandwidth, delay, loss, mode));
        }

        if (j.contains("name") && j["name"].is_string()) {
            topology.setName(j["name"].get<std::string>().c_str());
        }

        return topology;
    }
}
