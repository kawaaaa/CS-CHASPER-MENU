cmake_minimum_required(VERSION 3.20)
project(kawas_menu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include(FetchContent)

if (EXISTS "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp")
    set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
    message(STATUS "ImGui: local")
else()
    message(STATUS "ImGui: FetchContent")
    FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.5)
    FetchContent_MakeAvailable(imgui)
    set(IMGUI_DIR ${imgui_SOURCE_DIR})
endif()

if (NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/nlohmann/json.hpp")
    message(FATAL_ERROR "Lipseste third_party/nlohmann/json.hpp")
endif()

if (EXISTS "${CMAKE_SOURCE_DIR}/src/proc.cpp")
    set(PROC_SRC src/proc.cpp)
elseif (EXISTS "${CMAKE_SOURCE_DIR}/src/process.cpp")
    set(PROC_SRC src/process.cpp)
else()
    message(FATAL_ERROR "Lipseste src/proc.cpp sau src/process.cpp")
endif()

add_executable(kawas_menu
    src/main.cpp
    ${PROC_SRC}
    src/pattern.cpp
    src/overlay.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp)

target_include_directories(kawas_menu PRIVATE src third_party ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(kawas_menu PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(kawas_menu PRIVATE psapi.lib d3d11.lib dxgi.lib dwmapi.lib)

set_target_properties(kawas_menu PROPERTIES WIN32_EXECUTABLE FALSE)

add_custom_command(TARGET kawas_menu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/offsets.json" "$<TARGET_FILE_DIR:kawas_menu>")