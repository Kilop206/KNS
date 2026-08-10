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

        Topology topology;

        for (auto& l : j["links"]) {
            Link link(
                l["from"],
                l["to"],
                l["bandwidth"],
                l["delay"],
                l["loss"]
            );
            
            topology.addLink(link);
        }

        topology.setName(j["name"].get<std::string>().c_str());

        return topology;
    }

}
