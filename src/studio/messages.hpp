#pragma once
#include <windows.h>

namespace uagfx::studio {

// 2.3: what the studio's threads tell its window (PostMessage; the window frees what lParam points to).
constexpr UINT kEngineLog = WM_APP + 1;     // lParam: std::string* - a line of the engine's
constexpr UINT kEngineGone = WM_APP + 2;    // the engine ended or its pipe broke
constexpr UINT kCatalogReady = WM_APP + 3;  // the catalog's threads are done (or stopped)
constexpr UINT kJobsChanged = WM_APP + 4;   // a job began, went on or ended: the bar again
constexpr UINT kJobDone = WM_APP + 5;       // wParam: the job's id - its result is ready (Jobs::resultOf)
constexpr UINT kThumbReady = WM_APP + 6;    // a thumbnail or a picture was decoded
constexpr UINT kPicked = WM_APP + 7;        // the game picked a texture (link\pick.txt)
constexpr UINT kRowsReady = WM_APP + 8;     // wParam: the rows' generation; lParam: RowsResult* (textures_tab)
constexpr UINT kShownReady = WM_APP + 9;    // wParam: the inspector's generation; lParam: Shown* (inspector)
constexpr UINT kPackReady = WM_APP + 10;    // lParam: std::wstring* - the Pack tab's numbers

}  // namespace uagfx::studio
