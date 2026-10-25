// P2.1-c3 follow-up: note_text hashed `s[0] << 8` with a signed char -> negative left shift (UB) for
// non-ASCII text (GBK/UTF-8 lead bytes). Now the byte is cast to unsigned first. Build with
// -fsanitize=undefined -fno-sanitize-recover=all to prove the UB is gone.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cstdio>
static int fails = 0;
#define CHECK(c, what) do { bool ok_ = (c); std::printf("%s %s\n", ok_ ? "PASS" : "FAIL", what); if (!ok_) ++fails; } while (0)
static uint32_t expect(uint32_t sig, const std::string& s) {
    sig ^= uint32_t(s.size()) ^ (uint32_t(static_cast<unsigned char>(s[0])) << 8); return sig * 16777619u;
}
int main() {
    VMS_RS(g_drawacc).on = false;
    const std::string cases[] = {"\xE6\xB8\xB8", "\x80", "\xFF", "A", "\x7F"};
    for (const auto& s : cases) {
        const uint32_t before = VMS_RS(g_frame_sig); note_text(s);
        char what[96]; std::snprintf(what, sizeof what, "note_text lead byte 0x%02X hashes as unsigned (no UB)", unsigned(static_cast<unsigned char>(s[0])));
        CHECK(VMS_RS(g_frame_sig) == expect(before, s), what);
    }
    const uint32_t before = VMS_RS(g_frame_sig); note_text(std::string()); CHECK(VMS_RS(g_frame_sig) == before, "empty text leaves signature unchanged");
    std::printf("NOTE_TEXT_RESULT %s fails=%d\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
