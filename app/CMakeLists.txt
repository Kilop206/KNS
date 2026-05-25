add_executable(KNS
        main.cpp

        gui/src/Window.cpp
        gui/src/MetricsPannel.cpp
        gui/src/LatencyChart.cpp
        gui/src/PacketRenderer.cpp
)

target_link_libraries(KNS
        PRIVATE
        kns_core
        imgui_lib
)

add_custom_command(TARGET KNS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/app/topologies
        $<TARGET_FILE_DIR:KNS>/topologies
)