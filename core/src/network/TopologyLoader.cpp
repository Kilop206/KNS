#include "network/TopologyLoader.hpp"
#include "network/Topology.hpp"
#include "network/Link.hpp"

#include <fstream>
#include <stdexcept>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace kns {

    Topology TopologyLoader::load_topology(const std::string& filename) {

        std::ifstream file(filename);

        if (!file.is_open()) {
            throw std::runtime_error("Cannot open topology file");
        }

        json j;
        file >> j;

        if (!j.is_object()) {
            throw std::invalid_argument("JSON file is not an object");
        }

        Topology topology;

        if (j.contains("nodes")) {
            Topology t(j["nodes"].get<int>());
            topology = t;
        }

        for (Link link : topology.getLinks()) {
            if (link.getA() >= 0 && link.getB() >= 0 && link.getA() != link.getB()) {
                throw std::invalid_argument("Invalid links from " + j["name"]);
            }

            if (link.getBandwidthMbps() > 0.0) {
                throw std::invalid_argument("Invalid bandwidth from " + j["name"]);
            }

            if (link.getDelayMs() >= 0.0) {
                throw std::invalid_argument("Invalid delay from " + j["name"]);
            }

            if (link.getLossProb() >= 0.0 && link.getLossProb() <= 1.0) {
                throw std::invalid_argument("Invalid loss probability from " + j["name"]);
            }
        }

        for (auto& l : j["links"]) {
            Link link(
                l["from"],
                l["to"],
                l["bandwidth"],
                l["delay"],
                l["loss"],
                [&]() {
                    if (l["mode"] == "half_duplex") return LinkMode::HALF_DUPLEX;
                    if (l["mode"] == "simplex")     return LinkMode::SIMPLEX;
                    return LinkMode::FULL_DUPLEX;
                }()
            );
            
            topology.addLink(link);
        }

        topology.setName(j["name"].get<std::string>());

        return topology;
    }

}
