add_library(core
    src/Event.cpp
    src/EventQueue.cpp
    src/SimulationClock.cpp
    src/SimulationEngine.cpp
    src/PrintEvent.cpp)

target_include_directories(core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)