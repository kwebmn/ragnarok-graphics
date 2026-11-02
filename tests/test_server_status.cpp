// 3.0.11-beta.173 (the login concept: the server's state under the window): the status line's words and colour, and
// when a failed login is tried again by itself - only after the player pressed «Увійти», every 8 s
// (core/server_status)
#include <string>

#include "core/server_status.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(server_status_line_words) {
    const auto up = srvstatus::line(srvstatus::State::Up, 52, 157, 0, settings::Lang::Uk);
    CHECK(up.text.find("157") != std::string::npos && up.text.find("52") != std::string::npos);
    const auto down = srvstatus::line(srvstatus::State::Down, -1, -1, 8, settings::Lang::Uk);
    CHECK(down.text.find("8") != std::string::npos && down.rgb != up.rgb);
    CHECK(srvstatus::line(srvstatus::State::Up, 52, -1, 0, settings::Lang::En).text.find("online") ==
          std::string::npos);  // no count when the watcher has none
    CHECK(srvstatus::line(srvstatus::State::Unknown, -1, -1, 0, settings::Lang::Uk).text.empty());
    CHECK(srvstatus::line(srvstatus::State::Down, -1, 157, 0, settings::Lang::Uk).text !=
          srvstatus::line(srvstatus::State::Down, -1, 157, 5, settings::Lang::Uk).text);  // the countdown told
}

TEST(server_status_retry_only_after_a_press) {
    CHECK(!srvstatus::retryLogin(true, false, 30));
    CHECK(!srvstatus::retryLogin(true, true, 5));
    CHECK(srvstatus::retryLogin(true, true, 8));
    CHECK(!srvstatus::retryLogin(false, true, 8));
    CHECK(srvstatus::kProbeSec == 8);
}
