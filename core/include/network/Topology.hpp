#pragma once

#include "network/Link.hpp"

#include <vector>

namespace kns {
	class Topology {
	private:

		// Number of nodes in the topology
		int num_nodes;

		// Adjacency list representation of the topology. Each node has a list of outgoing links.
		std::vector<std::vector<Link>> adjacency_list;

	public:

		Topology() = default;

		// Constructor to initialize the topology with a given number of nodes
		Topology(int num_nodes);

		// Method to add a link to the topology
		void addLink(const Link& link);

		// Method to retrieve the list of links originating from a given node
		const std::vector<Link>& getLinksFromNode(int node_id) const;

		// Method to get the total number of nodes in the topology
		int size() const;

		std::vector<std::vector<Link>>& getLinks();

		void setGlobalLossProb(float value);
	};
}