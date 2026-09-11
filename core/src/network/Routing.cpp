#include <limits>
#include <array>
#include <vector>
#include <queue>
#include <cassert>
#include <functional>
#include <utility>

#include "network/Routing.hpp"
#include "network/Topology.hpp"
#include "network/Link.hpp"

namespace kns {

	namespace {
		constexpr std::array routingMetrics{
			std::pair{"delay", RoutingMetric::Delay},
			std::pair{"bandwidth", RoutingMetric::Bandwidth},
			std::pair{"hop-count", RoutingMetric::HopCount},
			std::pair{"delay-bandwidth", RoutingMetric::DelayBandwidth},
		};
	} // namespace

	std::string_view routingMetricName(RoutingMetric metric) noexcept {
		for (const auto& [name, candidate] : routingMetrics) {
			if (candidate == metric) {
				return name;
			}
		}

		return "unknown";
	}

	std::optional<RoutingMetric> parseRoutingMetric(
		std::string_view name
	) noexcept {
		for (const auto& [candidate_name, metric] : routingMetrics) {
			if (candidate_name == name) {
				return metric;
			}
		}

		return std::nullopt;
	}

	Routing::DijkstraResult Routing::buildDijkstra(const Topology& topology, int src,
	                                                 RoutingMetric metric) {
		int n = topology.size();
		assert(src >= 0 && src < n);

		const double inf = std::numeric_limits<double>::infinity();

		// For Bandwidth metric we maximise the minimum bandwidth, so we start with
		// 0.0 as "worst" and use a max-heap. For all other metrics we minimise cost
		// and start with +inf.
		const bool maximise = (metric == RoutingMetric::Bandwidth);

		std::vector<double> dist(n, maximise ? 0.0 : inf);
		std::vector<int> parent(n, -1);

		using QueueEntry = std::pair<double, int>;
		std::priority_queue<
			QueueEntry,
			std::vector<QueueEntry>,
			std::greater<>
		> pq;

		// Negating the bottleneck capacity lets the min-heap also service the
		// bandwidth maximisation case.
		const auto encode = [maximise](double value) {
			return maximise ? -value : value;
		};
		const auto decode = [maximise](double value) {
			return maximise ? -value : value;
		};
		const auto better = [maximise](double candidate, double current) {
			return maximise ? candidate > current : candidate < current;
		};

		dist[src] = maximise ? inf : 0.0;
		pq.push({encode(dist[src]), src});

		const auto linkCost = [metric, inf](const Link& link) -> double {
			switch (metric) {
				case RoutingMetric::Delay:
					return link.getDelayMs();
				case RoutingMetric::Bandwidth:
					return link.getBandwidthMbps();   // will be negated via encode()
				case RoutingMetric::HopCount:
					return 1.0;
				case RoutingMetric::DelayBandwidth:
					return (link.getBandwidthMbps() > 0.0)
					       ? link.getDelayMs() / link.getBandwidthMbps()
					       : inf;
				default:
					return link.getDelayMs();
			}
		};

		const auto combine = [metric](double current_dist, double edge_cost) -> double {
			if (metric == RoutingMetric::Bandwidth)
				return std::min(current_dist, edge_cost);   // bottleneck bandwidth
			return current_dist + edge_cost;
		};

		while (!pq.empty()) {
			auto [encoded, u] = pq.top();
			pq.pop();
			double currentDist = decode(encoded);

			if (!better(currentDist, dist[u]) && currentDist != dist[u]) {
				continue;
			}

			const auto& adjacency = topology.getLinksFromNode(u);

			for (const auto& link : adjacency) {
				if (!link || !link->isUp()) {
					continue;
				}

				int v = link->getOtherNode(u);
				if (!link->allowsTransmission(u, v)) {
					continue;
				}
				double newDist = combine(dist[u], linkCost(*link));

				if (better(newDist, dist[v])) {
					dist[v] = newDist;
					parent[v] = u;
					pq.push({encode(newDist), v});
				}
			}
		}

		return {dist, parent};
	}

	std::vector<Routing::RoutingEntry> Routing::buildRoutingTable(const Topology& topology, int src,
	                                                               RoutingMetric metric) {
		int n = topology.size();
		assert(src >= 0 && src < n);

		DijkstraResult result = buildDijkstra(topology, src, metric);

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

			if (d == src) {
				table.push_back(entry);
				continue;
			}

			const bool unreachable = (metric == RoutingMetric::Bandwidth)
			                             ? (dist[d] <= 0.0)
			                             : (dist[d] == inf);
			if (unreachable) {
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
