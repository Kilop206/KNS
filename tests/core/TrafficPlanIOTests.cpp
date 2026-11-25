#include <catch2/catch_test_macros.hpp>
#include "traffic/TrafficPlanIO.hpp"
#include "traffic/TrafficRun.hpp"
#include "engine/core/SimulationEngine.hpp"
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
    nlohmann::json plan() {
        return {{"schema_version", "1.0"}, {"seed", std::numeric_limits<std::uint64_t>::max()},
            {"max_packets", 20}, {"flows", nlohmann::json::array({
                {{"id", 4294967295ULL}, {"source", 0}, {"destination", 1}, {"start_s", 0},
                 {"end_s", 1}, {"wire_bytes", 100}, {"priority", 7}, {"ttl_hops", 12},
                 {"schedule", {{"kind", "custom"}, {"offsets_s", {0, 0.01}}}}}
            })}};
    }
}

TEST_CASE("Traffic plan input preserves full numeric identity and does not schedule work",
          "[traffic][plan-io]") {
    kns::SimulationEngine engine(kns::Topology(2));
    const auto expected = plan();
    std::istringstream input(expected.dump());
    REQUIRE(kns::traffic::readTrafficPlan(input, engine) == expected);
    REQUIRE(kns::traffic::parseTrafficPlan(expected.dump(), engine) == expected);
    REQUIRE(engine.pendingEventCount() == 0);
    REQUIRE(engine.now() == 0);
    REQUIRE_NOTHROW(kns::traffic::TrafficRun::schedule(engine, expected));
}

TEST_CASE("Traffic plan JSON rejects duplicate keys trailing values and excessive nesting",
          "[traffic][plan-io]") {
    kns::SimulationEngine engine(kns::Topology(2));
    const auto text = plan().dump();
    auto nested = text;
    const std::string key = "\"kind\":\"custom\"";
    nested.replace(nested.find(key), key.size(), key + "," + key);
    for (const auto& invalid : {"{\"seed\":0," + text.substr(1),
                                "{\"s\\u0065ed\":0," + text.substr(1), nested}) {
        try {
            kns::traffic::parseTrafficPlan(invalid, engine);
            FAIL("Duplicate key accepted");
        } catch (const std::invalid_argument& error) {
            REQUIRE(std::string(error.what()).find("Duplicate") != std::string::npos);
        }
    }
    REQUIRE_THROWS(kns::traffic::parseTrafficPlan(text + "{}", engine));
    REQUIRE_THROWS(kns::traffic::parseTrafficPlan("{", engine));
    REQUIRE_THROWS(kns::traffic::parseTrafficPlan("{}", engine));
    const auto deep = std::string(40, '[') + "0" + std::string(40, ']');
    try {
        kns::traffic::parseTrafficPlan(deep, engine);
        FAIL("Excessive nesting accepted");
    } catch (const std::invalid_argument& error) {
        REQUIRE(std::string(error.what()).find("nesting") != std::string::npos);
    }
    REQUIRE_FALSE(engine.hasEvents());
    REQUIRE_NOTHROW(kns::traffic::TrafficRun::schedule(engine, plan()));
}

TEST_CASE("Traffic plan stream reads are bounded at the actual input boundary", "[traffic][plan-io]") {
    kns::SimulationEngine engine(kns::Topology(2));
    auto exact = plan().dump();
    exact.resize(kns::traffic::MAX_PLAN_BYTES, ' ');
    std::istringstream exactInput(exact);
    REQUIRE(kns::traffic::readTrafficPlan(exactInput, engine) == plan());
    exact += ' ';
    REQUIRE_THROWS_AS(kns::traffic::parseTrafficPlan(exact, engine), std::invalid_argument);
    std::istringstream oversized(std::string(kns::traffic::MAX_PLAN_BYTES + 10000, ' '));
    REQUIRE_THROWS_AS(kns::traffic::readTrafficPlan(oversized, engine), std::invalid_argument);
    REQUIRE(oversized.tellg() == static_cast<std::streamoff>(kns::traffic::MAX_PLAN_BYTES + 1));
    std::istringstream failed("{}");
    failed.setstate(std::ios::badbit);
    REQUIRE_THROWS_AS(kns::traffic::readTrafficPlan(failed, engine), std::runtime_error);
    REQUIRE_FALSE(engine.hasEvents());
}
