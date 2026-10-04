#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <string>

#include "intelligence/IntelligenceConfigLoader.hpp"

namespace {

void setEnvironment(const char* name, const char* value)
{
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void clearEnvironment(const char* name)
{
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

struct EnvironmentCleanup {
    ~EnvironmentCleanup()
    {
        clearEnvironment("KNS_INTELLIGENCE_TOKEN");
        clearEnvironment("KNS_TOPOLOGY_HUB_TOKEN");
    }
};

} // namespace

TEST_CASE("intelligence token falls back to Topology Hub desktop token", "[intelligence][config]")
{
    EnvironmentCleanup cleanup;
    clearEnvironment("KNS_INTELLIGENCE_TOKEN");
    setEnvironment("KNS_TOPOLOGY_HUB_TOKEN", "knsh_hub-token");

    const auto config =
        kns::app::intelligence::IntelligenceConfigLoader::fromEnvironment();

    REQUIRE(config.bearer_token == "knsh_hub-token");
}

TEST_CASE("explicit intelligence token overrides Topology Hub token", "[intelligence][config]")
{
    EnvironmentCleanup cleanup;
    setEnvironment("KNS_TOPOLOGY_HUB_TOKEN", "knsh_hub-token");
    setEnvironment("KNS_INTELLIGENCE_TOKEN", "sentient-override");

    const auto config =
        kns::app::intelligence::IntelligenceConfigLoader::fromEnvironment();

    REQUIRE(config.bearer_token == "sentient-override");
}
