#include <catch2/catch_test_macros.hpp>
#include "gui/include/DeviceFinder.hpp"
#include "network/Topology.hpp"
#include <stdexcept>

TEST_CASE("Device search combines names types addresses and exact identity filters",
          "[device-finder]") {
    kns::Topology topology(3);
    topology.setNodeLabel(0, "Main##Gateway");
    kns::DeviceInfo info;
    info.type = kns::DeviceType::Router;
    info.addresses = {"192.0.2.10", "2001:db8::CAFE"};
    info.mac = "AA:BB:CC:DD:EE:FF";
    info.external_id = "SN-Gateway-17";
    topology.setNodeDeviceInfo(0, info);
    topology.setNodeLabel(1, "Main server");
    info.type = kns::DeviceType::Server;
    info.addresses = {"192.0.2.11"};
    info.mac.clear();
    info.external_id.clear();
    topology.setNodeDeviceInfo(1, info);
    topology.setNodeLabel(2, "Remote phone");
    REQUIRE(gui::findDevices(topology, "MAIN").nodes == std::vector<int>{0, 1});
    for (const auto* query : {"main type:ROUTER", "#0", "aa:bb", "sn-gateway-17",
                               "2001:DB8::cafe", "gateway 192.0.2"})
        REQUIRE(gui::findDevices(topology, query).nodes == std::vector<int>{0});
    REQUIRE(gui::findDevices(topology, "type:server #1").nodes == std::vector<int>{1});
    REQUIRE(gui::findDevices(topology, "  main\t192.0.2.11\n").nodes == std::vector<int>{1});
    for (const auto* query : {"#", "#0x", "#-1", "#999999999999999999999999999",
                               "type:invalid", "main phone", "#1 type:router"})
        REQUIRE(gui::findDevices(topology, query).nodes.empty());
}

TEST_CASE("Device finder reports bounded counts and searches live metadata without removed nodes",
          "[device-finder]") {
    kns::Topology topology(260);
    const auto result = gui::findDevices(topology, "");
    REQUIRE(result.total == 260);
    REQUIRE(result.nodes.size() == 100);
    REQUIRE(result.nodes.front() == 0);
    REQUIRE(result.nodes.back() == 99);
    REQUIRE(gui::findDevices(topology, "#259").nodes == std::vector<int>{259});
    REQUIRE(topology.removeNode(0));
    REQUIRE(gui::findDevices(topology, "#0").nodes.empty());
    REQUIRE(gui::findDevices(topology, "", 256).total == 259);
    topology.setNodeLabel(259, "new hostname");
    REQUIRE(gui::findDevices(topology, "hostname").nodes == std::vector<int>{259});
    topology.setNodeLabel(259, "changed");
    REQUIRE(gui::findDevices(topology, "hostname").nodes.empty());
    REQUIRE_THROWS_AS(gui::findDevices(topology, "", 0), std::invalid_argument);
    REQUIRE_THROWS_AS(gui::findDevices(topology, "", 257), std::invalid_argument);
}
