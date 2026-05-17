#include <limits>
#include <vector>
#include <queue>
#include <cassert>
#include <functional>
#include <utility>

#include "network/Routing.hpp"
#include "network/Topology.hpp"
#include "network/Link.hpp"

namespace kns {

	Routing::DijkstraResult Routing::buildDijkstra(const Topology& topology, int src) {
		int n = topology.size();
		assert(src >= 0 && src < n);

		const double inf = std::numeric_limits<double>::infinity();

		std::vector<double> dist(n, inf);
		std::vector<int> parent(n, -1);

		std::priority_queue<
			std::pair<double, int>,
			std::vector<std::pair<double, int>>,
			std::greater<>
		> pq;

		dist[src] = 0.0;
		pq.push({0.0, src});

		while (!pq.empty()) {
			auto [currentDist, u] = pq.top();
			pq.pop();

			if (currentDist > dist[u]) {
				continue;
			}

			const auto& adjacency = topology.getLinksFromNode(u);

			for (const Link& link : adjacency) {
				int v = link.getOtherNode(u);
				double newDist = dist[u] + link.delay_ms;

				if (newDist < dist[v]) {
					dist[v] = newDist;
					parent[v] = u;
					pq.push({newDist, v});
				}
			}
		}

		return {dist, parent};
	}

	std::vector<Routing::RoutingEntry> Routing::buildRoutingTable(const Topology& topology, int src) {
		int n = topology.size();
		assert(src >= 0 && src < n);

		DijkstraResult result = buildDijkstra(topology, src);

		const auto& parent = result.parent;
		const auto& dist = result.dist;

		const double inf = std::numeric_limits<double>::infinity();

		std::vector<RoutingEntry> table;
		table.reserve(n);

		for (int d = 0; d < n; ++d) {
			RoutingEntry entry;
			entry.destination = d;
			entry.distance = dist[d];
			entry.next_hop = -1;

			if (d == src || dist[d] == inf) {
				table.push_back(entry);
				continue;
			}

			int current = d;

			while (parent[current] != src) {
				current = parent[current];

				if (current == -1) {
					entry.next_hop = -1;
					table.push_back(entry);
					goto next_destination;
				}
			}

			entry.next_hop = current;
			table.push_back(entry);

		next_destination:
			continue;
		}

		return table;
	}

}