#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "network/DiscoveryDiff.hpp"
#include "network/DiscoveryDiffWatcher.hpp"

namespace {

struct DiffFile {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("kns-discovery-diff-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         ".json");

    ~DiffFile()
    {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    void write(const std::string& contents) const
    {
        std::ofstream(path) << contents;
    }
};

std::optional<kns::DiscoveryDiff> check(kns::DiscoveryDiffWatcher& watcher)
{
    const auto checks = watcher.completedChecks();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        auto result = watcher.poll(true);
        if (watcher.completedChecks() > checks) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    FAIL("Discovery diff watcher did not finish reading");
    return std::nullopt;
}

} // namespace

TEST_CASE("Discovery diff parser accepts canonical v1 payload", "[network][discovery][diff]")
{
    const auto diff = kns::parseDiscoveryDiff(nlohmann::json::parse(R"({
        "schema_version":"1.0",
        "baseline_available":true,
        "added_nodes":["device:new"],
        "removed_nodes":["device:gone"],
        "changed_nodes":["device:changed"],
        "added_links":["device:a<->device:b"],
        "removed_links":[],
        "changed_links":["device:b<->device:c"]
    })"));

    REQUIRE(diff.baseline_available);
    REQUIRE(diff.added_nodes == std::vector<std::string>{"device:new"});
    REQUIRE(diff.removed_nodes == std::vector<std::string>{"device:gone"});
    REQUIRE(diff.changed_nodes == std::vector<std::string>{"device:changed"});
    REQUIRE_FALSE(diff.empty());
    REQUIRE(diff.summary().find("nodes +1 / -1 / ~1") != std::string::npos);
    REQUIRE(diff.summary().find("links +1 / -0 / ~1") != std::string::npos);
}

TEST_CASE("Discovery diff parser is strict about schema and identities", "[network][discovery][diff]")
{
    REQUIRE_THROWS(kns::parseDiscoveryDiff(nlohmann::json::parse(R"({
        "schema_version":"2.0",
        "baseline_available":true
    })")));

    REQUIRE_THROWS(kns::parseDiscoveryDiff(nlohmann::json::parse(R"({
        "schema_version":"1.0",
        "baseline_available":"yes"
    })")));

    REQUIRE_THROWS(kns::parseDiscoveryDiff(nlohmann::json::parse(R"({
        "schema_version":"1.0",
        "baseline_available":true,
        "added_nodes":["device:a","device:a"]
    })")));

    REQUIRE_THROWS(kns::parseDiscoveryDiff(nlohmann::json::parse(R"({
        "schema_version":"1.0",
        "baseline_available":true,
        "unexpected":1
    })")));
}

TEST_CASE("Discovery diff watcher recovers after partial file", "[network][discovery][diff][watch]")
{
    DiffFile file;
    kns::DiscoveryDiffWatcher watcher;
    watcher.setSource(file.path.string(), true);

    file.write(R"({
        "schema_version":"1.0",
        "baseline_available":false,
        "added_nodes":["host:a"]
    })");
    auto initial = check(watcher);
    REQUIRE(initial.has_value());
    REQUIRE_FALSE(initial->baseline_available);
    REQUIRE(initial->summary().find("Initial discovery snapshot") != std::string::npos);

    REQUIRE_FALSE(check(watcher).has_value());

    file.write("{partial");
    REQUIRE_FALSE(check(watcher).has_value());
    REQUIRE_FALSE(watcher.error().empty());

    file.write(R"({
        "schema_version":"1.0",
        "baseline_available":true,
        "changed_nodes":["host:a"]
    })");
    auto recovered = check(watcher);
    REQUIRE(recovered.has_value());
    REQUIRE(recovered->baseline_available);
    REQUIRE(recovered->changed_nodes == std::vector<std::string>{"host:a"});
    REQUIRE(watcher.error().empty());
}
