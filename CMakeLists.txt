cmake_minimum_required(VERSION 3.20)
project(uaro_graphics LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# miniz (MIT): deflate and inflate for GRF archives (upscaled.grf). Third-party code: its own warnings are not ours.
add_library(uagfx_miniz STATIC third_party/miniz/miniz.c)
target_include_directories(uagfx_miniz PUBLIC third_party/miniz)
target_compile_options(uagfx_miniz PRIVATE -w)
target_compile_definitions(uagfx_miniz PUBLIC MINIZ_NO_STDIO MINIZ_NO_ARCHIVE_APIS MINIZ_NO_TIME)  # only (in)flate

# libwebp 1.5.0 (BSD): the texture pack's pictures (0.42) - encoded by the pack builder, decoded by the renderer.
# Only its decoder, encoder and their helpers; third-party code: its own warnings are not ours.
file(GLOB UAGFX_WEBP_SOURCES CONFIGURE_DEPENDS third_party/libwebp/src/dec/*.c third_party/libwebp/src/dsp/*.c
     third_party/libwebp/src/enc/*.c third_party/libwebp/src/utils/*.c third_party/libwebp/sharpyuv/*.c)
add_library(uagfx_webp STATIC ${UAGFX_WEBP_SOURCES})
target_include_directories(uagfx_webp PUBLIC third_party/libwebp/src PRIVATE third_party/libwebp)
target_compile_options(uagfx_webp PRIVATE -w)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "i686")
    target_compile_options(uagfx_webp PRIVATE -msse2)  # every processor Windows 10 runs on has it
endif()

# Portable core: formats, surfaces, config, decisions. No Windows headers; unit-tested on Linux.
# 0.49: stb_image's JPEG decoder (the game's .jpg pictures for the texture pack).
add_library(uagfx_stb STATIC third_party/stb/stb_image_jpeg.c)
target_include_directories(uagfx_stb PUBLIC third_party/stb)
target_compile_options(uagfx_stb PRIVATE -w)
file(GLOB_RECURSE UAGFX_CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
add_library(uagfx_core STATIC ${UAGFX_CORE_SOURCES})
target_include_directories(uagfx_core PUBLIC src)
target_compile_options(uagfx_core PRIVATE -Wall -Wextra)
target_link_libraries(uagfx_core PUBLIC uagfx_miniz uagfx_webp uagfx_stb)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    # uaro-render64.exe (part 3): the renderer in a 64-bit process the driver hands its calls to; the rest of the
    # project is 32-bit (cmake/mingw-x86_64.cmake builds only this).
    file(GLOB UAGFX_HOST_SOURCES CONFIGURE_DEPENDS src/win/gpu/*.cpp src/win/bridge/*.cpp)
    add_executable(uaro-render64 WIN32 src/host/main.cpp src/host/pack_builder.cpp src/host/streamline.cpp
                   ${UAGFX_HOST_SOURCES} src/win/log.cpp src/win/paths.cpp)
    # NVIDIA Streamline's headers (MIT); its DLLs are loaded at run time from uaro-graphics\dlss\.
    target_include_directories(uaro-render64 PRIVATE third_party/streamline/include)
    target_compile_definitions(uaro-render64 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX WIDL_EXPLICIT_AGGREGATE_RETURNS)
    target_compile_options(uaro-render64 PRIVATE -Wall -Wextra -Wno-unknown-pragmas)  # Streamline's #pragma region
    target_link_libraries(uaro-render64 PRIVATE uagfx_core)
    target_link_options(uaro-render64 PRIVATE -static -static-libgcc -static-libstdc++ -s)
elseif(WIN32)
    # ddraw.dll that uaRO.exe loads from its own folder instead of dgVoodoo's or the system one.
    file(GLOB_RECURSE UAGFX_WIN_SOURCES CONFIGURE_DEPENDS src/win/*.cpp)
    add_library(ddraw SHARED ${UAGFX_WIN_SOURCES} src/win/ddraw.def)
    set_target_properties(ddraw PROPERTIES PREFIX "" OUTPUT_NAME "ddraw")
    target_compile_definitions(ddraw PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX WIDL_EXPLICIT_AGGREGATE_RETURNS)
    target_compile_options(ddraw PRIVATE -Wall -Wextra)
    target_link_libraries(ddraw PRIVATE uagfx_core)
    target_link_options(ddraw PRIVATE -static -static-libgcc -static-libstdc++ -s)

    # The settings app's Windows layer (stage 2 spec §3.3): files, registry, system, its model.
    add_library(uagfx_app STATIC src/app/text.cpp src/app/utf16.cpp src/app/files.cpp src/app/registry.cpp
                src/app/system.cpp src/app/model.cpp src/app/rebuilds.cpp src/app/mnemonic.cpp src/app/ai_cache.cpp
                src/app/update.cpp
                src/win/system_probe.cpp)
    target_include_directories(uagfx_app PUBLIC src)
    target_compile_definitions(uagfx_app PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
                               WIDL_EXPLICIT_AGGREGATE_RETURNS)
    target_compile_options(uagfx_app PRIVATE -Wall -Wextra)
    target_link_libraries(uagfx_app PUBLIC uagfx_core comctl32 shell32 ole32 advapi32 winhttp)

    # uaro-graphics.exe, the settings app (stage 2 spec): its version comes from src/core/version.cpp.
    enable_language(RC)
    file(STRINGS src/core/version.cpp UAGFX_VERSION_LINE REGEX "kVersion = ")
    string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" UAGFX_VERSION "${UAGFX_VERSION_LINE}")
    set(UAGFX_V1 ${CMAKE_MATCH_1})
    set(UAGFX_V2 ${CMAKE_MATCH_2})
    set(UAGFX_V3 ${CMAKE_MATCH_3})
    configure_file(src/app/version_rc.h.in ${CMAKE_BINARY_DIR}/app/version_rc.h)
    add_executable(uaro-graphics WIN32 src/app/main.cpp src/app/main_window.cpp src/app/ui.cpp src/app/page.cpp
                   src/app/page_settings.cpp src/app/page_screen.cpp src/app/page_diagnostics.cpp
                   src/app/page_service.cpp src/app/page_ai.cpp src/app/page_ai_cache.cpp
                   src/app/page_effects.cpp src/app/page_pack.cpp src/app/app.rc)
    target_include_directories(uaro-graphics PRIVATE src/app dist ${CMAKE_BINARY_DIR}/app)
    target_compile_options(uaro-graphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-graphics PRIVATE uagfx_app comdlg32)
    target_link_options(uaro-graphics PRIVATE -static -static-libgcc -static-libstdc++ -s -municode)
else()
    enable_testing()
    file(GLOB UAGFX_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    # The settings app's model runs here too, over the stand-ins of tests/fake_windows.cpp for its Windows layer.
    add_executable(uagfx_tests ${UAGFX_TEST_SOURCES} src/app/model.cpp src/app/text.cpp src/app/rebuilds.cpp
                   src/app/mnemonic.cpp)
    target_include_directories(uagfx_tests PRIVATE tests)
    target_link_libraries(uagfx_tests PRIVATE uagfx_core)
    target_compile_definitions(uagfx_tests PRIVATE UAGFX_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME uagfx_tests COMMAND uagfx_tests)
endif()
