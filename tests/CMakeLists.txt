file(GLOB_RECURSE KNS_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

if(NOT KNS_BUILD_APP)
    list(FILTER KNS_TEST_SOURCES EXCLUDE REGEX "/app/ChatServiceTests.cpp$")
endif()

add_executable(kns_tests ${KNS_TEST_SOURCES})

target_link_libraries(kns_tests
    PRIVATE
        KNS::kns
        Catch2::Catch2WithMain
)

target_compile_features(kns_tests PRIVATE cxx_std_20)

if(KNS_BUILD_APP)
    target_sources(kns_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/app/intelligence/ChatService.cpp
        ${PROJECT_SOURCE_DIR}/app/intelligence/IntelligenceClient.cpp)
    target_include_directories(kns_tests PRIVATE ${PROJECT_SOURCE_DIR}/app)
    target_link_libraries(kns_tests PRIVATE httplib::httplib)
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
    add_test(NAME runner_tests
        COMMAND "${Python3_EXECUTABLE}" -m unittest discover
            -s "${PROJECT_SOURCE_DIR}/scripts" -p test_run.py -v
    )
    set_tests_properties(runner_tests PROPERTIES TIMEOUT 60)
    if(TARGET KNS)
        set_tests_properties(runner_tests PROPERTIES
            ENVIRONMENT "KNS_TEST_EXE=$<TARGET_FILE:KNS>"
        )
    endif()
endif()
