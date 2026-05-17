#include "network/TopologyLoader.hpp"
#include "network/Topology.hpp"
#include "network/Link.hpp"

#include <fstream>
#include <stdexcept>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace kns {

	// Loads a topology from a JSON file.
    Topology TopologyLoader::load_topology(const std::string& filename) {

		// Open the file
        std::ifstream file(filename);

		// Check if the file was opened successfully
        if (!file.is_open()) {
			// If not, throw an exception
            throw std::runtime_error("Cannot open topology file");
        }

		// Parse the JSON file
        json j;
        file >> j;

        int nodes = j["nodes"];

		// Create a topology with the specified number of nodes
        Topology topology(nodes);

		// Add links to the topology
        for (auto& l : j["links"]) {
            Link link;

            link.from = l["from"];
            link.to = l["to"];
            link.delay_ms = l["delay"];
            link.bandwidth_mbps = l["bandwidth"];
            link.loss_prob = l["loss"];
            topology.addLink(link);
        }

        return topology;
    }

}
