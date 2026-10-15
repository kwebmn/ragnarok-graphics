#include "win/gpu/ai_textures.hpp"

#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

#include "core/ai/texture_cache.hpp"
#include "core/ai/vram.hpp"
#include "core/sprite_exe.hpp"
#include "core/webp_image.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr int kNetScale = 4;  // every network enlarges 4 times; a result of twice the size is its first mip level
constexpr size_t kKeptLimit = size_t(512) << 20;
// 0.14.1: the interface's big pictures - the loading screen and the login's backgrounds are one texture of 1024 a
// side (the picture in its top left corner); (0.17) from 128 a side - a map's sign. A window's tiles (crisp text
// draws them from its twins) and the letters are not taken.
constexpr int kUiPictureSide = 128;
constexpr int kUiPictureMax = 1024;
constexpr int kTrimSide = 512;  // past this a picture's network buffers are freed once it is done
constexpr size_t kBatchBytes = size_t(32) << 20;  // pictures written to upscaled.grf at once
constexpr UINT kAiConstants = 0, kAiWeights = 1, kAiU0 = 2, kAiU1 = 3, kAiU2 = 4;  // root parameters

UINT64 alignUp64(UINT64 v, UINT64 a) { return (v + a - 1) / a * a; }

ID3D12Resource* buffer(ID3D12Device* device, D3D12_HEAP_TYPE type, UINT64 size, D3D12_RESOURCE_STATES state,
                       bool uav) {
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = type;
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d.Flags = uav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    ID3D12Resource* r = nullptr;
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, __uuidof(ID3D12Resource),
                                               reinterpret_cast<void**>(&r))))
        return nullptr;
    return r;
}

struct AiConstants {
    uint32_t v[16] = {};
};

}  // namespace

AiTextures::~AiTextures() {
    if (ready_) {
        finishReadbacks();  // the renderer waited for the GPU: the last results go into upscaled.grf too
        logNow(true);
    }
    if (ioThread_) {
        EnterCriticalSection(&ioLock_);
        ioStop_ = true;
        LeaveCriticalSection(&ioLock_);
        SetEvent(ioWake_);
        WaitForSingleObject(ioThread_, 15000);  // the last batch goes into upscaled.grf
        CloseHandle(ioThread_);
    }
    if (ioWake_) CloseHandle(ioWake_);
    if (ioLockMade_) DeleteCriticalSection(&ioLock_);
    archive_.close();
    if (writerMutex_) {
        ReleaseMutex(writerMutex_);
        CloseHandle(writerMutex_);
    }
    for (Readback& r : readbacks_) releaseCom(r.buffer);
    releaseCom(conv_);
    releaseCom(shuffle_);
    releaseCom(mip_);
    releaseCom(alpha_);
    releaseCom(root_);
    for (auto& n : nets_) releaseCom(n->weights);
    releaseCom(planes_);
    releaseCom(feat_[0]);
    releaseCom(feat_[1]);
}

namespace {

bool loadModel(const std::string& path, ai::Model& m, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (file.empty()) {
        error = "no model " + path + " (update.bat installs it)";
        return false;
    }
    return ai::parseModel(file, m, error);
}

// The model file of an [ai] model word.
const char* modelFile(const std::string& token) {
    return token == "anime" ? "realesr-animevideov3" : "realesr-general-x4v3";
}

}  // namespace

bool AiTextures::loadNet(const std::string& dir, const std::string& token, int denoise, Net& net,
                         std::string& error) {
    net.token = token;
    net.name = modelFile(token);
    if (!loadModel(dir + net.name + ".bin", net.model, error)) return false;
    if (net.model.scale != kNetScale) {
        error = net.name + " does not enlarge 4 times";
        return false;
    }
    // The general model's denoise strength: mixed with its weak-denoise twin (0.11.2 and older: the model alone,
    // 100 - sand came out flat). The anime model has no twin.
    if (token == "general") net.denoise = std::clamp(denoise, 0, 100);
    if (net.denoise < 100) {
        ai::Model weak, mixed;
        std::string why;
        if (loadModel(dir + weakName_ + ".bin", weak, why) && ai::mixModels(net.model, weak, net.denoise, mixed, why)) {
            net.model = std::move(mixed);
        } else {
            LOG_ERROR("AI textures: %s; the model alone, denoise 100%%", why.c_str());
            net.denoise = 100;
        }
    }
    net.folder = ai::netFolder(token, net.denoise, scale_);
    return true;
}

bool AiTextures::start(const std::string& gameDir, const AiSettings& s, std::string& error) {
    settings_ = s;
    targetMs_ = std::clamp(double(s.budget), 0.5, 20.0);
    scale_ = s.scale == 2 ? 2 : 4;
    minSide_ = std::min(s.minSize, s.maxSize);
    maxSide_ = std::max(s.minSize, s.maxSize);
    ahead_ = s.ahead;
    generate_ = s.generate;
    const std::string dir = gameDir + "\\uaro-graphics\\ai\\";
    bool anyKind = false;
    for (int k = 0; k < ai::kKinds; ++k) {
        if (!aiKind(s, k)) continue;
        kindOn_[k] = anyKind = true;
        const std::string& token = k < 2 ? s.model : s.spriteModel;  // the ground and objects, sprites and effects
        // 0.42: the folder of the kind's pictures in upscaled.grf, also without its network.
        folders_[k] = ai::netFolder(token, token == "general" ? std::clamp(s.denoise, 0, 100) : 100, scale_);
        if (!generate_) continue;  // the pack and upscaled.grf only: no network is loaded
        for (size_t i = 0; i < nets_.size() && netOf_[k] < 0; ++i)
            if (nets_[i]->token == token) netOf_[k] = int(i);
        if (netOf_[k] < 0) {
            auto net = std::make_unique<Net>();
            if (!loadNet(dir, token, s.denoise, *net, error)) return false;
            features_ = std::max(features_, net->model.features);
            nets_.push_back(std::move(net));
            netOf_[k] = int(nets_.size()) - 1;
        }
        folders_[k] = nets_[size_t(netOf_[k])]->folder;
    }
    if (!anyKind) {
        error = "no kind of texture is chosen (the AI textures page)";
        return false;
    }
    if (!createGpu(error)) return false;
    const ULONGLONG t0 = GetTickCount64();
    for (size_t i = 0; i < nets_.size(); ++i)
        if (!selfTest(int(i), error)) return false;
    const ULONGLONG tested = GetTickCount64() - t0;
    // 0.42: the texture pack - read by the I/O thread, looked up here.
    const std::string packPath = gameDir + "\\uaro-textures.zip";
    std::string packError;
    if (!pack_.open(packPath, packError) && GetFileAttributesA(packPath.c_str()) != INVALID_FILE_ATTRIBUTES)
        LOG_ERROR("AI textures: the texture pack cannot be read (%s)", packError.c_str());
    // 0.44: the kinds of the pack's world pictures and big interface pictures, by their keys - ahead, a map's
    // textures come from the pack as the game loads them, before they are drawn.
    std::vector<uint8_t> index;
    if (pack_.isOpen() && pack_.read(pack::kIndexName, index)) {
        const std::string text(index.begin(), index.end());
        std::unordered_map<std::string, ai::Kind> pictureKinds;  // a picture's name -> its kind
        for (const auto& e : pack::parseIndex(text)) {
            pack::Group g = pack::Group::World;
            std::string key;
            if (int(e.second) >= ai::kKinds || !pack::keyOfPicture(e.first, g, key) || g == pack::Group::Sprites)
                continue;
            pictureKinds[e.first] = e.second;
            if (ahead_ && kinds_.emplace(key, e.second).second) ++packAhead_;
        }
        // 0.45: a texture at a lower texture detail - its own key - takes its whole texture's picture.
        for (const auto& a : pack::parseAliases(text)) {
            if (!pack_.has(a.second)) continue;
            aliases_.emplace(a.first, a.second);
            const auto k = pictureKinds.find(a.second);
            if (ahead_ && k != pictureKinds.end() && kinds_.emplace(a.first, k->second).second) ++packAhead_;
        }
    }
    // The copies' share of the video memory.
    systemBudget_ = systemMemory(nullptr);
    vramBudget_ = ai::copiesBudget(systemBudget_);

    // upscaled.grf: written by one game at a time (a second client reads it only).
    writerMutex_ = CreateMutexA(nullptr, FALSE, "Local\\uaro-graphics-upscaled-grf");
    writable_ = writerMutex_ && WaitForSingleObject(writerMutex_, 0) == WAIT_OBJECT_0;
    const std::string cache = gameDir + "\\upscaled.grf";
    std::string why;
    if (!archive_.open(cache, writable_, why)) {
        LOG_ERROR("AI textures: %s; they are made again every start", why.c_str());
        writable_ = false;
    }
    // From here on only the I/O thread touches the archive; the renderer looks names up in known_. Pictures of other
    // settings stay in the file unused (the settings app deletes them); the tags count for every setting.
    unsigned other = 0;
    for (std::string& name : archive_.names()) {
        const ai::ParsedEntry e = ai::parseEntry(name);
        if (e.type == ai::ParsedEntry::KindTag) {
            kinds_.emplace(e.key, e.kind);
        } else if (e.type == ai::ParsedEntry::MapTag) {
            mapTags_.insert(std::move(name));
        } else if (e.type == ai::ParsedEntry::Picture) {
            bool ours = false;
            for (int k = 0; k < ai::kKinds; ++k)
                ours = ours || (kindOn_[k] && (e.folder == folders_[k] ||
                                               e.folder == ai::kindFolder(folders_[k], ai::Kind::Icons)));
            if (ours)
                known_.insert(std::move(name));
            else
                ++other;
        }
    }
    InitializeCriticalSection(&ioLock_);
    ioLockMade_ = true;
    ioWake_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    ioThread_ = ioWake_ ? CreateThread(nullptr, 0, ioMain, this, 0, nullptr) : nullptr;
    if (!ioThread_) {
        error = "no thread for upscaled.grf";
        return false;
    }
    ready_ = true;
    std::string kinds;
    for (int k = 0; k < ai::kKinds; ++k) {
        const Net* n = netOf_[k] >= 0 ? nets_[size_t(netOf_[k])].get() : nullptr;
        kinds += std::string(k ? ", " : "") + ai::kindName(ai::Kind(k)) + " " +
                 (n ? n->name : kindOn_[k] ? std::string("ready ones") : std::string("off"));
        if (n && n->token == "general") kinds += " (denoise " + std::to_string(n->denoise) + "%)";
    }
    char note[112] = "";
    if (other)
        std::snprintf(note, sizeof note, " (and %u of other settings or older versions, not used: the Ready textures "
                      "page deletes them)", other);
    LOG_INFO("AI textures: on - %s; x%d, textures %d-%d, ahead %s; %s; upscaled.grf has %u textures of these "
             "settings%s%s",
             kinds.c_str(), scale_, minSide_, maxSide_, ahead_ ? "on" : "off",
             generate_ ? ("making new ones in the game, self-test passed in " + std::to_string(tested) + " ms").c_str()
                       : "making none (the pack and upscaled.grf only)",
             static_cast<unsigned>(known_.size()), note, writable_ ? "" : " (read only: another game writes it)");
    if (pack_.isOpen())
        LOG_INFO("AI textures: the texture pack uaro-textures.zip has %u pictures (%u of the world and the interface "
                 "known ahead, %u keys of a lower texture detail)",
                 static_cast<unsigned>(pack_.count() - (pack_.has(pack::kIndexName) ? 1 : 0)), packAhead_,
                 static_cast<unsigned>(aliases_.size()));
    else
        LOG_INFO("AI textures: no texture pack (uaro-textures.zip) in the game's folder");
    LOG_INFO("AI textures: the copies keep to %llu MB of video memory (the system gives the game %llu MB)",
             static_cast<unsigned long long>(vramBudget_ >> 20), static_cast<unsigned long long>(systemBudget_ >> 20));
    return true;
}

uint64_t AiTextures::systemMemory(uint64_t* used) const {
    if (used) *used = 0;
    if (!m_.factory || !m_.device) return 0;
    IDXGIAdapter3* adapter = nullptr;
    if (FAILED(m_.factory->EnumAdapterByLuid(m_.device->GetAdapterLuid(), __uuidof(IDXGIAdapter3),
                                             reinterpret_cast<void**>(&adapter))))
        return 0;
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    const bool ok = SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info));
    adapter->Release();
    if (!ok) return 0;
    if (used) *used = info.CurrentUsage;
    return info.Budget;
}

// 0.44: over the budget, the copies of the textures not drawn for longest (two seconds and more) go.
void AiTextures::putAwayOld() {
    if (!vramBudget_ || aiBytes_ <= vramBudget_ || frame_ - lastPutAway_ < 30) return;
    lastPutAway_ = frame_;
    std::vector<ai::Copy> copies;
    for (const auto& e : items_)
        if (e.second.state == Item::Done && e.second.bytes)
            copies.push_back({e.first, e.second.bytes, e.second.lastUse});
    for (uint32_t id : ai::putAway(copies, aiBytes_, vramBudget_, frame_, 120)) {
        const auto it = items_.find(id);
        if (it == items_.end()) continue;
        dropAi(id);
        it->second.state = Item::Away;
        ++putAway_;
    }
}

void AiTextures::bringBack(uint32_t id, Item& item) {
    item.later = false;
    if (pack_.has(item.name)) {
        item.state = Item::Waiting;
        item.loading = true;
        requestPacked(id, item.name, item.width, item.height, Job());
    } else if (known_.count(item.name)) {
        item.state = Item::Waiting;
        item.loading = true;
        IoRequest r;
        r.id = id;
        r.name = item.name;
        request(std::move(r));
    } else {
        item.state = Item::Skipped;  // made by the network and not kept: the game's own now
        return;
    }
    ++broughtBack_;
}

bool AiTextures::startForPack(const std::string& gameDir, const AiSettings& s, std::string& error) {
    settings_ = s;
    scale_ = 4;
    generate_ = true;
    const std::string dir = gameDir + "\\uaro-graphics\\ai\\";
    for (int k = 0; k < ai::kKinds; ++k) {
        kindOn_[k] = true;
        const std::string& token = k < 2 ? s.model : s.spriteModel;  // the ground and objects, sprites and the rest
        for (size_t i = 0; i < nets_.size() && netOf_[k] < 0; ++i)
            if (nets_[i]->token == token) netOf_[k] = int(i);
        if (netOf_[k] >= 0) continue;
        auto net = std::make_unique<Net>();
        if (!loadNet(dir, token, s.denoise, *net, error)) return false;
        features_ = std::max(features_, net->model.features);
        nets_.push_back(std::move(net));
        netOf_[k] = int(nets_.size()) - 1;
    }
    if (!createGpu(error)) return false;
    for (size_t i = 0; i < nets_.size(); ++i)
        if (!selfTest(int(i), error)) return false;
    ready_ = true;
    return true;
}

bool AiTextures::enlarge(int net, const uint32_t* argb, int w, int h, uint32_t border, std::vector<uint32_t>& out,
                         std::string& error) {
    if (net < 0 || size_t(net) >= nets_.size() || w <= 0 || h <= 0) {
        error = "no network for it";
        return false;
    }
    const ai::Model& model = nets_[size_t(net)]->model;
    Job j;
    j.net = net;
    j.width = w;
    j.height = h;
    j.border = border;
    j.pixels.assign(argb, argb + size_t(w) * size_t(h));
    j.alpha = ai::hasTransparency(j.pixels.data(), w, h);
    const int ow = w * kNetScale, oh = h * kNetScale;
    std::vector<Level> levels;
    UINT64 bytes = 0;
    layout(ow, oh, levels, bytes);
    if (!ensureBuffers(w, h) || !m_.begin() || !uploadInput(j)) {
        error = "no GPU memory for a " + std::to_string(w) + "x" + std::to_string(h) + " picture";
        return false;
    }
    computeBound_ = false;
    for (j.conv = 0; size_t(j.conv) < model.convs.size();) runBand(j, 1e18);
    ID3D12Resource* image =
        buffer(m_.device, D3D12_HEAP_TYPE_DEFAULT, bytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
    const UINT64 pitch = levels[0].pitch;
    ID3D12Resource* back = buffer(m_.device, D3D12_HEAP_TYPE_READBACK, pitch * UINT64(oh),
                                  D3D12_RESOURCE_STATE_COPY_DEST, false);
    bool ok = image && back;
    if (ok) dispatchShuffle(j, image, levels[0]);
    if (ok && j.alpha) {  // the alpha's own pass, as in the game
        ok = uploadAlphaInput(j);
        if (ok) {
            for (j.conv = 0; size_t(j.conv) < model.convs.size();) runBand(j, 1e18);
            dispatchAlpha(j, image, levels[0]);
        }
    }
    if (ok) {
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        m_.transition(image, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_.list->CopyBufferRegion(back, 0, image, 0, pitch * UINT64(oh));
    }
    m_.waitIdle();
    computeBound_ = false;
    void* mapped = nullptr;
    ok = ok && !m_.lost && SUCCEEDED(back->Map(0, nullptr, &mapped));
    if (ok) {
        out.resize(size_t(ow) * size_t(oh));
        for (int y = 0; y < oh; ++y)
            std::memcpy(out.data() + size_t(y) * size_t(ow), static_cast<const uint8_t*>(mapped) + size_t(y) * pitch,
                        size_t(ow) * 4);
        back->Unmap(0, nullptr);
    }
    releaseCom(image);
    releaseCom(back);
    if (!ok) error = m_.lost ? "the graphics card was lost" : "no GPU memory for the picture";
    return ok;
}

bool AiTextures::createGpu(std::string& error) {
    ID3D12Device* device = m_.device;
    const auto compileFn = m_.compiler ? reinterpret_cast<pD3DCompile>(
                                             reinterpret_cast<void*>(GetProcAddress(m_.compiler, "D3DCompile")))
                                       : nullptr;
    using SerializeFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**,
                                         ID3DBlob**);
    const auto serialize = reinterpret_cast<SerializeFn>(
        reinterpret_cast<void*>(GetProcAddress(m_.d3d12, "D3D12SerializeRootSignature")));
    if (!compileFn || !serialize) {
        error = "no shader compiler";
        return false;
    }
    D3D12_ROOT_PARAMETER p[5] = {};
    p[kAiConstants].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    p[kAiConstants].Constants.Num32BitValues = 16;
    p[kAiWeights].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    for (UINT i = 0; i < 3; ++i) {
        p[kAiU0 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        p[kAiU0 + i].Descriptor.ShaderRegister = i;
    }
    for (D3D12_ROOT_PARAMETER& q : p) q.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rd{};
    rd.NumParameters = 5;
    rd.pParameters = p;
    ID3DBlob* sig = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT hr = serialize(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &errors);
    if (SUCCEEDED(hr))
        hr = device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(),
                                         __uuidof(ID3D12RootSignature), reinterpret_cast<void**>(&root_));
    releaseCom(sig);
    releaseCom(errors);
    if (FAILED(hr)) {
        error = "compute root signature";
        return false;
    }
    const char* src = aiShaderSource();
    const auto pso = [&](const char* entry, ID3D12PipelineState*& out) {
        ID3DBlob* cs = nullptr;
        ID3DBlob* msg = nullptr;
        HRESULT r = compileFn(src, std::strlen(src), "uaro-graphics-ai.hlsl", nullptr, nullptr, entry, "cs_5_0",
                              D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &cs, &msg);
        if (FAILED(r)) {
            error = std::string("shader ") + entry + ": " +
                    (msg ? std::string(static_cast<const char*>(msg->GetBufferPointer()), msg->GetBufferSize())
                         : std::string("compile failed"));
        } else {
            D3D12_COMPUTE_PIPELINE_STATE_DESC d{};
            d.pRootSignature = root_;
            d.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
            r = device->CreateComputePipelineState(&d, __uuidof(ID3D12PipelineState), reinterpret_cast<void**>(&out));
            if (FAILED(r)) error = std::string("pipeline ") + entry;
        }
        releaseCom(cs);
        releaseCom(msg);
        return SUCCEEDED(r);
    };
    if (!pso("ai_conv", conv_) || !pso("ai_shuffle", shuffle_) || !pso("ai_mip", mip_) || !pso("ai_alpha", alpha_))
        return false;
    for (auto& n : nets_)
        if (!uploadWeights(*n, error)) return false;
    return true;
}

// The weights, once, into GPU memory.
bool AiTextures::uploadWeights(Net& net, std::string& error) {
    ID3D12Device* device = m_.device;
    const UINT64 bytes = UINT64(net.model.data.size()) * 4;
    net.weights = buffer(device, D3D12_HEAP_TYPE_DEFAULT, bytes, D3D12_RESOURCE_STATE_COPY_DEST, false);
    ID3D12Resource* staging = buffer(device, D3D12_HEAP_TYPE_UPLOAD, bytes, D3D12_RESOURCE_STATE_GENERIC_READ, false);
    void* mapped = nullptr;
    if (!net.weights || !staging || FAILED(staging->Map(0, nullptr, &mapped)) || !m_.begin()) {
        releaseCom(staging);
        error = "no memory for the network's weights";
        return false;
    }
    std::memcpy(mapped, net.model.data.data(), bytes);
    staging->Unmap(0, nullptr);
    m_.list->CopyBufferRegion(net.weights, 0, staging, 0, bytes);
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = net.weights;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    b.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    m_.list->ResourceBarrier(1, &b);
    m_.waitIdle();  // submits too
    releaseCom(staging);
    return !m_.lost;
}

bool AiTextures::ensureBuffers(int width, int height) {
    const UINT64 n = UINT64(width) * UINT64(height);
    const UINT64 planes = n * 4 * 4, feat = n * UINT64(std::max(features_, 3 * kNetScale * kNetScale)) * 4;
    if (planes > planesBytes_) {
        if (planes_) m_.slots[m_.slot].garbage.push_back(planes_);
        planes_ = buffer(m_.device, D3D12_HEAP_TYPE_DEFAULT, planes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
        planesBytes_ = planes_ ? planes : 0;
        planesState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if (feat > featBytes_) {
        for (ID3D12Resource*& r : feat_) {
            if (r) m_.slots[m_.slot].garbage.push_back(r);
            r = buffer(m_.device, D3D12_HEAP_TYPE_DEFAULT, feat, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
        }
        featBytes_ = feat_[0] && feat_[1] ? feat : 0;
    }
    return planes_ && feat_[0] && feat_[1] && planesBytes_ >= planes && featBytes_ >= feat;
}

void AiTextures::bindCompute() {
    if (computeBound_) return;
    m_.list->SetComputeRootSignature(root_);
    computeBound_ = true;
    m_.boundPipeline = nullptr;  // the next draw sets its pipeline again
}

void AiTextures::uavBarrier() {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    b.UAV.pResource = nullptr;
    m_.list->ResourceBarrier(1, &b);
}

bool AiTextures::uploadInput(const Job& job) {
    std::vector<float> planes;
    ai::prepareInput(job.pixels.data(), job.width, job.height, planes);
    const UINT64 bytes = UINT64(planes.size()) * 4;
    const Upload u = m_.allocUpload(bytes, 256);
    if (!u.cpu) return false;
    std::memcpy(u.cpu, planes.data(), bytes);
    m_.transition(planes_, planesState_, D3D12_RESOURCE_STATE_COPY_DEST);
    m_.list->CopyBufferRegion(planes_, 0, m_.slots[m_.slot].upload, u.offset, bytes);
    m_.transition(planes_, planesState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    return true;
}

bool AiTextures::uploadAlphaInput(const Job& job) {
    const size_t n = size_t(job.width) * size_t(job.height);
    std::vector<float> planes(n * 4);
    for (size_t i = 0; i < n; ++i) {
        const float a = (job.pixels[i] >> 24) >= 128 ? 1.0f : 0.0f;
        for (int c = 0; c < 4; ++c) planes[size_t(c) * n + i] = a;
    }
    const UINT64 bytes = UINT64(planes.size()) * 4;
    const Upload u = m_.allocUpload(bytes, 256);
    if (!u.cpu) return false;
    std::memcpy(u.cpu, planes.data(), bytes);
    m_.transition(planes_, planesState_, D3D12_RESOURCE_STATE_COPY_DEST);
    m_.list->CopyBufferRegion(planes_, 0, m_.slots[m_.slot].upload, u.offset, bytes);
    m_.transition(planes_, planesState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    return true;
}

bool AiTextures::startAlpha(Job& job) {
    UINT64 bytes = 0;
    layout(job.width * kNetScale, job.height * kNetScale, job.levels, bytes);
    job.image = buffer(m_.device, D3D12_HEAP_TYPE_DEFAULT, bytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
    if (!job.image) return false;
    dispatchShuffle(job, job.image, job.levels[0]);  // the colour (and a bilinear alpha the next pass replaces)
    if (!uploadAlphaInput(job)) return false;
    job.phase = 1;
    job.conv = 0;
    job.row = 0;
    return true;
}

void AiTextures::dispatchAlpha(const Job& job, ID3D12Resource* image, const Level& level0) {
    AiConstants k;
    k.v[0] = uint32_t(job.width);
    k.v[1] = uint32_t(job.height);
    k.v[2] = level0.pitch / 4;
    k.v[3] = uint32_t(level0.at / 4);
    bindCompute();
    m_.list->SetPipelineState(alpha_);
    m_.list->SetComputeRoot32BitConstants(kAiConstants, 16, k.v, 0);
    const int last = int(nets_[size_t(job.net)]->model.convs.size()) - 1;
    m_.list->SetComputeRootUnorderedAccessView(kAiU0, feat_[last % 2]->GetGPUVirtualAddress());
    m_.list->SetComputeRootUnorderedAccessView(kAiU1, image->GetGPUVirtualAddress());
    m_.list->SetComputeRootUnorderedAccessView(kAiU2, planes_->GetGPUVirtualAddress());
    m_.list->Dispatch(UINT((job.width * kNetScale + 7) / 8), UINT((job.height * kNetScale + 7) / 8), 1);
    uavBarrier();
}

void AiTextures::trimBuffers() {
    const UINT64 normal = UINT64(kTrimSide) * UINT64(kTrimSide) * UINT64(features_) * 4;
    if (featBytes_ <= normal) return;
    for (ID3D12Resource*& r : feat_) {
        if (r) m_.slots[m_.slot].garbage.push_back(r);  // freed once this list has run
        r = nullptr;
    }
    if (planes_) m_.slots[m_.slot].garbage.push_back(planes_);
    planes_ = nullptr;
    featBytes_ = planesBytes_ = 0;
}

void AiTextures::dropJob() {
    if (job_ && job_->image) m_.slots[m_.slot].garbage.push_back(job_->image);  // freed once this list has run
    job_.reset();
    trimBuffers();
}

double AiTextures::runBand(Job& job, double budget) {
    const Net& net = *nets_[size_t(job.net)];
    const ai::Model& model = net.model;
    const ai::Model::Conv& c = model.convs[size_t(job.conv)];
    const double perRow = double(job.width) * c.out * c.in * 9;
    const double fits = budget / perRow;  // may be huge (the self-test runs all of it at once)
    const int rows = fits >= double(job.height) ? job.height : std::max(8, int(fits) / 8 * 8);
    const int end = std::min(job.row + rows, job.height);
    AiConstants k;
    k.v[0] = uint32_t(job.width);
    k.v[1] = uint32_t(job.height);
    k.v[2] = uint32_t(c.in);
    k.v[3] = uint32_t(c.out);
    k.v[4] = uint32_t(c.bias);
    k.v[5] = uint32_t(c.weight);
    k.v[6] = size_t(job.conv) < model.prelu.size() ? uint32_t(model.prelu[size_t(job.conv)]) : 0xFFFFFFFFu;
    k.v[7] = uint32_t(job.row);
    k.v[8] = uint32_t(end);
    // Past the edge: a world texture repeats, a sprite cell stands in nothing, an interface picture's edge goes on.
    k.v[9] = job.border;
    bindCompute();
    m_.list->SetComputeRootShaderResourceView(kAiWeights, net.weights->GetGPUVirtualAddress());
    m_.list->SetPipelineState(conv_);
    m_.list->SetComputeRoot32BitConstants(kAiConstants, 16, k.v, 0);
    ID3D12Resource* in = job.conv == 0 ? planes_ : feat_[(job.conv - 1) % 2];
    m_.list->SetComputeRootUnorderedAccessView(kAiU0, in->GetGPUVirtualAddress());
    m_.list->SetComputeRootUnorderedAccessView(kAiU1, feat_[job.conv % 2]->GetGPUVirtualAddress());
    m_.list->SetComputeRootUnorderedAccessView(kAiU2, planes_->GetGPUVirtualAddress());
    m_.list->Dispatch(UINT((job.width + 7) / 8), UINT((end - job.row + 7) / 8), UINT((c.out + 15) / 16));
    uavBarrier();
    const double macs = perRow * (end - job.row);
    job.row = end;
    if (job.row >= job.height) {
        ++job.conv;
        job.row = 0;
    }
    return macs;
}

void AiTextures::layout(int width, int height, std::vector<Level>& levels, UINT64& bytes) const {
    levels.clear();
    bytes = 0;
    for (int w = width, h = height;; w = std::max(1, w / 2), h = std::max(1, h / 2)) {
        Level l;
        l.at = bytes;
        l.width = w;
        l.height = h;
        l.pitch = UINT(alignUp64(UINT64(w) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
        levels.push_back(l);
        bytes = alignUp64(bytes + UINT64(l.pitch) * UINT64(h), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (w == 1 && h == 1) break;
    }
}

void AiTextures::dispatchShuffle(const Job& job, ID3D12Resource* image, const Level& level0) {
    AiConstants k;
    k.v[0] = uint32_t(job.width);
    k.v[1] = uint32_t(job.height);
    k.v[2] = level0.pitch / 4;
    k.v[3] = uint32_t(level0.at / 4);
    bindCompute();
    m_.list->SetPipelineState(shuffle_);
    m_.list->SetComputeRoot32BitConstants(kAiConstants, 16, k.v, 0);
    const int last = int(nets_[size_t(job.net)]->model.convs.size()) - 1;
    m_.list->SetComputeRootUnorderedAccessView(kAiU0, feat_[last % 2]->GetGPUVirtualAddress());
    m_.list->SetComputeRootUnorderedAccessView(kAiU1, image->GetGPUVirtualAddress());
    m_.list->SetComputeRootUnorderedAccessView(kAiU2, planes_->GetGPUVirtualAddress());
    m_.list->Dispatch(UINT((job.width * kNetScale + 7) / 8), UINT((job.height * kNetScale + 7) / 8), 1);
    uavBarrier();
}

void AiTextures::dispatchMips(ID3D12Resource* image, const std::vector<Level>& levels) {
    bindCompute();
    m_.list->SetPipelineState(mip_);
    m_.list->SetComputeRootUnorderedAccessView(kAiU0, image->GetGPUVirtualAddress());
    m_.list->SetComputeRootUnorderedAccessView(kAiU1, image->GetGPUVirtualAddress());
    m_.list->SetComputeRootUnorderedAccessView(kAiU2, image->GetGPUVirtualAddress());
    for (size_t i = 1; i < levels.size(); ++i) {
        const Level& s = levels[i - 1];
        const Level& d = levels[i];
        AiConstants k;
        k.v[0] = uint32_t(s.width);
        k.v[1] = uint32_t(s.height);
        k.v[2] = s.pitch / 4;
        k.v[3] = uint32_t(s.at / 4);
        k.v[4] = uint32_t(d.width);
        k.v[5] = uint32_t(d.height);
        k.v[6] = d.pitch / 4;
        k.v[7] = uint32_t(d.at / 4);
        m_.list->SetComputeRoot32BitConstants(kAiConstants, 16, k.v, 0);
        m_.list->Dispatch(UINT((d.width + 7) / 8), UINT((d.height + 7) / 8), 1);
        uavBarrier();
    }
}

ID3D12Resource* AiTextures::makeImage(Job& job, std::vector<Level>& levels) {
    if (job.image) {  // the colour's pass filled it: the alpha's now, then its mip levels
        ID3D12Resource* image = job.image;
        job.image = nullptr;
        levels = job.levels;
        m_.slots[m_.slot].garbage.push_back(image);  // freed once this list has run
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        dispatchAlpha(job, image, levels[0]);
        dispatchMips(image, levels);
        m_.transition(image, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
        return image;
    }
    // A network's picture is 4 times the game's texture; one from upscaled.grf is the result's size already.
    const int full = job.loaded ? scale_ : kNetScale;
    const int w = job.width * full, h = job.height * full;
    UINT64 bytes = 0;
    layout(w, h, levels, bytes);
    ID3D12Resource* image =
        buffer(m_.device, D3D12_HEAP_TYPE_DEFAULT, bytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
    if (!image) return nullptr;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    m_.slots[m_.slot].garbage.push_back(image);  // freed once this list has run
    if (job.loaded) {  // level 0 from upscaled.grf
        const Level& l0 = levels[0];
        const Upload u = m_.allocUpload(UINT64(l0.pitch) * UINT64(h), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (!u.cpu) return nullptr;
        for (int y = 0; y < h; ++y)
            std::memcpy(u.cpu + UINT64(y) * l0.pitch, job.pixels.data() + size_t(y) * size_t(w), size_t(w) * 4);
        m_.transition(image, state, D3D12_RESOURCE_STATE_COPY_DEST);
        m_.list->CopyBufferRegion(image, 0, m_.slots[m_.slot].upload, u.offset, UINT64(l0.pitch) * UINT64(h));
        m_.transition(image, state, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    } else {
        dispatchShuffle(job, image, levels[0]);
    }
    dispatchMips(image, levels);
    m_.transition(image, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    return image;
}

void AiTextures::readBack(const Job& job, ID3D12Resource* image, const Level& top) {
    if (job.loaded || (!writable_ && !job.icon)) return;  // an icon goes to the driver even when no file takes it
    Readback r;  // read once this list has run (finishReadbacks)
    const UINT64 size = UINT64(top.pitch) * UINT64(top.height);
    r.buffer = buffer(m_.device, D3D12_HEAP_TYPE_READBACK, size, D3D12_RESOURCE_STATE_COPY_DEST, false);
    if (!r.buffer) return;
    m_.list->CopyBufferRegion(r.buffer, 0, image, top.at, size);
    r.fence = m_.fenceValue + 1;  // the value the open list's submission signals
    r.name = job.name;
    r.width = top.width;
    r.height = top.height;
    r.pitch = top.pitch;
    r.icon = job.icon;
    r.iconKey = job.iconKey;
    readbacks_.push_back(r);
    if (writable_) known_.insert(job.name);
}

bool AiTextures::finish(Job& job) {
    std::vector<Level> levels;
    ID3D12Resource* image = makeImage(job, levels);
    if (!image) return false;
    // For a result of twice the size the network's first mip level is the texture's top level.
    const size_t skip = !job.loaded && scale_ == 2 ? 1 : 0;
    const Level& top = levels[skip];
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = UINT64(top.width);
    td.Height = UINT(top.height);
    td.DepthOrArraySize = 1;
    td.MipLevels = UINT16(levels.size() - skip);
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    ID3D12Resource* tex = nullptr;
    if (FAILED(m_.device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST,
                                                  nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&tex))))
        return false;
    for (size_t i = skip; i < levels.size(); ++i) {
        D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
        src.pResource = image;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = levels[i].at;
        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        src.PlacedFootprint.Footprint.Width = UINT(levels[i].width);
        src.PlacedFootprint.Footprint.Height = UINT(levels[i].height);
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = levels[i].pitch;
        dst.pResource = tex;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = UINT(i - skip);
        m_.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    readBack(job, image, top);
    // The game may have uploaded other pixels meanwhile, or released the texture: the picture is not its any more.
    const auto it = m_.textures.find(job.id);
    const auto item = items_.find(job.id);
    if (it == m_.textures.end() || item == items_.end() || item->second.state != Item::Waiting ||
        item->second.name != job.name) {
        m_.slots[m_.slot].garbage.push_back(tex);
        return true;
    }
    Texture& t = it->second;
    const UINT srv = m_.allocSrv();
    if (srv == kNoSrv) {
        m_.slots[m_.slot].garbage.push_back(tex);
        return false;
    }
    dropAi(job.id);
    t.ai = tex;
    t.aiState = D3D12_RESOURCE_STATE_COPY_DEST;
    m_.transition(t.ai, t.aiState, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    t.aiSrv = srv;
    t.aiWorld = !item->second.ui;
    t.aiUi = item->second.ui;
    m_.device->CreateShaderResourceView(tex, nullptr, m_.cpuSrvHandle(srv));
    item->second.state = Item::Done;
    item->second.bytes = 0;  // 0.44: the copy's video memory
    for (size_t i = skip; i < levels.size(); ++i)
        item->second.bytes += uint64_t(levels[i].width) * uint64_t(levels[i].height) * 4;
    aiBytes_ += item->second.bytes;
    item->second.lastUse = frame_;
    keptBytes_ -= item->second.argb.size() * 4;
    item->second.argb.clear();
    item->second.argb.shrink_to_fit();
    if (job.packed)
        ++donePack_;
    else if (job.loaded)
        ++doneLoaded_;
    else
        ++doneNetwork_;
    return true;
}

// The game's sprite cache (0.13): a 256x256 texture whose cells get one frame after another. It is never enlarged as
// a whole - its contents change all the time - but cell by cell into its enlarged copy (Texture::ai), where the game's
// own texture coordinates find the enlarged frame. A new frame goes in plainly enlarged at once, so that a cell
// never shows the frame it held before, and the network's picture replaces it once done, if the cell still holds it.
bool AiTextures::cellOn(uint8_t source) const {
    switch (sprite_exe::Source(source)) {
        case sprite_exe::Source::Monster: return settings_.monsters;
        case sprite_exe::Source::Player: return settings_.players;
        case sprite_exe::Source::Npc: return settings_.npcs;
        case sprite_exe::Source::Item: return settings_.items;
        case sprite_exe::Source::Effect: return settings_.effects;
        case sprite_exe::Source::Interface: return settings_.icons;  // the cursor
        case sprite_exe::Source::Unknown: break;
    }
    return settings_.monsters;  // a file the loader's hook did not see: as most sprites are
}

void AiTextures::noteCells(uint32_t id, Texture& t, const UploadCmd& u, const uint32_t* argb, const SpriteCell* cells,
                           uint32_t count) {
    const auto item = items_.find(id);
    if (item != items_.end()) {
        keptBytes_ -= item->second.argb.size() * 4;
        items_.erase(item);
    }
    // The network for sprites (the model for characters): any kind of theirs that is on has made it (0.42: when
    // generating); their pictures in upscaled.grf are in its folder.
    int net = -1, folder = -1;
    for (ai::Kind k : {ai::Kind::Sprites, ai::Kind::Effects, ai::Kind::Icons}) {
        if (net < 0) net = netOf_[int(k)];
        if (folder < 0 && kindOn_[int(k)]) folder = int(k);
    }
    if (folder < 0) return;
    if (net < 0 && !pack_.isOpen() && known_.empty()) return;  // 0.42: nothing to show and none made: as the game
    auto a = atlases_.find(id);
    const bool made = a == atlases_.end();
    if (made) {
        if (!makeAtlas(id, t)) return;
        a = atlases_.find(id);
    }
    if (!count) {  // a change of no frame: as it is
        standIn(t, a->second, u.rect, argb, u.rect.width());
        return;
    }
    if (made) {  // what it held already, as it is: where it has anything (the enlarged copy starts cleared)
        Rect box{u.rect.right, u.rect.bottom, u.rect.left, u.rect.top};
        for (int y = u.rect.top; y < u.rect.bottom; ++y)
            for (int x = u.rect.left; x < u.rect.right; ++x)
                if (argb[size_t(y - u.rect.top) * size_t(u.rect.width()) + size_t(x - u.rect.left)]) {
                    box.left = std::min(box.left, x);
                    box.top = std::min(box.top, y);
                    box.right = std::max(box.right, x + 1);
                    box.bottom = std::max(box.bottom, y + 1);
                }
        // A texel more on each side: the enlargement blends with its neighbours.
        box = Rect{std::max(box.left - 1, u.rect.left), std::max(box.top - 1, u.rect.top),
                   std::min(box.right + 1, u.rect.right), std::min(box.bottom + 1, u.rect.bottom)};
        if (!box.empty())
            standIn(t, a->second, box,
                    argb + size_t(box.top - u.rect.top) * size_t(u.rect.width()) + size_t(box.left - u.rect.left),
                    u.rect.width());
    }
    for (uint32_t i = 0; i < count; ++i) {
        const Rect c = cells[i].rect;
        if (c.empty() || c.left < u.rect.left || c.top < u.rect.top || c.right > u.rect.right ||
            c.bottom > u.rect.bottom)
            continue;
        const int w = c.width(), h = c.height();
        std::vector<uint32_t> pixels(size_t(w) * size_t(h));
        for (int y = 0; y < h; ++y)
            std::memcpy(pixels.data() + size_t(y) * size_t(w),
                        argb + size_t(c.top - u.rect.top + y) * size_t(u.rect.width()) + size_t(c.left - u.rect.left),
                        size_t(w) * 4);
        if (!made) standIn(t, a->second, c, pixels.data(), w);  // the frame as the game has it, at once
        ++cellsWritten_;
        if (!cellOn(cells[i].source)) continue;  // a kind that is off: it stays as the game has it
        // 0.22.2: a frame of soft light alone, as the game has it; 0.39.2: or shaped by a soft alpha.
        if (ai::softPicture(pixels.data(), w, h) || ai::softAlpha(pixels.data(), w, h)) {
            ++soft_;
            continue;
        }
        Job j;
        j.id = id;
        j.net = net;
        j.width = w;
        j.height = h;
        j.cell = true;
        j.cellX = c.left;
        j.cellY = c.top;
        j.border = 1;
        j.alpha = ai::hasTransparency(pixels.data(), w, h);
        j.frame = ++a->second.frames[uint32_t(c.top) << 16 | uint32_t(c.left)];
        const std::string key = ai::keyName(ai::textureKey(pixels.data(), w, h));
        // 0.42: from the texture pack first.
        const std::string packed = pack::pictureName(pack::Group::Sprites, key);
        if (pack_.has(packed)) {
            j.pixels = std::move(pixels);
            requestPacked(id, packed, w, h, std::move(j));
            continue;
        }
        j.name = ai::entryName(net >= 0 ? nets_[size_t(net)]->folder : folders_[folder], key);
        const sprite_exe::Source source = sprite_exe::Source(cells[i].source);
        const ai::Kind kind = source == sprite_exe::Source::Effect      ? ai::Kind::Effects
                              : source == sprite_exe::Source::Interface ? ai::Kind::Icons
                                                                        : ai::Kind::Sprites;
        if (kinds_.emplace(key, kind).second) tag(ai::kindTag(kind, key));
        if (!map_.empty()) {
            std::string name = ai::mapTag(map_, key);
            if (mapTags_.insert(name).second) tag(name);
        }
        if (known_.count(j.name)) {  // made before: from upscaled.grf
            IoRequest r;
            r.id = id;
            r.name = j.name;
            j.pixels = std::move(pixels);
            r.cell = std::move(j);
            request(std::move(r));
        } else if (net >= 0) {
            j.pixels = std::move(pixels);
            cells_.push_back(std::move(j));
            if (cells_.size() > 4096) cells_.pop_front();  // a crowd: the oldest frames are likely gone anyway
        }  // 0.42: else not generating - the frame stays as the game has it
    }
}

bool AiTextures::makeAtlas(uint32_t id, Texture& t) {
    Atlas a;
    a.levels = scale_ == 4 ? 4 : 3;  // down to half the game's texels: sprites are seldom smaller on the screen
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = UINT64(t.width) * UINT64(scale_);
    td.Height = UINT(t.height * scale_);
    td.DepthOrArraySize = 1;
    td.MipLevels = UINT16(a.levels);
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    ID3D12Resource* tex = nullptr;
    if (FAILED(m_.device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST,
                                                  nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&tex))))
        return false;
    const UINT srv = m_.allocSrv();
    if (srv == kNoSrv) {
        m_.slots[m_.slot].garbage.push_back(tex);
        return false;
    }
    dropAi(id);  // a picture of it as a whole, made before it was seen to be a sprite cache
    t.ai = tex;
    t.aiState = D3D12_RESOURCE_STATE_COPY_DEST;
    t.aiSrv = srv;
    t.aiWorld = true;
    t.aiUi = true;  // the cursor and the interface's other sprites come from the same cache
    m_.device->CreateShaderResourceView(tex, nullptr, m_.cpuSrvHandle(srv));
    atlases_[id] = std::move(a);
    return true;
}

void AiTextures::standIn(Texture& t, const Atlas& a, const Rect& r, const uint32_t* argb, int pitch) {
    const int w = r.width(), h = r.height();
    std::vector<uint32_t> src(size_t(w) * size_t(h));
    for (int y = 0; y < h; ++y)
        std::memcpy(src.data() + size_t(y) * size_t(w), argb + size_t(y) * size_t(pitch), size_t(w) * 4);
    std::vector<uint32_t> level, next;
    ai::enlargeNearest(src.data(), w, h, scale_, level);  // the game's own look until the network's picture comes
    int lw = w * scale_, lh = h * scale_;
    for (int i = 0; i < a.levels; ++i) {
        writeRegion(t, i, (r.left * scale_) >> i, (r.top * scale_) >> i, lw, lh, level);
        if (i + 1 == a.levels) break;
        int nw = 0, nh = 0;
        ai::halve(level, lw, lh, next, nw, nh);
        level.swap(next);
        lw = nw;
        lh = nh;
    }
}

void AiTextures::writeRegion(Texture& t, int level, int x, int y, int w, int h, const std::vector<uint32_t>& argb) {
    const int dw = std::max(1, (t.width * scale_) >> level), dh = std::max(1, (t.height * scale_) >> level);
    const int cw = std::min(w, dw - x), ch = std::min(h, dh - y);
    if (cw <= 0 || ch <= 0) return;
    const UINT pitch = UINT(alignUp64(UINT64(cw) * 4, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
    const Upload up = m_.allocUpload(UINT64(pitch) * UINT64(ch), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    if (!up.cpu) return;  // no room this frame: the cell keeps what it had until its picture comes
    for (int row = 0; row < ch; ++row)
        std::memcpy(up.cpu + UINT64(row) * pitch, argb.data() + size_t(row) * size_t(w), size_t(cw) * 4);
    m_.transition(t.ai, t.aiState, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
    src.pResource = m_.slots[m_.slot].upload;
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint.Offset = up.offset;
    src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    src.PlacedFootprint.Footprint.Width = UINT(cw);
    src.PlacedFootprint.Footprint.Height = UINT(ch);
    src.PlacedFootprint.Footprint.Depth = 1;
    src.PlacedFootprint.Footprint.RowPitch = pitch;
    dst.pResource = t.ai;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = UINT(level);
    m_.list->CopyTextureRegion(&dst, UINT(x), UINT(y), 0, &src, nullptr);
}

bool AiTextures::cellCurrent(const Job& job) const {
    const auto a = atlases_.find(job.id);
    if (a == atlases_.end()) return false;
    const auto f = a->second.frames.find(uint32_t(job.cellY) << 16 | uint32_t(job.cellX));
    const auto t = m_.textures.find(job.id);
    return f != a->second.frames.end() && f->second == job.frame && t != m_.textures.end() && t->second.ai;
}

bool AiTextures::finishIcon(Job& job) {
    std::vector<Level> levels;
    ID3D12Resource* image = makeImage(job, levels);
    if (!image) return false;
    readBack(job, image, levels[!job.loaded && scale_ == 2 ? 1 : 0]);  // to the driver once this list has run
    ++iconsNetwork_;
    return true;
}

void AiTextures::noteIcon(uint64_t key, int width, int height, const uint32_t* argb) {
    const int net = netOf_[int(ai::Kind::Icons)];
    if (!ready_ || !kindOn_[int(ai::Kind::Icons)] || width <= 0 || height <= 0 || !icons_.insert(key).second) return;
    Job j;
    j.net = net;
    j.width = width;
    j.height = height;
    j.icon = true;
    j.iconKey = key;
    j.border = 2;
    j.pixels.assign(argb, argb + size_t(width) * size_t(height));
    j.alpha = ai::hasTransparency(j.pixels.data(), width, height);
    const std::string k = ai::keyName(ai::textureKey(j.pixels.data(), width, height));
    const std::string packed = pack::pictureName(pack::Group::Ui, k);  // 0.42: the texture pack first
    if (pack_.has(packed)) {
        requestPacked(0, packed, width, height, std::move(j));
        return;
    }
    j.name = ai::entryName(ai::kindFolder(folders_[int(ai::Kind::Icons)], ai::Kind::Icons), k);
    if (kinds_.emplace(k, ai::Kind::Icons).second) tag(ai::kindTag(ai::Kind::Icons, k));
    if (known_.count(j.name)) {  // made before: from upscaled.grf
        IoRequest r;
        r.name = j.name;
        r.cell = std::move(j);
        request(std::move(r));
    } else if (net >= 0) {  // in turn with the sprite frames - ahead of them (0.22.8): a window shows blocks meanwhile
        cells_.insert(std::find_if(cells_.begin(), cells_.end(), [](const Job& c) { return !c.icon; }), std::move(j));
    } else if (m_.iconSink) {  // 0.47.1: none made and none to make - crisp text makes it by xBR
        m_.iconSink(m_.iconContext, key, 0, 0, nullptr);
    }
}

bool AiTextures::finishCell(Job& job) {
    std::vector<Level> levels;
    ID3D12Resource* image = makeImage(job, levels);
    if (!image) return false;
    const size_t skip = !job.loaded && scale_ == 2 ? 1 : 0;
    readBack(job, image, levels[skip]);
    if (!cellCurrent(job)) return true;  // the cell has another frame by now: the picture waits in upscaled.grf
    Texture& t = m_.textures.find(job.id)->second;
    const Atlas& a = atlases_.find(job.id)->second;
    m_.transition(t.ai, t.aiState, D3D12_RESOURCE_STATE_COPY_DEST);
    for (int i = 0; i < a.levels && skip + size_t(i) < levels.size(); ++i) {
        const Level& l = levels[skip + size_t(i)];
        const int x = (job.cellX * scale_) >> i, y = (job.cellY * scale_) >> i;
        const int dw = std::max(1, (t.width * scale_) >> i), dh = std::max(1, (t.height * scale_) >> i);
        if (x >= dw || y >= dh) break;
        D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
        src.pResource = image;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = l.at;
        src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        src.PlacedFootprint.Footprint.Width = UINT(l.width);
        src.PlacedFootprint.Footprint.Height = UINT(l.height);
        src.PlacedFootprint.Footprint.Depth = 1;
        src.PlacedFootprint.Footprint.RowPitch = l.pitch;
        dst.pResource = t.ai;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = UINT(i);
        const D3D12_BOX box{0, 0, 0, UINT(std::min(l.width, dw - x)), UINT(std::min(l.height, dh - y)), 1};
        m_.list->CopyTextureRegion(&dst, UINT(x), UINT(y), 0, &src, &box);
    }
    if (job.packed)
        ++cellsPack_;
    else if (job.loaded)
        ++cellsLoaded_;
    else
        ++cellsNetwork_;
    return true;
}

void AiTextures::dropAi(uint32_t id) {
    const auto item = items_.find(id);
    if (item != items_.end() && item->second.bytes) {
        aiBytes_ -= std::min(aiBytes_, item->second.bytes);
        item->second.bytes = 0;
    }
    const auto it = m_.textures.find(id);
    if (it == m_.textures.end() || !it->second.ai) return;
    m_.slots[m_.slot].garbage.push_back(it->second.ai);
    m_.freeSrv.push_back(it->second.aiSrv);
    it->second.ai = nullptr;
    it->second.aiSrv = 0;
}

void AiTextures::drop(Item& item) {
    keptBytes_ -= item.argb.size() * 4;
    item.argb.clear();
    item.argb.shrink_to_fit();
    item.state = Item::Skipped;
}

void AiTextures::noteUpload(uint32_t id, Texture& t, const UploadCmd& u, const uint32_t* argb,
                            const SpriteCell* cells, uint32_t cellCount) {
    if (!ready_ || u.level != 0) return;
    if (cellCount || atlases_.count(id)) {  // a sprite cache: cell by cell
        noteCells(id, t, u, argb, cells, cellCount);
        return;
    }
    if (t.ai) dropAi(id);  // the game changed a texture the network had done: its own pixels again
    const bool whole = u.rect.left == 0 && u.rect.top == 0 && u.rect.width() == t.width && u.rect.height() == t.height;
    const int most = std::max(maxSide_, kUiPictureMax);  // the world's textures get the setting's at begin()
    if (!whole || t.width < minSide_ || t.height < minSide_ || t.width > most || t.height > most) {
        const auto it = items_.find(id);
        if (it != items_.end()) {
            keptBytes_ -= it->second.argb.size() * 4;
            items_.erase(it);
        }
        return;
    }
    Item& item = items_[id];
    keptBytes_ -= item.argb.size() * 4;
    item.argb.clear();
    item.state = Item::Kept;  // drawn again, it gets its turn again (a result of the old pixels is not taken)
    item.later = item.loading = item.ui = false;
    item.name.clear();
    item.mapSerial = 0;
    if (++item.uploads > 3) {  // changed again and again (an effect): not worth the network
        item.state = Item::Skipped;
        return;
    }
    const size_t n = size_t(t.width) * size_t(t.height);
    if (keptBytes_ + n * 4 > kKeptLimit) {
        item.state = Item::Skipped;
        return;
    }
    item.argb.assign(argb, argb + n);
    keptBytes_ += n * 4;
    item.width = t.width;
    item.height = t.height;
    item.key = ai::keyName(ai::textureKey(argb, t.width, t.height));
    // Ahead: a texture of a kind upscaled.grf knows from an earlier visit goes before it is drawn, after the drawn.
    if (ahead_) {
        const auto k = kinds_.find(item.key);
        if (k != kinds_.end()) begin(id, item, k->second, true);
    }
}

void AiTextures::begin(uint32_t id, Item& item, ai::Kind kind, bool later) {
    item.kind = kind;
    const int k = int(kind);
    if (k >= ai::kKinds || !kindOn_[k]) {  // a lightmap, no world texture, or a kind that is off
        drop(item);
        return;
    }
    item.ui = kind == ai::Kind::Icons;  // through noteUpload only the interface's big pictures have this kind
    if (!item.ui && (item.width > maxSide_ || item.height > maxSide_)) {  // past the page's "texture sides"
        drop(item);
        return;
    }
    // 0.22.2: soft light alone - a torch's glow, a halo, a gradient - stays the game's, also when upscaled.grf has
    // a picture of it from before: the network would turn its gradients into bands. 0.39.2: so does a picture shaped
    // by a soft alpha (smoke, glowing particles): the network's alpha is all or nothing.
    if (item.argb.size() >= size_t(item.width) * size_t(item.height) &&
        (ai::softPicture(item.argb.data(), item.width, item.height) ||
         ai::softAlpha(item.argb.data(), item.width, item.height))) {
        ++soft_;
        drop(item);
        return;
    }
    item.net = netOf_[k];  // -1 when not generating (0.42)
    item.state = Item::Waiting;
    if (kinds_.emplace(item.key, kind).second) tag(ai::kindTag(kind, item.key));
    // 0.42: the texture pack first, then upscaled.grf, then - generating - the network.
    std::string packed = pack::pictureName(item.ui ? pack::Group::Ui : pack::Group::World, item.key);
    if (!pack_.has(packed) && !item.ui) {  // 0.45: at a lower texture detail - the whole texture's picture
        const auto a = aliases_.find(item.key);
        if (a != aliases_.end()) packed = a->second;
    }
    if (pack_.has(packed)) {
        item.name = packed;
        item.loading = true;
        requestPacked(id, packed, item.width, item.height, Job());
        return;
    }
    item.name = ai::entryName(ai::kindFolder(folders_[k], kind), item.key);
    if (known_.count(item.name)) {  // done before: from upscaled.grf
        item.loading = true;
        IoRequest r;
        r.id = id;
        r.name = item.name;
        request(std::move(r));
    } else if (item.net < 0) {  // not generating: it stays the game's
        drop(item);
    } else if (later) {
        item.later = true;
        later_.push_back(id);
    } else {
        queue_.push_back(id);
    }
}

void AiTextures::requestPacked(uint32_t id, const std::string& name, int width, int height, Job cell) {
    IoRequest r;
    r.pack = true;
    r.id = id;
    r.name = name;
    r.width = width;
    r.height = height;
    r.cell = std::move(cell);
    request(std::move(r));
}

void AiTextures::tag(const std::string& name) {
    if (!writable_ || !generate_) return;  // 0.42: the tags are the generating game's
    IoRequest r;
    r.write = true;
    r.tag = true;
    r.name = name;
    request(std::move(r));
}

void AiTextures::noteWorldUse(uint32_t id, ai::Kind kind) {
    const auto it = items_.find(id);
    if (it == items_.end()) return;
    Item& item = it->second;
    if (kind == ai::Kind::None) return;  // a second stage of a draw that is no ground: says nothing of the texture
    item.lastUse = frame_;
    if (item.state == Item::Away) {  // 0.44: its copy was put away - back from the pack or upscaled.grf
        bringBack(id, item);
        return;
    }
    if (item.state == Item::Kept) {
        ++seen_[size_t(kind)];
        begin(id, item, kind, false);
    } else if (item.state == Item::Waiting && item.later) {  // done ahead, and in view now: its turn now
        item.later = false;
        if (!item.loading) queue_.push_back(id);
    }
    // The maps a texture of the kinds on is seen on, once a map each.
    if (item.mapSerial != mapSerial_ && (item.state == Item::Waiting || item.state == Item::Done)) {
        item.mapSerial = mapSerial_;
        if (!map_.empty()) {
            std::string name = ai::mapTag(map_, item.key);
            if (mapTags_.insert(name).second) tag(name);
        }
    }
}

void AiTextures::noteUiUse(uint32_t id) {
    const auto it = items_.find(id);
    if (it == items_.end()) return;
    Item& item = it->second;
    item.lastUse = frame_;
    if (item.state == Item::Away) {
        bringBack(id, item);
        return;
    }
    if (item.state != Item::Kept) return;
    if (item.width < kUiPictureSide || item.height < kUiPictureSide) return;  // no big picture: maybe a world one
    ++seen_[int(ai::Kind::Icons)];
    begin(id, item, ai::Kind::Icons, false);
}

void AiTextures::setMap(const std::string& map) {
    if (map == map_) return;
    map_ = map;
    ++mapSerial_;
}

void AiTextures::forget(uint32_t id) {
    const auto it = items_.find(id);
    if (it != items_.end()) {
        aiBytes_ -= std::min(aiBytes_, it->second.bytes);  // its copy goes with the texture (Renderer::retireDying)
        keptBytes_ -= it->second.argb.size() * 4;
        items_.erase(it);
    }
    atlases_.erase(id);  // its enlarged copy goes with the texture (Renderer: Texture::ai)
    if (job_ && job_->id == id) dropJob();  // its buffers serve the next one
}

bool AiTextures::nextJob() {
    if (!loaded_.empty()) {
        job_ = std::make_unique<Job>(std::move(loaded_.front()));
        loaded_.pop_front();
        return true;
    }
    // Sprite frames and the rest of the world take turns: a crowd's stream of new frames does not hold up the ground.
    cellTurn_ = !cellTurn_;
    while (!cells_.empty() && (cellTurn_ || queue_.empty())) {
        Job j = std::move(cells_.front());
        cells_.pop_front();
        if (!j.icon && !cellCurrent(j)) continue;  // the cell has another frame by now
        job_ = std::make_unique<Job>(std::move(j));
        return true;
    }
    for (std::deque<uint32_t>* q : {&queue_, &later_}) {
        while (!q->empty()) {
            const uint32_t id = q->front();
            q->pop_front();
            const auto it = items_.find(id);
            if (it == items_.end()) continue;
            Item& item = it->second;
            if (item.state != Item::Waiting || item.loading || item.argb.empty()) continue;
            if (q == &later_ && !item.later) continue;  // drawn meanwhile: queue_ has it
            item.later = false;
            job_ = std::make_unique<Job>();
            job_->id = id;
            job_->net = item.net;
            job_->name = item.name;
            job_->width = item.width;
            job_->height = item.height;
            job_->pixels = item.argb;
            job_->alpha = ai::hasTransparency(item.argb.data(), item.width, item.height);
            job_->border = item.ui ? 2 : 0;
            return true;
        }
    }
    while (!cells_.empty()) {  // the world's turn, and it had nothing
        Job j = std::move(cells_.front());
        cells_.pop_front();
        if (!j.icon && !cellCurrent(j)) continue;
        job_ = std::make_unique<Job>(std::move(j));
        return true;
    }
    return false;
}

void AiTextures::takeLoaded() {
    std::deque<IoResult> results;
    EnterCriticalSection(&ioLock_);
    results.swap(ioOut_);
    LeaveCriticalSection(&ioLock_);
    for (IoResult& r : results) {
        if (r.cell.icon) {  // an icon: straight back to the driver, or the network after all (0.42: generating)
            Job j = std::move(r.cell);
            if (r.ok && r.width == j.width * scale_ && r.height == j.height * scale_ && m_.iconSink) {
                m_.iconSink(m_.iconContext, j.iconKey, r.width, r.height, r.argb.data());
                ++(r.pack ? iconsPack_ : iconsLoaded_);
            } else if (j.net >= 0) {
                cells_.push_back(std::move(j));
            } else if (m_.iconSink) {  // 0.47.1: not usable and none to make
                m_.iconSink(m_.iconContext, j.iconKey, 0, 0, nullptr);
            }
            continue;
        }
        if (r.cell.cell) {  // a sprite cache's cell: the picture, or the network after all (0.42: generating)
            Job j = std::move(r.cell);
            if (!cellCurrent(j)) continue;
            if (r.ok && r.width == j.width * scale_ && r.height == j.height * scale_) {
                j.loaded = true;
                j.packed = r.pack;
                j.pixels = std::move(r.argb);
                loaded_.push_back(std::move(j));
            } else if (j.net >= 0) {
                cells_.push_back(std::move(j));
            }
            continue;
        }
        const auto it = items_.find(r.id);
        if (it == items_.end()) continue;
        Item& item = it->second;
        if (item.state != Item::Waiting || !item.loading || item.name != r.name) continue;
        item.loading = false;
        if (!r.ok || r.width != item.width * scale_ || r.height != item.height * scale_) {
            if (item.net < 0 || item.argb.empty()) {  // 0.42: not generating (0.44: or brought back) - the game's
                drop(item);
                continue;
            }
            (item.later ? later_ : queue_).push_back(r.id);  // not usable: the network makes it again
            continue;
        }
        item.later = false;
        Job j;
        j.id = r.id;
        j.net = item.net;
        j.name = item.name;
        j.width = item.width;
        j.height = item.height;
        j.loaded = true;
        j.packed = r.pack;
        j.pixels = std::move(r.argb);
        loaded_.push_back(std::move(j));
    }
}

void AiTextures::finishReadbacks() {
    if (readbacks_.empty()) return;
    const UINT64 done = m_.fence->GetCompletedValue();
    for (size_t i = 0; i < readbacks_.size();) {
        Readback& r = readbacks_[i];
        if (r.fence > done) {
            ++i;
            continue;
        }
        void* mapped = nullptr;
        D3D12_RANGE range{0, SIZE_T(r.pitch) * SIZE_T(r.height)};
        if (SUCCEEDED(r.buffer->Map(0, &range, &mapped))) {
            IoRequest w;
            w.write = true;
            w.name = r.name;
            w.width = r.width;
            w.height = r.height;
            w.argb.resize(size_t(r.width) * size_t(r.height));
            for (int y = 0; y < r.height; ++y)
                std::memcpy(w.argb.data() + size_t(y) * size_t(r.width),
                            static_cast<const uint8_t*>(mapped) + size_t(y) * r.pitch, size_t(r.width) * 4);
            D3D12_RANGE none{0, 0};
            r.buffer->Unmap(0, &none);
            if (r.icon && m_.iconSink) m_.iconSink(m_.iconContext, r.iconKey, w.width, w.height, w.argb.data());
            if (writable_) request(std::move(w));
        }
        releaseCom(r.buffer);
        readbacks_.erase(readbacks_.begin() + std::ptrdiff_t(i));
    }
}

void AiTextures::work() {
    if (!ready_ || m_.lost || !m_.open) return;
    computeBound_ = false;
    ++frame_;
    putAwayOld();
    takeLoaded();
    finishReadbacks();
    double budget = budget_;
    double spent = 0;
    // Finished pictures: their mips and copies are not free either - up to 4M texels a frame (a 512x512 texture
    // enlarged 4 times, or many sprite frames).
    double finishing = 4.0 * 1024 * 1024;
    const auto texels = [this](const Job& j) {
        return double(j.width) * double(j.height) * (j.loaded ? scale_ * scale_ : kNetScale * kNetScale);
    };
    while (budget > 0) {
        if (!job_ && !nextJob()) break;
        Job& j = *job_;
        if (j.loaded) {
            if (finishing <= 0) break;
            finishing -= texels(j);
            if (j.cell) {
                finishCell(j);
            } else if (j.icon) {
                finishIcon(j);
            } else if (!finish(j)) {
                const auto it = items_.find(j.id);
                if (it != items_.end() && it->second.state == Item::Waiting) queue_.push_back(j.id);
            }
            dropJob();
            continue;
        }
        if (j.conv < 0) {
            if (!ensureBuffers(j.width, j.height) || !uploadInput(j)) {
                LOG_ERROR("AI textures: no GPU memory for a %dx%d texture; it stays as it is", j.width, j.height);
                const auto it = j.cell || j.icon ? items_.end() : items_.find(j.id);
                if (it != items_.end()) drop(it->second);
                dropJob();
                continue;
            }
            j.conv = 0;
            j.row = 0;
        }
        if (size_t(j.conv) < nets_[size_t(j.net)]->model.convs.size()) {
            const double macs = runBand(j, budget);
            budget -= macs;
            spent += macs;
            continue;
        }
        if (j.alpha && j.phase == 0) {  // transparent texels: the alpha's own pass from the next band on
            if (!startAlpha(j)) {
                LOG_ERROR("AI textures: no GPU memory for a %dx%d picture; it stays as it is", j.width, j.height);
                const auto it = j.cell || j.icon ? items_.end() : items_.find(j.id);
                if (it != items_.end()) drop(it->second);
                dropJob();
            }
            continue;
        }
        if (finishing <= 0) break;
        finishing -= texels(j);
        if (j.cell) {
            finishCell(j);
        } else if (j.icon) {
            finishIcon(j);
        } else if (!finish(j)) {
            const auto it = items_.find(j.id);
            if (it != items_.end()) drop(it->second);
        }
        dropJob();
    }
    scheduled_[m_.slot % 2] = spent;
    logNow(false);
}

void AiTextures::observe(UINT slot, double ms) {
    const double macs = scheduled_[slot % 2];
    scheduled_[slot % 2] = 0;
    if (macs < 1e7 || ms < 0.02) return;  // too little to measure
    const double want = macs / ms * targetMs_;
    budget_ = std::clamp(budget_ * 0.5 + want * 0.5, 5e7, 5e10);
}

void AiTextures::logNow(bool final) {
    const ULONGLONG now = GetTickCount64();
    if (!final && now - lastLog_ < 30000) return;
    lastLog_ = now;
    if (!doneNetwork_ && !doneLoaded_ && !donePack_ && queue_.empty() && later_.empty() && !cellsWritten_ &&
        icons_.empty())
        return;
    // What the draws showed: how the kinds come out in the game (the network takes only the ones that are on).
    std::string seen;
    for (int k = 0; k < ai::kKinds; ++k)
        seen += std::string(k ? ", " : "") + ai::kindName(ai::Kind(k)) + " " + std::to_string(seen_[k]) +
                (!kindOn_[k] ? " (off)" : "");
    LOG_INFO("AI textures: %u from the pack, %u made by the network, %u from upscaled.grf, %u waiting, %u ahead, %u "
             "written; seen %s, lightmaps %u; %u of soft light left as the game's",
             donePack_, doneNetwork_, doneLoaded_, static_cast<unsigned>(queue_.size() + (job_ ? 1 : 0)),
             static_cast<unsigned>(later_.size()), written_, seen.c_str(), seen_[int(ai::Kind::Lightmap)], soft_);
    if (cellsWritten_)
        LOG_INFO("AI textures: sprite frames - %u written by the game into %u caches, %u from the pack, %u made by the "
                 "network, %u from upscaled.grf, %u waiting",
                 cellsWritten_, static_cast<unsigned>(atlases_.size()), cellsPack_, cellsNetwork_, cellsLoaded_,
                 static_cast<unsigned>(cells_.size()));
    if (!icons_.empty())
        LOG_INFO("AI textures: icons - %u asked, %u from the pack, %u made by the network, %u from upscaled.grf",
                 static_cast<unsigned>(icons_.size()), iconsPack_, iconsNetwork_, iconsLoaded_);
    uint64_t used = 0;
    const uint64_t system = systemMemory(&used);
    size_t copies = 0;
    for (const auto& e : items_) copies += e.second.bytes ? 1 : 0;
    LOG_INFO("AI textures: video memory - the copies %llu MB of their %llu MB (%u textures), %u put away, %u brought "
             "back; the game %llu MB of the %llu MB the system gives it",
             static_cast<unsigned long long>(aiBytes_ >> 20), static_cast<unsigned long long>(vramBudget_ >> 20),
             static_cast<unsigned>(copies), putAway_, broughtBack_, static_cast<unsigned long long>(used >> 20),
             static_cast<unsigned long long>(system >> 20));
}

void AiTextures::request(IoRequest r) {
    EnterCriticalSection(&ioLock_);
    ioIn_.push_back(std::move(r));
    LeaveCriticalSection(&ioLock_);
    SetEvent(ioWake_);
}

DWORD WINAPI AiTextures::ioMain(void* self) {
    static_cast<AiTextures*>(self)->ioLoop();
    return 0;
}

void AiTextures::ioLoop() {
    // Pictures and tags (empty entries) go into upscaled.grf every 30 seconds or 32 MB, and when the game closes:
    // every write puts a new file table at the end and leaves the old one unused in the file - with thousands of
    // sprite frames and their tags that table is hundreds of kilobytes.
    std::vector<std::pair<std::string, std::vector<uint8_t>>> batch;
    std::unordered_map<std::string, size_t> pending;  // a picture's place in the batch
    size_t bytes = 0;
    uint32_t pictures = 0;
    ULONGLONG since = 0, tagsSince = 0;
    bool logged = false;
    for (;;) {
        WaitForSingleObject(ioWake_, batch.empty() ? INFINITE : 1000);
        std::deque<IoRequest> work;
        EnterCriticalSection(&ioLock_);
        work.swap(ioIn_);
        const bool stop = ioStop_;
        LeaveCriticalSection(&ioLock_);
        for (IoRequest& r : work) {
            if (r.write && r.tag) {
                batch.emplace_back(r.name, std::vector<uint8_t>());
                if (!tagsSince) tagsSince = GetTickCount64();
                continue;
            }
            if (r.write) {
                batch.emplace_back(r.name, ai::encodeTexture(r.argb.data(), r.width, r.height));
                pending[r.name] = batch.size() - 1;
                bytes += batch.back().second.size();
                ++pictures;
                if (!since) since = GetTickCount64();
                continue;
            }
            IoResult res;
            res.id = r.id;
            res.name = r.name;
            res.pack = r.pack;
            res.cell = std::move(r.cell);
            std::vector<uint8_t> blob;
            std::string error;
            const auto waiting = pending.find(r.name);  // made a moment ago, not in the file yet
            if (r.pack) {  // 0.42: 4 times the game's texture - twice at x2
                res.ok = pack_.read(r.name, blob) && webp::decode(blob.data(), blob.size(), res.argb, res.width,
                                                                  res.height);
                // 0.45: and a whole texture's picture for one at a lower texture detail, 8 or 16 times its size.
                while (res.ok && r.width > 0 && r.height > 0 && res.width > r.width * scale_ &&
                       res.height > r.height * scale_ && res.width % 2 == 0 && res.height % 2 == 0)
                    ai::halve(res.argb, res.width, res.height);
            } else if (waiting != pending.end())
                res.ok = ai::decodeTexture(batch[waiting->second].second, res.argb, res.width, res.height);
            else
                res.ok = archive_.read(r.name, blob, error) &&
                         ai::decodeTexture(blob, res.argb, res.width, res.height);
            EnterCriticalSection(&ioLock_);
            ioOut_.push_back(std::move(res));
            LeaveCriticalSection(&ioLock_);
        }
        const ULONGLONG now = GetTickCount64();
        const bool due = stop || bytes >= kBatchBytes || (since && now - since >= 30000) ||
                         (tagsSince && now - tagsSince >= 30000);
        if (!batch.empty() && due) {
            std::string error;
            if (writable_ && archive_.append(batch, error)) {
                written_ += pictures;
            } else if (!logged) {
                logged = true;
                LOG_ERROR("AI textures: upscaled.grf not written (%s)", error.empty() ? "read only" : error.c_str());
            }
            batch.clear();
            pending.clear();
            bytes = 0;
            pictures = 0;
            since = tagsSince = 0;
        }
        if (stop) break;
    }
}

bool AiTextures::selfTest(int net, std::string& error) {
    // A 16x16 texture with a transparent band through the network on the GPU, and the same on the CPU (core/ai): the
    // colour with each way past the edge (a world texture's, a sprite cell's, an interface picture's) and the alpha's
    // own pass - every channel within 2 of 255 and nearly the same transparency, or the feature stays off.
    const ai::Model& model = nets_[size_t(net)]->model;
    const int w = 16, h = 16, ow = w * kNetScale, oh = h * kNetScale;
    std::vector<uint32_t> argb(size_t(w * h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uint32_t r = uint32_t(x * 15), g = uint32_t(y * 15), b = uint32_t((x * 7 + y * 11) % 256);
            const uint32_t a = (x + y > 12 && x + y < 17) ? 0u : 255u;
            argb[size_t(y * w + x)] = a << 24 | r << 16 | g << 8 | b;
        }
    std::vector<float> planes;
    ai::prepareInput(argb.data(), w, h, planes);
    std::vector<Level> levels;
    UINT64 bytes = 0;
    layout(ow, oh, levels, bytes);
    const ai::Border borders[3] = {ai::Border::Wrap, ai::Border::Zero, ai::Border::Clamp};  // as Job::border
    for (uint32_t mode = 0; mode < 3; ++mode) {
        std::vector<float> out(size_t(3 * ow * oh));
        ai::upscale(model, planes.data(), w, h, out.data(), borders[mode]);
        std::vector<uint8_t> alpha;
        if (mode == 2) ai::upscaleAlpha(model, planes.data() + 3 * w * h, w, h, alpha, ai::Border::Clamp);
        Job j;
        j.net = net;
        j.width = w;
        j.height = h;
        j.pixels = argb;
        j.border = mode;
        if (!ensureBuffers(w, h) || !m_.begin() || !uploadInput(j)) {
            error = "no GPU memory for the self-test";
            return false;
        }
        computeBound_ = false;
        for (j.conv = 0; size_t(j.conv) < model.convs.size();) runBand(j, 1e18);
        ID3D12Resource* image =
            buffer(m_.device, D3D12_HEAP_TYPE_DEFAULT, bytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
        ID3D12Resource* back = buffer(m_.device, D3D12_HEAP_TYPE_READBACK, UINT64(levels[0].pitch) * UINT64(oh),
                                      D3D12_RESOURCE_STATE_COPY_DEST, false);
        if (image && back) dispatchShuffle(j, image, levels[0]);
        if (!image || !back || (mode == 2 && !uploadAlphaInput(j))) {
            releaseCom(image);
            releaseCom(back);
            m_.submit();
            error = "no GPU memory for the self-test";
            return false;
        }
        if (mode == 2) {  // the alpha's own pass, as an interface picture's
            for (j.conv = 0; size_t(j.conv) < model.convs.size();) runBand(j, 1e18);
            dispatchAlpha(j, image, levels[0]);
        }
        D3D12_RESOURCE_STATES imageState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        m_.transition(image, imageState, D3D12_RESOURCE_STATE_COPY_SOURCE);
        m_.list->CopyBufferRegion(back, 0, image, 0, UINT64(levels[0].pitch) * UINT64(oh));
        m_.waitIdle();
        computeBound_ = false;
        int worst = 0, alphaDiffers = 0;
        void* mapped = nullptr;
        const bool ok = !m_.lost && SUCCEEDED(back->Map(0, nullptr, &mapped));
        if (ok) {
            const size_t plane = size_t(ow) * size_t(oh);
            for (int y = 0; y < oh; ++y)
                for (int x = 0; x < ow; ++x) {
                    const uint32_t g =
                        static_cast<const uint32_t*>(mapped)[size_t(y) * (levels[0].pitch / 4) + size_t(x)];
                    const size_t i = size_t(y) * size_t(ow) + size_t(x);
                    for (int c = 0; c < 3; ++c) {
                        const int want = int(out[size_t(c) * plane + i] * 255.0f + 0.5f);
                        const int got = int((g >> (16 - 8 * c)) & 255);
                        worst = std::max(worst, std::abs(want - got));
                    }
                    if (mode == 2 && ((g >> 24) != 0) != (alpha[i] != 0)) ++alphaDiffers;
                }
            back->Unmap(0, nullptr);
        }
        releaseCom(image);
        releaseCom(back);
        if (!ok || worst > 2 || alphaDiffers > ow * oh / 50) {
            char text[220];
            std::snprintf(text, sizeof text, "%s: the GPU's network does not match the CPU's (border %u: largest "
                          "difference %d of 255, %d transparency differences)", nets_[size_t(net)]->name.c_str(),
                          mode, worst, alphaDiffers);
            error = ok ? text : "the self-test could not be read";
            return false;
        }
    }
    return true;
}

}  // namespace uagfx::gpu
