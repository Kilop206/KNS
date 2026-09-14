#include "engine/core/SimulationClock.hpp"
#include <cmath>
#include <stdexcept>

namespace kns {

    SimulationClock::SimulationClock()
        : current_time_(0.0)
    {
    }

    void SimulationClock::tick(double delta_time_) {
        if (!std::isfinite(delta_time_) || delta_time_ < 0.0) {
            throw std::invalid_argument("Clock increment must be finite and non-negative");
        }
        setTime(current_time_ + delta_time_);
    }

    double SimulationClock::now() const {
        return current_time_;
    }

    void SimulationClock::setTime(double new_current_time_) {
        if (!std::isfinite(new_current_time_) || new_current_time_ < current_time_) {
            throw std::invalid_argument("Clock time must be finite and monotonic");
        }
        current_time_ = new_current_time_;
    }

}
