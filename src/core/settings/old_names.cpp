#include "core/settings/old_names.hpp"

namespace uagfx::settings {
namespace {

struct Old {
    const char* key;
    const char* names[kLangCount];  // English, Ukrainian, Russian
};

// Their labels until 2.6.21
const Old kOld[] = {
    {"effects.day_cycle", {"Time of day", "Час доби", "Время суток"}},
    {"effects.night_windows", {"Windows at night", "Вікна вночі", "Окна ночью"}},
    {"effects.no_server_night", {"No server night", "Без ночі сервера", "Без ночи сервера"}},
    {"effects.wet_ground", {"Wet ground in the rain", "Мокра земля в дощ", "Мокрая земля в дождь"}},
    {"effects.rain_wet", {"Wet in the rain", "Мокре під дощем", "Мокрое под дождём"}},
    {"effects.actor_shadows", {"Characters' shadows", "Тіні персонажів", "Тени персонажей"}},
    {"effects.sprite_light", {"Living characters", "Живі персонажі", "Живые персонажи"}},
    {"effects.round_light", {"Round characters", "Об’ємні персонажі", "Объёмные персонажи"}},
    {"effects.rim_light", {"Rim light on characters", "Контрове світло на персонажах", "Контровой свет на персонажах"}},
    {"effects.footprints", {"Footprints", "Сліди", "Следы"}},
    {"effects.breath", {"Breath in the cold", "Пара в холод", "Пар на морозе"}},
    {"effects.skills_soft", {"Softer skills", "М'якші вміння", "Мягкие умения"}},
    {"effects.fireflies", {"Fireflies", "Світлячки", "Светлячки"}},
    {"effects.glow", {"Glow of effects", "Світіння ефектів", "Свечение эффектов"}},
    {"effects.shafts", {"Light in the air", "Світло в повітрі", "Свет в воздухе"}},
    {"effects.materials", {"Materials of the world", "Матеріали світу", "Материалы мира"}},
    {"effects.ground_seams", {"Smooth seams of the ground", "Плавні стики землі", "Плавные стыки земли"}},
    {"effects.smooth_hills", {"Smooth hills", "Плавні пагорби", "Плавные холмы"}},
    {"effects.ao", {"Soft shadows", "М'які тіні", "Мягкие тени"}},
    {"effects.floor_reflections", {"Reflections on the floor", "Відбиття на підлозі", "Отражения на полу"}},
    {"effects.tree_tints", {"Every tree its own", "Кожне дерево своє", "Каждое дерево своё"}},
    {"effects.splashes", {"Splashes", "Бризки", "Брызги"}},
    {"effects.underwater", {"Under the sea", "Під водою", "Под водой"}},
    {"effects.night_fireflies", {"Fireflies in the grass", "Світлячки в траві", "Светлячки в траве"}},
    {"effects.wet_walls", {"Damp cave walls", "Вологі стіни печер", "Влажные стены пещер"}},
    {"effects.cave_drips", {"Drips in the caves", "Краплі в печерах", "Капли в пещерах"}},
    {"effects.lit_dust", {"Dust in the light", "Пил у світлі", "Пыль в свете"}},
    {"effects.motes", {"Motes in the air", "Порошинки в повітрі", "Частицы в воздухе"}},
    {"effects.smooth_light", {"Smooth map light", "М'яке світло карти", "Мягкий свет карты"}},
    {"effects.soft_particles", {"Soft effects at the ground", "М’які ефекти біля землі", "Мягкие эффекты у земли"}},
};

}  // namespace

const char* oldName(const std::string& sectionKey, Lang l) {
    for (const Old& o : kOld)
        if (sectionKey == o.key) return o.names[int(l)];
    return nullptr;
}

}  // namespace uagfx::settings
