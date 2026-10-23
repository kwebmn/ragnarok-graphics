#include "core/miles.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "minimp3_ex.h"

namespace uagfx::miles {
namespace {

uint16_t u16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t u32(const uint8_t* p) { return uint32_t(p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24); }
int16_t s16(const uint8_t* p) { return int16_t(u16(p)); }

int16_t clamp16(int v) { return int16_t(v < -32768 ? -32768 : v > 32767 ? 32767 : v); }

// An MPEG audio frame's header at p: its layer (1-3), 0 for none
int mpegLayer(const uint8_t* p) {
    if (p[0] != 0xFF || (p[1] & 0xE0) != 0xE0) return 0;
    const int version = (p[1] >> 3) & 3, layer = (p[1] >> 1) & 3, bitrate = p[2] >> 4, rate = (p[2] >> 2) & 3;
    if (version == 1 || layer == 0 || bitrate == 15 || rate == 3) return 0;
    return 4 - layer;
}

const int kImaIndex[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
const int kImaStep[89] = {7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,
                          25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,
                          88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,   253,   279,
                          307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,   876,   963,
                          1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,  3327,
                          3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487,
                          12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

struct Ima {
    int predicted = 0, index = 0;
    int16_t next(int nibble) {
        const int step = kImaStep[index];
        int delta = step >> 3;
        if (nibble & 4) delta += step;
        if (nibble & 2) delta += step >> 1;
        if (nibble & 1) delta += step >> 2;
        predicted = clamp16(predicted + ((nibble & 8) ? -delta : delta));
        index = std::clamp(index + kImaIndex[nibble & 15], 0, 88);
        return int16_t(predicted);
    }
};

// The frames a block of `have` bytes holds (the last one may be cut short), at most `perBlock`. IMA: a header for
// each channel with its first sample, then 4 bytes of each channel in turn (mono: any byte); Microsoft: 7 bytes of
// each channel's header with its two first samples, then a nibble for each channel in turn
uint32_t blockFrames(int format, uint32_t ch, uint32_t have, uint32_t perBlock) {
    uint32_t can = 0;
    if (format == 0x11 && have >= 4 * ch) can = ch == 1 ? 1 + 2 * (have - 4) : 1 + 8 * ((have - 4 * ch) / (4 * ch));
    if (format == 2 && have >= 7 * ch) can = 2 + (have - 7 * ch) * 2 / ch;
    return std::min(can, perBlock);
}

// Its frames per block: the fmt chunk's, else what a whole block holds
uint32_t perBlockOf(const SoundInfo& in) {
    if (in.fmtLen >= 20 && u16(in.fmt + 18) != 0) return u16(in.fmt + 18);
    return blockFrames(in.format, uint32_t(in.channels), in.blockSize, 0xFFFFFFFFu);
}

bool decodeIma(const SoundInfo& in, std::vector<int16_t>& out) {
    const uint32_t ch = uint32_t(in.channels), block = in.blockSize, perBlock = perBlockOf(in);
    for (uint32_t at = 0; at < in.dataLen; at += block) {
        const uint8_t* b = in.data + at;
        const uint32_t have = std::min(block, in.dataLen - at), frames = blockFrames(0x11, ch, have, perBlock);
        if (frames == 0) break;
        std::vector<Ima> state(ch);
        std::vector<int16_t> chans(size_t(frames) * ch);
        for (uint32_t c = 0; c < ch; ++c) {
            state[c].predicted = s16(b + 4 * c);
            state[c].index = std::clamp(int(b[4 * c + 2]), 0, 88);
            chans[c] = int16_t(state[c].predicted);
        }
        // the nibbles: sample f of channel c at frame f (the first one the header's)
        for (uint32_t f = 1; f < frames; ++f)
            for (uint32_t c = 0; c < ch; ++c) {
                const uint32_t n = f - 1, byte = 4 * ch + (n / 8) * 4 * ch + 4 * c + (n % 8) / 2;
                const int nibble = (n & 1) ? b[byte] >> 4 : b[byte] & 15;
                chans[size_t(f) * ch + c] = state[c].next(nibble);
            }
        out.insert(out.end(), chans.begin(), chans.end());
    }
    return !out.empty();
}

const int kMsAdapt[16] = {230, 230, 230, 230, 307, 409, 512, 614, 768, 614, 512, 409, 307, 230, 230, 230};
const int16_t kMsCoef[7][2] = {{256, 0}, {512, -256}, {0, 0}, {192, 64}, {240, 0}, {460, -208}, {392, -232}};

bool decodeMs(const SoundInfo& in, const std::vector<int16_t>& coefs, std::vector<int16_t>& out) {
    const uint32_t ch = uint32_t(in.channels), block = in.blockSize, perBlock = perBlockOf(in);
    const uint32_t ncoef = uint32_t(coefs.size() / 2);
    for (uint32_t at = 0; at < in.dataLen; at += block) {
        const uint8_t* b = in.data + at;
        const uint32_t have = std::min(block, in.dataLen - at), frames = blockFrames(2, ch, have, perBlock);
        if (frames < 2) break;
        std::vector<int> c1(ch), c2(ch), delta(ch), s1(ch), s2(ch);
        for (uint32_t c = 0; c < ch; ++c) {
            const uint32_t pred = std::min<uint32_t>(b[c], ncoef - 1);
            c1[c] = coefs[2 * pred];
            c2[c] = coefs[2 * pred + 1];
            delta[c] = s16(b + ch + 2 * c);
            s1[c] = s16(b + 3 * ch + 2 * c);
            s2[c] = s16(b + 5 * ch + 2 * c);
        }
        for (uint32_t c = 0; c < ch; ++c) out.push_back(int16_t(s2[c]));  // sample 2 is the older one
        for (uint32_t c = 0; c < ch; ++c) out.push_back(int16_t(s1[c]));
        for (uint32_t n = 0; n < (frames - 2) * ch; ++n) {
            const uint32_t c = n % ch;
            const int nibble = (n & 1) ? b[7 * ch + n / 2] & 15 : b[7 * ch + n / 2] >> 4;
            const int predicted = (s1[c] * c1[c] + s2[c] * c2[c]) / 256;
            const int16_t sample = clamp16(predicted + (nibble >= 8 ? nibble - 16 : nibble) * delta[c]);
            s2[c] = s1[c];
            s1[c] = sample;
            delta[c] = std::max(16, kMsAdapt[nibble] * delta[c] / 256);
            out.push_back(sample);
        }
    }
    return !out.empty();
}

}  // namespace

int fileType(const uint8_t* data, size_t size) {
    if (!data || size < 4) return kUnknown;
    if (size >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WAVE", 4) == 0) {
        SoundInfo info;
        if (!wavInfo(data, size, info)) return kOtherWav;
        if (info.format == 1) return kPcmWav;
        return info.format == 0x11 || info.format == 2 ? kAdpcmWav : kOtherWav;
    }
    if (size >= 3 && std::memcmp(data, "ID3", 3) == 0) return kMpegL3;
    const int layer = mpegLayer(data);
    return layer == 3 ? kMpegL3 : layer == 2 ? kMpegL2 : layer == 1 ? kMpegL1 : kUnknown;
}

bool wavInfo(const uint8_t* image, size_t most, SoundInfo& out) {
    if (!image || most < 12 || std::memcmp(image, "RIFF", 4) != 0 || std::memcmp(image + 8, "WAVE", 4) != 0)
        return false;
    const size_t end = std::min<size_t>(most, size_t(u32(image + 4)) + 8);
    SoundInfo s;
    s.initial = image;
    for (size_t at = 12; at + 8 <= end;) {
        const uint32_t size = u32(image + at + 4);
        const uint8_t* body = image + at + 8;
        const size_t room = end - (at + 8);
        if (std::memcmp(image + at, "fmt ", 4) == 0 && size >= 16 && size <= room) {
            s.format = u16(body);
            s.channels = u16(body + 2);
            s.rate = u32(body + 4);
            s.blockSize = u16(body + 12);
            s.bits = u16(body + 14);
            s.fmt = body;
            s.fmtLen = size;
        } else if (std::memcmp(image + at, "data", 4) == 0) {
            if (!s.fmt || s.channels <= 0 || s.blockSize == 0) return false;
            s.data = body;
            s.dataLen = uint32_t(std::min<size_t>(size, room));
            if (s.format == 0x11 || s.format == 2) {
                const uint32_t ch = uint32_t(s.channels), per = perBlockOf(s);
                s.samples = s.dataLen / s.blockSize * blockFrames(s.format, ch, s.blockSize, per) +
                            blockFrames(s.format, ch, s.dataLen % s.blockSize, per);
            } else {
                s.samples = s.dataLen / s.blockSize;
            }
            out = s;
            return true;
        }
        if (size > room) break;
        at += 8 + size + (size & 1);
    }
    return false;
}

bool decodeAdpcm(const SoundInfo& info, std::vector<int16_t>& pcm) {
    pcm.clear();
    if (!info.data || !info.fmt || info.channels < 1 || info.channels > 2) return false;
    if (info.format == 0x11) return decodeIma(info, pcm);
    if (info.format != 2) return false;
    std::vector<int16_t> coefs;
    const uint32_t ncoef = info.fmtLen >= 22 ? u16(info.fmt + 20) : 0;
    if (ncoef && info.fmtLen >= 22 + 4 * ncoef) {
        for (uint32_t i = 0; i < 2 * ncoef; ++i) coefs.push_back(s16(info.fmt + 22 + 2 * i));
    } else {
        for (const auto& c : kMsCoef) coefs.insert(coefs.end(), {c[0], c[1]});
    }
    return decodeMs(info, coefs, pcm);
}

std::vector<uint8_t> makeWav(const int16_t* pcm, size_t frames, int channels, uint32_t rate) {
    const uint32_t bytes = uint32_t(frames * size_t(channels) * 2), align = uint32_t(channels * 2);
    std::vector<uint8_t> v(44 + bytes);
    uint8_t* p = v.data();
    const auto put32 = [&](size_t at, uint32_t x) { std::memcpy(p + at, &x, 4); };
    const auto put16 = [&](size_t at, uint16_t x) { std::memcpy(p + at, &x, 2); };
    std::memcpy(p, "RIFF", 4);
    put32(4, 36 + bytes);
    std::memcpy(p + 8, "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1);
    put16(22, uint16_t(channels));
    put32(24, rate);
    put32(28, rate * align);
    put16(32, uint16_t(align));
    put16(34, 16);
    std::memcpy(p + 36, "data", 4);
    put32(40, bytes);
    if (bytes) std::memcpy(p + 44, pcm, bytes);
    return v;
}

bool decodeMp3(const uint8_t* data, size_t size, std::vector<int16_t>& pcm, int& channels, uint32_t& rate) {
    pcm.clear();
    if (!data || size == 0) return false;
    mp3dec_t dec;
    mp3dec_file_info_t info{};
    if (mp3dec_load_buf(&dec, data, size, &info, nullptr, nullptr) != 0 || !info.buffer || info.samples == 0 ||
        info.channels < 1) {
        std::free(info.buffer);
        return false;
    }
    pcm.assign(info.buffer, info.buffer + info.samples);
    channels = info.channels;
    rate = uint32_t(info.hz);
    std::free(info.buffer);
    return true;
}

struct Mp3Stream::Impl {
    std::vector<uint8_t> file;
    mp3dec_ex_t dec{};
    bool open = false;
};

Mp3Stream::Mp3Stream() : impl_(new Impl) {}
Mp3Stream::~Mp3Stream() {
    if (impl_->open) mp3dec_ex_close(&impl_->dec);
}

bool Mp3Stream::open(std::vector<uint8_t> file) {
    if (impl_->open) mp3dec_ex_close(&impl_->dec);
    impl_->open = false;
    impl_->file = std::move(file);
    if (impl_->file.empty() || mp3dec_ex_open_buf(&impl_->dec, impl_->file.data(), impl_->file.size(),
                                                  MP3D_SEEK_TO_SAMPLE) != 0)
        return false;
    impl_->open = impl_->dec.info.channels > 0 && impl_->dec.info.hz > 0;
    return impl_->open;
}

size_t Mp3Stream::read(int16_t* out, size_t samples) {
    return impl_->open ? mp3dec_ex_read(&impl_->dec, out, samples) : 0;
}

void Mp3Stream::rewind() {
    if (impl_->open) mp3dec_ex_seek(&impl_->dec, 0);
}

int Mp3Stream::channels() const { return impl_->open ? impl_->dec.info.channels : 0; }
uint32_t Mp3Stream::rate() const { return impl_->open ? uint32_t(impl_->dec.info.hz) : 0; }

float gainOf(int volume) { return float(std::clamp(volume, 0, 127)) / 127.0f; }

void panGains(int pan, float& left, float& right) {
    const float p = float(std::clamp(pan, 0, 127)) / 127.0f;
    left = std::min(1.0f, 2.0f * (1.0f - p));
    right = std::min(1.0f, 2.0f * p);
}

void place3D(const float pos[3], const float front[3], const float up[3], float nearest, float farthest, float& gain,
             float& pan) {
    const float d = std::sqrt(pos[0] * pos[0] + pos[1] * pos[1] + pos[2] * pos[2]);
    const float full = std::max(nearest, 0.001f);
    gain = d <= full ? 1.0f : (farthest > 0.0f && d >= farthest) ? 0.0f : full / d;
    // the listener's right: up x front (x right, y up, z ahead)
    float r[3] = {up[1] * front[2] - up[2] * front[1], up[2] * front[0] - up[0] * front[2],
                  up[0] * front[1] - up[1] * front[0]};
    const float rl = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
    if (d < 1e-4f || rl < 1e-6f) {
        pan = 0.5f;
        return;
    }
    const float side = (pos[0] * r[0] + pos[1] * r[1] + pos[2] * r[2]) / (d * rl);
    pan = std::clamp(0.5f + 0.5f * side, 0.0f, 1.0f);
}

}  // namespace uagfx::miles
