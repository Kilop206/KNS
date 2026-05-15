#pragma once

namespace kns {
	struct Link {

		// Node IDs of the two nodes connected by this link
		int from;
		int to;

		// Link properties

		// Delay in milliseconds
		double delay_ms;
		
		// Bandwidth in Mbps
		double bandwidth_mbps;

		// Loss probability (0.0 to 1.0)
		double loss_prob;

		// Returns the ID of the other node connected by this link
		int getOtherNode(int node) const;

		// Determines if a packet should be dropped based on the loss probability
		bool should_drop() const;
	};
}