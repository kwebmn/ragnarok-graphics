#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/config.hpp"

// 3.0.22-beta.5 (the owner's «ок» to idea 4: 9 of 16 computers are laptops, the Radeon R7 M370 gave 39 frames a
// second): the set «Для ноутбука» - «Картинка» «Авто», «Текстури» «HD економно» and [frames] battery_saver (on the
// battery 30 frames and the weak set's effects; plugged in nothing more). The first start's card offers it on a laptop;
// «Гра» offers it once the game measured the laptop slow, and then tells what it gave - the frames the game measured
// with the set and without it, plugged in (the battery saver's 30 frames are no measure), on the maps, in front.
// The driver measures (uaro-graphics\cache\laptop-frames.txt), the app keeps the try (laptop-try.txt).
namespace uagfx::laptopset {

std::vector<std::pair<std::string, std::string>> keys();  // "section.key" -> the token it writes
bool on(const Config& c);                                 // every key of it as the set has it

struct Frames {
    double fps = 0, minutes = 0;
};
struct Record {
    Frames other, laptop;  // without the set, with it
};
constexpr double kMostMinutes = 30.0;  // a rolling half hour each
class Meter {
public:
    void frame(double seconds, bool laptopSet);  // a frame counted: its seconds
    const Record& record() const { return record_; }
    void set(const Record& r);  // from the file, gone on from
private:
    struct Sum {
        double frames = 0, seconds = 0;
    };
    Sum sums_[2];
    Record record_;
};
std::string format(const Record& r);  // "other 39.2 12.5\nlaptop 52.0 7.0\n"
bool parse(const std::string& text, Record& out);

struct Try {
    double before = -1;  // the frames without the set when it was tried; < 0 never tried
    bool told = false, declined = false;
    std::vector<std::pair<std::string, std::string>> undo;  // the keys' tokens before it
};
std::string format(const Try& t);
bool parse(const std::string& text, Try& out);

enum class Plate : uint8_t { None, Offer, Better, NoBetter };
constexpr double kSlowFps = 45.0, kEnoughMinutes = 5.0, kBetterBy = 3.0;
// «Гра»'s plate: the offer (a laptop measured slow without the set, never tried nor declined), what the try gave (the
// set on, measured long enough, not told yet)
Plate plate(bool laptop, bool setOn, const Record& r, const Try& t);

}  // namespace uagfx::laptopset
