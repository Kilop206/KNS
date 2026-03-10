#include <iostream>
#include "core/PrintEvent.h"

namespace core {
		PrintEvent::PrintEvent(int time, const std::string& message)
		: Event(time), message(message) {}

		std::uint64_t PrintEvent::getTimestamp() const noexcept {
			return static_cast<int>(Event::getTimestamp());
		}

	void PrintEvent::execute(SimulationEngine& engine) {
		std::cout << "Time: " << getTimestamp() << " - " << message << std::endl;
	}
	
}