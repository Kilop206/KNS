add_executable(simulator main.cpp)

target_link_libraries(simulator
    PRIVATE
        core
)