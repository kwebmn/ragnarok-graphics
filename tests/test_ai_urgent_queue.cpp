#include <string>

#include "core/ai/urgent_queue.hpp"
#include "test.hpp"

using namespace uagfx::ai;

// 3.0.2-beta.10: the decoding pool takes the most urgent first - a sprite frame (0) before a texture in view (1)
// before one known ahead (2) - and of one rank the oldest
TEST(ai_urgent_queue_lowest_rank_first_then_the_oldest) {
    UrgentQueue<std::string> q;
    q.push(2, "ahead a");
    q.push(0, "frame a");
    q.push(1, "in view");
    q.push(0, "frame b");
    q.push(2, "ahead b");
    CHECK_EQ(q.size(), size_t(5));
    std::string v;
    for (const char* want : {"frame a", "frame b", "in view", "ahead a", "ahead b"}) {
        CHECK(q.pop(v));
        CHECK_EQ(v, std::string(want));
    }
    CHECK(q.empty());
    CHECK(!q.pop(v));
}

// One pushed after some were taken still goes by its rank
TEST(ai_urgent_queue_a_late_frame_goes_first) {
    UrgentQueue<int> q;
    q.push(2, 1);
    q.push(2, 2);
    int v = 0;
    CHECK(q.pop(v) && v == 1);
    q.push(0, 9);
    CHECK(q.pop(v) && v == 9);
    CHECK(q.pop(v) && v == 2);
}
