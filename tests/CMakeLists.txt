file(GLOB_RECURSE KNS_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(kns_tests ${KNS_TEST_SOURCES})

target_link_libraries(kns_tests
    PRIVATE
        KNS::kns
        Catch2::Catch2WithMain
)

target_compile_features(kns_tests PRIVATE cxx_std_20)

if(KNS_ENABLE_WARNINGS)
    kns_enable_warnings(kns_tests)
endif()

include(CTest)
include(Catch)
catch_discover_tests(kns_tests)
