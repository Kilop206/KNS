add_executable(KNS main.cpp
    gui/src/Window.cpp
    gui/src/MetricsPannel.cpp
    gui/src/LatencyChart.cpp
    gui/src/PacketRenderer.cpp
)

target_link_libraries(KNS 
    kns_core 
    imgui
)

if(WIN32)
    target_link_libraries(KNS opengl32 gdi32 user32 shell32)
elseif(UNIX)
    target_link_libraries(KNS GL dl)
endif()

add_custom_command(TARGET KNS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/app/topologies
    $<TARGET_FILE_DIR:KNS>/topologies
)
