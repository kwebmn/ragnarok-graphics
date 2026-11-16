#pragma once
#include <string>
#include <vector>

#include "core/ground_heights.hpp"
#include "core/rsm.hpp"
#include "core/rsw.hpp"

namespace uagfx::cave_vault {

// 3.0.19-beta.32 (the 3D view's concept https://claude.ai/artifact/RadnVxG35hjE9d8HgdqzAy, the owner's decision 4:
// «Подземелья: свод из отражённого рельефа пещеры и туман, без отдельной настройки плотности»; its picture: «каменный
// свод: рельеф пещеры, отражённый вверх. Над стенами он опускается к ним, над проходами поднимается куполом»): a
// dungeon's vault from its ground - each cell's height mirrored up about the walls' top (their kWallShare percentile),
// at least kLeastOver over a wall's top and kLeastRoom over a passage's floor, kMostRoom at most, smoothed over
// kSmoothCells each way; a cave whose walls are its models (its ground within kLeastRelief) a plain dome kFlatRoom
// over its floor. The renderers' 3D sky (sky3d_ps) meets it on each ray where nothing is drawn.
struct Vault {
    int width = 0, height = 0;  // the ground's cells
    float zoom = 10.0f;         // units a cell
    std::vector<float> up;      // over each cell's middle, up positive (the ground's -y), row by row
    float wallTop = 0.0f;       // the walls' top it is mirrored about (up positive)
    float floorLevel = 0.0f;    // the passages' (the heights' kFloorShare percentile)
    float ceiling = 0.0f;       // 3.0.20-beta.52: a ceiling's height (buildCeiling); 0: a dungeon's vault
};
constexpr float kWallShare = 0.92f, kFloorShare = 0.2f;
constexpr float kLeastOver = 6.0f, kLeastRoom = 48.0f, kMostRoom = 160.0f, kLeastRelief = 12.0f, kFlatRoom = 80.0f;
constexpr int kSmoothCells = 3;
bool build(const ground_heights::Ground& g, Vault& out);
// 3.0.20-beta.52 (the owner's «делай 1-9», 5 - ceilings indoors): a house's or a castle's hall (ceilingFor: not under
// the sky, no dungeon) had a dark nothing over its walls in the 3D view. A ceiling - flat: kCeilingOver over the
// walls' top where the map's ground makes its walls (its relief kLeastRelief at least); where its walls are models,
// over their height from its floor (modelsTop: wallsByModels of its tall models, 0 none known - kCeilingFlat then).
// Over a wall higher than it: kLeastOver over that wall
constexpr float kCeilingOver = 2.0f, kCeilingFlat = 44.0f;
bool buildCeiling(const ground_heights::Ground& g, float modelsTop, Vault& out);
// the walls' height by a map's models: the 80th of a hundred of the heights given (its models that stand kPostLeast
// tall at least - walls, pillars, tall shelves); none: 0
float wallsByModels(std::vector<float> tall);
// a model's height as the map stands it (its faces' corners, rsw::placeInMap); 0 none
float tallOf(const rsm::Model& model, const rsw::Model& at);
// the maps given a ceiling: a house's or a hall's by its name - «_in» in it or «in_» before it, a church, a
// castle's inside («castle», Glast Heim's gl_cas and gl_knt), a tower, the pyramids («pryd») - while not under the
// sky and no dungeon. The rest of what outdoors() takes for a roof keeps its dark sky: a guild castle is mostly
// courtyards, an arena, a maze or a graveyard has no roof
bool ceilingFor(const std::string& map, bool outdoors);
// 3.0.20-beta.46 (R-3T9MZC «колоны как будто в воздухе», pay_dun00: its pillars 외부소품\동굴리얼기둥.rsm stand 31-34
// tall - the walls' height, 33 - their flared heads' black caps hanging 15-30 under a vault mirrored up to 48-66 over
// the passage): the vault comes down to the posts that reach the walls' height - a funnel of rock round each one's
// head, its cap kPostSink within the rock, the vault's own height again kPostSpread past the head's edge.
// A post: a model its makers named a pillar («기둥») or a stalactite («종유석»), standing tall (kPostLeast), no wider
// than tall, its top tenth at least kPostHead of its widest across both ways - a pillar's head; a stalagmite ends in
// a point, a boulder is wider than tall. By the name too: in the census of the 113 dungeon maps' models
// (/root/uagfx-tools/audit/vault) the shape alone took statues, a clock, curtains, book shelves and trees as well
struct Post {
    float x = 0.0f, z = 0.0f;  // its head's middle, the map file's place
    float radius = 0.0f;       // its head's half width (the wider way)
    float top = 0.0f;          // up positive
};
constexpr float kPostLeast = 20.0f, kPostHead = 0.5f, kPostReach = 0.75f;
constexpr float kPostSink = 1.5f, kPostPad = 2.0f, kPostSpread = 22.0f;
constexpr int kFine = 4;  // the vault's heights a cell's side once it has posts (a funnel some 20 units across)
// Whether a model's file (as the map names it, cp949 bytes) is named a pillar or a stalactite - the words whole, by
// the name's own characters
bool postName(const std::string& file);
// A model as the map stands it (rsw::placeInMap of its faces' corners): whether a post, and which
bool postOf(const rsm::Model& model, const rsw::Model& at, Post& out);
// The vault's heights `times` a cell's side: the same vault between the cells' middles (heightAt's), a crease where
// two cells' slopes met smoothed away
void refine(Vault& v, int times);
// The vault brought down onto the posts whose tops reach kPostReach of the way from their floor to the walls' top and
// that it lies over; the number of them. None under a plain dome (no walls' height known)
size_t restOn(Vault& v, const ground_heights::Ground& g, const std::vector<Post>& posts);
// A map's vault with its posts: build's, and where some post reaches it refined by kFine and brought down onto them
// (met: how many); no such post - build's own, a height a cell
bool build(const ground_heights::Ground& g, const std::vector<Post>& posts, Vault& out, size_t* met = nullptr);
// Over a place of the map file (x, z: the .rsw's, the map's middle at 0), between the cells' middles; false off it
bool heightAt(const Vault& v, float x, float z, float& up);
// The maps given one: the dungeons' by their names («dun»: pay_dun00, gef_dun01, orcsdun01 ...; anthell) - a house's
// or a castle's hall indoors keeps its dark sky
bool forMap(const std::string& map);
// 3.0.19-beta.38 (R-S3NQBF: the vault near black, the walls under it three times lighter): its rock's colour by the
// map's own light (its ambient and a share of its diffuse, as the game lights its ground) on a warm stone; none known
// - kRockUnlit
constexpr float kRock[3] = {0.62f, 0.55f, 0.46f}, kRockUnlit[3] = {0.18f, 0.16f, 0.14f};
void rockOf(bool lightKnown, const float ambient[3], const float diffuse[3], float out[3]);

}  // namespace uagfx::cave_vault
