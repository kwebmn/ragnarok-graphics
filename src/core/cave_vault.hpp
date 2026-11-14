#pragma once
#include <string>
#include <vector>

#include "core/ground_heights.hpp"

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
};
constexpr float kWallShare = 0.92f, kFloorShare = 0.2f;
constexpr float kLeastOver = 6.0f, kLeastRoom = 48.0f, kMostRoom = 160.0f, kLeastRelief = 12.0f, kFlatRoom = 80.0f;
constexpr int kSmoothCells = 3;
bool build(const ground_heights::Ground& g, Vault& out);
// Over a place of the map file (x, z: the .rsw's, the map's middle at 0), between the cells' middles; false off it
bool heightAt(const Vault& v, float x, float z, float& up);
// The maps given one: the dungeons' by their names («dun»: pay_dun00, gef_dun01, orcsdun01 ...; anthell) - a house's
// or a castle's hall indoors keeps its dark sky
bool forMap(const std::string& map);

}  // namespace uagfx::cave_vault
