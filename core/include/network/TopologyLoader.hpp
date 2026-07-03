#pragma once

#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

#include "network/Topology.hpp"

using json = nlohmann::json;

namespace kns {

	class TopologyLoader {

	public:
		static Topology load_topology(const std::string& filename);
	};

}