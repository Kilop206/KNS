file(GLOB_RECURSE KNS_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

add_library(kns_core ${KNS_SOURCES}
        include/engine/events/TCPSynAckEvent.hpp
        include/engine/events/TCPAckEvent.hpp
        src/engine/events/TCPSynAckEvent.cpp
        src/engine/events/TCPAckEvent.cpp
)

target_include_directories(kns_core PUBLIC include)