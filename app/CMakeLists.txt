add_executable(KNS)

target_sources(KNS PRIVATE
    main.cpp
    gui/src/Window.cpp
    gui/src/MetricsPannel.cpp
    gui/src/LatencyChart.cpp
    gui/src/VisualPacketManager.cpp
    gui/src/PacketRenderer.cpp
    gui/src/GUIFormat.cpp
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

add_custom_command(TARGET KNS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/topologies
            $<TARGET_FILE_DIR:KNS>/topologies
)

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