#include "core/SimulationClock.h"

namespace gns::core {

    SimulationClock::SimulationClock(double time_step)
        : current_time_(0.0),
        dt_(time_step)
    {
    }

    void SimulationClock::tick() {
        current_time_ += dt_;
    }

    double SimulationClock::now() const {
        return current_time_;
    }

    double SimulationClock::timeStep() const {
        return dt_;
    }

}