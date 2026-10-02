#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "network/LiveTopologyWatcher.hpp"
#include "engine/core/SimulationEngine.hpp"
#include "network/TopologyLoader.hpp"

namespace {
struct WatchFile {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("kns-watch-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    ~WatchFile() { std::error_code error; std::filesystem::remove(path, error); }
    void write(const std::string& contents) const { std::ofstream(path) << contents; }
};

std::optional<kns::Topology> check(kns::LiveTopologyWatcher& watcher)
{
    const auto checks = watcher.completedChecks();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        auto result = watcher.poll(true);
        if (watcher.completedChecks() > checks) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FAIL("Live watcher did not finish reading");
    return std::nullopt;
}
}

TEST_CASE("Live file rejects malformed updates and recovers without losing topology", "[network][discovery][watch]")
{
    WatchFile file;
    kns::LiveTopologyWatcher watcher;
    watcher.setSource(file.path.string(), true);
    file.write(R"({"nodes":2,"links":[]})");
    auto first = check(watcher);
    REQUIRE(first.has_value());
    REQUIRE(first->size() == 2);
    REQUIRE_FALSE(check(watcher).has_value());
    file.write("{partial");
    REQUIRE_FALSE(check(watcher).has_value());
    REQUIRE_FALSE(watcher.error().empty());
    REQUIRE(first->size() == 2);
    file.write(R"({"nodes":3,"links":[]})");
    auto next = check(watcher);
    REQUIRE(next.has_value());
    REQUIRE(next->size() == 3);
    REQUIRE(watcher.error().empty());
}

TEST_CASE("Live file ignores late results after disabling source", "[network][discovery][watch]")
{
    WatchFile file;
    file.write(R"({"nodes":2,"links":[]})");
    kns::LiveTopologyWatcher watcher;
    watcher.setSource(file.path.string(), true);
    REQUIRE_FALSE(watcher.poll(true).has_value());
    REQUIRE(watcher.busy());
    watcher.setSource(file.path.string(), false);
    REQUIRE_FALSE(check(watcher).has_value());
    REQUIRE_FALSE(watcher.busy());
}

TEST_CASE("Live file reports unavailable empty and oversized files and recovers", "[network][discovery][watch]")
{
    WatchFile file;
    kns::LiveTopologyWatcher watcher;
    watcher.setSource(file.path.string(), true);
    REQUIRE_FALSE(check(watcher).has_value());
    REQUIRE_FALSE(watcher.error().empty());
    file.write("");
    REQUIRE_FALSE(check(watcher).has_value());
    REQUIRE(watcher.error() == "Live topology file is empty");
    file.write(std::string(4 * 1024 * 1024 + 1, ' '));
    REQUIRE_FALSE(check(watcher).has_value());
    REQUIRE(watcher.error() == "Live topology exceeds 4 MiB");
    file.write(R"({"nodes":1,"links":[]})");
    REQUIRE(check(watcher).has_value());
    REQUIRE(watcher.error().empty());
}

TEST_CASE("Switching live sources discards a pending snapshot", "[network][discovery][watch]")
{
    WatchFile first;
    WatchFile second;
    first.write(R"({"nodes":2,"links":[]})");
    second.write(R"({"nodes":3,"links":[]})");
    kns::LiveTopologyWatcher watcher;
    watcher.setSource(first.path.string(), true);
    REQUIRE_FALSE(watcher.poll(true).has_value());
    watcher.setSource(second.path.string(), true);
    REQUIRE_FALSE(check(watcher).has_value());
    auto next = check(watcher);
    REQUIRE(next.has_value());
    REQUIRE(next->size() == 3);

    watcher.setSource(second.path.string(), false);
    REQUIRE_FALSE(watcher.poll(true).has_value());
    watcher.setSource(second.path.string(), true);
    REQUIRE(check(watcher).has_value());
}

TEST_CASE("Live snapshots update a running engine without resetting sessions time or queues", "[network][discovery][watch]")
{
    WatchFile file;
    auto document = nlohmann::json::parse(R"({
        "schema_version":"1.0",
        "nodes":[
            {"id":0,"external_id":"host:a","type":"computer"},
            {"id":1,"external_id":"device:b","type":"router"}
        ],
        "links":[{"from":0,"to":1,"bandwidth":100,"delay":1,"loss":0}]
    })");
    file.write(document.dump());
    kns::LiveTopologyWatcher watcher;
    watcher.setSource(file.path.string(), true);
    auto initial = check(watcher);
    REQUIRE(initial.has_value());
    kns::SimulationEngine engine(*initial);
    auto& session = engine.createTCPSession(0, 1);
    const auto session_id = session.getSession_id();
    const auto link = engine.getTopology().getLinks().front();
    engine.scheduleLinkFailure(5.0, link->getId(), true);
    REQUIRE(engine.processEvent());
    REQUIRE(engine.now() == 5.0);
    link->enqueueTransmission(0, 1, 5.0, 10.0);
    engine.scheduleLinkFailure(20.0, link->getId(), false);

    document["nodes"][0]["label"] = "Updated computer";
    file.write(document.dump());
    auto update = check(watcher);
    REQUIRE(update.has_value());
    REQUIRE(engine.synchronizeTopology(*update));
    REQUIRE(engine.now() == 5.0);
    REQUIRE(&engine.getTCPSession(session_id) == &session);
    REQUIRE(engine.getTopology().getLinks().front() == link);
    REQUIRE(link->getQueueSize() == 1);
    REQUIRE(engine.getTopology().getNode(0)->getLabel() == "Updated computer");
    REQUIRE(engine.peekNextEventTime() == 20.0);

    file.write("{partial");
    REQUIRE_FALSE(check(watcher).has_value());
    REQUIRE(engine.getTopology().getLinks().front() == link);
    file.write(document.dump());
    REQUIRE_FALSE(check(watcher).has_value());
    REQUIRE(watcher.error().empty());
    REQUIRE(engine.processEvent());
    REQUIRE_FALSE(link->isUp());
}
