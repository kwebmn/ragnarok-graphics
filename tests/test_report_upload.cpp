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

TEST(report_upload_names_the_computer_and_the_kind) {
    // 2.13: «Звернення» - the computer's key (its reports come back to it) and what the player says it is.
    Upload u;
    CHECK(uploadJson(u).find("\"computer\"") == std::string::npos);
    CHECK(uploadJson(u).find("\"kind\"") == std::string::npos);
    u.computer = "0123456789abcdef0123456789abcdef";
    u.kind = "slow";
    u.comment = std::string(3000, 'a');  // longer than before 2.13 (2000)
    const std::string j = uploadJson(u);
    CHECK(j.find("\"computer\":\"0123456789abcdef0123456789abcdef\"") != std::string::npos);
    CHECK(j.find("\"kind\":\"slow\"") != std::string::npos);
    CHECK(j.find(std::string(3000, 'a')) != std::string::npos);
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
    // 3.0.18-beta.3: the hang watch's line and the game never closed - a hang; it moved again and closed - fine
    const std::string opened = "x DirectDrawCreateEx = 0x00000000 (uaRO renderer)\n";
    const std::string hang = "ERROR hang: the game's thread has not moved for 15 s\n";
    CHECK(reasonOfLog(opened + hang) == "hang");
    CHECK(reasonOfLog(opened + hang + "INFO  DirectDraw released, renderer closed\n").empty());
    CHECK(reasonOfLog(hang + ok + opened).empty() == false);  // an old hang, the last opening not closed: a crash
    CHECK(reasonOfLog(hang + ok + opened) == "crash");
    CHECK(reasonOfLog(opened + hang + "ERROR crash: exception 0xc0000005 at x") == "crash");  // a crash wins
}

// 3.0.20-beta.76 (the owner's own log of 16.11 22:40:59, beta.68: «the 64-bit renderer exited (exit code
// 0xc0000409); the picture stops here, restart the game» six seconds into Prontera - he closed the game by hand, it
// «closed the renderer», and the watcher took the log for a fine end: no report ever came of the 64-bit renderer's
// death): the renderer's process gone is a bad end however the game itself ended; a lost graphics card stays the
// card's
TEST(report_upload_takes_the_64_bit_renderers_death_for_a_bad_end) {
    const std::string opened = "x DirectDrawCreateEx = 0x00000000 (uaRO renderer)\n";
    const std::string closed = "INFO  DirectDraw released, renderer closed\n";
    const std::string died = "22:40:59.433 ERROR the 64-bit renderer exited (exit code 0xc0000409); the picture stops "
                             "here, restart the game\n";
    CHECK(reasonOfLog(opened + died + closed) == "crash");
    CHECK(reasonOfLog(opened + died) == "crash");
    CHECK(reasonOfLog(opened + "ERROR the 64-bit renderer is gone (exit code 0x00000000); the picture stops here, "
                               "restart the game\n" + closed) == "crash");
    // an earlier game's in the same log (the game opens DirectDraw again as it changes modes: one log a game) too
    CHECK(reasonOfLog(opened + died + closed + opened + closed) == "crash");
    // the card lost, told by the renderer through the same words: the card's
    CHECK(reasonOfLog(opened + "ERROR render64: Present: the GPU device was removed (0x887a0005) (exit code "
                               "0x00000000); the picture stops here, restart the game\n" + closed) == "gpu-lost");
    CHECK(reasonOfLog(opened + closed).empty());
}

TEST(report_upload_tells_the_api_from_the_log) {
    CHECK(apiOfLog("INFO  Direct3D 11 renderer on NVIDIA\n") == "d3d11");
    CHECK(apiOfLog("Direct3D 11 renderer on A\nDirect3D 12 renderer on A\n") == "d3d12");  // the last start
    CHECK(apiOfLog("INFO  DirectDrawCreateEx = 0x00000000\n") == "system");  // 2.2: passed to the system's DirectDraw
    CHECK(apiOfLog("").empty());
}

// 3.0.22-beta.8 (R-JW5W4N): the 64-bit renderer's death however the driver words it - «exited» (at its start) or «is
// gone» (a message not sent) - both lines end «the picture stops here»; the next game's report takes the dead
// renderer's own log (uaro-render64.prev.log) by it - by «exited» alone it missed «is gone»
TEST(report_upload_the_renderer_died_either_wording) {
    CHECK(rendererDied("07:24:37.627 ERROR the 64-bit renderer is gone (exit code 0xc0000409); the picture "
                               "stops here, restart the game\n"));
    CHECK(rendererDied("ERROR the 64-bit renderer exited at its start (exit code 0xc0000409); the picture "
                               "stops here, restart the game"));
    CHECK(!rendererDied("07:25:39.450 INFO  DirectDraw released, renderer closed\n"));
}

// 3.0.24-beta.31 (R-QYF9Z9: the 64-bit renderer died in CreateTexture, the report took only the newer dump - the
// game's, made after it): the other dump of the week goes too, as a file of its name and .b64 (base64; the site keeps
// a report's files as they come, the one dump field as before)
TEST(report_second_dump_as_a_file) {
    const std::pair<std::string, std::string> f = dumpAsFile("crash-render64.dmp", std::string("MDMP\x01", 5));
    CHECK_EQ(f.first, std::string("crash-render64.dmp.b64"));
    CHECK_EQ(f.second, base64(std::string("MDMP\x01", 5)));
    CHECK_EQ(f.second, std::string("TURNUAE="));
    // the site's one file at most 1 600 000 letters (GraphicsReportController MAX_FILE): a bigger dump goes not
    // (the whole report would be refused)
    CHECK(dumpAsFile("crash.dmp", std::string(1200000, 'x')).first == "crash.dmp.b64");
    CHECK(dumpAsFile("crash.dmp", std::string(1200001, 'x')).first.empty());
}
