#include "cli/DeviceConsole.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/TopologyLoader.hpp"
#include "network/services/DeviceCLI.hpp"

#include <charconv>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>

namespace kns::app {
int runDeviceConsole(SimulationEngine& engine, int device, std::istream& input, std::ostream& output) {
    const auto active = [&](int id) {
        const auto* node = engine.getTopology().getNode(id);
        return node && node->isActive();
    };
    if (!active(device)) { output << "Error: select an active device\n"; return 1; }
    output << "KNS device CLI. help: service commands; device <id>, run, save \"file.json\", exit.\n";
    bool failed = false;
    std::string line;
    while (output << "device-" << device << "> " << std::flush, std::getline(input, line)) {
        try {
            std::istringstream command(line);
            std::string verb;
            command >> verb;
            const auto end = [&] {
                command >> std::ws;
                if (!command.eof()) throw std::invalid_argument("Unexpected extra arguments");
            };
            if (verb.empty()) continue;
            if (verb == "exit") { end(); break; }
            if (verb == "device") {
                std::string value;
                command >> value;
                int selected = -1;
                const auto result = std::from_chars(value.data(), value.data() + value.size(), selected);
                end();
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || !active(selected))
                    throw std::invalid_argument("Use device <active-device-id>");
                device = selected;
            } else if (verb == "run") {
                end();
                engine.run();
                output << DeviceCLI::execute(engine, device, "show requests").output;
            } else if (verb == "save") {
                std::string path;
                if (!(command >> std::quoted(path))) throw std::invalid_argument("Use save \"file.json\"");
                end();
                TopologyLoader::save_topology(engine.getTopology(), path);
                output << "Topology saved\n";
            } else {
                const auto result = DeviceCLI::execute(engine, device, line);
                output << result.output;
                failed |= !result.ok;
                if (verb == "help") output << "Console: device <id> | run | save \"file.json\" | exit\n";
            }
        } catch (const std::exception& error) {
            failed = true;
            output << "Error: " << error.what() << '\n';
        }
    }
    return failed ? 1 : 0;
}
} // namespace kns::app
