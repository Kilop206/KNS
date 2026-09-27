#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include "network/LiveTopologyWatcher.hpp"
#include "network/TopologyLoader.hpp"

namespace {
namespace fs = std::filesystem;
struct SaveDirectory {
    fs::path path = fs::temp_directory_path() /
        ("kns-save-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    SaveDirectory() { fs::create_directory(path); }
    ~SaveDirectory() { std::error_code error; fs::remove_all(path, error); }
};

std::string utf8(const fs::path& path)
{
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}
} // namespace

TEST_CASE("Saved topology preserves metadata tombstones and link configuration", "[network][save]")
{
    SaveDirectory directory;
    const auto path = utf8(directory.path / "network.json");
    kns::Topology topology(3);
    topology.setName("Edited LAN");
    topology.setNodeLabel(0, "Workstation");
    kns::DeviceInfo device;
    device.external_id = "host:workstation";
    device.type = kns::DeviceType::Computer;
    device.addresses = {"192.0.2.10"};
    device.mac = "02:00:00:00:00:01";
    device.evidence = "user_override";
    topology.setNodeDeviceInfo(0, device);
    topology.removeNode(2);
    const auto link = topology.addLinkPtr(0, 1, 42, 3, 0.1, kns::LinkMode::HALF_DUPLEX, 7);
    link->setDiscoveryMetadata(true, "shared_segment");
    link->enqueueTransmission(0, 1, 0, 10);
    const auto revision = topology.getRoutingRevision();

    kns::TopologyLoader::save_topology(topology, path);
    const auto loaded = kns::TopologyLoader::load_topology(path);
    REQUIRE(kns::TopologyLoader::toJson(loaded) == kns::TopologyLoader::toJson(topology));
    REQUIRE(loaded.getLinks().front()->getQueueSize() == 0);
    REQUIRE(link->getQueueSize() == 1);
    REQUIRE(topology.getRoutingRevision() == revision);

    topology.setNodeLabel(0, "Renamed");
    kns::TopologyLoader::save_topology(topology, path);
    REQUIRE(kns::TopologyLoader::load_topology(path).getNode(0)->getLabel() == "Renamed");
    REQUIRE(std::distance(fs::directory_iterator(directory.path), fs::directory_iterator{}) == 1);
}

TEST_CASE("Failed topology saves preserve existing files and clean temporary data", "[network][save]")
{
    SaveDirectory directory;
    const auto path = utf8(directory.path / "network.json");
    kns::TopologyLoader::save_topology(kns::Topology(2), path);
    REQUIRE_THROWS(kns::TopologyLoader::save_topology(kns::Topology(4097), path));
    REQUIRE(kns::TopologyLoader::load_topology(path).size() == 2);
    REQUIRE_THROWS(kns::TopologyLoader::save_topology(kns::Topology(1), ""));
    REQUIRE_THROWS(kns::TopologyLoader::save_topology(kns::Topology(1), utf8(directory.path / "missing" / "network.json")));

    const auto blocked = directory.path / "directory.json";
    fs::create_directory(blocked);
    std::ofstream(blocked / "keep.txt") << "keep";
    REQUIRE_THROWS(kns::TopologyLoader::save_topology(kns::Topology(1), utf8(blocked)));
    REQUIRE(fs::exists(blocked / "keep.txt"));
    REQUIRE(std::distance(fs::directory_iterator(directory.path), fs::directory_iterator{}) == 2);
}

TEST_CASE("Saved UTF-8 topology paths can be loaded and watched", "[network][save][watch]")
{
    SaveDirectory directory;
    const auto path = utf8(directory.path / fs::path(u8"rede-\u00e7-\u7f51.json"));
    kns::TopologyLoader::save_topology(kns::Topology(2), path);
    REQUIRE(kns::TopologyLoader::load_topology(path).size() == 2);
    kns::LiveTopologyWatcher watcher;
    watcher.setSource(path, true);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    std::optional<kns::Topology> snapshot;
    while (!snapshot && watcher.completedChecks() == 0 && std::chrono::steady_clock::now() < deadline) {
        snapshot = watcher.poll(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    REQUIRE(watcher.error().empty());
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->size() == 2);
}
