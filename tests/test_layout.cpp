// 2.8: where uaRO Graphics keeps its files in the game's folder (core/layout) - all in uaro-graphics\.
#include <set>
#include <string>

#include "core/layout.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(layout_every_file_of_ours_lives_in_uaro_graphics) {
    for (int i = 0; i < layout::kFileCount; ++i) {
        const std::string now = layout::now(layout::File(i));
        CHECK(now.rfind("uaro-graphics\\", 0) == 0);
        CHECK(now != layout::before(layout::File(i)));
    }
}

TEST(layout_places_of_the_files_2x_kept_in_the_root) {
    CHECK_EQ(std::string(layout::before(layout::File::Ini)), std::string("uaro-graphics.ini"));
    CHECK_EQ(std::string(layout::now(layout::File::Ini)), std::string("uaro-graphics\\uaro-graphics.ini"));
    CHECK_EQ(std::string(layout::now(layout::File::DriverLog)), std::string("uaro-graphics\\logs\\uaro-graphics.log"));
    CHECK_EQ(std::string(layout::before(layout::File::Pipelines)), std::string("uaro-graphics-pipelines.txt"));
    CHECK_EQ(std::string(layout::now(layout::File::Pipelines)), std::string("uaro-graphics\\cache\\pipelines.txt"));
    CHECK_EQ(std::string(layout::now(layout::File::Pack)), std::string("uaro-graphics\\textures\\uaro-textures.zip"));
    CHECK_EQ(std::string(layout::before(layout::File::Backups)), std::string("uaro-graphics-backup"));
    CHECK_EQ(std::string(layout::now(layout::File::DgVoodoo)), std::string("uaro-graphics\\dgvoodoo"));
    CHECK(layout::isFolder(layout::File::Backups) && !layout::isFolder(layout::File::Pack));
    CHECK_EQ(layout::notes("uk"), std::string("uaro-graphics\\notes\\TEST.uk.md"));
}

TEST(layout_locate_prefers_the_new_place_and_keeps_an_unmoved_file) {
    std::set<std::string> there;
    const auto exists = [&](const std::string& p) { return there.count(p) > 0; };
    const std::string fresh = "C:\\uaRO\\uaro-graphics\\textures\\uaro-textures.zip";
    CHECK_EQ(layout::locate("C:\\uaRO", layout::File::Pack, exists), fresh);  // a fresh install
    there.insert("C:\\uaRO\\uaro-textures.zip");
    CHECK_EQ(layout::locate("C:\\uaRO", layout::File::Pack, exists), std::string("C:\\uaRO\\uaro-textures.zip"));
    there.insert(fresh);
    CHECK_EQ(layout::locate("C:\\uaRO", layout::File::Pack, exists), fresh);
}

TEST(layout_a_2x_backup_path_goes_where_the_file_is_now) {
    CHECK_EQ(layout::relativeNow("uaro-graphics.ini"), std::string("uaro-graphics\\uaro-graphics.ini"));
    CHECK_EQ(layout::relativeNow("UARO-GRAPHICS.INI"), std::string("uaro-graphics\\uaro-graphics.ini"));
    CHECK_EQ(layout::relativeNow("dinput8.ini"), std::string("dinput8.ini"));
    CHECK_EQ(layout::relativeNow("uaro-graphics\\uaro-graphics.ini"), std::string("uaro-graphics\\uaro-graphics.ini"));
}

TEST(layout_the_folders_made_at_start_hold_the_caches_and_a_new_pack) {
    // what writes into them - the shader caches, the pipelines, a new texture pack, the logs - does not make them
    const std::set<std::string> made(std::begin(layout::kMadeAtStart), std::end(layout::kMadeAtStart));
    for (layout::File f : {layout::File::Shaders32, layout::File::Shaders64, layout::File::Pipelines,
                           layout::File::Pack, layout::File::DriverLog}) {
        const std::string now = layout::now(f);
        CHECK(made.count(now.substr(0, now.find_last_of('\\'))) > 0);
    }
}

TEST(layout_the_pads_files_live_in_uaro_graphics) {
    CHECK_EQ(std::string(layout::now(layout::File::PadIni)), std::string("uaro-graphics\\uaro-pad.ini"));
    CHECK_EQ(std::string(layout::before(layout::File::PadBinds)), std::string("uaro-pad-binds.ini"));
    CHECK_EQ(std::string(layout::now(layout::File::PadBinds)), std::string("uaro-graphics\\uaro-pad-binds.ini"));
    CHECK_EQ(std::string(layout::now(layout::File::PadLog)), std::string("uaro-graphics\\logs\\uaro-pad.log"));
}
