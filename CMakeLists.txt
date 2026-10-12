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

if(NOT WIN32)
    enable_testing()
    file(GLOB UAGFX_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(uagfx_tests ${UAGFX_TEST_SOURCES})
    target_include_directories(uagfx_tests PRIVATE tests)
    target_link_libraries(uagfx_tests PRIVATE uagfx_core)
    add_test(NAME uagfx_tests COMMAND uagfx_tests)
endif()
