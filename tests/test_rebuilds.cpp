// When the settings app's window makes its page again (stage 2 spec §6; 0.6.0 review C1, I2).
#include "app/rebuilds.hpp"
#include "test.hpp"

using uagfx::app::Rebuilds;

namespace {

// The main window's loop as far as rebuilds go (src/app/main_window.cpp). A rebuild first takes the number of the
// field that has the focus - its page asks for a rebuild again - then moves the focus away from the field, whose
// EN_KILLFOCUS asks once more unless the page is quiet.
struct Window {
    Rebuilds r;
    int queue = 0;  // kRebuild messages posted
    int builds = 0;
    void ask() {
        if (r.request()) ++queue;
    }
    void build() {
        r.begin();
        ++builds;
        ask();                  // the field's number is taken
        if (!r.quiet()) ask();  // the focus leaves the field
        r.end();
    }
    void run() {  // until nothing is posted, or clearly never
        while (queue > 0 && builds < 100) {
            --queue;
            if (r.arrived()) build();
        }
    }
};

}  // namespace

TEST(rebuilds_a_rebuild_with_the_focus_in_a_number_field_asks_for_no_other) {
    Window w;
    w.ask();  // Tab from the window's width to its height
    w.run();
    CHECK_EQ(w.builds, 1);
    CHECK_EQ(w.queue, 0);
}

TEST(rebuilds_one_message_serves_every_request_before_it) {
    Rebuilds r;
    CHECK(r.request());
    CHECK(!r.request());
    CHECK(r.arrived());
    CHECK(!r.arrived());
    CHECK(r.request());  // a later change posts again
}

TEST(rebuilds_a_rebuild_made_at_once_serves_the_message_on_its_way) {
    Rebuilds r;
    CHECK(r.request());
    r.begin();  // another page picked in the list: made at once
    r.end();
    CHECK(!r.arrived());
}

TEST(rebuilds_wait_while_a_message_box_is_open) {
    Rebuilds r;
    r.boxOpened();
    CHECK(!r.request());   // the timer: the game was closed meanwhile
    CHECK(r.boxClosed());  // the window posts now
    CHECK(r.arrived());
}

TEST(rebuilds_a_message_posted_before_a_box_waits_for_it) {
    Rebuilds r;
    CHECK(r.request());
    r.boxOpened();
    CHECK(!r.arrived());  // the box's own loop takes the message: the page whose code opened the box stays
    CHECK(r.boxClosed());
    CHECK(r.arrived());
}

TEST(rebuilds_a_box_with_nothing_wanted_posts_nothing) {
    Rebuilds r;
    r.boxOpened();
    CHECK(!r.boxClosed());
    CHECK(!r.arrived());
}

TEST(rebuilds_wait_for_the_last_of_two_boxes) {
    Rebuilds r;
    r.boxOpened();
    r.boxOpened();
    CHECK(!r.request());
    CHECK(!r.boxClosed());
    CHECK(r.boxClosed());
}
