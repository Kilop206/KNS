#pragma once
#include <string>

namespace kns {
class SimulationEngine;
struct DeviceCommandResult {
    bool ok = false;
    bool scheduled = false;
    std::string output;
};

// Shared by the GUI device terminal and the stdin CLI. Never executes host commands.
class DeviceCLI {
public:
    static DeviceCommandResult execute(SimulationEngine& engine, int device, const std::string& command);
    static std::string help();
};
} // namespace kns
