// 3.0.9-beta.5: «Звернення» as a messenger - the chat's order and the report's path (core/report_talk)
#include <string>
#include <vector>

#include "core/report_talk.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

reports::Message said(bool dev, const std::string& text, const std::string& at) {
    reports::Message m;
    m.fromDev = dev;
    m.text = text;
    m.at = at;
    return m;
}

std::string kinds(const std::vector<talk::Item>& items) {  // "M0 M1 R M2 S2 M3"
    std::string out;
    for (const talk::Item& i : items) {
        if (!out.empty()) out += ' ';
        switch (i.kind) {
            case talk::Item::AutoSent: out += "A"; break;
            case talk::Item::Message: out += "M" + std::to_string(i.message); break;
            case talk::Item::Status: out += "S" + std::to_string(i.message); break;
            case talk::Item::DevRead: out += "R"; break;
        }
    }
    return out;
}

}  // namespace

TEST(report_talk_lays_out_the_chat) {
    reports::Conversation c;
    c.messages.push_back(said(false, "включил берсерк", "2026-10-27T00:06:52+00:00"));
    c.messages.push_back(said(false, "а вот вышел", "2026-10-27T00:12:28+00:00"));
    c.messages.push_back(said(true, "з Pause, будь ласка", "2026-10-27T00:31:00+00:00"));
    c.messages.back().hasStatus = true;
    c.messages.back().status = reports::Status::NeedInfo;
    c.messages.push_back(said(false, "ось, з Pause", "2026-10-27T08:44:00+00:00"));
    CHECK_EQ(kinds(talk::timeline(c)), std::string("M0 M1 M2 S2 M3"));  // the developer's opening unknown
    c.devSeenAt = "2026-10-27T03:20:00+03:00";  // 00:20 UTC: after the player's two, before the answer
    CHECK_EQ(kinds(talk::timeline(c)), std::string("M0 M1 R M2 S2 M3"));
    c.devSeenAt = "2026-10-27T09:00:00+00:00";  // after the last
    CHECK_EQ(kinds(talk::timeline(c)), std::string("M0 M1 M2 S2 M3 R"));
    reports::Conversation automatic;
    automatic.automatic = true;
    automatic.messages.push_back(said(true, "", "2026-10-27T10:00:00+00:00"));  // a status alone: its line only
    automatic.messages.back().hasStatus = true;
    automatic.messages.back().status = reports::Status::Closed;
    CHECK_EQ(kinds(talk::timeline(automatic)), std::string("A S0"));
    reports::Conversation shot;
    shot.messages.push_back(said(false, "", "2026-10-27T10:00:00+00:00"));
    shot.messages.back().shots.push_back("m5-uaro-1.jpg");  // a screenshot without words is a message
    CHECK_EQ(kinds(talk::timeline(shot)), std::string("M0"));
}

TEST(report_talk_steps_follow_the_status) {
    reports::Conversation c;
    c.status = reports::Status::Read;
    std::vector<talk::Step> s = talk::steps(c);
    CHECK_EQ(s.size(), size_t(5));  // no «Потрібна інформація»: it never was
    if (s.size() == 5) {
        CHECK(s[0].status == reports::Status::New && s[0].done && !s[0].current);
        CHECK(s[1].status == reports::Status::Read && !s[1].done && s[1].current);
        CHECK(s[2].status == reports::Status::Fixing && !s[2].done && !s[2].current);
        CHECK(s[4].status == reports::Status::Closed);
    }
    c.status = reports::Status::Fixing;
    c.messages.push_back(said(true, "з Pause", "2026-10-27T00:31:00+00:00"));
    c.messages.back().hasStatus = true;
    c.messages.back().status = reports::Status::NeedInfo;
    s = talk::steps(c);
    CHECK_EQ(s.size(), size_t(6));
    if (s.size() == 6) {
        CHECK(s[2].status == reports::Status::NeedInfo && s[2].done);
        CHECK(s[3].status == reports::Status::Fixing && s[3].current);
        CHECK(!s[4].done && !s[4].current);
    }
    CHECK(talk::closedTab(reports::Status::Closed));
    CHECK(!talk::closedTab(reports::Status::Fixed));  // waits for the player's «Підтверджую»
}

TEST(report_talk_reads_when_the_developer_saw_it) {
    reports::Conversation c;
    std::string error;
    CHECK(reports::parseConversation(
        "{\"code\":\"R-7BDUE3\",\"status\":\"need_info\",\"dev_seen_at\":\"2026-10-27T00:20:00+00:00\",\"messages\":[]}",
        c, error));
    CHECK_EQ(c.devSeenAt, std::string("2026-10-27T00:20:00+00:00"));
    CHECK(reports::parseConversation("{\"code\":\"R-7BDUE3\",\"status\":\"new\",\"messages\":[]}", c, error));
    CHECK_EQ(c.devSeenAt, std::string(""));  // a site before 3.0.9-beta.5's
}

TEST(report_talk_knows_when_the_shown_one_is_behind) {
    // 3.0.10-beta.2 (the owner: the open conversation showed a new answer only after another was opened and this one
    // again): the list asked every few seconds tells whether the shown conversation is behind the site
    reports::Conversation shown;
    shown.status = reports::Status::Fixed;
    shown.fixedIn = "3.0.10-beta.1";
    shown.messages.push_back(said(false, "тест", "2026-10-28T19:04:02+00:00"));
    shown.messages.push_back(said(true, "Дякуємо за перевірку!", "2026-10-28T19:34:50+00:00"));
    reports::Summary now;
    now.status = reports::Status::Fixed;
    now.fixedIn = "3.0.10-beta.1";
    now.lastAuthor = "dev";
    now.lastAt = "2026-10-28T19:34:50+00:00";
    CHECK(!talk::behind(now, shown));
    now.lastAt = "2026-10-28T19:36:09+00:00";  // the developer wrote again
    CHECK(talk::behind(now, shown));
    now.lastAt = "2026-10-28T19:34:50+00:00";
    now.unread = 1;  // an answer not seen
    CHECK(talk::behind(now, shown));
    now.unread = 0;
    now.status = reports::Status::NeedInfo;  // a status set without words
    CHECK(talk::behind(now, shown));
    now.status = reports::Status::Fixed;
    now.fixedIn = "3.0.10-beta.2";
    CHECK(talk::behind(now, shown));
    reports::Conversation none;  // nothing shown yet, the list has a message
    CHECK(talk::behind(now, none));
}

TEST(report_talk_words_the_bar_of_what_is_awaited) {
    // 3.0.10-beta.2 (the owner: the yellow bar did not fit): the developer's last words in one run - lines joined,
    // spaces made one, the signature («— команда uaRO») left out; the window fits it into its lines
    reports::Conversation c;
    c.messages.push_back(said(false, "тест", "2026-10-28T19:04:02+00:00"));
    c.messages.push_back(said(true,
                              "Ще одне питання: де ви побачили відповідь?\n\n1. Картка в грі.\r\n"
                              "2.  Сповіщення.\n\n— команда uaRO",
                              "2026-10-28T19:36:09+00:00"));
    c.messages.push_back(said(true, "", "2026-10-28T19:37:00+00:00"));  // a status alone: not the question
    CHECK_EQ(talk::waitsText(c),
             std::string("Ще одне питання: де ви побачили відповідь? 1. Картка в грі. 2. Сповіщення."));
    reports::Conversation quiet;
    quiet.messages.push_back(said(false, "тест", "2026-10-28T19:04:02+00:00"));
    CHECK_EQ(talk::waitsText(quiet), std::string(""));
}
