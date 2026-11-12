// 3.0.18-beta.222 (the owner: «делай» - every window sized by its corner drawn sharp, R-BN9TJA's kind): a document's
// own dp scale (RmlUi patched, third_party/rmlui) - its dp lengths and dp fonts computed at the context's ratio times
// it, its letters drawn at that size, not a finished picture scaled by a transform
#include <cmath>
#include <string>

#include "rml_host.hpp"
#include "test.hpp"

TEST(rml_document_dp_scale_lays_out_and_sizes_letters) {
    std::string error;
    const char* rml = R"(<rml><head><style>
body { position: absolute; width: 200dp; height: 100dp; font-size: 14dp; }
.box { display: block; width: 100dp; height: 20dp; font-size: 20dp; }
</style></head><body><div class="box" id="b">Hi</div></body></rml>)";
    Rml::ElementDocument* doc = rmlhost::load(rml, nullptr, 0, 0, &error);
    CHECK_EQ(error, std::string());
    Rml::Element* b = doc ? doc->GetElementById("b") : nullptr;
    CHECK(b != nullptr);
    if (!b) return;
    const float w1 = b->GetBox().GetSize().x, f1 = b->GetComputedValues().font_size();
    CHECK_EQ(doc->GetDpScale(), 1.0f);
    doc->SetDpScale(0.5f);  // the window pulled to half its size
    rmlhost::update();
    const float w2 = b->GetBox().GetSize().x, f2 = b->GetComputedValues().font_size();
    CHECK(std::fabs(w2 - w1 * 0.5f) < 0.6f);  // laid out at half
    CHECK(std::fabs(f2 - f1 * 0.5f) < 0.6f);  // its letters at half their size (drawn so, not shrunk)
    CHECK(std::fabs(doc->GetBox().GetSize().x - 100.0f) < 0.6f);
    doc->SetDpScale(1.0f);
    rmlhost::update();
    CHECK(std::fabs(b->GetBox().GetSize().x - w1) < 0.6f);
    rmlhost::close(doc);
}
