#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "engine/core/SimulationEngine.hpp"

using namespace kns;

TEST_CASE("TCP control serialization is independent of application payload size", "[tcp][wire]")
{
    for (auto flag : {TCPFlag::SYN, TCPFlag::SYN | TCPFlag::ACK, TCPFlag::ACK,
                      TCPFlag::FIN, TCPFlag::RST | TCPFlag::ACK}) {
        for (int size : {64, 65535}) {
            Topology topology(2);
            topology.addLink(0, 1, 1.0, 0.0);
            SimulationEngine engine(topology);
            engine.setGlobalPacketSize(size);
            Packet packet(0, 1, 0, 0.0, size, 0);
            packet.tcp.flags = flag;
            engine.setPacketObserver([](const Packet& observed, auto, auto, auto, auto, auto) {
                REQUIRE(observed.packet_size_bytes == 40);
            });
            REQUIRE(engine.sendPacket(packet, *engine.getTopology().getLinks()[0], 0.0));
            REQUIRE(engine.peekNextEventTime() == Catch::Approx(40.0 * 8 / 1e6));
        }
    }
}

TEST_CASE("TCP DATA serialized size includes headers exactly once", "[tcp][wire]")
{
    Topology topology(2);
    topology.addLink(0, 1, 1.0, 0.0);
    SimulationEngine engine(topology);
    Packet packet(0, 1, 0, 0.0, 100, 0);
    packet.tcp.flags = TCPFlag::PSH | TCPFlag::ACK;
    packet.tcp.payload.resize(100);
    packet.packet_size_bytes = packet.serializedSize();
    REQUIRE(packet.serializedSize() == 140);
    REQUIRE(engine.sendPacket(packet, *engine.getTopology().getLinks()[0], 0.0));
    REQUIRE(engine.peekNextEventTime() == Catch::Approx(140.0 * 8 / 1e6));
}
