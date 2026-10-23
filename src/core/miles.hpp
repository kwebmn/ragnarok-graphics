#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace uagfx::miles {

// 3.0.1-beta.7 (our own libraries in place of the client's old ones): the core of our mss32.dll - Miles Sound System
// 6 as uaRO.exe uses it (src/clientlibs/mss32.cpp): its sound files (data\wav: PCM, IMA and Microsoft ADPCM, one mp3),
// the music's mp3 stream (BGM\*.mp3, minimp3) and the mix.

// Miles' file types (AILFILETYPE_*) - the game's loader (0x439900) decodes 2 and 13 into PCM
enum FileType : int {
    kUnknown = 0,
    kPcmWav = 1,
    kAdpcmWav = 2,
    kOtherWav = 3,
    kMpegL1 = 11,
    kMpegL2 = 12,
    kMpegL3 = 13,
};
int fileType(const uint8_t* data, size_t size);

// AILSOUNDINFO's, and the fmt chunk for ADPCM's own fields
struct SoundInfo {
    int format = 0;  // the WAVE format tag: 1 PCM, 0x11 IMA ADPCM, 2 Microsoft ADPCM
    const uint8_t* data = nullptr;
    uint32_t dataLen = 0, rate = 0;
    int bits = 0, channels = 0;
    uint32_t samples = 0;    // frames
    uint32_t blockSize = 0;  // its block's bytes (nBlockAlign)
    const uint8_t* initial = nullptr;  // the image
    const uint8_t* fmt = nullptr;      // its fmt chunk's body
    uint32_t fmtLen = 0;
};
bool wavInfo(const uint8_t* image, size_t most, SoundInfo& out);
// IMA (0x11) or Microsoft (0x02) ADPCM into 16-bit samples, the channels interleaved
bool decodeAdpcm(const SoundInfo& info, std::vector<int16_t>& pcm);
// A 16-bit PCM RIFF WAVE image of `frames` (the channels interleaved)
std::vector<uint8_t> makeWav(const int16_t* pcm, size_t frames, int channels, uint32_t rate);
// A whole mp3 into 16-bit samples
bool decodeMp3(const uint8_t* data, size_t size, std::vector<int16_t>& pcm, int& channels, uint32_t& rate);

// The music: an mp3 file read a piece at a time, from its start again for a loop
class Mp3Stream {
public:
    Mp3Stream();
    ~Mp3Stream();
    Mp3Stream(const Mp3Stream&) = delete;
    Mp3Stream& operator=(const Mp3Stream&) = delete;
    bool open(std::vector<uint8_t> file);
    size_t read(int16_t* out, size_t samples);  // interleaved samples; 0 at its end
    void rewind();
    int channels() const;
    uint32_t rate() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// A sound as an XAudio2 voice plays it (mss32's samples): 8 or 16-bit PCM, mono or stereo
struct Pcm {
    std::vector<uint8_t> bytes;
    int channels = 0, bits = 0;
    uint32_t rate = 0;
};
// A file image's size by its own RIFF header (at most 256 MB), 0 for anything else: the game passes its WAVE images
// without their size
size_t imageSize(const uint8_t* image);
// A WAVE image's sound (at most `most` bytes of it): PCM as it is (its whole frames), ADPCM decoded to 16 bits; false
// for what XAudio2 does not take (other formats, 3 channels or more, under 1000 Hz or over 200000 Hz) or no sound
bool pcmOf(const uint8_t* image, size_t most, Pcm& out);

// The music (mss32's streams): an mp3 read a piece at a time or a WAVE decoded whole, played `loops` times (0: for
// ever); nothing at all from one that did not open
class Music {
public:
    bool open(std::vector<uint8_t> file);
    int channels() const { return channels_; }
    uint32_t rate() const { return rate_; }
    void setLoops(int loops) { loops_ = loops < 0 ? 0 : loops; }
    void restart();  // from its start, its loops counted anew
    // Up to `frames` frames (the channels interleaved), from its start again while it loops; fewer: it has ended
    size_t fill(int16_t* out, size_t frames);
    bool ended() const { return ended_; }

private:
    size_t read(int16_t* out, size_t samples);
    void rewind();
    Mp3Stream mp3_;
    bool isMp3_ = false;
    std::vector<int16_t> pcm_;
    size_t at_ = 0;
    int channels_ = 0, loops_ = 1, played_ = 0;
    uint32_t rate_ = 0;
    bool ended_ = false;
};

// The mix: a Miles volume (0-127) as a gain; a pan (0 left, 64 the middle, 127 right) as the sides' gains - the
// middle both full, as a sound never panned
float gainOf(int volume);
void panGains(int pan, float& left, float& right);
// A 3D sound at `pos` from the listener (its front and up; Miles' axes: x right, y up, z ahead): full within
// `nearest`, nearest / distance beyond, silent past `farthest`; its pan 0 left - 1 right
void place3D(const float pos[3], const float front[3], const float up[3], float nearest, float farthest, float& gain,
             float& pan);

}  // namespace uagfx::miles
