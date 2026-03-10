#include "core/Event.h"
#include "core/SimulationEngine.h"

namespace core {

	std::uint64_t core::Event::nextId_ = 0;

	void Event::execute(core::SimulationEngine& engine) {
	}

	Event::Event(std::uint64_t timestamp)
		: timestamp_(timestamp)
		, id_(nextId_++)
	{
	}

	std::uint64_t Event::getTimestamp() const noexcept {
		return timestamp_;
	}

	std::uint64_t Event::getId() const noexcept {
		return id_;
	}

}
