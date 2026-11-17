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
    // their labels until 3.0.21-beta.5 (the concept «Настройки uaRO Graphics», question 5: names that say what
    // is seen; the inner words out of them)
    {"view3d.own_models", {"Models by our renderer", "Моделі нашим рендером", "Модели нашим рендером"}},
    {"effects.own_ground_card", {"Ground on the graphics card", "Земля на відеокарті", "Земля на видеокарте"}},
    {"effects.leaves_card", {"Leaves on the graphics card", "Листя на відеокарті", "Листва на видеокарте"}},
    {"quality.batch_ground", {"Ground in batches", "Земля пачками", "Земля пачками"}},
    {"client.roext",
     {"Window, mouse and keys: ROExt's work in the driver (dinput8.dll)",
      "Вікно, миша й клавіші — робота ROExt у драйвері (dinput8.dll)",
      "Окно, мышь и клавиши — работа ROExt в драйвере (dinput8.dll)"}},
    {"client.cps",
     {"Files' compression: our zlib (cps.dll)", "Стиснення файлів гри — свій zlib (cps.dll)",
      "Сжатие файлов игры — свой zlib (cps.dll)"}},
    {"client.bink",
     {"Bink video: none (binkw32.dll)", "Відео Bink — без нього (binkw32.dll)",
      "Видео Bink — без него (binkw32.dll)"}},
    {"client.ijl",
     {"JPEG pictures: ours (ijl15.dll)", "Картинки JPEG — свої (ijl15.dll)",
      "Картинки JPEG — свои (ijl15.dll)"}},
    {"client.mss", {"Game sound: ours (mss32.dll)", "Звук гри — свій (mss32.dll)", "Звук игры — свой (mss32.dll)"}},
    {"renderer.bridge", {"64-bit renderer", "64-бітний рендер", "64-битный рендер"}},
    {"renderer.cross_present",
     {"Frames through the screen's graphics card", "Кадр через відеокарту екрана",
      "Кадр через видеокарту экрана"}},
    {"quality.taa", {"TAA (any graphics card)", "TAA (будь-яка відеокарта)", "TAA (любая видеокарта)"}},
    {"interface.uaui_motion",
     {"Smooth motion (new library)", "Плавний рух (нова бібліотека)",
      "Плавное движение (новая библиотека)"}},
    {"interface.uaui_effects", {"Effects (new library)", "Ефекти (нова бібліотека)", "Эффекты (новая библиотека)"}},
    {"interface.uaui_glass",
     {"Frosted glass windows (new library)", "Скляні вікна (нова бібліотека)",
      "Стеклянные окна (новая библиотека)"}},
    {"interface.pack_icons",
     {"Icons from the AI texture pack", "Іконки з пака текстур ШІ",
      "Иконки из пака текстур ИИ"}},
    {"sound.engine", {"Surround sound (new engine)", "Об'ємний звук (новий рушій)", "Объёмный звук (новый движок)"}},
    {"view3d.shadows",
     {"Shadows of houses and trees (trial)", "Тіні від будинків і дерев (проба)",
      "Тени от домов и деревьев (проба)"}},
    {"interface.predict_walk",
     {"Walk without waiting for the server (an experiment)", "Хода без очікування сервера (експеримент)",
      "Ходьба без ожидания сервера (эксперимент)"}},
    {"interface.minimap_reach",
     {"Light where you can walk (an experiment)", "Підсвітити, куди можна дійти (експеримент)",
      "Подсветить, куда можно дойти (эксперимент)"}},
    {"display.dpi_aware",
     {"Window with Windows scaling (test)", "Вікно з масштабом Windows (тест)",
      "Окно с масштабом Windows (тест)"}},
    {"renderer.remix", {"RTX Remix (experiment)", "RTX Remix (експеримент)", "RTX Remix (эксперимент)"}},
    {"renderer.render_thread",
     {"Direct3D 11 on a thread of its own (test)", "Direct3D 11 у власному потоці (перевірка)",
      "Direct3D 11 в своём потоке (проверка)"}},
    // until 3.0.21-beta.7: Alt+D opened the console (the laboratory now)
    {"interface.detail_console",
     {"Alt+D opens the graphics console", "Alt+D відкриває консоль графіки", "Alt+D открывает консоль графики"}},
    // until 3.0.21-beta.7: a card of the interface's look, [interface] options
    {"interface.settings_window", {"Game options card", "Картка налаштувань гри", "Карточка настроек игры"}},
};

}  // namespace

const char* oldName(const std::string& sectionKey, Lang l) {
    for (const Old& o : kOld)
        if (sectionKey == o.key) return o.names[int(l)];
    return nullptr;
}

}  // namespace uagfx::settings
