cmake_minimum_required(VERSION 3.20)
project(uaro_graphics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Portable core: formats, surfaces, config, decisions. No Windows headers; unit-tested on Linux.
file(GLOB_RECURSE UAGFX_CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
add_library(uagfx_core STATIC ${UAGFX_CORE_SOURCES})
target_include_directories(uagfx_core PUBLIC src)
target_compile_options(uagfx_core PRIVATE -Wall -Wextra)

if(WIN32)
    # ddraw.dll that uaRO.exe loads from its own folder instead of dgVoodoo's or the system one.
    file(GLOB_RECURSE UAGFX_WIN_SOURCES CONFIGURE_DEPENDS src/win/*.cpp)
    add_library(ddraw SHARED ${UAGFX_WIN_SOURCES} src/win/ddraw.def)
    set_target_properties(ddraw PROPERTIES PREFIX "" OUTPUT_NAME "ddraw")
    target_compile_definitions(ddraw PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX WIDL_EXPLICIT_AGGREGATE_RETURNS)
    target_compile_options(ddraw PRIVATE -Wall -Wextra)
    target_link_libraries(ddraw PRIVATE uagfx_core)
    target_link_options(ddraw PRIVATE -static -static-libgcc -static-libstdc++ -s)

    # The settings app's Windows layer (stage 2 spec §3.3): files, registry, system, its model.
    add_library(uagfx_app STATIC src/app/text.cpp src/app/files.cpp src/app/registry.cpp src/app/system.cpp
                src/app/model.cpp src/win/system_probe.cpp)
    target_include_directories(uagfx_app PUBLIC src)
    target_compile_definitions(uagfx_app PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
                               WIDL_EXPLICIT_AGGREGATE_RETURNS)
    target_compile_options(uagfx_app PRIVATE -Wall -Wextra)
    target_link_libraries(uagfx_app PUBLIC uagfx_core comctl32 shell32 ole32 advapi32)
else()
    enable_testing()
    file(GLOB UAGFX_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(uagfx_tests ${UAGFX_TEST_SOURCES})
    target_include_directories(uagfx_tests PRIVATE tests)
    target_link_libraries(uagfx_tests PRIVATE uagfx_core)
    target_compile_definitions(uagfx_tests PRIVATE UAGFX_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME uagfx_tests COMMAND uagfx_tests)
endif()
