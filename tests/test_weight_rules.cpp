// 3.0.11-beta.202 (the tester: «бреше в чат на 50%», «тут теж на 70% показує іконку 50»): uAthena on uaRO stops HP
// and SP coming back at 70 % of the most weight (not 50 %): the game's own line (msgstringtable's 0x132 «... over 50%
// of the Weight Limit.») said 50 and came at 50 % - its number made 70 in place, whatever the table's language
#include <string>

#include "core/weight_rules.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(weight_rules_the_line_says_seventy) {
    std::string en = "HP/SP will not be restored when your carried weight is over 50% of the Weight Limit.";
    CHECK(weightrules::seventy(en) && en == "HP/SP will not be restored when your carried weight is over 70% of the "
                                           "Weight Limit.");
    std::string fr = "Vos points HP/SP ne seront pas regeneres si votre poids est au dessus de 50 % de votre maximum.";
    CHECK(weightrules::seventy(fr) && fr.find("70 %") != std::string::npos && fr.find("50") == std::string::npos);
    std::string done = en;
    CHECK(!weightrules::seventy(done) && done == en);  // already 70: nothing to do
    std::string none = "You are over your Weight Limit.";
    CHECK(!weightrules::seventy(none));
    std::string year = "In 1550 50%";  // the 50 before a per cent sign, not another number's digits
    CHECK(weightrules::seventy(year) && year == "In 1550 70%");
}
