#pragma once
#include <cstdint>

#include "core/horizon.hpp"

// 3.0.22-beta.11: the CPU twin of the horizon bake's land (shaders.cpp `hz...`, the same math line for line) - the
// tests and the preview tool (tools/horizon_preview) see what the card draws. Panorama space: the map's middle at 0,
// x and z along the world's ground, y UP (the shader's world y points down: it turns the ray once, at the start).
namespace uagfx::horizon {

uint32_t hash2(int x, int y, uint32_t seed);  // an integer hash, the HLSL twin's bit for bit
struct Noise {
    float v = 0, dx = 0, dz = 0;  // value noise in [-1, 1] and its derivatives
};
Noise noised(float x, float z, uint32_t seed);
float fbm(float x, float z, int octaves, uint32_t seed);
float eroded(float x, float z, int octaves, uint32_t seed);  // derivative-damped fbm: smooth valleys, sharp ridges

constexpr float kLandRise = 1400.0f;  // past the map's edge the land rises from 0 to whole over this
constexpr float kRoof = 0.30f;        // no land higher than this share of its distance (under kRiseHigh)
float heightAt(const Climate& c, float mapRadius, float x, float z);  // units up; 0 within mapRadius; < 0 the sea
struct Vec3 {
    float x = 0, y = 0, z = 0;
};
Vec3 normalAt(const Climate& c, float mapRadius, float x, float z, float eps);
// upness: the normal's up part (1 flat, 0 a wall)
Rgb albedoAt(const Climate& c, float x, float z, float height, float upness);

// A ray from the eye over the map's middle (dir unit, y up), from the map's edge out: a hit, or the least angular
// clearance's coverage (the skyline's soft edge over pixelAngle) with the place it passed closest
struct Hit {
    bool hit = false;
    float distance = 0, height = 0, coverage = 0;
    float x = 0, z = 0;
};
Hit march(const Climate& c, float mapRadius, float eyeHeight, float dirX, float dirUp, float dirZ, int steps,
          float pixelAngle);
// The sun's soft shadow at a point of the land (1 lit, 0 in shadow); steps 0: 1
float shadowAt(const Climate& c, float mapRadius, Vec3 at, Vec3 normal, Vec3 toSun, int steps);
// How much of the haze's colour lies over land this far and this high (0..1)
float hazeOf(const Climate& c, float distance, float height);

}  // namespace uagfx::horizon
