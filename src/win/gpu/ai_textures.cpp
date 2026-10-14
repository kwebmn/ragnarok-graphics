#include "win/gpu/ai_textures.hpp"

#include <d3dcompiler.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

#include "core/ai/texture_cache.hpp"
#include "win/gpu/impl.hpp"
#include "win/log.hpp"

namespace uagfx::gpu {
namespace {

constexpr int kNetScale = 4;  // every network enlarges 4 times; a result of twice the size is its first mip level
constexpr size_t kKeptLimit = size_t(512) << 20;
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
    targetMs_ = std::clamp(double(s.budget), 0.5, 20.0);
    scale_ = s.scale == 2 ? 2 : 4;
    minSide_ = std::min(s.minSize, s.maxSize);
    maxSide_ = std::max(s.minSize, s.maxSize);
    ahead_ = s.ahead;
    const std::string dir = gameDir + "\\uaro-graphics\\ai\\";
    for (int k = 0; k < ai::kKinds; ++k) {
        if (!aiKind(s, k)) continue;
        const std::string& token = k < 2 ? s.model : s.spriteModel;  // the ground and objects, sprites and effects
        for (size_t i = 0; i < nets_.size() && netOf_[k] < 0; ++i)
            if (nets_[i]->token == token) netOf_[k] = int(i);
        if (netOf_[k] >= 0) continue;
        auto net = std::make_unique<Net>();
        if (!loadNet(dir, token, s.denoise, *net, error)) return false;
        features_ = std::max(features_, net->model.features);
        nets_.push_back(std::move(net));
        netOf_[k] = int(nets_.size()) - 1;
    }
    if (nets_.empty()) {
        error = "no kind of texture is chosen (the AI textures page)";
        return false;
    }
    if (!createGpu(error)) return false;
    const ULONGLONG t0 = GetTickCount64();
    for (size_t i = 0; i < nets_.size(); ++i)
        if (!selfTest(int(i), error)) return false;
    const ULONGLONG tested = GetTickCount64() - t0;

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
            for (const auto& n : nets_) ours = ours || e.folder == n->folder;
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
        kinds += std::string(k ? ", " : "") + ai::kindName(ai::Kind(k)) + " " + (n ? n->name : std::string("off"));
        if (n && n->token == "general") kinds += " (denoise " + std::to_string(n->denoise) + "%)";
    }
    char note[112] = "";
    if (other)
        std::snprintf(note, sizeof note, " (and %u of other settings or older versions, not used: the Ready textures "
                      "page deletes them)", other);
    LOG_INFO("AI textures: on - %s; x%d, textures %d-%d, ahead %s; on the GPU, self-test passed in %llu ms; "
             "upscaled.grf has %u textures of these settings%s%s",
             kinds.c_str(), scale_, minSide_, maxSide_, ahead_ ? "on" : "off",
             static_cast<unsigned long long>(tested), static_cast<unsigned>(known_.size()), note,
             writable_ ? "" : " (read only: another game writes it)");
    return true;
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
    if (!pso("ai_conv", conv_) || !pso("ai_shuffle", shuffle_) || !pso("ai_mip", mip_)) return false;
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

bool AiTextures::finish(Job& job) {
    // A network's picture is 4 times the game's texture; for a result of twice the size its first mip level is the
    // texture's top level. A picture from upscaled.grf is the result's size already.
    const int skip = !job.loaded && scale_ == 2 ? 1 : 0;
    const int full = job.loaded ? scale_ : kNetScale;
    const int w = job.width * full, h = job.height * full;
    std::vector<Level> levels;
    UINT64 bytes = 0;
    layout(w, h, levels, bytes);
    ID3D12Resource* image =
        buffer(m_.device, D3D12_HEAP_TYPE_DEFAULT, bytes, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
    if (!image) return false;
    D3D12_RESOURCE_STATES imageState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    m_.slots[m_.slot].garbage.push_back(image);  // freed once this list has run
    if (job.loaded) {  // level 0 from upscaled.grf
        const Level& l0 = levels[0];
        const Upload u = m_.allocUpload(UINT64(l0.pitch) * UINT64(h), D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        if (!u.cpu) return false;
        for (int y = 0; y < h; ++y)
            std::memcpy(u.cpu + UINT64(y) * l0.pitch, job.pixels.data() + size_t(y) * size_t(w), size_t(w) * 4);
        m_.transition(image, imageState, D3D12_RESOURCE_STATE_COPY_DEST);
        m_.list->CopyBufferRegion(image, 0, m_.slots[m_.slot].upload, u.offset, UINT64(l0.pitch) * UINT64(h));
        m_.transition(image, imageState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    } else {
        dispatchShuffle(job, image, levels[0]);
    }
    dispatchMips(image, levels);

    const Level& top = levels[size_t(skip)];
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC td{};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    td.Width = UINT64(top.width);
    td.Height = UINT(top.height);
    td.DepthOrArraySize = 1;
    td.MipLevels = UINT16(levels.size() - size_t(skip));
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    ID3D12Resource* tex = nullptr;
    if (FAILED(m_.device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST,
                                                  nullptr, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&tex))))
        return false;
    m_.transition(image, imageState, D3D12_RESOURCE_STATE_COPY_SOURCE);
    for (size_t i = size_t(skip); i < levels.size(); ++i) {
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
        dst.SubresourceIndex = UINT(i - size_t(skip));
        m_.list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    if (!job.loaded && writable_) {  // the result for upscaled.grf, read once this list has run
        Readback r;
        const UINT64 size = UINT64(top.pitch) * UINT64(top.height);
        r.buffer = buffer(m_.device, D3D12_HEAP_TYPE_READBACK, size, D3D12_RESOURCE_STATE_COPY_DEST, false);
        if (r.buffer) {
            m_.list->CopyBufferRegion(r.buffer, 0, image, top.at, size);
            r.fence = m_.fenceValue + 1;  // the value the open list's submission signals
            r.name = job.name;
            r.width = top.width;
            r.height = top.height;
            r.pitch = top.pitch;
            readbacks_.push_back(r);
            known_.insert(job.name);
        }
    }
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
    m_.device->CreateShaderResourceView(tex, nullptr, m_.cpuSrvHandle(srv));
    item->second.state = Item::Done;
    keptBytes_ -= item->second.argb.size() * 4;
    item->second.argb.clear();
    item->second.argb.shrink_to_fit();
    if (job.loaded)
        ++doneLoaded_;
    else
        ++doneNetwork_;
    return true;
}

void AiTextures::dropAi(uint32_t id) {
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

void AiTextures::noteUpload(uint32_t id, Texture& t, const UploadCmd& u, const uint32_t* argb) {
    if (!ready_ || u.level != 0) return;
    if (t.ai) dropAi(id);  // the game changed a texture the network had done: its own pixels again
    const bool whole = u.rect.left == 0 && u.rect.top == 0 && u.rect.width() == t.width && u.rect.height() == t.height;
    if (!whole || t.width < minSide_ || t.height < minSide_ || t.width > maxSide_ || t.height > maxSide_) {
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
    item.later = item.loading = false;
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
    if (k >= ai::kKinds || netOf_[k] < 0) {  // a lightmap, no world texture, or a kind that is off
        drop(item);
        return;
    }
    item.net = netOf_[k];
    item.name = ai::entryName(nets_[size_t(item.net)]->folder, item.key);
    item.state = Item::Waiting;
    if (kinds_.emplace(item.key, kind).second) tag(ai::kindTag(kind, item.key));
    if (known_.count(item.name)) {  // done before: from upscaled.grf
        item.loading = true;
        IoRequest r;
        r.id = id;
        r.name = item.name;
        request(std::move(r));
    } else if (later) {
        item.later = true;
        later_.push_back(id);
    } else {
        queue_.push_back(id);
    }
}

void AiTextures::tag(const std::string& name) {
    if (!writable_) return;
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

void AiTextures::setMap(const std::string& map) {
    if (map == map_) return;
    map_ = map;
    ++mapSerial_;
}

void AiTextures::forget(uint32_t id) {
    const auto it = items_.find(id);
    if (it != items_.end()) {
        keptBytes_ -= it->second.argb.size() * 4;
        items_.erase(it);
    }
    if (job_ && job_->id == id) job_.reset();  // its buffers serve the next one
}

bool AiTextures::nextJob() {
    if (!loaded_.empty()) {
        job_ = std::make_unique<Job>(std::move(loaded_.front()));
        loaded_.pop_front();
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
            return true;
        }
    }
    return false;
}

void AiTextures::takeLoaded() {
    std::deque<IoResult> results;
    EnterCriticalSection(&ioLock_);
    results.swap(ioOut_);
    LeaveCriticalSection(&ioLock_);
    for (IoResult& r : results) {
        const auto it = items_.find(r.id);
        if (it == items_.end()) continue;
        Item& item = it->second;
        if (item.state != Item::Waiting || !item.loading || item.name != r.name) continue;
        item.loading = false;
        if (!r.ok || r.width != item.width * scale_ || r.height != item.height * scale_) {
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
            request(std::move(w));
        }
        releaseCom(r.buffer);
        readbacks_.erase(readbacks_.begin() + std::ptrdiff_t(i));
    }
}

void AiTextures::work() {
    if (!ready_ || m_.lost || !m_.open) return;
    computeBound_ = false;
    takeLoaded();
    finishReadbacks();
    double budget = budget_;
    double spent = 0;
    bool finished = false;  // one finished picture a frame: its mips and copies are not free either
    while (budget > 0) {
        if (!job_ && !nextJob()) break;
        Job& j = *job_;
        if (j.loaded) {
            if (finished) break;
            if (!finish(j)) {
                const auto it = items_.find(j.id);
                if (it != items_.end() && it->second.state == Item::Waiting) queue_.push_back(j.id);
            }
            finished = true;
            job_.reset();
            continue;
        }
        if (j.conv < 0) {
            if (!ensureBuffers(j.width, j.height) || !uploadInput(j)) {
                LOG_ERROR("AI textures: no GPU memory for a %dx%d texture; it stays as it is", j.width, j.height);
                const auto it = items_.find(j.id);
                if (it != items_.end()) drop(it->second);
                job_.reset();
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
        if (finished) break;
        if (!finish(j)) {
            const auto it = items_.find(j.id);
            if (it != items_.end()) drop(it->second);
        }
        finished = true;
        job_.reset();
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
    if (!doneNetwork_ && !doneLoaded_ && queue_.empty() && later_.empty()) return;
    // What the draws showed: how the kinds come out in the game (the network takes only the ones that are on).
    std::string seen;
    for (int k = 0; k < ai::kKinds; ++k)
        seen += std::string(k ? ", " : "") + ai::kindName(ai::Kind(k)) + " " + std::to_string(seen_[k]) +
                (netOf_[k] < 0 ? " (off)" : "");
    LOG_INFO("AI textures: %u made by the network, %u from upscaled.grf, %u waiting, %u ahead, %u written; seen %s, "
             "lightmaps %u",
             doneNetwork_, doneLoaded_, static_cast<unsigned>(queue_.size() + (job_ ? 1 : 0)),
             static_cast<unsigned>(later_.size()), written_, seen.c_str(), seen_[int(ai::Kind::Lightmap)]);
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
    // Pictures go into upscaled.grf every 3 seconds or 32 MB, tags (empty entries) with them or every 15 seconds:
    // every write puts a new file table at the end.
    std::vector<std::pair<std::string, std::vector<uint8_t>>> batch;
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
                bytes += batch.back().second.size();
                ++pictures;
                if (!since) since = GetTickCount64();
                continue;
            }
            IoResult res;
            res.id = r.id;
            res.name = r.name;
            std::vector<uint8_t> blob;
            std::string error;
            res.ok = archive_.read(r.name, blob, error) && ai::decodeTexture(blob, res.argb, res.width, res.height);
            EnterCriticalSection(&ioLock_);
            ioOut_.push_back(std::move(res));
            LeaveCriticalSection(&ioLock_);
        }
        const ULONGLONG now = GetTickCount64();
        const bool due = stop || bytes >= kBatchBytes || (since && now - since >= 3000) ||
                         (tagsSince && now - tagsSince >= 15000);
        if (!batch.empty() && due) {
            std::string error;
            if (writable_ && archive_.append(batch, error)) {
                written_ += pictures;
            } else if (!logged) {
                logged = true;
                LOG_ERROR("AI textures: upscaled.grf not written (%s)", error.empty() ? "read only" : error.c_str());
            }
            batch.clear();
            bytes = 0;
            pictures = 0;
            since = tagsSince = 0;
        }
        if (stop) break;
    }
}

bool AiTextures::selfTest(int net, std::string& error) {
    // A 16x16 texture with a transparent band through the network on the GPU, and the same on the CPU (core/ai):
    // every channel within 2 of 255 and the same transparency, or the feature stays off.
    const ai::Model& model = nets_[size_t(net)]->model;
    const int w = 16, h = 16;
    std::vector<uint32_t> argb(size_t(w * h));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uint32_t r = uint32_t(x * 15), g = uint32_t(y * 15), b = uint32_t((x * 7 + y * 11) % 256);
            const uint32_t a = (x + y > 12 && x + y < 17) ? 0u : 255u;
            argb[size_t(y * w + x)] = a << 24 | r << 16 | g << 8 | b;
        }
    std::vector<float> planes;
    ai::prepareInput(argb.data(), w, h, planes);
    std::vector<float> out(size_t(3 * w * h * kNetScale * kNetScale));
    ai::upscale(model, planes.data(), w, h, out.data(), ai::Border::Wrap);  // as the GPU's ai_conv
    std::vector<uint8_t> alpha;
    ai::enlargeAlpha(planes.data() + 3 * w * h, w, h, kNetScale, alpha);

    Job j;
    j.net = net;
    j.width = w;
    j.height = h;
    j.pixels = argb;
    const int ow = w * kNetScale, oh = h * kNetScale;
    std::vector<Level> levels;
    UINT64 bytes = 0;
    layout(ow, oh, levels, bytes);
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
    if (!image || !back) {
        releaseCom(image);
        releaseCom(back);
        m_.submit();
        error = "no GPU memory for the self-test";
        return false;
    }
    dispatchShuffle(j, image, levels[0]);
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
                const uint32_t g = static_cast<const uint32_t*>(mapped)[size_t(y) * (levels[0].pitch / 4) + size_t(x)];
                const size_t i = size_t(y) * size_t(ow) + size_t(x);
                for (int c = 0; c < 3; ++c) {
                    const int want = int(out[size_t(c) * plane + i] * 255.0f + 0.5f);
                    const int got = int((g >> (16 - 8 * c)) & 255);
                    worst = std::max(worst, std::abs(want - got));
                }
                if (((g >> 24) != 0) != (alpha[i] != 0)) ++alphaDiffers;
            }
        back->Unmap(0, nullptr);
    }
    releaseCom(image);
    releaseCom(back);
    if (!ok || worst > 2 || alphaDiffers > ow * oh / 50) {
        char text[200];
        std::snprintf(text, sizeof text, "%s: the GPU's network does not match the CPU's (largest difference %d of "
                      "255, %d transparency differences)", nets_[size_t(net)]->name.c_str(), worst, alphaDiffers);
        error = ok ? text : "the self-test could not be read";
        return false;
    }
    return true;
}

}  // namespace uagfx::gpu
