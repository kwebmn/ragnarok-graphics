#include "core/map_objects.hpp"
#include "core/sprite_exe.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(map_objects_files_of_the_effects_folder) {
    // 0.24: the maps' objects the driver draws anew, known by their sprite files (EUC-KR names under 이팩트\).
    using objects::Kind;
    using sprite_exe::objectFile;
    const std::string effect = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\";  // 이팩트
    CHECK(objectFile((effect + "torch_01.spr").c_str()) == objects::File::Torch);
    CHECK(objectFile((effect + "\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2.spr").c_str()) == objects::File::Smoke);  // 굴뚝연기
    CHECK(objectFile("\xC0\xCC\xC6\xD1\xC6\xAE\\TORCH_01.SPR") == objects::File::Torch);  // relative, any case
    CHECK(objectFile((effect + "particle1.spr").c_str()) == objects::File::None);
    CHECK(objectFile("data\\sprite\\\xB8\xF3\xBD\xBA\xC5\xCD\\torch_01.spr") == objects::File::None);  // not effects
    CHECK(objectFile(nullptr) == objects::File::None);
    // A file's 32-bit frames: the torch's first is its glow, the rest its flame; the smoke's all a puff.
    CHECK(objects::kindOf(objects::File::Torch, 0) == Kind::TorchGlow);
    CHECK(objects::kindOf(objects::File::Torch, 3) == Kind::TorchFlame);
    CHECK(objects::kindOf(objects::File::Smoke, 0) == Kind::Smoke);
    CHECK(objects::kindOf(objects::File::None, 0) == Kind::None);
}

TEST(map_objects_textures_are_known_until_released) {
    objects::Textures t;
    CHECK(t.empty());
    t.note(9, objects::Kind::TorchFlame);
    t.note(10, objects::Kind::Smoke);
    CHECK(t.of(9) == objects::Kind::TorchFlame);
    CHECK(t.of(10) == objects::Kind::Smoke);
    CHECK(t.of(11) == objects::Kind::None);
    t.note(9, objects::Kind::None);  // another picture now
    CHECK(t.of(9) == objects::Kind::None);
    t.forget(10);
    CHECK(t.empty());
}
