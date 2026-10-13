#include "core/settings/reg_file.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

const char kRoot[] = "HKEY_CURRENT_USER\\Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Gravity Soft\\Ragnarok";

RegCopy sample() {
    RegCopy c;
    c.exists = true;
    c.keys[""]["WIDTH"] = RegValue::dword(0x780);
    c.keys[""]["ID"] = RegValue::sz("say \"hi\" \\ there");
    c.keys[""]["GUIDDEVICE"] = RegValue::binary(halDeviceGuid());
    c.keys["Option"]["SKINNAME"] = RegValue::sz("<Basic Skin>");
    c.keys["Option"]["g_outdoorViewLatitude"] = RegValue::dword(0xc2340000u);
    c.keys["BookMark\\uAthena\\\xD0\xA2\xD0\xB5\xD1\x81\xD1\x82\xD0\xB5\xD1\x80"]["Level 7 Cookbook"] = RegValue::dword(1);
    RegValue multi;
    multi.type = uint32_t(RegType::MultiSz);
    multi.bytes = {0x61, 0, 0, 0, 0, 0};
    c.keys["Option"]["list"] = multi;
    return c;
}

}  // namespace

TEST(reg_file_writes_regedit_text) {
    const std::string text = toRegFile(sample(), kRoot);
    CHECK(text.rfind("Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software", 0) == 0);
    CHECK(text.find("\"WIDTH\"=dword:00000780\r\n") != std::string::npos);
    CHECK(text.find("\"ID\"=\"say \\\"hi\\\" \\\\ there\"\r\n") != std::string::npos);
    CHECK(text.find("\"GUIDDEVICE\"=hex:e0,3d,e6,84,aa,46,cf,11,81,6f,00,00,c0,20,15,6e\r\n") != std::string::npos);
    CHECK(text.find("\"list\"=hex(7):61,00,00,00,00,00\r\n") != std::string::npos);
    CHECK(text.find("Gravity Soft\\Ragnarok\\Option]\r\n") != std::string::npos);
    CHECK(text.find("[" + std::string(kRoot) + "]") < text.find("\\Option]"));  // the key itself comes first
}

TEST(reg_file_reads_back_what_it_wrote) {
    const RegCopy in = sample();
    RegCopy out;
    CHECK(fromRegFile(toRegFile(in, kRoot), kRoot, out));
    CHECK(out.exists);
    CHECK(out.keys.size() == in.keys.size());
    for (const auto& k : in.keys)
        for (const auto& v : k.second) {
            const RegValue* got = out.find(k.first, v.first);
            CHECK(got != nullptr);
            if (got) CHECK(*got == v.second);
        }
}

TEST(reg_file_reads_regedit_files_with_wrapped_lines_and_other_keys) {
    const std::string text =
        "\xEF\xBB\xBFWindows Registry Editor Version 5.00\r\n\r\n"
        "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Other]\r\n\"WIDTH\"=dword:00000001\r\n\r\n"
        "[" + std::string(kRoot) + "]\r\n\"GUIDDEVICE\"=hex:e0,3d,e6,84,aa,46,cf,11,\\\r\n  81,6f,00,00,c0,20,15,6e\r\n"
        "@=\"default\"\r\n\"gone\"=-\r\n\r\n"
        "[-" + std::string(kRoot) + "\\Old]\r\n";
    RegCopy out;
    CHECK(fromRegFile(text, kRoot, out));
    CHECK(out.find("", "GUIDDEVICE")->bytes == halDeviceGuid());
    CHECK(out.find("", "")->text == "default");
    CHECK(!out.find("", "WIDTH"));  // that key is someone else's
    CHECK(!out.find("", "gone"));
    CHECK(out.keys.count("Old") == 0);
}

TEST(reg_file_refuses_other_text_and_writes_an_empty_copy_as_a_header) {
    RegCopy out;
    CHECK(!fromRegFile("[display]\nmode = auto\n", kRoot, out));
    CHECK(toRegFile(RegCopy{}, kRoot) == "Windows Registry Editor Version 5.00\r\n\r\n");
    RegCopy empty;
    empty.exists = true;
    CHECK(toRegFile(empty, kRoot) == "Windows Registry Editor Version 5.00\r\n\r\n[" + std::string(kRoot) + "]\r\n\r\n");
}
