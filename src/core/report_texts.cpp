#include "core/report_texts.hpp"

namespace uagfx::reporttexts {

using settings::Text;

std::string status(settings::Lang l, reports::Status s, const std::string& fixedIn) {
    switch (s) {
        case reports::Status::Read: return settings::tr(l, Text::ReportStatusRead);
        case reports::Status::NeedInfo: return settings::tr(l, Text::ReportStatusNeedInfo);
        case reports::Status::Fixing: return settings::tr(l, Text::ReportStatusFixing);
        case reports::Status::Fixed: return settings::fmt(settings::tr(l, Text::ReportStatusFixed), {fixedIn});
        case reports::Status::Closed: return settings::tr(l, Text::ReportStatusClosed);
        case reports::Status::New: break;
    }
    return settings::tr(l, Text::ReportStatusNew);
}

std::string strip(settings::Lang l, const reports::Answer& a) {
    const Text what = a.status == reports::Status::NeedInfo ? Text::AnswerWaits
                      : a.status == reports::Status::Fixed  ? Text::AnswerUpdate
                                                            : Text::AnswerNew;
    return settings::fmt(settings::tr(l, Text::AnswerStrip),
                         {a.code, status(l, a.status, a.fixedIn), settings::tr(l, what)});
}

std::string hint(settings::Lang l, const reports::Answer& a) {
    if (a.status == reports::Status::NeedInfo) return settings::tr(l, Text::CardWaits);
    if (a.status == reports::Status::Fixed) return settings::fmt(settings::tr(l, Text::CardUpdate), {a.fixedIn});
    return settings::tr(l, Text::CardNew);
}

std::string listLine(settings::Lang l, const reports::Summary& s) {
    if (!s.lastText.empty()) {
        std::string words = s.lastText;
        for (char& ch : words)
            if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
        return std::string(settings::tr(l, s.lastAuthor == "dev" ? Text::ReportDev : Text::ReportYou)) + ": " + words;
    }
    reports::Answer a;
    a.status = s.status;
    a.fixedIn = s.fixedIn;
    if (s.status == reports::Status::New) return settings::tr(l, Text::ListWaitsDev);
    if (s.status == reports::Status::NeedInfo || s.status == reports::Status::Fixed) return hint(l, a);
    return status(l, s.status, s.fixedIn);
}

}  // namespace uagfx::reporttexts
