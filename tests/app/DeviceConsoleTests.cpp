#include <catch2/catch_test_macros.hpp>
#include "cli/DeviceConsole.hpp"
#include "engine/core/SimulationEngine.hpp"

#include <sstream>

TEST_CASE("Device console configures HTTP DNS and executes client requests without GUI", "[services][cli]") {
    kns::Topology topology(2);
    topology.addLink(0, 1, 100, 1, 0);
    kns::SimulationEngine engine(topology);
    std::istringstream input(
        "service add http web 80\n"
        "service page web / 200 \"Hello from terminal\"\n"
        "service add dns names 53\n"
        "service record names web.example 192.0.2.1\n"
        "device 0\n"
        "http get 1 80 /\n"
        "dns query 1 53 web.example\n"
        "run\n"
        "show requests\n"
        "exit\n");
    std::ostringstream output;
    REQUIRE(kns::app::runDeviceConsole(engine, 1, input, output) == 0);
    REQUIRE(output.str().find("Hello from terminal") != std::string::npos);
    REQUIRE(output.str().find("192.0.2.1") != std::string::npos);
    REQUIRE(engine.networkServices().requests().size() == 2);
    REQUIRE(engine.validateSimulation().passed());
}

TEST_CASE("Device console reports script errors without changing the selected device", "[services][cli]") {
    kns::SimulationEngine engine(kns::Topology(2));
    std::istringstream input("device 1oops\ndevice 999\nservice add http web 80\nexit extra\nexit\n");
    std::ostringstream output;
    REQUIRE(kns::app::runDeviceConsole(engine, 0, input, output) == 1);
    REQUIRE(engine.getTopology().getNode(0)->getServices().size() == 1);
    REQUIRE(engine.getTopology().getNode(1)->getServices().empty());
}
