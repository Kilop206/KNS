#include "engine/time/SimulationClock.hpp"

namespace kns {

    SimulationClock::SimulationClock()
        : current_time_(0.0)
    {
    }

    void SimulationClock::tick(double delta_time_) {
        current_time_ += delta_time_;
    }

    double SimulationClock::now() const {
        return current_time_;
    }

    void SimulationClock::setTime(double new_current_time_) {
        current_time_ = new_current_time_;
    }

}