#include <iostream>
#include "core/SimulationClock.h"
#include "core/SimulationEngine.h"
#include "core/PrintEvent.h"

int main() {

    core::SimulationEngine engine;

    engine.schedule(new core::PrintEvent(10, "Evento B"));
    engine.schedule(new core::PrintEvent(5, "Evento A"));
    engine.schedule(new core::PrintEvent(12, "Evento C"));

    engine.run();

    return 0;
}