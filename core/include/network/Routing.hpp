#pragma once

#include <vector>

namespace kns {

class Topology;

	class Routing {
		public:
			struct DijkstraResult {
				std::vector<double> dist;
				std::vector<int> parent;
			};

			struct RoutingEntry {
				int destination = -1;
				int next_hop = -1;
				double distance = 0.0;
			};

			DijkstraResult buildDijkstra(const Topology& topology, int src);
			std::vector<RoutingEntry> buildRoutingTable(const Topology& topology, int src);
	};

} // namespace kns