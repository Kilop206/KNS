#pragma once

#include <cstdint>

namespace kns {

	// Forward declaration of the SimulationEngine class. This allows the Event class to reference the SimulationEngine class without needing to include its full definition, 
    // which can help reduce compilation dependencies and improve build times.
    class SimulationEngine;

    class Event {
    public:

		// Virtual destructor for the Event class. 
        // This allows for proper cleanup of derived event classes when they are deleted through a pointer to the base Event class, 
        // ensuring that any resources allocated by the derived classes are released correctly.
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