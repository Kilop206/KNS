#pragma once

#include <cstdint>

namespace kns {

    class SimulationEngine;

    class Event {
    public:

        virtual ~Event() = default;

		// Pure virtual function that must be implemented by derived event classes.
        virtual void execute(kns::SimulationEngine& engine) = 0;

		// Getter methods for the timestamp and ID of the event.
        double getTimestamp() const noexcept;

        std::uint64_t getId() const noexcept;

    protected:
		// Protected constructor for the Event class.
        Event(double timestamp);
        
		// The timestamp of the event, which indicates when the event should be executed in the simulation.
        double timestamp_;

    private:

		// Private member variables for the Event class.

		// The unique ID of the event, which can be used to identify and manage events within the simulation engine.
        std::uint64_t id_;

		// Static member variable to keep track of the next available ID for events.
        static std::uint64_t nextId_;
    };
}