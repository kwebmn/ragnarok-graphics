#include <cstdio>
#include <functional>

#include "app/files.hpp"
#include "app/page.hpp"
#include "core/live_config.hpp"
#include "core/settings/compat_flags.hpp"
#include "core/settings/report.hpp"
#include "core/version.hpp"

namespace uagfx::app {
namespace {

using namespace settings;

constexpr int kList = 1301;
constexpr int kFix = 1302;
constexpr int kOpenLog = 1303;
constexpr int kOpenFolder = 1304;
constexpr int kReport = 1305;
constexpr int kRecheck = 1306;
constexpr int kVerbose = 1307;

struct Row {
    std::wstring check;
    std::wstring result;
    Tone tone = Tone::Normal;
    std::function<std::wstring(Lang)> fix;  // empty: nothing to fix here
};

std::string hresult(long hr) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08lX", static_cast<unsigned long>(hr));
    return buf;
}

Text effectText(LayerEffect e) {
    switch (e) {
        case LayerEffect::RunAsAdmin: return Text::CompatRunAsAdmin;
        case LayerEffect::OldWindows: return Text::CompatOldWindows;
        case LayerEffect::Dpi: return Text::CompatDpi;
        case LayerEffect::Colors: return Text::CompatColors;
        case LayerEffect::Other: break;
    }
    return Text::CompatOther;
}

// The Diagnostics page (stage 2 spec §7).
class DiagnosticsPage : public Page {
public:
    explicit DiagnosticsPage(PageHost& host) : Page(host) {}

    void build(Panel& p) override {
        Model& m = host_.model();
        rows_ = rows();
        p.heading(t(Text::PageDiagnostics));
        list_ = p.listView(kList, {{t(Text::DiagCheck), 200}, {t(Text::DiagResult), 440}}, 11);
        for (size_t i = 0; i < rows_.size(); ++i) {
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = int(i);
            item.pszText = const_cast<wchar_t*>(rows_[i].check.c_str());
            SendMessageW(list_, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item));
            item.iSubItem = 1;
            item.pszText = const_cast<wchar_t*>(rows_[i].result.c_str());
            SendMessageW(list_, LVM_SETITEMTEXTW, i, reinterpret_cast<LPARAM>(&item));
        }
        p.buttons({{kFix, t(Text::DiagFix)}, {kRecheck, t(Text::DiagRecheck)}});
        EnableWindow(GetDlgItem(host_.window(), kFix), FALSE);
        p.buttons({{kOpenLog, t(Text::DiagOpenLog)}, {kOpenFolder, t(Text::DiagOpenFolder)}, {kReport, t(Text::DiagReport)}});
        EnableWindow(GetDlgItem(host_.window(), kOpenLog), m.now().log.found);
        p.check(kVerbose, t(Text::DiagVerboseLog), m.token(*findSetting("debug.log")) == "verbose",
                t(Text::DiagVerboseLogHint));
    }

    void command(int id, int code) override {
        Model& m = host_.model();
        if (code != BN_CLICKED) return;
        if (id == kFix) {
            const LRESULT i = SendMessageW(list_, LVM_GETNEXTITEM, WPARAM(-1), LVNI_SELECTED);
            if (i < 0 || size_t(i) >= rows_.size() || !rows_[size_t(i)].fix) return;
            const std::wstring err = rows_[size_t(i)].fix(host_.lang());
            host_.message(err.empty() ? t(Text::DiagFixed) : err, !err.empty());
            host_.reload();
        } else if (id == kRecheck) {
            m.load();
            host_.reload();
        } else if (id == kOpenLog) {
            openInShell(m.paths().log);
        } else if (id == kOpenFolder) {
            openInShell(m.paths().dir);
        } else if (id == kReport) {
            const std::string text = makeReport(facts());
            const bool saved = writeFileAtomic(m.paths().report, text);
            copyToClipboard(host_.window(), wide(text));
            host_.message(saved ? t(Text::DiagReportDone) : tf(Text::ErrWriteFile, {"uaro-graphics-report.txt"}), !saved);
        } else if (id == kVerbose) {
            const IniSetting& s = *findSetting("debug.log");
            const std::string token =
                SendMessageW(GetDlgItem(host_.window(), id), BM_GETCHECK, 0, 0) == BST_CHECKED ? "verbose" : "normal";
            m.setToken(s, token);
            host_.changed();
        }
    }

    LRESULT notify(const NMHDR* n) override {
        if (n->idFrom != kList) return 0;
        if (n->code == NM_CUSTOMDRAW) {
            auto* cd = reinterpret_cast<NMLVCUSTOMDRAW*>(const_cast<NMHDR*>(n));
            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                const size_t i = size_t(cd->nmcd.dwItemSpec);
                if (i < rows_.size()) cd->clrText = toneColor(rows_[i].tone);
                return CDRF_NEWFONT;  // a colour set here is taken, as documented (0.6.0 review M15)
            }
        } else if (n->code == LVN_ITEMCHANGED) {
            const LRESULT i = SendMessageW(list_, LVM_GETNEXTITEM, WPARAM(-1), LVNI_SELECTED);
            EnableWindow(GetDlgItem(host_.window(), kFix), i >= 0 && size_t(i) < rows_.size() && rows_[size_t(i)].fix);
        }
        return 0;
    }

private:
    std::vector<Row> rows() {
        Model& m = host_.model();
        const SystemInfo& sys = m.system();
        const Snapshot& s = m.now();
        std::vector<Row> out;
        const bool win10 = sys.windows.major >= 10;
        out.push_back({t(Text::DiagWindows),
                       tf(Text::DiagWindowsValue, {num(long(sys.windows.major)), num(long(sys.windows.minor)),
                                                   num(long(sys.windows.build))}) +
                           (win10 ? L"" : t(Text::DiagWindowsOld)),
                       win10 ? Tone::Good : Tone::Warn, {}});
        if (sys.d3d12Probed) {
            const std::vector<std::string> gpu{sys.d3d12.adapter, num(static_cast<long long>(sys.d3d12.vramMb)),
                                               sys.gpuDriver.empty() ? utf8(t(Text::Unknown)) : sys.gpuDriver};
            out.push_back({t(Text::DiagGpu),
                           tf(sys.d3d12.vramMb >= kVramSeenAtMost ? Text::DiagGpuValueCapped : Text::DiagGpuValue, gpu),
                           Tone::Normal, {}});
            out.push_back({t(Text::DiagD3D12),
                           sys.d3d12.ok ? t(Text::DiagD3D12Ok) : tf(Text::DiagD3D12Failed, {hresult(sys.d3d12.hr)}),
                           sys.d3d12.ok ? Tone::Good : Tone::Bad, {}});
        }
        for (size_t i = 0; i < sys.monitors.size(); ++i) {
            const MonitorInfo& mon = sys.monitors[i];
            out.push_back({tf(Text::DiagMonitor, {num(long(i) + 1)}),
                           tf(Text::DiagMonitorValue, {num(mon.pixels.width), num(mon.pixels.height), num(mon.refresh),
                                                       num(mon.scale), utf8(t(mon.hdr ? Text::Yes : Text::No))}) +
                               (mon.primary ? t(Text::MonitorMain) : L""),
                           Tone::Normal, {}});
            out.push_back({tf(Text::DiagMonitorWork, {num(long(i) + 1)}),
                           tf(Text::DiagMonitorWorkValue, {num(mon.work.width), num(mon.work.height)}), Tone::Normal,
                           {}});
        }
        if (s.ddraw == Ddraw::Ours)
            out.push_back({t(Text::DiagDdraw), wide("uaRO Graphics " + s.installedVersion), Tone::Good, {}});
        else
            out.push_back({t(Text::DiagDdraw), t(s.ddraw == Ddraw::Other ? Text::DiagDdrawOther : Text::DiagDdrawNone),
                           s.ddraw == Ddraw::Other ? Tone::Warn : Tone::Bad, {}});
        out.push_back({L"dgVoodoo", t(s.dgvoodoo ? Text::DiagDgVoodooOk : Text::DiagDgVoodooMissing),
                       s.dgvoodoo ? Tone::Good : (s.config.backend == Backend::DgVoodoo ? Tone::Bad : Tone::Warn), {}});
        if (s.modules.empty()) out.push_back({t(Text::DiagInput), t(Text::DiagInputNone), Tone::Muted, {}});
        for (const ModuleFile& mod : s.modules) {
            std::wstring what = mod.info.kind == ModuleKind::Roext     ? tf(Text::DiagModuleRoext, {mod.info.ini})
                                : mod.info.kind == ModuleKind::UaroPad ? L"uaRO Pad"
                                                                       : t(Text::DiagModuleOther);
            if (!mod.loaded) what += t(Text::DiagModuleOff);
            out.push_back({mod.file, what, mod.loaded ? Tone::Normal : Tone::Muted, {}});
        }
        if (!s.folderWritable)
            out.push_back({t(Text::DiagFolder), t(Text::DiagFolderReadOnly), Tone::Bad, {}});
        else
            out.push_back({t(Text::DiagFolder), t(s.programFiles ? Text::DiagFolderProgramFiles : Text::DiagFolderOk),
                           s.programFiles ? Tone::Warn : Tone::Good, {}});
        if (sys.elevated) out.push_back({t(Text::DiagApp), t(Text::DiagElevated), Tone::Bad, {}});
        out.push_back({t(Text::DiagGame), t(s.gameRunning ? Text::DiagGameRunning : Text::DiagGameClosed),
                       Tone::Normal, {}});
        for (const CompatFlag& f : s.compat) {
            const LayerEffect e = layerEffect(f.flag);
            Row r{tf(Text::DiagCompat, {f.flag, utf8(t(f.allUsers ? Text::DiagCompatAll : Text::DiagCompatUser))}),
                  t(effectText(e)) + (f.allUsers ? t(Text::CompatAllUsersHint) : L""),
                  e == LayerEffect::RunAsAdmin ? Tone::Bad : e == LayerEffect::Other ? Tone::Muted : Tone::Warn, {}};
            if (!f.allUsers) {
                const CompatFlag flag = f;
                r.fix = [&m, flag](Lang l) { return m.removeCompatFlag(flag, l); };
            }
            out.push_back(r);
        }
        if (!gameKeyExists(s.registry)) {
            out.push_back({t(Text::DiagRegistry), t(Text::DiagRegistryNone), Tone::Warn, {}});
        } else {
            out.push_back({t(Text::DiagRegistry),
                           tf(Text::DiagRegistryValue, {utf8(t(s.registry.personal.exists ? Text::Yes : Text::No)),
                                                        utf8(t(s.registry.common.exists ? Text::Yes : Text::No))}),
                           Tone::Normal, {}});
            for (const CopyDifference& d : copyDifferences(s.registry, setupValues())) {  // screen and sound (§7)
                const std::string n = d.name;
                out.push_back({wide(n), tf(Text::DiagRegistryDiffers, {d.personal, d.common}), Tone::Warn,
                               [&m, n](Lang l) { return m.resetRegistryValue(n, l); }});
            }
        }
        if (!s.log.found) {
            out.push_back({t(Text::DiagLog), t(Text::DiagLogNone), Tone::Muted, {}});
        } else {
            if (!s.log.renderer.empty())
                out.push_back({t(Text::DiagLogRenderer), wide(s.log.renderer), Tone::Normal, {}});
            if (!s.log.crisp.empty()) {  // spec §7: the lines of crisp text
                const Tone tone = s.log.crisp == "on"                     ? Tone::Good
                                  : s.log.crisp == "off (crisp_text = 0)" ? Tone::Muted
                                                                          : Tone::Warn;
                out.push_back({t(Text::DiagLogCrisp),
                               wide(s.log.crisp + (s.log.crispScale.empty() ? "" : "; " + s.log.crispScale)), tone,
                               {}});
            }
            out.push_back({t(Text::DiagLogErrors),
                           s.log.errorCount == 0
                               ? t(Text::DiagLogErrorsNone)
                               : tf(Text::DiagLogErrorsSome, {num(static_cast<long long>(s.log.errorCount)),
                                                              s.log.errors.back()}),
                           s.log.errorCount == 0 ? Tone::Good : Tone::Warn, {}});
            if (!s.log.fps.empty()) out.push_back({t(Text::DiagLogFps), wide(s.log.fps), Tone::Normal, {}});
        }
        return out;
    }

    ReportFacts facts() {
        Model& m = host_.model();
        const SystemInfo& sys = m.system();
        const Snapshot& s = m.now();
        ReportFacts f;
        f.utilityVersion = kVersion;
        f.driverVersion = s.installedVersion;
        f.windows = num(long(sys.windows.major)) + "." + num(long(sys.windows.minor)) + " build " +
                    num(long(sys.windows.build));
        f.gpu = sys.d3d12Probed ? sys.d3d12.adapter + ", " + vramText(sys.d3d12.vramMb) + ", driver " +
                                      (sys.gpuDriver.empty() ? "unknown" : sys.gpuDriver) +
                                      ", Direct3D 12: " + (sys.d3d12.ok ? "OK" : "failed " + hresult(sys.d3d12.hr))
                                : "not checked (Windows before 10)";
        for (const MonitorInfo& mon : sys.monitors)
            f.monitors.push_back(num(mon.pixels.width) + "x" + num(mon.pixels.height) + ", " + num(mon.refresh) +
                                 " Hz, " + num(mon.scale) + " %, HDR " + (mon.hdr ? "yes" : "no") +
                                 (mon.primary ? ", main" : ""));
        f.iniText = s.iniText;
        const ScreenNow now = m.screenNow();
        f.screen = now.choice.show == Show::Window
                       ? "window " + num(now.choice.window.width) + "x" + num(now.choice.window.height) + ", interface " +
                             num(now.choice.ui) + " %, game " + num(now.game.width) + "x" + num(now.game.height)
                       : "full screen, game " + num(now.game.width) + "x" + num(now.game.height) + ", interface " +
                             num(now.choice.ui) + " %";
        if (now.roextOverride) f.screen += " (ROExt sets the window)";
        for (const ModuleFile& mod : s.modules)
            f.inputModules.push_back(utf8(mod.file) + ": " +
                                     (mod.info.kind == ModuleKind::Roext     ? "ROExt (" + mod.info.ini + ")"
                                      : mod.info.kind == ModuleKind::UaroPad ? std::string("uaRO Pad")
                                                                             : std::string("unknown")) +
                                     (mod.loaded ? "" : ", off"));
        for (const CompatFlag& c : s.compat) f.compatFlags.push_back(std::string(c.allUsers ? "HKLM: " : "HKCU: ") + c.flag);
        for (const Row& r : rows_)
            if (r.tone == Tone::Bad || r.tone == Tone::Warn) f.problems.push_back(utf8(r.check + L": " + r.result));
        f.log = s.log;
        return f;
    }

    std::vector<Row> rows_;
    HWND list_ = nullptr;
};

}  // namespace

std::unique_ptr<Page> makeDiagnosticsPage(PageHost& host) { return std::make_unique<DiagnosticsPage>(host); }

}  // namespace uagfx::app
