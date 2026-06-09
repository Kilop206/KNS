file(GLOB_RECURSE KNS_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(kns STATIC)
add_library(KNS::kns ALIAS kns)

target_sources(kns PRIVATE
    ${KNS_CORE_SOURCES}
)

target_include_directories(kns
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(kns PUBLIC cxx_std_20)

target_link_libraries(kns
    PUBLIC
        Threads::Threads
        nlohmann_json::nlohmann_json
)

if(KNS_ENABLE_WARNINGS)
    kns_enable_warnings(kns)
endif()