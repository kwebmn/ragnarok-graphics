cmake_minimum_required(VERSION 3.20)
project(uaro_graphics LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 2.5.14: the 32-bit build's floating point in SSE2 (every processor Windows 10 runs on has it), as the tests' on
# x86-64 - the x87's turned each float to int into two changes of its control word, and the mip levels the game
# stretches its textures into (core/image.cpp) cost a third of its thread (R-7NUW85)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "i686")
    add_compile_options(-msse2 -mfpmath=sse)
endif()

# 3.0.18-beta.162 (the owner: «делай 1 и 2» - the update smaller without the game slower): in the 32-bit build the
# code that runs now and then - the windows opened for a while (storage, shops, trade, equipment, skills, options...),
# the login and the character screens, the settings, the reports, the site's parts, «Довідник»'s pages - is compiled
# for size (-Os); everything that runs each frame (the panels, the bar, the chat, the party, the HP bars, the buffs,
# the world's signs, the pad, uaUI's plumbing) and the drawing, the textures, the hooks, the sound, the files and the
# map's loading stay at full speed (-O3). 3.0.18-beta.163: the panels taken out of it - beta.162's -Os for them made
# R-MBCDBU's interface 1.85-2.36 ms a frame at 55-90 draws where -O3's took 1.31. The 64-bit build is left as it was.
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "i686" AND CMAKE_BUILD_TYPE STREQUAL "Release")
    file(GLOB UAGFX_SMALL_SOURCES CONFIGURE_DEPENDS
        src/app/*.cpp src/core/settings/*.cpp
        src/win/accounts_store.cpp src/win/answer_card.cpp src/win/ask_card.cpp src/win/cart_window.cpp
        src/win/char_diary.cpp src/win/char_make.cpp src/win/char_select.cpp src/win/chatroom_window.cpp
        src/win/command_window.cpp src/win/console.cpp src/win/detail_window.cpp src/win/emote_window.cpp
        src/win/equip_window.cpp src/win/game_logins.cpp src/win/guild_card.cpp src/win/homun_ai_files.cpp
        src/win/homun_card.cpp src/win/hud_edit.cpp src/win/info_card.cpp src/win/item_lists.cpp src/win/item_window.cpp
        src/win/login_*.cpp src/win/macro_card.cpp src/win/menu_window.cpp src/win/merc_card.cpp
        src/win/mob_card.cpp src/win/msg_box.cpp src/win/my_shop.cpp src/win/npc_talk.cpp src/win/old_windows.cpp
        src/win/open_page.cpp src/win/options_card.cpp src/win/own_windows.cpp src/win/party_setup.cpp
        src/win/pet_card.cpp src/win/pick_card.cpp src/win/popup_window.cpp src/win/portrait.cpp
        src/win/recap_window.cpp src/win/reference_load.cpp src/win/reference_window.cpp src/win/room_window.cpp
        src/win/server_card.cpp src/win/server_probe.cpp src/win/setting_edit.cpp src/win/several_windows.cpp
        src/win/shop_window.cpp src/win/site_*.cpp src/win/skill_window.cpp src/win/social_card.cpp
        src/win/start_screen.cpp src/win/stat_window.cpp src/win/storage_window.cpp src/win/system_probe.cpp
        src/win/text_window.cpp src/win/tip_window.cpp src/win/trade_window.cpp src/win/uaui_showcase.cpp
        src/win/user_files.cpp src/win/vend_setup.cpp src/win/vend_shop.cpp src/win/whisper_setup.cpp
        src/win/whisper_window.cpp
        src/core/accounts.cpp src/core/answer_card.cpp src/core/ask_card.cpp src/core/card_names.cpp
        src/core/cart_window.cpp src/core/char_diary.cpp src/core/char_make.cpp src/core/char_select.cpp
        src/core/chatroom_card.cpp src/core/client_patch.cpp src/core/command_card.cpp src/core/computer.cpp
        src/core/config.cpp src/core/console_box.cpp src/core/console_pins.cpp src/core/console_tiles.cpp
        src/core/death_recap.cpp src/core/emote_card.cpp src/core/equip_sets.cpp src/core/equip_window.cpp
        src/core/first_game.cpp src/core/gear_compare.cpp src/core/graphics_test.cpp src/core/guild_card.cpp
        src/core/guild_tabs.cpp src/core/homun_ai.cpp src/core/homun_card.cpp src/core/hud_layout_panel.cpp
        src/core/info_card.cpp src/core/ini_upgrade.cpp src/core/install_ping.cpp src/core/item_elements.cpp
        src/core/item_elements_data.cpp src/core/item_pick.cpp src/core/item_window.cpp src/core/launcher.cpp
        src/core/layout_migration.cpp src/core/log_files.cpp src/core/login_*.cpp src/core/macro_card.cpp
        src/core/menu_window.cpp src/core/merc_ai.cpp src/core/merc_card.cpp src/core/mob_search.cpp
        src/core/my_shop.cpp src/core/npc_talk.cpp src/core/options_card.cpp src/core/party_setup.cpp
        src/core/pet_card.cpp src/core/pick_card.cpp src/core/popup_card.cpp src/core/reference_live.cpp
        src/core/reference_now.cpp src/core/reference_pack.cpp src/core/reference_pages.cpp
        src/core/reference_quests.cpp src/core/reference_world_paint.cpp src/core/register_form.cpp
        src/core/renderer_choice.cpp src/core/report*.cpp src/core/room_card.cpp src/core/safe_mode.cpp
        src/core/server_status.cpp src/core/settings_window.cpp src/core/shelf_merge.cpp src/core/shelf_toast.cpp
        src/core/shop_window.cpp src/core/shot_spec.cpp src/core/site_api.cpp src/core/site_signin.cpp
        src/core/skill_info.cpp src/core/skill_plan.cpp src/core/skill_tree.cpp src/core/skill_tree_view.cpp
        src/core/skill_window.cpp src/core/social_card.cpp src/core/start_failures.cpp src/core/start_screen.cpp
        src/core/stash_book.cpp src/core/stat_math.cpp src/core/stat_window.cpp src/core/studio_link.cpp
        src/core/studio_state.cpp src/core/studio_status.cpp src/core/switch_card.cpp src/core/text_field.cpp
        src/core/tip_card.cpp src/core/tip_texts.cpp src/core/trade_window.cpp src/core/update_check.cpp
        src/core/usage.cpp src/core/vend_setup.cpp src/core/vend_shop.cpp src/core/version.cpp
        src/core/weight_fit.cpp src/core/whisper_card.cpp src/core/window_plan.cpp)
    set_source_files_properties(${UAGFX_SMALL_SOURCES} PROPERTIES COMPILE_OPTIONS "-Os")
endif()

# miniz (MIT): deflate and inflate for GRF archives (upscaled.grf). Third-party code: its own warnings are not ours.
add_library(uagfx_miniz STATIC third_party/miniz/miniz.c)
target_include_directories(uagfx_miniz PUBLIC third_party/miniz)
target_compile_options(uagfx_miniz PRIVATE -w)
target_compile_definitions(uagfx_miniz PUBLIC MINIZ_NO_STDIO MINIZ_NO_ARCHIVE_APIS MINIZ_NO_TIME)  # only (in)flate
# libdeflate (MIT, 1.26): cps.dll's zlib (3.0.2-beta.11) - its zlib streams' files alone, C11.
add_library(uagfx_libdeflate STATIC third_party/libdeflate/lib/deflate_compress.c
    third_party/libdeflate/lib/deflate_decompress.c third_party/libdeflate/lib/zlib_compress.c
    third_party/libdeflate/lib/zlib_decompress.c third_party/libdeflate/lib/adler32.c
    third_party/libdeflate/lib/utils.c third_party/libdeflate/lib/x86/cpu_features.c)
target_include_directories(uagfx_libdeflate PUBLIC third_party/libdeflate)
set_target_properties(uagfx_libdeflate PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_compile_options(uagfx_libdeflate PRIVATE -w -O2)

# libwebp 1.5.0 (BSD): the texture pack's pictures (0.42) - encoded by the pack builder, decoded by the renderer.
# Only its decoder, encoder and their helpers; third-party code: its own warnings are not ours.
file(GLOB UAGFX_WEBP_SOURCES CONFIGURE_DEPENDS third_party/libwebp/src/dec/*.c third_party/libwebp/src/dsp/*.c
     third_party/libwebp/src/enc/*.c third_party/libwebp/src/utils/*.c third_party/libwebp/sharpyuv/*.c)
add_library(uagfx_webp STATIC ${UAGFX_WEBP_SOURCES})
target_include_directories(uagfx_webp PUBLIC third_party/libwebp/src PRIVATE third_party/libwebp)
target_compile_options(uagfx_webp PRIVATE -w)

# Portable core: formats, surfaces, config, decisions. No Windows headers; unit-tested on Linux.
# 0.49: stb_image's JPEG decoder (the game's .jpg pictures for the texture pack). 3.0.1-beta.6: stb_image_write's JPEG
# writer (our ijl15.dll's ijlWrite).
add_library(uagfx_stb STATIC third_party/stb/stb_image_jpeg.c third_party/stb/stb_image_write_jpeg.c)
target_include_directories(uagfx_stb PUBLIC third_party/stb)
target_compile_options(uagfx_stb PRIVATE -w)
# 3.0.1-beta.7: minimp3 (CC0): the mp3 decoder of our mss32.dll (the music, the game's mp3 sounds).
add_library(uagfx_minimp3 STATIC third_party/minimp3/minimp3_impl.c)
target_include_directories(uagfx_minimp3 PUBLIC third_party/minimp3)
target_compile_options(uagfx_minimp3 PRIVATE -w)
file(GLOB_RECURSE UAGFX_CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
add_library(uagfx_core STATIC ${UAGFX_CORE_SOURCES})
target_include_directories(uagfx_core PUBLIC src)
target_compile_options(uagfx_core PRIVATE -Wall -Wextra)
target_link_libraries(uagfx_core PUBLIC uagfx_miniz uagfx_webp uagfx_stb uagfx_minimp3 uagfx_libdeflate)

# 3.0.18-beta.7: RmlUi 6.3's core (third_party/rmlui, MIT; its FreeType font engine left out - ours is GDI's).
# 3.0.18-beta.94: built for the host too - src/uikit (the live windows' markup, models and hit places, no Windows
# headers) is laid out by it in the host tests (tests/rml_host: a stand-in font engine).
if(NOT (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8))  # not the 64-bit renderer
    file(GLOB_RECURSE UAGFX_RMLUI_SOURCES CONFIGURE_DEPENDS third_party/rmlui/Source/Core/*.cpp)
    add_library(uagfx_rmlui STATIC ${UAGFX_RMLUI_SOURCES})
    target_include_directories(uagfx_rmlui PUBLIC third_party/rmlui/Include)
    target_compile_definitions(uagfx_rmlui PUBLIC RMLUI_STATIC_LIB PRIVATE RMLUI_VERSION="6.3")
    target_compile_options(uagfx_rmlui PRIVATE -w -O2)
    file(GLOB UAGFX_UIKIT_SOURCES CONFIGURE_DEPENDS src/uikit/*.cpp)
    add_library(uagfx_uikit STATIC ${UAGFX_UIKIT_SOURCES})
    target_include_directories(uagfx_uikit PUBLIC src)
    target_compile_options(uagfx_uikit PRIVATE -Wall -Wextra)
    target_link_libraries(uagfx_uikit PUBLIC uagfx_core uagfx_rmlui)
endif()

# 2.9: uaRO Pad's portable core (from uaro-gamepad e59889d): the protocol, the world, the controller's logic. Its
# Windows part (src/pad/win) is built into ddraw.dll, which mounts the pad itself.
file(GLOB_RECURSE UAGFX_PAD_SOURCES CONFIGURE_DEPENDS src/pad/core/*.cpp)
add_library(uagfx_pad STATIC ${UAGFX_PAD_SOURCES})
target_include_directories(uagfx_pad PUBLIC src)
target_compile_options(uagfx_pad PRIVATE -Wall -Wextra)
target_link_libraries(uagfx_pad PUBLIC uagfx_core)

if(WIN32)
    # The version of src/core/version.cpp for the resources of the settings app (32-bit) and uaRO Studio (64-bit).
    enable_language(RC)
    file(STRINGS src/core/version.cpp UAGFX_VERSION_LINE REGEX "kVersion = ")
    string(REGEX MATCH "([0-9]+)\\.([0-9]+)\\.([0-9]+)" UAGFX_VERSION "${UAGFX_VERSION_LINE}")
    set(UAGFX_V1 ${CMAKE_MATCH_1})
    set(UAGFX_V2 ${CMAKE_MATCH_2})
    set(UAGFX_V3 ${CMAKE_MATCH_3})
    configure_file(src/app/version_rc.h.in ${CMAKE_BINARY_DIR}/app/version_rc.h)
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    # uaro-render64.exe (part 3): the renderer in a 64-bit process the driver hands its calls to; the rest of the
    # project is 32-bit (cmake/mingw-x86_64.cmake builds only this).
    file(GLOB UAGFX_HOST_SOURCES CONFIGURE_DEPENDS src/win/gpu/*.cpp src/win/bridge/*.cpp)
    add_executable(uaro-render64 WIN32 src/host/main.cpp src/host/pack_builder.cpp src/host/streamline.cpp
                   src/host/studio_engine.cpp
                   ${UAGFX_HOST_SOURCES} src/win/log.cpp src/win/paths.cpp src/win/dpi.cpp
                   src/win/game_files.cpp)  # 3.0.4-beta.7: map_world's archives
    # NVIDIA Streamline's headers (MIT); its DLLs are loaded at run time from uaro-graphics\dlss\.
    target_include_directories(uaro-render64 PRIVATE third_party/streamline/include)
    target_compile_definitions(uaro-render64 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX WIDL_EXPLICIT_AGGREGATE_RETURNS)
    target_compile_options(uaro-render64 PRIVATE -Wall -Wextra -Wno-unknown-pragmas)  # Streamline's #pragma region
    target_link_libraries(uaro-render64 PRIVATE uagfx_core)
    target_link_options(uaro-render64 PRIVATE -static -static-libgcc -static-libstdc++ -s)

    # 2.3: uaro-studio.exe, uaRO Studio (docs/superpowers/specs/2026-10-18-texture-studio-design.md) - 64-bit, its
    # engine a copy of uaro-render64.exe; the settings app's UI kit and texts.
    file(GLOB UAGFX_STUDIO_SOURCES CONFIGURE_DEPENDS src/studio/*.cpp)
    add_executable(uaro-studio WIN32 ${UAGFX_STUDIO_SOURCES} src/app/ui.cpp src/app/text.cpp src/app/utf16.cpp
                   src/app/mnemonic.cpp src/app/pack_compact.cpp src/win/paths.cpp src/studio/studio.rc)
    target_include_directories(uaro-studio PRIVATE ${CMAKE_BINARY_DIR}/app)
    target_compile_definitions(uaro-studio PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(uaro-studio PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-studio PRIVATE uagfx_core comctl32 shell32 ole32 uuid)
    target_link_options(uaro-studio PRIVATE -static -static-libgcc -static-libstdc++ -s -municode)
elseif(WIN32)
    # ddraw.dll that uaRO.exe loads from its own folder instead of dgVoodoo's or the system one.
    file(GLOB_RECURSE UAGFX_WIN_SOURCES CONFIGURE_DEPENDS src/win/*.cpp)
    # 2.9: uaRO Pad's Windows part (DirectInput 7 wrappers, the client's memory, its windows), mounted by the driver
    file(GLOB UAGFX_PAD_WIN_SOURCES CONFIGURE_DEPENDS src/pad/win/*.cpp)
    # 3.0.11-beta.66: IBM Plex Sans (third_party/plex) as its resources - the new interface's words (win/ui_font)
    # 3.0.11-beta.178: the classes' portraits of the login screen's cards too (src/win/class_art.rc)
    # 3.0.18-beta.7: RmlUi 6.3's core (third_party/rmlui, MIT; its FreeType font engine left out - ours is GDI's):
    # the interface library's spike (win/uaui, Claude Doc «Концепция: единая библиотека интерфейса uaRO Graphics»)
    # (3.0.18-beta.94: uagfx_rmlui and uagfx_uikit are made with the core, above - the host tests use them too)
    add_library(ddraw SHARED ${UAGFX_WIN_SOURCES} ${UAGFX_PAD_WIN_SOURCES} src/win/ddraw.def src/win/ui_font.rc
                src/win/class_art.rc)
    target_include_directories(ddraw PRIVATE third_party/plex third_party/class_art)  # the .rc's files
    set_target_properties(ddraw PROPERTIES PREFIX "" OUTPUT_NAME "ddraw")
    target_compile_definitions(ddraw PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX WIDL_EXPLICIT_AGGREGATE_RETURNS
                               DIRECTINPUT_VERSION=0x0700)
    target_compile_options(ddraw PRIVATE -Wall -Wextra)
    target_link_libraries(ddraw PRIVATE uagfx_pad uagfx_uikit uagfx_core uagfx_rmlui uuid)
    # 2.5.5: its link map beside it - the profile's places in ddraw.dll (offsets) are found in it afterwards
    target_link_options(ddraw PRIVATE -static -static-libgcc -static-libstdc++ -s
                        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/ddraw.map")

    # 3.0.1-beta.5: our own libraries in place of the client's old ones (docs/superpowers/specs/
    # 2026-10-23-own-client-libraries-design.md) - uaRO Graphics puts them in the game's root, the originals aside.
    add_library(cps SHARED src/clientlibs/cps.cpp src/clientlibs/cps.def)  # zlib's two functions over libdeflate
    set_target_properties(cps PROPERTIES PREFIX "" OUTPUT_NAME "cps")
    target_compile_options(cps PRIVATE -Wall -Wextra)
    target_link_libraries(cps PRIVATE uagfx_core)  # core/client_libs: zlib's two, tested on the host
    target_link_options(cps PRIVATE -static -static-libgcc -static-libstdc++ -s)
    add_library(ijl15 SHARED src/clientlibs/ijl15.cpp src/clientlibs/ijl15.def)  # 3.0.1-beta.6: JPEG over stb
    set_target_properties(ijl15 PROPERTIES PREFIX "" OUTPUT_NAME "ijl15")
    target_compile_options(ijl15 PRIVATE -Wall -Wextra)
    target_link_libraries(ijl15 PRIVATE uagfx_core)
    target_link_options(ijl15 PRIVATE -static -static-libgcc -static-libstdc++ -s)
    add_library(binkw32 SHARED src/clientlibs/binkw32.cpp src/clientlibs/binkw32.def)  # Bink: no video
    set_target_properties(binkw32 PROPERTIES PREFIX "" OUTPUT_NAME "binkw32")
    target_compile_options(binkw32 PRIVATE -Wall -Wextra)
    target_link_options(binkw32 PRIVATE -static -static-libgcc -static-libstdc++ -s)
    # 3.0.1-beta.7: Miles Sound System 6 over XAudio2 2.9 (core/miles; minimp3 for the music)
    add_library(mss32 SHARED src/clientlibs/mss32.cpp src/win/audio_devices.cpp src/clientlibs/mss32.def)
    set_target_properties(mss32 PROPERTIES PREFIX "" OUTPUT_NAME "mss32")
    target_compile_options(mss32 PRIVATE -Wall -Wextra)
    target_link_libraries(mss32 PRIVATE uagfx_core)
    target_link_options(mss32 PRIVATE -static -static-libgcc -static-libstdc++ -s)

    # The settings app's Windows layer (stage 2 spec §3.3): files, registry, system, its model.
    add_library(uagfx_app STATIC src/app/text.cpp src/app/utf16.cpp src/app/files.cpp src/app/app_disk.cpp
                src/app/registry.cpp src/app/system.cpp src/app/model.cpp src/app/rebuilds.cpp src/app/mnemonic.cpp
                src/app/ai_cache.cpp src/app/update.cpp src/app/report_send.cpp src/app/pack_compact.cpp
                src/app/shots.cpp src/app/palettes.cpp src/app/installer.cpp src/app/update_run.cpp src/app/download.cpp src/app/args.cpp
                src/app/launcher.cpp src/app/launcher_thread.cpp src/app/shortcuts.cpp src/app/reports.cpp
                src/app/check_notify.cpp src/app/reference_pack.cpp
                src/app/client_patch.cpp src/win/system_probe.cpp src/win/gpu/adapters.cpp src/win/audio_devices.cpp)
    target_include_directories(uagfx_app PUBLIC src)
    target_compile_definitions(uagfx_app PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
                               WIDL_EXPLICIT_AGGREGATE_RETURNS)
    target_compile_options(uagfx_app PRIVATE -Wall -Wextra)
    # 3.0.11-beta.89: the page «Контролер» edits the pad's layouts with its own core (bindings, i18n)
    target_link_libraries(uagfx_app PUBLIC uagfx_core uagfx_pad comctl32 shell32 ole32 uuid advapi32 winhttp)

    # uaro-graphics.exe, the settings app (stage 2 spec): its version comes from src/core/version.cpp.
    add_executable(uaro-graphics WIN32 src/app/main.cpp src/app/main_window.cpp src/app/ui.cpp src/app/page.cpp
                   src/app/page_settings.cpp src/app/page_screen.cpp src/app/page_diagnostics.cpp
                   src/app/page_service.cpp src/app/page_ai.cpp src/app/page_ai_cache.cpp
                   src/app/page_effects.cpp src/app/page_pack.cpp src/app/page_pad.cpp src/app/preview_window.cpp
                   src/app/page_parts.cpp src/app/page_debug.cpp src/app/page_main.cpp src/app/status_info.cpp
                   src/app/page_search.cpp src/app/notes_window.cpp src/app/update_window.cpp src/app/modes.cpp
                   src/app/page_help.cpp src/app/reports_window.cpp src/app/reports_thread.cpp
                   src/app/client_patch_thread.cpp src/app/chat_view.cpp src/app/launcher_view.cpp
                   src/app/app.rc)
    # 3.0.18-beta.193: the launcher's fonts (third_party/launcher_fonts) - the .rc's files
    target_include_directories(uaro-graphics PRIVATE src/app dist ${CMAKE_BINARY_DIR}/app third_party/launcher_fonts)
    target_compile_options(uaro-graphics PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
    target_link_libraries(uaro-graphics PRIVATE uagfx_app comdlg32)
    target_link_options(uaro-graphics PRIVATE -static -static-libgcc -static-libstdc++ -s -municode)
else()
    enable_testing()
    file(GLOB UAGFX_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    # The settings app's model runs here too, over the stand-ins of tests/fake_windows.cpp for its Windows layer.
    add_executable(uagfx_tests ${UAGFX_TEST_SOURCES} src/app/model.cpp src/app/app_disk.cpp src/app/text.cpp
                   src/app/rebuilds.cpp src/app/mnemonic.cpp src/app/installer.cpp src/app/update_run.cpp src/app/args.cpp
                   src/app/launcher.cpp src/app/reports.cpp src/app/client_patch.cpp src/app/reference_pack.cpp)
    target_include_directories(uagfx_tests PRIVATE tests)
    target_link_libraries(uagfx_tests PRIVATE uagfx_uikit uagfx_core uagfx_pad)
    target_compile_definitions(uagfx_tests PRIVATE UAGFX_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME uagfx_tests COMMAND uagfx_tests)
endif()
