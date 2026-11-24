#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "gui/include/CableGeometry.hpp"
#include "gui/include/VisualPacketManager.hpp"
#include "network/Topology.hpp"
#include "network/utils/PacketUtils.hpp"
#include "engine/core/SimulationEngine.hpp"
#include <cmath>

TEST_CASE("Cable lanes separate reversed parallel declarations and preserve exact endpoints",
          "[cable-geometry]") {
    kns::Topology topology(2);
    const auto first = topology.addLinkPtr(0, 1, 100, 1);
    const auto second = topology.addLinkPtr(1, 0, 100, 1, 0, kns::LinkMode::SIMPLEX);
    auto paths = gui::buildCablePaths(gui::layoutCableLanes(topology), {{0, 0}, {200, 0}}, 1);
    REQUIRE(paths.at(first->getId()).point(0).x == 0);
    REQUIRE(paths.at(first->getId()).point(1).x == 200);
    REQUIRE(paths.at(second->getId()).point(0).x == 200);
    REQUIRE(paths.at(second->getId()).point(1).x == 0);
    const auto upper = paths.at(first->getId()).point(0.5f);
    const auto lower = paths.at(second->getId()).point(0.5f);
    REQUIRE(upper.y == Catch::Approx(-14));
    REQUIRE(lower.y == Catch::Approx(14));
    REQUIRE(paths.at(first->getId()).distance(upper) == Catch::Approx(0));
    REQUIRE(paths.at(second->getId()).distance(upper) > 7);
    const auto zoomed = gui::buildCablePaths(gui::layoutCableLanes(topology),
                                            {{100, 200}, {500, 200}}, 2);
    REQUIRE(zoomed.at(first->getId()).point(0.5f).x == Catch::Approx(300));
    REQUIRE(zoomed.at(first->getId()).point(0.5f).y == Catch::Approx(172));
}

TEST_CASE("Single and coincident cable layouts remain finite and invalid endpoints are skipped",
          "[cable-geometry]") {
    kns::Topology topology(2);
    auto link = topology.addLinkPtr(0, 1, 100, 1);
    const auto lanes = gui::layoutCableLanes(topology);
    const auto straight = gui::buildCablePaths(lanes, {{0, 0}, {200, 100}}, 1);
    const auto mid = straight.at(link->getId()).point(0.5f);
    REQUIRE(mid.x == Catch::Approx(100));
    REQUIRE(mid.y == Catch::Approx(50));
    REQUIRE(straight.at(link->getId()).point(-1).x == 0);
    REQUIRE(straight.at(link->getId()).point(2).x == 200);
    const auto coincident = gui::buildCablePaths(lanes, {{20, 30}, {20, 30}}, 0.1f);
    REQUIRE(coincident.at(link->getId()).distance({20, 40}) == Catch::Approx(10));
    REQUIRE(std::isfinite(coincident.at(link->getId()).point(0.5f).y));
    REQUIRE(gui::buildCablePaths(lanes, {{0, 0}}, 1).empty());
}

TEST_CASE("Packet visualization preserves observed cable identity including zero and expires at arrival",
          "[visual-packets]") {
    gui::VisualPacketManager manager;
    kns::Packet packet;
    packet.link_id = 0;
    manager.observePacket(packet, 12, 0, 1, 0.5, 1.5);
    packet.link_id = 23;
    manager.observePacket(packet, 13, 1, 0, 0.5, 2.0);
    manager.update(1);
    REQUIRE(manager.getActivePackets().size() == 2);
    REQUIRE(manager.getActivePackets()[0].link_id == std::optional<std::uint64_t>{0});
    REQUIRE(manager.getActivePackets()[1].link_id == std::optional<std::uint64_t>{23});
    manager.update(1.5);
    REQUIRE(manager.getActivePackets().size() == 1);
    REQUIRE(manager.getActivePackets()[0].link_id == std::optional<std::uint64_t>{23});
    manager.clear();
    REQUIRE(manager.getActivePackets().empty());
}

TEST_CASE("Multihop visual observations follow each accepted transmission cable",
          "[visual-packets][packet-observer]") {
    kns::Topology topology(3);
    const auto primary = topology.addLinkPtr(0, 1, 100, 1);
    topology.addLink(0, 1, 20, 5);
    const auto finalHop = topology.addLinkPtr(1, 2, 100, 1);
    kns::SimulationEngine engine(topology);
    gui::VisualPacketManager manager;
    engine.setPacketObserver([&](const kns::Packet& p, auto session, int from, int to,
                                 double departure, double arrival) {
        manager.observePacket(p, session, from, to, departure, arrival);
    });
    kns::Packet packet(0, 2, 0, 0, 100, 0);
    REQUIRE(kns::PacketUtils::sendPacketThroughTopology(engine, packet));
    engine.run();
    manager.update(0);
    const auto& seen = manager.getActivePackets();
    REQUIRE(seen.size() == 2);
    REQUIRE(seen[0].link_id == std::optional{primary->getId()});
    REQUIRE(seen[1].link_id == std::optional{finalHop->getId()});
    REQUIRE(seen[0].from == 0);
    REQUIRE(seen[1].from == 1);
    manager.update(engine.now());
    REQUIRE(manager.getActivePackets().empty());
}
