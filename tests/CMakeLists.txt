file(GLOB_RECURSE KNS_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

if(NOT KNS_BUILD_APP)
    list(FILTER KNS_TEST_SOURCES EXCLUDE REGEX "/app/[^/]+\\.cpp$")
endif()

add_executable(kns_tests ${KNS_TEST_SOURCES})
kns_configure_target(kns_tests)

target_link_libraries(kns_tests
    PRIVATE
        KNS::kns
        Catch2::Catch2WithMain
)

target_compile_features(kns_tests PRIVATE cxx_std_20)

if(KNS_BUILD_APP)
    target_sources(kns_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/app/cli/DeviceConsole.cpp
        ${PROJECT_SOURCE_DIR}/app/gui/src/NetworkServicesPanel.cpp
        ${PROJECT_SOURCE_DIR}/app/intelligence/ChatService.cpp
        ${PROJECT_SOURCE_DIR}/app/intelligence/IntelligenceClient.cpp
        ${PROJECT_SOURCE_DIR}/app/intelligence/IntelligenceConfigLoader.cpp
        ${PROJECT_SOURCE_DIR}/app/hub/TopologyHubClient.cpp
        ${PROJECT_SOURCE_DIR}/app/gui/src/TopologyCanvas.cpp
        ${PROJECT_SOURCE_DIR}/app/gui/src/DeviceIcons.cpp
        ${PROJECT_SOURCE_DIR}/app/gui/src/PacketRenderer.cpp
        ${PROJECT_SOURCE_DIR}/app/gui/src/TranslationService.cpp)
    target_include_directories(kns_tests PRIVATE ${PROJECT_SOURCE_DIR}/app)
    target_include_directories(kns_tests PRIVATE ${PROJECT_SOURCE_DIR}/app/gui/include)
    target_link_libraries(kns_tests PRIVATE httplib::httplib imgui_lib)
    if(WIN32)
        # The HTTP client has the same Windows 10 API requirements as the app.
        target_compile_definitions(kns_tests PRIVATE
            _WIN32_WINNT=0x0A00 WINVER=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
endif()

if(MSVC)
    target_compile_options(kns_tests PRIVATE /utf-8)
endif()

if(KNS_ENABLE_WARNINGS)
    kns_enable_warnings(kns_tests)
endif()

include(CTest)
include(Catch)
catch_discover_tests(kns_tests)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    set(KNS_PYTHON_TEST_TIMEOUT 600 CACHE STRING "Bounded Python integration test timeout in real seconds (1..3600)")
    if(NOT KNS_PYTHON_TEST_TIMEOUT MATCHES "^[1-9][0-9]*$" OR KNS_PYTHON_TEST_TIMEOUT GREATER 3600)
        message(FATAL_ERROR "KNS_PYTHON_TEST_TIMEOUT must be an integer in 1..3600 real seconds")
    endif()
    add_test(NAME pcap_trace_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p "test_pcap*.py" -v)
    set_tests_properties(pcap_trace_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME trace_import_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_trace_import.py -v)
    set_tests_properties(trace_import_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME capacity_planner_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_capacity_planner.py -v)
    set_tests_properties(capacity_planner_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME industrial_scenario_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_industrial_scenario.py -v)
    set_tests_properties(industrial_scenario_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME result_validation_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_validate_result.py -v)
    set_tests_properties(result_validation_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME traffic_config_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_traffic_config.py -v)
    set_tests_properties(traffic_config_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME experiment_manifest_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_experiment_manifest.py -v)
    set_tests_properties(experiment_manifest_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME runner_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_run.py -v
    )
    set_tests_properties(runner_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME experiment_profile_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_experiment_profile.py -v
    )
    set_tests_properties(experiment_profile_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME experiment_comparison_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_compare_experiments.py -v
    )
    set_tests_properties(experiment_comparison_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    add_test(NAME analytics_report_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_analytics_report.py -v)
    set_tests_properties(analytics_report_tests PROPERTIES TIMEOUT "${KNS_PYTHON_TEST_TIMEOUT}")
    if(TARGET KNS)
        set_tests_properties(pcap_trace_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>")
        set_tests_properties(trace_import_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>")
        set_tests_properties(capacity_planner_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>")
        set_tests_properties(industrial_scenario_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>")
        set_tests_properties(result_validation_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>")
        set_tests_properties(traffic_config_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>")
        set_tests_properties(analytics_report_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>")
        set_tests_properties(experiment_profile_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>"
        )
        set_tests_properties(runner_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>"
        )
        if(KNS_ENABLE_TLS)
            find_program(KNS_OPENSSL_EXECUTABLE openssl REQUIRED)
            add_test(NAME desktop_https
                COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/test_https.py"
                    "$<TARGET_FILE:KNS>" "${KNS_OPENSSL_EXECUTABLE}")
            set_tests_properties(desktop_https PROPERTIES TIMEOUT 120)
        endif()
    endif()
endif()
