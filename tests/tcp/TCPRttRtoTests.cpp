#include <catch2/catch_test_macros.hpp>

// Hidden until RTT/RTO estimation has executable behavior to assert.
TEST_CASE("TCP RTT/RTO placeholder", "[tcp][rtt-rto][.]")
{
    // RTT/RTO tracking is planned for future implementation.
    // This test ensures the test harness compiles successfully.
    SUCCEED("Placeholder test for RTT/RTO tracking");
}
