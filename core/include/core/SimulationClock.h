#pragma once

namespace gns::core {

    class SimulationClock {
    public:
        explicit SimulationClock(double time_step);

        void tick();
        double now() const;
        double timeStep() const;

    private:
        double current_time_;
        double dt_;
    };

}