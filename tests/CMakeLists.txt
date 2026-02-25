add_executable(tests
    test_simulation_clock.cpp
)

target_link_libraries(tests
    PRIVATE
    core
    Catch2::Catch2WithMain
)

add_test(NAME simulation_tests COMMAND tests)