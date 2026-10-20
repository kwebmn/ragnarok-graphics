#include "core/critters.hpp"

#include <algorithm>
#include <cmath>

namespace uagfx::critters {
namespace {

constexpr float kPi = 3.14159265f;

float distXz(const Point& a, const Point& b) { return std::hypot(a.x - b.x, a.z - b.z); }

Sprite sprite(Kind k, const Point& at, float size, float alpha) {
    Sprite s;
    s.kind = k;
    s.at = at;
    s.size = size;
    s.alpha = alpha;
    return s;
}

}  // namespace

float World::random() {
    seed_ ^= seed_ << 13;
    seed_ ^= seed_ >> 17;
    seed_ ^= seed_ << 5;
    return float(seed_ & 0xffffff) / float(0x1000000);
}

bool fireOf(const torch::Flame& f, const camera::Projection& p, const float viewToWorld[12], Point& out) {
    if (f.rhw <= 0.0f) return false;
    const float vz = 1.0f / f.rhw, gx = f.baseX + f.upX * 0.8f, gy = f.baseY + f.upY * 0.8f;
    const float vx = (gx - p.cx) * vz / p.f0, vy = (gy - p.cy) * vz / p.f1;
    const float* m = viewToWorld;
    out = Point{vx * m[0] + vy * m[3] + vz * m[6] + m[9], vx * m[1] + vy * m[4] + vz * m[7] + m[10],
                vx * m[2] + vy * m[5] + vz * m[8] + m[11]};
    return true;
}

Point World::around(const Point& c, float near, float far) {
    const float a = random() * 2.0f * kPi, r = near + (far - near) * random();
    return Point{c.x + std::cos(a) * r, c.y, c.z + std::sin(a) * r};
}

void World::setPlace(const living::Place& p) {
    place_ = p;
    flies_.clear();
    flock_.clear();
    motes_.clear();
    drips_.clear();
    vents_.clear();
    glows_.clear();
    flockAgain_ = 0.0;
}

void World::update(const Switches& on, const Frame& f) {
    out_.clear();
    if (!f.centreKnown) return;
    flies(on, f);
    birds(on, f);
    motes(on, f);
    drips(on, f);
    fireflies(on, f);
    if (out_.size() > kMaxSprites) out_.resize(kMaxSprites);
}

// 2.6.11: where the butterflies fly by day, fireflies drift low over the grass from dusk to dawn (by the hour: day
// under 0.3), each lit a while and dark a while; they come out as the light goes.
void World::fireflies(const Switches& on, const Frame& f) {
    if (!on.nightFireflies || !place_.wildlife || f.day >= 0.3f) {
        glows_.clear();
        return;
    }
    const float dusk = std::clamp((0.3f - f.day) / 0.3f, 0.0f, 1.0f);
    while (glows_.size() < 14) {
        Glow g;
        g.at = around(f.centre, 4.0f, 38.0f);
        g.heading = random() * 2.0f * kPi;
        g.phase = random();
        g.period = 2.5f + 3.0f * random();
        g.altitude = 0.4f + 1.8f * random();
        glows_.push_back(g);
    }
    for (Glow& g : glows_) {
        if (distXz(g.at, f.centre) > 50.0f) g.at = around(f.centre, 10.0f, 38.0f);  // left behind: elsewhere
        g.heading += std::sin(float(f.now) * 0.7f + g.phase * 6.0f) * 0.9f * f.dt;
        g.at.x += std::cos(g.heading) * 0.5f * f.dt;
        g.at.z += std::sin(g.heading) * 0.5f * f.dt;
        const float cycle = std::fmod(float(f.now) / g.period + g.phase, 1.0f);
        const float lit = std::pow(std::max(0.0f, std::sin(cycle * 2.0f * kPi)), 3.0f);  // dark half the time
        Point p = g.at;
        p.y = f.centre.y - g.altitude - 0.25f * std::sin(float(f.now) * 1.3f + g.phase * 6.0f);
        Sprite s = sprite(Firefly, p, 0.5f, dusk);
        s.p1 = lit;
        out_.push_back(s);
    }
}

// Butterflies wandering over the grass by day, a dragonfly or two darting over the water.
void World::flies(const Switches& on, const Frame& f) {
    const bool want = on.butterflies && place_.wildlife && f.day > 0.2f && f.night < 0.3f;
    if (!want) {
        flies_.clear();
        return;
    }
    const size_t butterflies = 6, dragonflies = on.dragonflies && f.waterKnown ? 3 : 0;
    if (!on.dragonflies)  // 2.4: their box off - the ones about go at once
        flies_.erase(std::remove_if(flies_.begin(), flies_.end(), [](const Fly& x) { return x.dragonfly; }),
                     flies_.end());
    size_t haveB = 0, haveD = 0;
    for (const Fly& x : flies_) ++(x.dragonfly ? haveD : haveB);
    while (haveB < butterflies || haveD < dragonflies) {
        Fly x;
        x.dragonfly = haveB >= butterflies;
        x.at = around(f.centre, 12.0f, 40.0f);
        x.altitude = x.dragonfly ? 2.0f + random() : 1.5f + 3.0f * random();
        x.heading = random() * 2.0f * kPi;
        x.phase = random() * 10.0f;
        x.colour = float(int(random() * 4.0f));
        x.target = around(x.at, 3.0f, 9.0f);
        flies_.push_back(x);
        ++(x.dragonfly ? haveD : haveB);
    }
    for (Fly& x : flies_) {
        if (distXz(x.at, f.centre) > 55.0f) {  // left behind: comes in again elsewhere
            x.at = around(f.centre, 15.0f, 40.0f);
            x.target = around(x.at, 3.0f, 9.0f);
        }
        if (x.dragonfly) {
            x.phase += f.dt * 60.0f;
            if (x.hover > 0.0f) {
                x.hover -= f.dt;
            } else {
                const float k = std::min(1.0f, f.dt * 5.0f);
                x.at.x += (x.target.x - x.at.x) * k;
                x.at.z += (x.target.z - x.at.z) * k;
                x.heading = std::atan2(x.target.z - x.at.z, x.target.x - x.at.x);
                if (distXz(x.at, x.target) < 0.3f) {
                    x.hover = 0.6f + 1.6f * random();
                    x.target = around(x.at, 3.0f, 8.0f);
                }
            }
            Point p = x.at;
            p.y = f.waterLevel - x.altitude - 0.15f * std::sin(float(f.now) * 7.0f + x.phase * 0.01f);
            Sprite s = sprite(Dragonfly, p, 0.85f, 1.0f);  // 1.25: 1.4 was a stick with a rim from above
            s.p1 = x.phase;
            s.headingX = std::cos(x.heading);
            s.headingZ = std::sin(x.heading);
            out_.push_back(s);
            continue;
        }
        x.phase += f.dt * 14.0f;
        x.heading += std::sin(float(f.now) * 1.3f + x.colour * 2.1f + x.phase * 0.02f) * 1.8f * f.dt;
        x.at.x += std::cos(x.heading) * 1.6f * f.dt;
        x.at.z += std::sin(x.heading) * 1.6f * f.dt;
        Point p = x.at;
        p.y = f.centre.y - x.altitude - 0.4f * std::sin(float(f.now) * 2.5f + x.colour);
        Sprite s = sprite(Butterfly, p, 1.3f, 1.0f);  // 1.8.2: 0.7 was a speck at 4K
        s.p1 = x.phase;
        s.p2 = x.colour;
        s.headingX = std::cos(x.heading);
        s.headingZ = std::sin(x.heading);
        out_.push_back(s);
        Sprite shadow = sprite(Shadow, Point{x.at.x, f.centre.y, x.at.z}, 0.6f, 0.18f);
        out_.push_back(shadow);
    }
}

// A flock pecking on the ground; anyone coming near sends it up and away, and another comes later elsewhere.
void World::birds(const Switches& on, const Frame& f) {
    const bool want = on.birds && place_.wildlife && f.day > 0.2f && f.night < 0.3f;
    if (!want) {
        flock_.clear();
        return;
    }
    if (flock_.empty() && f.now >= flockAgain_) {
        Point c = around(f.centre, 18.0f, 34.0f);
        bool clear = true;
        for (const Point& foot : f.feet) clear = clear && distXz(foot, c) > 14.0f;
        if (clear) {
            const int n = 5 + int(random() * 4.0f);
            for (int k = 0; k < n; ++k) {
                Bird b;
                b.at = Point{c.x + (random() - 0.5f) * 5.0f, f.centre.y, c.z + (random() - 0.5f) * 5.0f};
                b.phase = random() * 10.0f;
                b.facing = random() < 0.5f ? -1.0f : 1.0f;
                flock_.push_back(b);
            }
        }
    }
    bool startled = false;
    for (const Bird& b : flock_)
        for (const Point& foot : f.feet)
            if (!b.scared && distXz(b.at, foot) < 9.0f) startled = true;
    bool gone = !flock_.empty();
    for (Bird& b : flock_) {
        if (startled && !b.scared) {  // up and away from the nearest one, each a little its own way
            Point threat = f.centre;
            float best = 1e9f;
            for (const Point& foot : f.feet)
                if (distXz(foot, b.at) < best) {
                    best = distXz(foot, b.at);
                    threat = foot;
                }
            const float dx = b.at.x - threat.x, dz = b.at.z - threat.z;
            const float len = std::max(std::hypot(dx, dz), 0.1f);
            const float spread = (random() - 0.5f) * 1.2f, c = std::cos(spread), s = std::sin(spread);
            const float ax = (dx * c - dz * s) / len, az = (dx * s + dz * c) / len, speed = 8.0f + 3.0f * random();
            b.away = Point{ax * speed, -(4.0f + 2.0f * random()), az * speed};
            b.scared = true;
            b.flown = -random() * 0.25f;  // not all at once
        }
        if (b.scared) b.flown += f.dt;
        const bool flying = b.scared && b.flown > 0.0f;
        b.phase += f.dt * (flying ? 22.0f : 1.0f);
        Point p = b.at;
        if (flying) {
            p.x += b.away.x * b.flown;
            p.z += b.away.z * b.flown;
            p.y += b.away.y * b.flown;
            if (b.flown < 5.0f) gone = false;
            Sprite s = sprite(BirdFlying, p, 1.2f, std::clamp(5.0f - b.flown, 0.0f, 1.0f));
            s.p1 = b.phase;
            out_.push_back(s);
            const float high = b.at.y - p.y, fade = std::clamp(1.0f - high / 25.0f, 0.0f, 1.0f);
            out_.push_back(sprite(Shadow, Point{p.x, b.at.y, p.z}, 0.45f, 0.2f * fade));
        } else {
            gone = false;
            Sprite s = sprite(BirdGround, p, 1.1f, 1.0f);
            s.p1 = b.phase;
            s.p2 = b.facing;
            out_.push_back(s);
            out_.push_back(sprite(Shadow, p, 0.3f, 0.25f));
        }
    }
    if (gone || (!flock_.empty() && distXz(flock_.front().at, f.centre) > 70.0f)) {
        flock_.clear();
        flockAgain_ = f.now + 8.0 + 12.0 * double(random());
    }
}

// What falls, rises or flies off and goes: leaves from the crowns, embers from the fires, breath, drops, bubbles.
void World::motes(const Switches& on, const Frame& f) {
    // Leaves: about one every few seconds a crown, more in autumn.
    if (on.leaves && !f.crowns.empty()) {
        leafCarry_ += f.dt * float(std::min<size_t>(f.crowns.size(), 12)) * (place_.leaves ? 0.6f : 0.3f);
        while (leafCarry_ >= 1.0f) {
            leafCarry_ -= 1.0f;
            const Crown& c = f.crowns[size_t(random() * float(f.crowns.size())) % f.crowns.size()];
            Mote m;
            m.kind = Leaf;
            m.at = Point{c.centre.x + (random() - 0.5f) * c.radius * 1.2f, c.centre.y + c.radius * 0.2f * random(),
                         c.centre.z + (random() - 0.5f) * c.radius * 1.2f};
            m.v = Point{0.4f * (random() - 0.3f), 0.9f + 0.6f * random(), 0.3f * (random() - 0.5f)};
            m.life = 14.0f;
            m.size = place_.leaves == 2 ? 0.28f : 0.38f;
            m.spin = random() * 6.0f;
            m.colour = place_.leaves == 2 ? 3.0f : place_.leaves == 1 ? float(int(random() * 3.0f)) : 0.0f;
            m.ground = f.centre.y;
            if (motes_.size() < 400) motes_.push_back(m);
        }
    }
    // Embers: a few a second from each flame.
    if (on.embers && !f.fires.empty()) {
        fireCarry_ += f.dt * 2.5f * float(std::min<size_t>(f.fires.size(), 16));
        while (fireCarry_ >= 1.0f) {
            fireCarry_ -= 1.0f;
            const Point& c = f.fires[size_t(random() * float(f.fires.size())) % f.fires.size()];
            Mote m;
            m.kind = Ember;
            m.at = Point{c.x + (random() - 0.5f) * 0.6f, c.y, c.z + (random() - 0.5f) * 0.6f};
            m.v = Point{(random() - 0.5f) * 0.8f, -(2.0f + 2.0f * random()), (random() - 0.5f) * 0.8f};
            m.life = 1.2f + 1.4f * random();
            m.size = 0.16f;
            m.spin = random() * 6.0f;
            if (motes_.size() < 400) motes_.push_back(m);
        }
    }
    // Breath in the cold: each character's a puff every 2.6 s.
    if (on.breath && place_.snow) {
        for (size_t k = 0; k < f.feet.size() && k < 16; ++k) {
            const float cycle = float(std::fmod(f.now / 2.6 + double(k) * 0.37, 1.0));
            const float before = float(std::fmod((f.now - double(f.dt)) / 2.6 + double(k) * 0.37, 1.0));
            if (cycle >= before) continue;  // a new cycle began: a puff
            Mote m;
            m.kind = Puff;
            m.at = Point{f.feet[k].x + 0.4f, f.feet[k].y - 6.2f, f.feet[k].z};
            m.v = Point{0.5f * (random() - 0.5f), -0.5f, 0.5f * (random() - 0.5f)};
            m.life = 1.3f;
            m.size = 0.5f;
            if (motes_.size() < 400) motes_.push_back(m);
        }
    }
    // A step into shallow water: drops fly up.
    if (on.splashes && f.waterKnown) {
        for (const Point& s : f.steps) {
            if (s.y < f.waterLevel - 0.2f || s.y > f.waterLevel + 3.0f) continue;  // dry ground, or swimming deep
            for (int k = 0; k < 9; ++k) {
                const float a = random() * 2.0f * kPi, sp = 1.5f + 2.0f * random();
                Mote m;
                m.kind = Droplet;
                m.at = Point{s.x + std::cos(a) * 0.3f, f.waterLevel, s.z + std::sin(a) * 0.3f};
                m.v = Point{std::cos(a) * sp, -(3.0f + 2.5f * random()), std::sin(a) * sp};
                m.life = 1.5f;
                m.size = 0.13f;
                m.ground = f.waterLevel;
                if (motes_.size() < 400) motes_.push_back(m);
            }
        }
    }
    // Under the sea: bubbles from a few vents about the player.
    if (on.underwater && place_.underwater) {
        vents_.erase(std::remove_if(vents_.begin(), vents_.end(),
                                    [&](const Vent& v) { return distXz(v.at, f.centre) > 50.0f; }),
                     vents_.end());
        while (vents_.size() < 6) {
            Vent v;
            v.at = around(f.centre, 6.0f, 35.0f);
            v.next = float(f.now) + random();
            vents_.push_back(v);
        }
        for (Vent& v : vents_) {
            if (float(f.now) < v.next) continue;
            v.next = float(f.now) + 0.25f + 0.5f * random();
            Mote m;
            m.kind = Bubble;
            m.at = Point{v.at.x + (random() - 0.5f) * 0.4f, f.centre.y, v.at.z + (random() - 0.5f) * 0.4f};
            m.v = Point{0.0f, -(2.0f + 1.5f * random()), 0.0f};
            m.life = 6.0f;
            m.size = 0.12f + 0.18f * random();
            m.spin = random() * 6.0f;
            if (motes_.size() < 400) motes_.push_back(m);
        }
    } else {
        vents_.clear();
    }
    for (Mote& m : motes_) {
        m.age += f.dt;
        switch (m.kind) {
            case Leaf:
                if (!m.landed) {
                    m.at.x += (m.v.x + std::sin(float(f.now) * 1.7f + m.spin) * 0.9f) * f.dt;
                    m.at.z += (m.v.z + std::cos(float(f.now) * 1.3f + m.spin) * 0.6f) * f.dt;
                    m.at.y += m.v.y * f.dt;
                    if (m.at.y >= m.ground) {
                        m.at.y = m.ground;
                        m.landed = true;
                        m.life = m.age + 5.0f;
                    }
                }
                break;
            case Ember:
                m.at.x += (m.v.x + std::sin(float(f.now) * 3.0f + m.spin) * 0.7f) * f.dt;
                m.at.z += m.v.z * f.dt;
                m.at.y += m.v.y * f.dt;
                m.v.y *= 1.0f - 0.3f * f.dt;
                break;
            case Puff:
                m.at.x += m.v.x * f.dt;
                m.at.y += m.v.y * f.dt;
                m.at.z += m.v.z * f.dt;
                break;
            case Droplet:
                m.v.y += 22.0f * f.dt;  // falls (up is -y)
                m.at.x += m.v.x * f.dt;
                m.at.y += m.v.y * f.dt;
                m.at.z += m.v.z * f.dt;
                if (m.at.y > m.ground) m.age = m.life;
                break;
            case Bubble:
                m.at.x += std::sin(float(f.now) * 4.0f + m.spin) * 0.4f * f.dt;
                m.at.y += m.v.y * f.dt;
                break;
            default: break;
        }
    }
    motes_.erase(std::remove_if(motes_.begin(), motes_.end(),
                                [&](const Mote& m) { return m.age >= m.life || distXz(m.at, f.centre) > 90.0f; }),
                 motes_.end());
    for (const Mote& m : motes_) {
        const float k = std::clamp(m.age / m.life, 0.0f, 1.0f);
        Sprite s;
        switch (m.kind) {
            case Leaf:
                s = sprite(Leaf, m.at, m.size, m.landed ? std::clamp((m.life - m.age) / 2.0f, 0.0f, 1.0f) : 1.0f);
                s.p1 = m.landed ? m.spin : m.spin + float(f.now) * 2.5f;
                s.p2 = m.colour;
                break;
            case Ember:
                s = sprite(Ember, m.at, m.size, (1.0f - k) * (0.7f + 0.3f * std::sin(m.age * 20.0f + m.spin)));
                break;
            case Puff: s = sprite(Puff, m.at, m.size + 1.3f * k, 0.45f * (1.0f - k) * (1.0f - 0.6f * f.night)); break;
            case Droplet: s = sprite(Droplet, m.at, m.size, 0.9f); break;
            case Bubble: s = sprite(Bubble, m.at, m.size * (1.0f + 0.5f * k), std::min(1.0f, (1.0f - k) * 3.0f)); break;
            default: continue;
        }
        out_.push_back(s);
    }
}

// The caves' drips: a few spots about the player where a drop falls every few seconds into a ring.
void World::drips(const Switches& on, const Frame& f) {
    if (!on.drips || !place_.cave) {
        drips_.clear();
        return;
    }
    drips_.erase(std::remove_if(drips_.begin(), drips_.end(),
                                [&](const DripSpot& d) { return distXz(d.at, f.centre) > 45.0f; }),
                 drips_.end());
    while (drips_.size() < 5) {
        DripSpot d;
        d.at = around(f.centre, 5.0f, 30.0f);
        d.period = 2.2f + 2.0f * random();
        d.offset = random() * d.period;
        drips_.push_back(d);
    }
    constexpr float kFall = 0.9f, kHeight = 22.0f, kRing = 1.5f;
    for (const DripSpot& d : drips_) {
        const float t = float(std::fmod(f.now + double(d.offset), double(d.period)));
        const Point ground{d.at.x, f.centre.y, d.at.z};
        if (t < kFall) {
            const float k = t / kFall;
            Sprite s = sprite(Drip, Point{d.at.x, f.centre.y - kHeight * (1.0f - k * k), d.at.z}, 0.12f, 0.85f);
            s.p1 = 0.5f + 2.5f * k;  // longer as it speeds
            out_.push_back(s);
        } else if (t < kFall + kRing) {
            const float k = (t - kFall) / kRing;
            Sprite s = sprite(Ring, ground, 0.25f + 1.3f * k, 1.0f - k);
            s.p1 = k;
            out_.push_back(s);
        }
    }
}

}  // namespace uagfx::critters
