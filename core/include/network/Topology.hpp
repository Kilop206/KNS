#pragma once

#include <memory>
#include <vector>

#include "network/Link.hpp"

namespace kns {

	class Topology {
		public:
			using LinkPtr = std::shared_ptr<Link>;

			explicit Topology(int nodes = 0);

			void addLink(const Link& link);
			void addLink(
				int a,
				int b,
				double bandwidth_mbps,
				double delay_ms,
				double loss_prob = 0.0,
				LinkMode mode = LinkMode::FULL_DUPLEX
			);

			std::vector<LinkPtr>& getLinksFromNode(int);

			const std::vector<LinkPtr>& getLinksFromNode(int) const;

			int size() const noexcept;

			void setGlobalLossProb(double value);

			const char* getName() const noexcept;

			void setName(const char* name) noexcept;

		private:
			std::vector<LinkPtr> links_;
			std::vector<std::vector<LinkPtr>> adjacency_list_;
			const char* name;
	};

}