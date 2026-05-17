#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

#include "network/Topology.hpp"

// Forward declaration of the json type from nlohmann/json.hpp
using json = nlohmann::json;

namespace kns {

	class TopologyLoader {

	public:
		// Loads a topology from a JSON file. The JSON file should have the following format:
		/*
		{
		"nodes": 4,
			"links": [
				{"from": 0, "to": 1, "delay_ms": 10, "bandwidth_mbps": 100, "loss_prob": 0.01},
				{"from": 1, "to": 2, "delay_ms": 20, "bandwidth_mbps": 50, "loss_prob": 0.02},
				{"from": 2, "to": 3, "delay_ms": 30, "bandwidth_mbps": 25, "loss_prob": 0.03},
				{"from": 3, "to": 0, "delay_ms": 40, "bandwidth_mbps": 10, "loss_prob": 0.04}
			]
		}
		*/
		static Topology load_topology(const std::string& filename);
	};

}