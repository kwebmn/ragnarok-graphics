// 3.0.1-beta.7: our mss32.dll's core (core/miles) - the sound files as Miles Sound System 6 reads them for uaRO.exe,
// the music's mp3 stream, and the mix (volume, pan, a 3D sound's place).
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "core/miles.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

void put16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(uint8_t(x));
    v.push_back(uint8_t(x >> 8));
}
void put32(std::vector<uint8_t>& v, uint32_t x) {
    put16(v, uint16_t(x));
    put16(v, uint16_t(x >> 16));
}

// A RIFF WAVE image: a fmt chunk of `tag` (its extension after the 16 bytes) and `data`
std::vector<uint8_t> wave(uint16_t tag, uint16_t channels, uint32_t rate, uint16_t bits, uint16_t blockAlign,
                          const std::vector<uint8_t>& extension, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> fmt;
    put16(fmt, tag);
    put16(fmt, channels);
    put32(fmt, rate);
    put32(fmt, rate * blockAlign);
    put16(fmt, blockAlign);
    put16(fmt, bits);
    if (!extension.empty()) {
        put16(fmt, uint16_t(extension.size()));
        fmt.insert(fmt.end(), extension.begin(), extension.end());
    }
    std::vector<uint8_t> v = {'R', 'I', 'F', 'F'};
    put32(v, uint32_t(4 + 8 + fmt.size() + 8 + data.size()));
    v.insert(v.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    put32(v, uint32_t(fmt.size()));
    v.insert(v.end(), fmt.begin(), fmt.end());
    v.insert(v.end(), {'d', 'a', 't', 'a'});
    put32(v, uint32_t(data.size()));
    v.insert(v.end(), data.begin(), data.end());
    return v;
}

// IMA ADPCM's tables and an encoder (the reference's), for the decoder's test
const int kIndex[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
const int kStep[89] = {7,     8,     9,     10,    11,    12,    13,    14,    16,    17,    19,    21,    23,
                       25,    28,    31,    34,    37,    41,    45,    50,    55,    60,    66,    73,    80,
                       88,    97,    107,   118,   130,   143,   157,   173,   190,   209,   230,   253,   279,
                       307,   337,   371,   408,   449,   494,   544,   598,   658,   724,   796,   876,   963,
                       1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,  2272,  2499,  2749,  3024,  3327,
                       3660,  4026,  4428,  4871,  5358,  5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487,
                       12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

uint8_t imaNibble(int sample, int& predicted, int& index) {
    const int step = kStep[index];
    int diff = sample - predicted, code = 0;
    if (diff < 0) {
        code = 8;
        diff = -diff;
    }
    int delta = step >> 3;
    if (diff >= step) code |= 4, diff -= step, delta += step;
    if (diff >= step >> 1) code |= 2, diff -= step >> 1, delta += step >> 1;
    if (diff >= step >> 2) code |= 1, delta += step >> 2;
    predicted += (code & 8) ? -delta : delta;
    predicted = predicted < -32768 ? -32768 : predicted > 32767 ? 32767 : predicted;
    index += kIndex[code];
    index = index < 0 ? 0 : index > 88 ? 88 : index;
    return uint8_t(code);
}

// Mono IMA ADPCM blocks of 256 bytes (505 samples each) of `pcm`; `heard` the encoder's own reconstruction
std::vector<uint8_t> imaEncode(const std::vector<int16_t>& pcm, uint16_t& samplesPerBlock,
                               std::vector<int16_t>& heard) {
    const int block = 256;
    samplesPerBlock = uint16_t((block - 4) * 2 + 1);
    std::vector<uint8_t> out;
    int predicted = 0, index = 0;
    for (size_t at = 0; at < pcm.size(); at += samplesPerBlock) {
        predicted = pcm[at];
        heard.push_back(int16_t(predicted));
        put16(out, uint16_t(int16_t(predicted)));
        out.push_back(uint8_t(index));
        out.push_back(0);
        for (int i = 0; i < block - 4; ++i) {
            const size_t a = at + 1 + size_t(i) * 2, b = a + 1;
            const uint8_t lo = imaNibble(a < pcm.size() ? pcm[a] : 0, predicted, index);
            heard.push_back(int16_t(predicted));
            const uint8_t hi = imaNibble(b < pcm.size() ? pcm[b] : 0, predicted, index);
            heard.push_back(int16_t(predicted));
            out.push_back(uint8_t(lo | hi << 4));
        }
    }
    return out;
}

// Three MPEG-1 layer III frames of silence (mono, 44100 Hz, 128 kbit/s: 417 bytes each, all side info 0)
std::vector<uint8_t> silentMp3(int frames) {
    std::vector<uint8_t> v;
    for (int f = 0; f < frames; ++f) {
        const size_t at = v.size();
        v.resize(at + 417, 0);
        v[at] = 0xFF, v[at + 1] = 0xFB, v[at + 2] = 0x90, v[at + 3] = 0xC0;
    }
    return v;
}

}  // namespace

TEST(miles_knows_the_file_types) {
    const std::vector<uint8_t> pcm = wave(1, 1, 22050, 16, 2, {}, std::vector<uint8_t>(8, 0));
    CHECK_EQ(miles::fileType(pcm.data(), pcm.size()), miles::kPcmWav);
    const std::vector<uint8_t> ima = wave(0x11, 1, 22050, 4, 256, {0xF9, 0x01}, std::vector<uint8_t>(256, 0));
    CHECK_EQ(miles::fileType(ima.data(), ima.size()), miles::kAdpcmWav);
    const std::vector<uint8_t> ms = wave(2, 1, 22050, 4, 256, {0xF4, 0x01, 7, 0}, std::vector<uint8_t>(256, 0));
    CHECK_EQ(miles::fileType(ms.data(), ms.size()), miles::kAdpcmWav);  // decoded by ours too
    const std::vector<uint8_t> other = wave(3, 1, 22050, 32, 4, {}, std::vector<uint8_t>(8, 0));
    CHECK_EQ(miles::fileType(other.data(), other.size()), miles::kOtherWav);
    const std::vector<uint8_t> mp3 = silentMp3(3);
    CHECK_EQ(miles::fileType(mp3.data(), mp3.size()), miles::kMpegL3);
    const uint8_t id3[] = {'I', 'D', '3', 3, 0, 0, 0, 0, 0, 0};
    CHECK_EQ(miles::fileType(id3, sizeof id3), miles::kMpegL3);
    const uint8_t layer2[] = {0xFF, 0xFD, 0x90, 0xC0};
    CHECK_EQ(miles::fileType(layer2, sizeof layer2), miles::kMpegL2);
    const char garbage[] = "garbage, no sound";
    CHECK_EQ(miles::fileType(reinterpret_cast<const uint8_t*>(garbage), sizeof garbage), miles::kUnknown);
}

TEST(miles_reads_a_wavs_info) {
    std::vector<int16_t> samples(100, 1234);
    const std::vector<uint8_t> image = miles::makeWav(samples.data(), 100, 1, 22050);
    miles::SoundInfo info;
    CHECK(miles::wavInfo(image.data(), image.size(), info));
    CHECK(info.format == 1 && info.rate == 22050 && info.bits == 16 && info.channels == 1);
    CHECK(info.samples == 100 && info.dataLen == 200 && info.blockSize == 2 && info.initial == image.data());
    int16_t first = 0;
    std::memcpy(&first, info.data, 2);
    CHECK_EQ(first, 1234);
    CHECK(!miles::wavInfo(reinterpret_cast<const uint8_t*>("RIFF"), 4, info));
}

TEST(miles_decodes_ima_adpcm) {
    std::vector<int16_t> sine(2000);
    for (size_t i = 0; i < sine.size(); ++i) sine[i] = int16_t(10000.0 * std::sin(double(i) * 0.05));
    uint16_t spb = 0;
    std::vector<int16_t> heard;
    const std::vector<uint8_t> blocks = imaEncode(sine, spb, heard);
    const std::vector<uint8_t> image = wave(0x11, 1, 22050, 4, 256, {uint8_t(spb), uint8_t(spb >> 8)}, blocks);
    miles::SoundInfo info;
    CHECK(miles::wavInfo(image.data(), image.size(), info));
    CHECK(info.samples >= sine.size());
    std::vector<int16_t> pcm;
    CHECK(miles::decodeAdpcm(info, pcm));
    CHECK(pcm.size() == heard.size());
    CHECK(pcm == heard);  // what the encoder meant, sample for sample
    int worst = 0;  // and the sine itself once the step has grown from the first block's index 0
    for (size_t i = 16; i < sine.size(); ++i) worst = std::max(worst, std::abs(int(pcm[i]) - int(sine[i])));
    CHECK(worst < 200);
    // Miles' AILSOUNDINFO alone (no fmt chunk): the same samples, the frames a block holds by its size
    miles::SoundInfo bare = info;
    bare.fmt = nullptr;
    bare.fmtLen = 0;
    std::vector<int16_t> again;
    CHECK(miles::decodeAdpcm(bare, again));
    CHECK(again == pcm);
    bare.blockSize = 0;
    CHECK(!miles::decodeAdpcm(bare, again));
    // a last block cut short: its whole bytes still decode, and the count says so (3 blocks, then 1 + 2 * 242)
    const std::vector<uint8_t> cut(blocks.begin(), blocks.end() - 10);
    const std::vector<uint8_t> cutImage = wave(0x11, 1, 22050, 4, 256, {uint8_t(spb), uint8_t(spb >> 8)}, cut);
    CHECK(miles::wavInfo(cutImage.data(), cutImage.size(), info));
    CHECK_EQ(info.samples, 3u * 505u + 485u);
    CHECK(miles::decodeAdpcm(info, pcm));
    CHECK_EQ(pcm.size(), size_t(info.samples));
    CHECK(std::equal(pcm.begin(), pcm.end(), heard.begin()));
}

// Microsoft ADPCM: a block's header (predictor, delta, the two first samples, sample 2 out first), then nibbles
TEST(miles_decodes_ms_adpcm) {
    std::vector<uint8_t> block = {0};  // predictor 0: coefficients 256, 0
    put16(block, 16);                   // delta
    put16(block, 100);                  // sample 1
    put16(block, 50);                   // sample 2
    block.push_back(0x00);              // two nibbles 0: the prediction itself
    block.push_back(0x10);              // nibble 1 (+delta), then 0
    std::vector<uint8_t> ext = {4, 0, 7, 0};  // 4 samples a block... and the 7 standard coefficient pairs
    const int16_t coef[14] = {256, 0, 512, -256, 0, 0, 192, 64, 240, 0, 460, -208, 392, -232};
    for (int16_t c : coef) put16(ext, uint16_t(c));
    ext[0] = 6;  // samples per block: 2 + 2 * 2
    const std::vector<uint8_t> image = wave(2, 1, 22050, 4, uint16_t(block.size()), ext, block);
    miles::SoundInfo info;
    CHECK(miles::wavInfo(image.data(), image.size(), info));
    std::vector<int16_t> pcm;
    CHECK(miles::decodeAdpcm(info, pcm));
    CHECK(pcm.size() == 6);
    CHECK(pcm[0] == 50 && pcm[1] == 100 && pcm[2] == 100 && pcm[3] == 100);
    CHECK(pcm[4] == 116 && pcm[5] == 116);  // +1 x delta 16, then the prediction (256 x 116 / 256)
}

TEST(miles_decodes_mp3) {
    const std::vector<uint8_t> mp3 = silentMp3(6);
    std::vector<int16_t> pcm;
    int channels = 0;
    uint32_t rate = 0;
    CHECK(miles::decodeMp3(mp3.data(), mp3.size(), pcm, channels, rate));
    CHECK(channels == 1 && rate == 44100 && pcm.size() >= 1152 * 4);
    bool silent = true;
    for (int16_t s : pcm) silent = silent && s == 0;
    CHECK(silent);
    miles::Mp3Stream stream;  // the music: read in pieces, and from the start again for its loop
    CHECK(stream.open(mp3));
    CHECK(stream.channels() == 1 && stream.rate() == 44100);
    std::vector<int16_t> piece(1000);
    size_t total = 0;
    for (size_t n; (n = stream.read(piece.data(), piece.size())) > 0;) total += n;
    CHECK_EQ(total, pcm.size());
    stream.rewind();
    CHECK(stream.read(piece.data(), piece.size()) == piece.size());
    const char garbage[] = "garbage";
    CHECK(!miles::decodeMp3(reinterpret_cast<const uint8_t*>(garbage), sizeof garbage, pcm, channels, rate));
}

TEST(miles_mixes_as_miles) {
    CHECK(miles::gainOf(0) == 0.0f && miles::gainOf(127) == 1.0f && miles::gainOf(200) == 1.0f);
    CHECK(std::fabs(miles::gainOf(64) - 64.0f / 127.0f) < 1e-6f);
    float l = 0, r = 0;
    miles::panGains(0, l, r);
    CHECK(l == 1.0f && r == 0.0f);
    miles::panGains(127, l, r);
    CHECK(l == 0.0f && r == 1.0f);
    miles::panGains(64, l, r);  // the middle: both full, as an unpanned sound
    CHECK(l > 0.98f && r > 0.98f);
    const float front[3] = {0, 0, 1}, up[3] = {0, 1, 0};
    float gain = 0, pan = 0;
    const float nearHere[3] = {0, 0, 5};
    miles::place3D(nearHere, front, up, 10, 100, gain, pan);
    CHECK(gain == 1.0f && std::fabs(pan - 0.5f) < 1e-3f);
    const float right[3] = {50, 0, 0};
    miles::place3D(right, front, up, 10, 100, gain, pan);
    CHECK(std::fabs(gain - 0.2f) < 1e-4f && pan > 0.99f);
    const float left[3] = {-50, 0, 0};
    miles::place3D(left, front, up, 10, 100, gain, pan);
    CHECK(pan < 0.01f);
    const float far[3] = {0, 0, 150};
    miles::place3D(far, front, up, 10, 100, gain, pan);
    CHECK(gain == 0.0f);
}

// A sample's sound (mss32's AIL_set_sample_file): PCM as it is, ADPCM decoded to 16 bits; what XAudio2 cannot play
// refused
TEST(miles_makes_a_samples_sound) {
    const std::vector<int16_t> two = {100, -100, 200, -200};
    const std::vector<uint8_t> mono = miles::makeWav(two.data(), 4, 1, 22050);
    CHECK_EQ(miles::imageSize(mono.data()), mono.size());
    miles::Pcm p;
    CHECK(miles::pcmOf(mono.data(), mono.size(), p));
    CHECK(p.channels == 1 && p.bits == 16 && p.rate == 22050 && p.bytes.size() == 8);
    CHECK(std::memcmp(p.bytes.data(), two.data(), 8) == 0);
    const std::vector<uint8_t> eight = wave(1, 2, 11025, 8, 2, {}, {128, 128, 255, 0, 7});  // a byte over: dropped
    CHECK(miles::pcmOf(eight.data(), eight.size(), p));
    CHECK(p.channels == 2 && p.bits == 8 && p.rate == 11025 && p.bytes.size() == 4);
    std::vector<int16_t> sine(600);
    for (size_t i = 0; i < sine.size(); ++i) sine[i] = int16_t(8000.0 * std::sin(double(i) * 0.1));
    uint16_t spb = 0;
    std::vector<int16_t> heard;
    const std::vector<uint8_t> blocks = imaEncode(sine, spb, heard);
    const std::vector<uint8_t> ima = wave(0x11, 1, 44100, 4, 256, {uint8_t(spb), uint8_t(spb >> 8)}, blocks);
    CHECK(miles::pcmOf(ima.data(), ima.size(), p));
    CHECK(p.channels == 1 && p.bits == 16 && p.rate == 44100 && p.bytes.size() == heard.size() * 2);
    CHECK(std::memcmp(p.bytes.data(), heard.data(), p.bytes.size()) == 0);
    const std::vector<uint8_t> floats = wave(3, 1, 22050, 32, 4, {}, std::vector<uint8_t>(8, 0));
    CHECK(!miles::pcmOf(floats.data(), floats.size(), p));
    const std::vector<uint8_t> slow = wave(1, 1, 500, 16, 2, {}, std::vector<uint8_t>(8, 0));  // under 1000 Hz
    CHECK(!miles::pcmOf(slow.data(), slow.size(), p));
    const std::vector<uint8_t> three = wave(1, 3, 22050, 16, 6, {}, std::vector<uint8_t>(12, 0));
    CHECK(!miles::pcmOf(three.data(), three.size(), p));
    const std::vector<uint8_t> empty = wave(1, 1, 22050, 16, 2, {}, {});
    CHECK(!miles::pcmOf(empty.data(), empty.size(), p));
    std::vector<uint8_t> cut = mono;  // its header says more than there is: what there is
    cut.resize(cut.size() - 4);
    CHECK(miles::pcmOf(cut.data(), cut.size(), p) && p.bytes.size() == 4);
    const uint8_t mp3[] = {0xFF, 0xFB, 0x90, 0xC0, 0, 0, 0, 0};
    CHECK_EQ(miles::imageSize(mp3), size_t(0));  // no size of its own
    CHECK(!miles::pcmOf(mp3, sizeof mp3, p) && !miles::pcmOf(nullptr, 0, p));
}

// The music (mss32's streams): a WAVE or an mp3 played `loops` times, 0 for ever, from its start again on restart
TEST(miles_plays_music_in_loops) {
    std::vector<int16_t> ten(20);
    for (size_t i = 0; i < ten.size(); ++i) ten[i] = int16_t(i);
    miles::Music m;
    CHECK(m.open(miles::makeWav(ten.data(), 10, 2, 22050)));
    CHECK(m.channels() == 2 && m.rate() == 22050);
    std::vector<int16_t> out(60, -1);
    m.setLoops(2);
    CHECK_EQ(m.fill(out.data(), 25), size_t(20));  // twice, then its end
    CHECK(m.ended());
    CHECK(out[0] == 0 && out[19] == 19 && out[20] == 0 && out[39] == 19);
    CHECK_EQ(m.fill(out.data(), 5), size_t(0));
    m.restart();
    CHECK(!m.ended());
    m.setLoops(0);  // for ever
    CHECK_EQ(m.fill(out.data(), 25), size_t(25));
    CHECK(!m.ended() && out[48] == 8);
    std::vector<uint8_t> eight = wave(1, 1, 8000, 8, 1, {}, {0, 128, 255});  // 8-bit PCM made 16
    CHECK(m.open(eight));
    m.setLoops(1);
    CHECK_EQ(m.fill(out.data(), 10), size_t(3));
    CHECK(out[0] == -32768 && out[1] == 0 && out[2] == 32512);
    CHECK(m.open(silentMp3(8)));
    CHECK(m.channels() == 1 && m.rate() == 44100);
    m.setLoops(3);
    std::vector<int16_t> big(1152 * 40);
    const size_t thrice = m.fill(big.data(), big.size());
    CHECK(m.ended() && thrice >= 3 * 1152 * 6 && thrice % 3 == 0);
    CHECK(!m.open({}) && !m.open(std::vector<uint8_t>(100, 0x55)) && !m.open(wave(1, 1, 22050, 16, 2, {}, {})));
    CHECK_EQ(m.fill(out.data(), 10), size_t(0));  // a failed one plays nothing
}
