add_executable(KNS)

target_sources(KNS PRIVATE
    main.cpp
    gui/src/Window.cpp
    gui/src/MetricsPannel.cpp
    gui/src/LatencyChart.cpp
    gui/src/VisualPacketManager.cpp
    gui/src/PacketRenderer.cpp
    gui/src/GUIFormat.cpp
    gui/src/ThemeManager.cpp

    # Painéis adicionais que não estavam sendo compilados antes (fix issue #29)
    gui/src/TcpCongestionPanel.cpp
    gui/src/TcpConnectionPanel.cpp
    gui/src/TcpTimelinePanel.cpp
    gui/src/TopologyPannel.cpp
)

target_include_directories(KNS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gui/include
)

target_link_libraries(KNS
    PRIVATE
        kns
        imgui_lib
)

target_compile_features(KNS PRIVATE cxx_std_20)

if(KNS_ENABLE_WARNINGS)
    kns_enable_warnings(KNS)
endif()

if(KNS_BUILD_TESTS)
    foreach(metric delay bandwidth hop-count delay-bandwidth)
        add_test(
            headless_routing_metric_${metric}
            ${CMAKE_COMMAND}
            -DKNS_APP_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -DKNS_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
            -DKNS_TOPOLOGY=${CMAKE_CURRENT_SOURCE_DIR}/topologies/mesh4.json
            -DKNS_METRIC=${metric}
            -DKNS_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/headless-tests/${metric}.csv
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunHeadlessRoutingMetric.cmake
        )
    endforeach()

    add_test(
        headless_routing_metric_rejects_invalid_value
        ${CMAKE_COMMAND}
        -DKNS_APP_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -DKNS_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
        -DKNS_TOPOLOGY=${CMAKE_CURRENT_SOURCE_DIR}/topologies/mesh4.json
        -DKNS_METRIC=invalid
        -DKNS_EXPECT_FAILURE=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunHeadlessRoutingMetric.cmake
    )
endif()

# ============================================
# Assets
# ============================================

target_compile_definitions(KNS PRIVATE
    KNS_ASSET_DIR="$<TARGET_FILE_DIR:KNS>/assets"
)

add_custom_command(TARGET KNS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:KNS>/assets
    COMMENT "Copying assets..."
)

add_custom_command(TARGET KNS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${PROJECT_SOURCE_DIR}/app/topologies
            $<TARGET_FILE_DIR:KNS>/topologies
    COMMENT "Copying topologies..."
)
