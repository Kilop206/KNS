#pragma once

#include "core/EventQueue.h"
#include "core/SimulationClock.h"

namespace core {

    class Event;

    class SimulationEngine {
    public:
        SimulationEngine();
        ~SimulationEngine();

        void run();
        void schedule(Event* event);

    private:
        core::SimulationClock clock;
        core::EventQueue eventQueue;
    };

}