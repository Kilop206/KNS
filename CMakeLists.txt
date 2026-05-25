cmake_minimum_required(VERSION 3.20)
project(KNS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
include(CTest)

# ==================================================
# Catch2
# ==================================================
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# ==================================================
# GLFW
# ==================================================
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# ==================================================
# ImGui
# ==================================================
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
        GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(imgui)

# ==================================================
# ImGuiFileDialog
# ==================================================
FetchContent_Declare(
        imguifiledialog
        GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
        GIT_TAG master
        GIT_SHALLOW TRUE
)

FetchContent_GetProperties(imguifiledialog)

if(NOT imguifiledialog_POPULATED)
    FetchContent_Populate(imguifiledialog)
endif()

# ==================================================
# ImGui Library
# ==================================================
add_library(imgui_lib STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp

        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp

        ${imguifiledialog_SOURCE_DIR}/ImGuiFileDialog.cpp
)

target_include_directories(imgui_lib PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imguifiledialog_SOURCE_DIR}
)

target_link_libraries(imgui_lib PUBLIC glfw)

if(WIN32)
    target_link_libraries(imgui_lib PUBLIC
            opengl32
            gdi32
            user32
            shell32
    )
elseif(UNIX)
    target_link_libraries(imgui_lib PUBLIC
            GL
            dl
    )
endif()

# ==================================================
# Project
# ==================================================
add_subdirectory(core)
add_subdirectory(app)

target_sources(kns_core PRIVATE
        core/src/network/utils/PacketUtils.cpp
)