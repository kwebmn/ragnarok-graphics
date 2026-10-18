#include "studio/making.hpp"

#include <windows.h>

#include <cstdio>
#include <ctime>
#include <vector>

#include "app/text.hpp"
#include "core/ai/chain.hpp"
#include "core/ai/groups.hpp"
#include "core/studio_link.hpp"
#include "core/webp_image.hpp"
#include "studio/catalog_work.hpp"
#include "studio/jobs.hpp"
#include "studio/pack_edit.hpp"
#include "studio/pictures.hpp"
#include "studio/places.hpp"

namespace uagfx::studio {
namespace {

bool readWhole(const std::wstring& path, std::string& out) {
    std::FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return false;
    out.clear();
    char buf[1 << 16];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

bool writeWhole(const std::wstring& path, const std::string& bytes) {
    std::FILE* f = _wfopen((path + L".part").c_str(), L"wb");
    if (!f) return false;
    const bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (std::fclose(f) != 0 || !ok) return false;
    return MoveFileExW((path + L".part").c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

// The catalog's picture of a key in a file (its indices change with the catalog; the key and the name do not).
bool find(const catalog::Catalog& c, const std::string& file, const std::string& key, size_t& fi, size_t& pi) {
    for (fi = 0; fi < c.files.size(); ++fi) {
        if (c.files[fi].name != file) continue;
        for (pi = 0; pi < c.files[fi].pictures.size(); ++pi)
            if (c.files[fi].pictures[pi].key == key) return true;
    }
    return false;
}

uint32_t borderOf(uint8_t group) {  // past a picture's edge: the world's wrap, a sprite's nothing, the interface's edge
    return group == uint8_t(pack::Group::World) ? 0u : group == uint8_t(pack::Group::Sprites) ? 1u : 2u;
}

}  // namespace

std::wstring madeFile(const std::string& key) {
    std::string name = key;
    for (char& c : name)
        if (c == '\\' || c == '/') c = '-';
    return studioRoot() + L"\\made\\" + app::wide(name) + L".raw";
}

void addPictureWork(Jobs& jobs, CatalogWork& catalog, settings::Lang lang) {
    jobs.on("compact", [&catalog, lang](JobContext&, json::Value& result, std::string& why) {
        const std::string gameOpen = app::utf8(app::wtr(lang, settings::Text::StudioPackGameOpen));
        if (!compactPack(app::ansi(catalog.packPath()), why, gameOpen)) return false;
        catalog.readPack();
        result.set("op", "compact");
        return true;
    });
    jobs.on("make", [&catalog](JobContext& ctx, json::Value& result, std::string& why) {
        const json::Value& a = ctx.job.args;
        const std::string file = a["file"].asString(), key = a["key"].asString();
        const auto cat = catalog.catalog();
        size_t fi = 0, pi = 0;
        if (!cat || !find(*cat, file, key, fi, pi)) return why = "the texture is not in the catalog any more", false;
        const catalog::FileEntry& f = cat->files[fi];
        const catalog::PictureOf& p = f.pictures[pi];
        ai::Chain chain;
        if (!ai::parseChain(a["chain"].asString(), chain)) return why = "a chain it cannot read", false;
        Image original;
        if (!originalOf(catalog, *cat, f, p, original)) return why = "the game's file of it cannot be read", false;
        if (!ctx.engine(why)) return false;
        const int calls = (chain.tiles ? f.grid.cols * f.grid.rows : 1) * (chain.blendModel.empty() ? 1 : 2);
        int call = 0;
        makeFolder(studioRoot() + L"\\work");
        const ai::Enlarge enlarge = [&](const std::string& model, int denoise, const std::vector<uint32_t>& argb,
                                        int w, int h, std::vector<uint32_t>& out, std::string& error) {
            JobProgress progress;
            progress.stage = model;
            progress.current = utf8OfGame(file);
            progress.done = uint64_t(call);
            progress.total = uint64_t(calls);
            ctx.progress(progress);
            const std::wstring base = studioRoot() + L"\\work\\" + std::to_wstring(ctx.job.id) + L"-" +
                                      std::to_wstring(call++);
            if (!writeWhole(base + L".in", studio::rawOf(w, h, argb))) return error = "the disk is full?", false;
            json::Value body = json::Value::object();
            body.set("in", app::utf8(base + L".in"));
            body.set("out", app::utf8(base + L".out"));
            body.set("model", model);
            body.set("denoise", denoise);
            body.set("border", int(borderOf(p.group)));
            json::Value reply;
            const bool ok = !ctx.stopAsked() && ctx.engineRef().call("enlarge", body, reply, error, 600000);
            std::string bytes;
            int ow = 0, oh = 0;
            const bool read = ok && readWhole(base + L".out", bytes) && parseRaw(bytes, ow, oh, out);
            DeleteFileW((base + L".in").c_str());
            DeleteFileW((base + L".out").c_str());
            if (ok && !read) error = "the engine's picture cannot be read";
            if (ctx.stopAsked()) error = "stopped";
            return read;
        };
        std::vector<uint32_t> out;
        int W = 0, H = 0;
        if (!ai::runChain(chain, original.argb.data(), original.width, original.height, f.grid, enlarge, out, W, H,
                          why))
            return false;
        makeFolder(studioRoot() + L"\\made");
        const std::wstring made = madeFile(key);
        if (!writeWhole(made, rawOf(W, H, out)) || !writeWhole(made.substr(0, made.size() - 4) + L".txt",
                                                                ai::chainText(chain)))
            return why = "the picture cannot be written into the studio's folder", false;
        result.set("op", "make");
        result.set("key", key);
        result.set("file", file);
        return true;
    });

    jobs.on("keep", [&catalog](JobContext& ctx, json::Value& result, std::string& why) {
        const json::Value& a = ctx.job.args;
        const std::string file = a["file"].asString(), key = a["key"].asString();
        const auto cat = catalog.catalog();
        size_t fi = 0, pi = 0;
        if (!cat || !find(*cat, file, key, fi, pi)) return why = "the texture is not in the catalog any more", false;
        const catalog::FileEntry& f = cat->files[fi];
        const catalog::PictureOf& p = f.pictures[pi];
        const std::wstring made = madeFile(key);
        std::string bytes, chainText;
        int w = 0, h = 0;
        std::vector<uint32_t> argb;
        if (!readWhole(made, bytes) || !parseRaw(bytes, w, h, argb)) return why = "no picture made of it", false;
        readWhole(made.substr(0, made.size() - 4) + L".txt", chainText);
        ai::Chain chain;
        ai::parseChain(chainText, chain);
        JobProgress progress;
        progress.stage = "WebP";
        progress.current = utf8OfGame(file);
        ctx.progress(progress);
        const std::vector<uint8_t> webp = webp::encode(argb.data(), w, h, 90);
        if (webp.empty()) return why = "WebP did not take it", false;
        const std::string name = pack::pictureName(pack::Group(p.group), key);
        const std::string archive = f.archives.empty() ? "" : cat->archives[f.archives[0]].path;
        progress.stage = "pack";
        ctx.progress(progress);
        if (!packEdit(app::ansi(catalog.packPath()),
                      [&](pack::Writer& wr, pack::Index& index, std::string& error) {
                          if (!wr.add(name, webp.data(), webp.size(), error, true)) return false;
                          pack::IndexEntry& e = index.pictures[name];
                          if (e.kind == ai::Kind::None) e.kind = ai::Kind(f.kind);
                          e.model = chain.model;
                          e.group = ai::groupName(ai::Group(p.aiGroup));
                          index.made[name] =
                              pack::Provenance{file, archive, int64_t(std::time(nullptr)), chain.model, chainText};
                          for (const std::string& low : p.lowKeys) index.aliases[low] = name;
                          return true;
                      },
                      why))
            return false;
        DeleteFileW(made.c_str());
        DeleteFileW((made.substr(0, made.size() - 4) + L".txt").c_str());
        catalog.readPack();
        tellGame({key});
        result.set("op", "keep");
        result.set("key", key);
        return true;
    });

    jobs.on("takeout", [&catalog](JobContext& ctx, json::Value& result, std::string& why) {
        std::vector<std::string> names, keys;
        for (const json::Value& n : ctx.job.args["names"].items()) names.push_back(n.asString());
        for (const json::Value& k : ctx.job.args["keys"].items()) keys.push_back(k.asString());
        if (!packEdit(app::ansi(catalog.packPath()),
                      [&](pack::Writer& wr, pack::Index& index, std::string&) {
                          for (const std::string& n : names) {
                              wr.remove(n);
                              index.pictures.erase(n);
                              index.made.erase(n);
                          }
                          return true;
                      },
                      why))
            return false;
        catalog.readPack();
        tellGame(keys);
        result.set("op", "takeout");
        return true;
    });

    jobs.on("own", [&catalog](JobContext& ctx, json::Value& result, std::string& why) {
        const std::string key = ctx.job.args["key"].asString();
        const bool on = ctx.job.args["on"].asBool();
        if (!packEdit(app::ansi(catalog.packPath()),
                      [&](pack::Writer&, pack::Index& index, std::string&) {
                          if (on) index.ownAlways.insert(key);
                          else index.ownAlways.erase(key);
                          return true;
                      },
                      why))
            return false;
        catalog.readPack();
        tellGame({key});
        result.set("op", "own");
        result.set("key", key);
        return true;
    });
}

}  // namespace uagfx::studio
