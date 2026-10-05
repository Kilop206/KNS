#pragma once
#include <iosfwd>
namespace kns { class SimulationEngine; }
namespace kns::app {
int runDeviceConsole(SimulationEngine& engine, int device, std::istream& input, std::ostream& output);
}
