#pragma once
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "core/ai/kinds.hpp"
#include "core/config.hpp"
#include "core/near_picture.hpp"
#include "core/texture_pack.hpp"

namespace uagfx::ai {

// 0.47: the AI textures' ready pictures - the texture pack's (uaro-textures.zip; 0.48: upscaled.grf no more) - looked
// up by a game texture's key (keyName): the world's, the sprite cache's frames' and the interface's groups (0.45: a
// texture at a lower texture detail takes its whole texture's picture). Direct3D 11 shows what it finds; Direct3D 12
// makes what it does not, when [ai] generate is on, and writes it into the pack (addMade).
class ReadyPictures {
public:
    explicit ReadyPictures(const AiSettings& settings);
    // The pack's entries (has) and its index.txt (the kinds of its pictures, the keys of a lower texture detail).
    void addPack(std::function<bool(const std::string&)> has, const std::string& index);
    void addMade(const std::string& name);              // a picture this game made and writes into the pack
    void noteKind(const std::string& key, Kind kind);   // drawn as kind: known ahead from now on (the first stays)
    bool kindOn(Kind kind) const { return int(kind) < kKinds && on_[int(kind)]; }
    bool anyCells() const;  // sprites, effects or icons: the frames of the game's sprite cache are looked up
    // The kind a texture is known as before it is drawn (the pack's index, what was drawn) - with [ai] ahead.
    bool knownKind(const std::string& key, Kind& kind) const;
    // The picture's name in the pack of a texture drawn as kind (Icons: the interface's), of a frame of the sprite
    // cache, of a picture of a window (crisp text's twins) - false when the pack has none.
    bool texture(const std::string& key, Kind kind, std::string& name) const;
    bool cell(const std::string& key, std::string& name) const;
    bool icon(const std::string& key, std::string& name) const;
    // 0.49: a texture the game decoded itself (a .jpg) by its likeness to the pack's ones of its size - drawn as kind:
    // the interface's pictures for Icons, the world's for the rest.
    bool hasAlike(int width, int height) const { return alike_.has(width, height); }
    bool alike(const uint32_t* argb, int width, int height, Kind kind, std::string& name) const;
    // The same in two steps - the likeness found as the texture is uploaded, the kind known when it is drawn.
    bool alikeName(const uint32_t* argb, int width, int height, std::string& name) const;
    bool alikeFits(const std::string& name, Kind kind) const;
    size_t ahead() const { return kinds_.size(); }
    // 2.3, uaRO Studio's "the game's own always" (the index's '!' lines): no picture for these keys.
    bool marked(const std::string& key) const { return marked_.count(key) != 0; }
    // 2.3: a texture's material scaled ('#' lines); false and 1, 1, 1 (as built) when it is not
    bool materialOf(const std::string& key, pack::MaterialScales& out) const;
    size_t aliases() const { return aliases_.size(); }

private:
    bool has(const std::string& name) const { return (packHas_ && packHas_(name)) || made_.count(name) != 0; }

    AiSettings settings_;
    bool on_[kKinds] = {};
    std::function<bool(const std::string&)> packHas_;
    std::unordered_set<std::string> made_;
    std::unordered_map<std::string, Kind> kinds_;           // a key -> its kind, known ahead
    std::unordered_map<std::string, std::string> aliases_;  // a key of a lower texture detail -> the pack's picture
    pack::NearPictures alike_;  // 0.49
    std::unordered_set<std::string> marked_;  // 2.3
    std::unordered_map<std::string, pack::MaterialScales> materials_;  // 2.3
};

}  // namespace uagfx::ai
