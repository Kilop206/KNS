#include "core/SimulationEngine.h"
#include "core/Event.h"
#include <memory>

namespace core {

    SimulationEngine::SimulationEngine() = default;

    SimulationEngine::~SimulationEngine() = default;

}

void core::SimulationEngine::run() {
    while (eventQueue.hasEvents()) {
        auto event = eventQueue.next();
        clock.tick(event->getTimestamp());
        event->execute(*this);
    }
}

void core::SimulationEngine::schedule(Event* event) {
    eventQueue.schedule(std::unique_ptr<Event>(event));
}