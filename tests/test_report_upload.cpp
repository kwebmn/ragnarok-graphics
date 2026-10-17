#include <string>

#include "core/report_upload.hpp"
#include "test.hpp"

using namespace uagfx::report;

TEST(report_upload_json_has_the_sites_fields) {
    // 0.51: what uaro.kiev.ua's POST /api/graphics/reports reads.
    Upload u;
    u.reason = "crash";
    u.automatic = true;
    u.version = "0.51.0";
    u.gpu = "Radeon \"HD\" 6570";
    u.comment = "line 1\nline\t2\\";
    u.report = "report";
    u.files = {{"uaro-graphics.log", "a\r\nb\x01"}, {"uaro-graphics.ini", ""}};
    const std::string j = uploadJson(u);
    CHECK(j.find("\"reason\":\"crash\"") != std::string::npos);
    CHECK(j.find("\"auto\":true") != std::string::npos);
    CHECK(j.find("\"gpu\":\"Radeon \\\"HD\\\" 6570\"") != std::string::npos);
    CHECK(j.find("\"comment\":\"line 1\\nline\\t2\\\\\"") != std::string::npos);
    CHECK(j.find("{\"name\":\"uaro-graphics.log\",\"text\":\"a\\r\\nb\\u0001\"}") != std::string::npos);
    CHECK(j.find("{\"name\":\"uaro-graphics.ini\",\"text\":\"\"}],\"images\":[]}") != std::string::npos);
    CHECK(j.front() == '{' && j.back() == '}');
}

TEST(report_upload_sends_the_missing_pictures) {
    // 0.52: BMPs as base64; anything else, or past the count, is left out.
    CHECK(base64("") == "" && base64("M") == "TQ==" && base64("Ma") == "TWE=" && base64("Man") == "TWFu");
    Upload u;
    u.images = {{"player-32x32_ab.bmp", "BMxx"}, {"not.bmp", "PNG"}};
    for (size_t i = 0; i < kMostImages + 3; ++i) u.images.emplace_back("m" + std::to_string(i) + ".bmp", "BM");
    const std::string j = uploadJson(u);
    CHECK(j.find("{\"name\":\"player-32x32_ab.bmp\",\"data\":\"Qk14eA==\"}") != std::string::npos);
    CHECK(j.find("not.bmp") == std::string::npos);
    CHECK(j.find("\"m62.bmp\"") != std::string::npos && j.find("\"m63.bmp\"") == std::string::npos);
    CHECK(j.find("\"dump\"") == std::string::npos);
    u.dump = "MDMPxx";  // a minidump (0.52)
    CHECK(uploadJson(u).find(",\"dump\":\"TURNUHh4\"}") != std::string::npos);
    u.dump = "nonsense";
    CHECK(uploadJson(u).find("\"dump\"") == std::string::npos);
}

TEST(report_upload_text_is_made_valid_utf8) {
    CHECK(validUtf8("Привіт, ok") == "Привіт, ok");
    CHECK(validUtf8("C:\\\xC8\xE3\xF0\xE8\\x") == "C:\\????\\x");  // a Windows-1251 path
    CHECK(validUtf8("\xE2\x82") == "??");                         // cut short
    CHECK(validUtf8("\xC0\xAF") == "??");                         // overlong
    CHECK(validUtf8("\xF0\x9F\x98\x80") == "\xF0\x9F\x98\x80");   // a 4-byte one
    CHECK(validUtf8("\xED\xA0\x80") == "???");                    // a surrogate
}

TEST(report_upload_takes_a_logs_end) {
    const std::string log = "first line\nsecond line\nthird line\n";
    CHECK(tailOf(log, 1000) == log);
    const std::string t = tailOf(log, 15);
    CHECK(t.find("third line\n") != std::string::npos);
    CHECK(t.find("second") == std::string::npos);  // from the start of a line
    CHECK(t.rfind("(23 bytes before this left out)\n", 0) == 0);
}

TEST(report_upload_reads_the_sites_answer) {
    std::string code;
    CHECK(codeOf("{\"code\":\"R-7K3F2A\"}", code) && code == "R-7K3F2A");
    CHECK(!codeOf("{\"message\":\"Too Many Attempts.\"}", code));
    CHECK(!codeOf("{\"code\":\"\"}", code));
}

TEST(report_upload_tells_what_went_wrong_from_the_log) {
    const std::string ok =
        "x DirectDrawCreateEx = 0x00000000 (uaRO renderer)\ny\nINFO  DirectDraw released, renderer closed\n";
    CHECK(reasonOfLog(ok).empty());
    CHECK(reasonOfLog("INFO  dgVoodoo\n").empty());  // the uaRO renderer never ran
    CHECK(reasonOfLog("x DirectDrawCreateEx = 0x00000000 (uaRO renderer)\nframes\n") == "crash");
    // Closed, opened again (a change of mode) and not closed: a crash.
    CHECK(reasonOfLog(ok + "z DirectDrawCreateEx = 0x00000000 (uaRO renderer)\n") == "crash");
    CHECK(reasonOfLog("ERROR the uaRO renderer could not start (no adapter): falling back") == "start-failed");
    CHECK(reasonOfLog(ok + "ERROR crash: exception 0xc0000005 at uaRO.exe+0x1234 (thread 7)") == "crash");
    CHECK(reasonOfLog(ok + "ERROR Present: the GPU device was removed (0x887a0005") == "gpu-lost");
    CHECK(reasonOfLog(ok + "ERROR Direct3D 11: Present - the graphics card was lost (0x") == "gpu-lost");
}

TEST(report_upload_tells_the_api_from_the_log) {
    CHECK(apiOfLog("INFO  Direct3D 11 renderer on NVIDIA\n") == "d3d11");
    CHECK(apiOfLog("Direct3D 11 renderer on A\nDirect3D 12 renderer on A\n") == "d3d12");  // the last start
    CHECK(apiOfLog("INFO  DirectDrawCreateEx = 0x00000000\n") == "system");  // 2.2: passed to the system's DirectDraw
    CHECK(apiOfLog("").empty());
}
