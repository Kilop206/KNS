#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "engine/core/SimulationEngine.hpp"
#include "engine/core/RunConfig.hpp"
#include "engine/events/PacketReceivedEvent.hpp"

TEST_CASE("CSV DATA latency matches observer samples independently of control traffic", "[core][stats][latency]")
{
    kns::SimulationEngine engine(kns::Topology(2));
    std::vector<double> samples;
    engine.setLatencyObserver([&](double value) { samples.push_back(value); });
    bool include_data = true;
    int controls = 0;
    SECTION("DATA alone") {}
    SECTION("DATA and control packets") { controls = 5; }
    SECTION("Control packets without DATA") { include_data = false; controls = 5; }
    SECTION("No packets") { include_data = false; }

    if (include_data) {
        for (double time : {2.0, 4.0}) {
            kns::Packet packet(0, 1, 1, 0.0, 100, 999);
            packet.tcp.payload = {1, 2, 3};
            engine.schedule(std::make_unique<kns::PacketReceivedEvent>(time, packet));
        }
    }
    for (int i = 0; i < controls; ++i) {
        kns::Packet packet(0, 1, 1, 0.0, 40, 999);
        packet.tcp.flags = kns::TCPFlag::RST;
        engine.schedule(std::make_unique<kns::PacketReceivedEvent>(5.0, packet));
    }
    engine.run();
    REQUIRE(samples.size() == (include_data ? 2 : 0));
    const auto sample_count = static_cast<int>(samples.size());
    REQUIRE(engine.getStats().data_packets_delivered == sample_count);
    REQUIRE(engine.getStats().packets_delivered == sample_count + controls);
    if (include_data) {
        REQUIRE(samples[0] == 2.0);
        REQUIRE(samples[1] == 4.0);
    }

    struct TemporaryCSV {
        std::filesystem::path path = "stats-latency-regression.csv";
        ~TemporaryCSV() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } csv;
    kns::RunConfig config;
    config.filename = csv.path.string();
    engine.exportStatsCSV(config);
    std::ifstream input(csv.path);
    std::string header, row;
    REQUIRE(static_cast<bool>(std::getline(input, header)));
    REQUIRE(static_cast<bool>(std::getline(input, row)));
    std::istringstream headers(header), values(row);
    std::map<std::string, double> fields;
    std::string name, value;
    while (std::getline(headers, name, ',')) {
        REQUIRE(static_cast<bool>(std::getline(values, value, ',')));
        fields.emplace(name, std::stod(value));
    }
    REQUIRE(fields.at("data_packets_delivered") == samples.size());
    REQUIRE(fields.at("avg_latency") == Catch::Approx(include_data ? 3.0 : 0.0));
    REQUIRE(fields.at("total_latency") == Catch::Approx(include_data ? 6.0 : 0.0));
}
