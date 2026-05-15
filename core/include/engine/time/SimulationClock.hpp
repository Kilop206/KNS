#pragma once

namespace kns {
	class SimulationClock {
	private:

		// The current time in the simulation, in seconds.
		double current_time_;
	public:

		// Initializes the simulation clock to start at time 0.
		explicit SimulationClock();

		// Advances the simulation clock by the specified amount of time (in seconds).
		void tick(double delta_time_);

		// Returns the current time in the simulation, in seconds.
		double now() const;

		// Sets the current time in the simulation to the specified value (in seconds).
		void setTime(double new_current_time_);
	};
}