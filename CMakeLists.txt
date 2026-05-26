cmake_minimum_required(VERSION 3.20)

project(KNS VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(FetchContent)
include(CTest)

# ==================================================
# Configurações globais
# ==================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KNS_BUILD_TESTS "Build unit tests" ON)
option(KNS_ENABLE_WARNINGS "Enable compiler warnings" ON)

# Evita mudanças inesperadas em dependências baixadas
set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

if(POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

# OpenGL deve ser resolvido pelo CMake, não por libs hardcoded
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# ==================================================
# Função auxiliar para warnings
# ==================================================
function(kns_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# ==================================================
# Catch2 (somente se testes estiverem habilitados)
# ==================================================
if(KNS_BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(Catch2)

    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
endif()

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
# Biblioteca ImGui agregada ao projeto
# ==================================================
add_library(imgui_lib STATIC)
add_library(KNS::imgui_lib ALIAS imgui_lib)

target_sources(imgui_lib PRIVATE
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
    $<BUILD_INTERFACE:${imgui_SOURCE_DIR}>
    $<BUILD_INTERFACE:${imgui_SOURCE_DIR}/backends>
    $<BUILD_INTERFACE:${imguifiledialog_SOURCE_DIR}>
)

target_link_libraries(imgui_lib
    PUBLIC
        glfw
        OpenGL::GL
        Threads::Threads
)

target_compile_features(imgui_lib PUBLIC cxx_std_20)

if(KNS_ENABLE_WARNINGS)
    kns_enable_warnings(imgui_lib)
endif()

# ==================================================
# Projeto principal
# ==================================================
add_subdirectory(core)
add_subdirectory(app)

# Se kns existir no subdiretório core, adiciona fontes extras com segurança
if(TARGET kns)
    target_sources(kns PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core/src/network/utils/PacketUtils.cpp
    )
    target_compile_features(kns PUBLIC cxx_std_20)

    if(KNS_ENABLE_WARNINGS)
        kns_enable_warnings(kns)
    endif()
endif()