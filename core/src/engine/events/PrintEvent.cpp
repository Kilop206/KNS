#include <iostream>
#include "engine/events/PrintEvent.hpp"

namespace kns {
		PrintEvent::PrintEvent(int time, const std::string& message)
		: Event(time), message(message) {}

		std::uint64_t kns::PrintEvent::getTimestamp() const noexcept {
			return static_cast<int>(kns::Event::getTimestamp());
		}

	void kns::PrintEvent::execute(SimulationEngine& engine) {
		std::cout << "Time: " << getTimestamp() << " - " << message << std::endl;
	}
	
}