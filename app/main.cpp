#include <iostream>
#include "core/SimulationClock.h"

int main() {
    gns::core::SimulationClock clock(0.1);

    for (int i = 0; i < 5; ++i) {
        clock.tick();
        std::cout << "Tempo atual: " << clock.now() << "\n";
    }

    return 0;
}