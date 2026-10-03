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
        /// Save graph configuration atomically. Paths are UTF-8; runtime state is excluded.
        static void save_topology(const Topology& topology, const std::string& filename);
        static Topology fromJson(const nlohmann::json& document);
        static nlohmann::json toJson(const Topology& topology);
	};

}
