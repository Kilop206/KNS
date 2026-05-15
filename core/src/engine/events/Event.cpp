#include "engine/events/Event.hpp"
#include "engine/core/SimulationEngine.hpp"

namespace kns {

	// Initialize the static member variable nextId_ to 0.
	std::uint64_t Event::nextId_ = 0;

	// Default implementation of the execute method for the Event class.
	void Event::execute(SimulationEngine& engine) {
	}

	// Constructor for the Event class that initializes the timestamp and assigns a unique ID to each event.
	Event::Event(double timestamp)
		: timestamp_(timestamp)
		, id_(nextId_++)
	{
	}

	// Getter method for the timestamp of the event.
	double Event::getTimestamp() const noexcept {
		return timestamp_;
	}

	// Getter method for the unique ID of the event.
	std::uint64_t Event::getId() const noexcept {
		return id_;
	}

}