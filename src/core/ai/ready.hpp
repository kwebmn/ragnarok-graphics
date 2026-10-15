#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/ai/kinds.hpp"
#include "core/config.hpp"

namespace uagfx::ai {

// 0.47: the AI textures' ready pictures - the texture pack's (uaro-textures.zip) and upscaled.grf's - for a renderer
// that makes none itself (Direct3D 11). A game texture's picture is looked up by its key (keyName) as Direct3D 12's
// AiTextures looks it up: the pack first (0.45: a texture at a lower texture detail takes its whole texture's
// picture), then upscaled.grf's folder of the kind's network settings (netFolder, kindFolder).
class ReadyPictures {
public:
    struct Source {
        std::string name;   // the picture's entry
        bool pack = false;  // the pack's (a WebP, 4 times the game texture); else upscaled.grf's (its scale times)
    };
    explicit ReadyPictures(const AiSettings& settings);
    // The pack's entries (has) and its index.txt (the kinds of its pictures, the keys of a lower texture detail).
    void addPack(std::function<bool(const std::string&)> has, const std::string& index);
    // upscaled.grf's entries: the pictures of these settings, the kinds' tags.
    void addGrf(const std::vector<std::string>& names);
    bool kindOn(Kind kind) const { return int(kind) < kKinds && on_[int(kind)]; }
    bool anyCells() const;  // sprites, effects or icons: the frames of the game's sprite cache are looked up
    // The kind a texture is known as before it is drawn (the pack's index, upscaled.grf's tags) - with [ai] ahead.
    bool knownKind(const std::string& key, Kind& kind) const;
    bool texture(const std::string& key, Kind kind, Source& out) const;  // drawn as kind (Icons: the interface's)
    bool cell(const std::string& key, Source& out) const;  // a frame of the game's sprite cache
    bool icon(const std::string& key, Source& out) const;  // a picture of a window (crisp text's twins)
    size_t grfPictures() const { return grf_.size(); }
    size_t otherPictures() const { return other_; }  // upscaled.grf's of other settings
    size_t ahead() const { return kinds_.size(); }
    size_t aliases() const { return aliases_.size(); }

private:
    bool find(const std::string& packed, const std::string& grf, Source& out) const;

    AiSettings settings_;
    bool on_[kKinds] = {};
    std::string folders_[kKinds];  // upscaled.grf's folder of each kind's pictures
    std::function<bool(const std::string&)> packHas_;
    std::unordered_map<std::string, Kind> kinds_;           // a key -> its kind, known ahead
    std::unordered_map<std::string, std::string> aliases_;  // a key of a lower texture detail -> the pack's picture
    std::unordered_set<std::string> grf_;                   // upscaled.grf's pictures of these settings
    size_t other_ = 0;
};

}  // namespace uagfx::ai
