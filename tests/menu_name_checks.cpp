// P8.11 desktop check: manifest name rule, first-read timing, scroll rendering.
#include "../src/game_names.h"
#include "../src/host_font.h"
#include "../src/host_menu.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <unistd.h>
static int64_t now_us() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static std::vector<std::string> list(const std::string& d) {
    std::vector<std::string> v; DIR* p = opendir(d.c_str()); if (!p) return v;
    while (dirent* e = readdir(p)) { std::string n = e->d_name; if (n.size() > 4 && n.substr(n.size() - 4) == ".jar") v.push_back(n); }
    closedir(p); std::sort(v.begin(), v.end()); return v;
}
static void ppm(const std::string& f, const std::vector<uint32_t>& px) {
    FILE* o = std::fopen(f.c_str(), "wb"); std::fprintf(o, "P6 1280 720 255\n");
    for (uint32_t c : px) { unsigned char rgb[3] = {uint8_t(c), uint8_t(c >> 8), uint8_t(c >> 16)}; std::fwrite(rgb, 1, 3, o); }
    std::fclose(o);
}
// Stored ZIP fixtures exercise the production central-directory reader without dependencies.
static void stored_jar(const std::string& path,const std::string& manifest,size_t bytes) {
    const std::string entry="META-INF/MANIFEST.MF";
    std::string zip;
    auto u16=[&](uint32_t n){zip+=char(n);zip+=char(n>>8);};
    auto u32=[&](uint32_t n){u16(n);u16(n>>16);};
    u32(0x04034b50);u16(20);u16(0);u16(0);u16(0);u16(0);u32(0);
    u32(manifest.size());u32(manifest.size());u16(entry.size());u16(0);zip+=entry;zip+=manifest;
    const auto offset=zip.size();
    u32(0x02014b50);u16(20);u16(20);u16(0);u16(0);u16(0);u16(0);u32(0);
    u32(manifest.size());u32(manifest.size());u16(entry.size());u16(0);u16(0);u16(0);u16(0);u32(0);u32(0);zip+=entry;
    const auto central_size=zip.size()-offset;
    assert(bytes>=zip.size()+22&&bytes-zip.size()-22<=65535);
    const auto padding=bytes-zip.size()-22;
    u32(0x06054b50);u16(0);u16(0);u16(1);u16(1);u32(central_size);u32(offset);u16(padding);
    zip.append(padding,'x');std::ofstream f(path,std::ios::binary);f<<zip;
}
static void metadata_checks() {
    char folder[]="/tmp/menu-metadata-XXXXXX";assert(mkdtemp(folder));
    const std::string root=folder,known=root+"/known.jar",fallback=root+"/fallback.jar",bad=root+"/broken.jar",missing=root+"/missing.jar";
    stored_jar(known,"MIDlet-Name: English\r\nMIDlet-1: 游戏名, icon.png, Main\r\nMIDlet-Version: 1.\r\n 0\r\nMIDlet-Jar-Size: 999999\r\n",1025);
    stored_jar(fallback,"MIDlet-Name: \xff\r\nMIDlet-Version:   \r\n",1024);
    {std::ofstream f(bad,std::ios::binary);f<<std::string(2049,'x');}
    game_names::Cache cache;
    assert(cache.get(known)=="游戏名"); // Original CJK selection rule stays intact.
    assert(cache.get_detail(known)=="游戏名 · 版本 1.0 · 2 KB");
    assert(cache.get(fallback)=="fallback");
    assert(cache.get_detail(fallback)=="fallback · 版本未知 · 1 KB");
    assert(cache.get_detail(bad)=="broken · 版本未知 · 3 KB");
    assert(cache.get_detail(missing)=="missing · 版本未知 · 大小未知");
    assert(cache.reads==4&&cache.misses==3);
    std::remove(known.c_str());std::remove(fallback.c_str());std::remove(bad.c_str());
    assert(cache.get_detail(known)=="游戏名 · 版本 1.0 · 2 KB"&&cache.reads==4);
    assert(cache.get_detail(missing)=="missing · 版本未知 · 大小未知"&&cache.reads==4);
    assert(rmdir(folder)==0);
    std::printf("PASS metadata: UTF-8/CJK name, folded version, actual rounded JAR bytes, invalid/missing fallback, one read/cache\n");
}
int main(int argc, char** argv) {
    metadata_checks();
    if(argc==1)return 0;
    assert(argc==6);
    const std::string real = argv[1], fx22 = argv[2], extra = argv[3], font = argv[4], out = argv[5];
    game_names::Cache c;
    // Rule checks on the real test JARs (read-only) and synthetic extras.
    struct { const char* f; const char* want; } rules[] = {
        {"bio.jar", "生化危机3D正版"}, {"city.jar", "丧尸危机-全城爆发"}, {"zxx.jar", "醉仙侠-九天逍遥"},
        {"dnf.jar", "地下城与勇士-鬼剑士"}, {"petking6.jar", "PetKing6"}, {"ArtOfWar2-60fps.jar", "Art Of War 2 - Global Confederation"}};
    for (auto& r : rules) { const std::string got = c.get(real + "/" + r.f); std::printf("RULE %s -> %s %s\n", r.f, got.c_str(), got == r.want ? "OK" : "FAIL"); assert(got == r.want); }
    assert(c.get(extra + "/noname.jar") == "noname"); assert(c.get(extra + "/nomanifest.jar") == "nomanifest");
    assert(game_names::has_cjk(c.get(extra + "/long-cn.jar")));
    assert(c.get(fx22 + "/Resident Evil Degeneration.jar") == "RED http://www.universosymbian.org/");
    std::printf("RULE extras OK\n");
    // First menu read of the 22 SD-card manifests (fresh cache), then cached pass.
    auto names = list(fx22); assert(names.size() == 22);
    game_names::Cache cold; int64_t t0 = now_us();
    for (auto& n : names) cold.get(fx22 + "/" + n);
    const int64_t cold_us = now_us() - t0; t0 = now_us();
    for (auto& n : names) cold.get(fx22 + "/" + n);
    const int64_t warm_us = now_us() - t0;
    std::printf("TIMING jars=22 first_read_us=%lld cached_pass_us=%lld reads=%d hits=%d fallback=%d\n", (long long)cold_us, (long long)warm_us, cold.reads, cold.hits, cold.misses);
    for (auto& n : names) std::printf("NAME %s => %s\n", n.c_str(), cold.get(fx22 + "/" + n).c_str());
    // Rendering with the real CJK font through the shipped render_menu.
    std::ifstream in(font, std::ios::binary); std::vector<char> fb((std::istreambuf_iterator<char>(in)), {});
    host_font::Font F; assert(F.add_font(fb.data(), fb.size()));
    std::vector<std::string> labels = names, details;
    labels.push_back("long-cn.jar"); std::sort(labels.begin(), labels.end());
    for (auto& l : labels) details.push_back(cold.get_detail((l == "long-cn.jar" ? extra : fx22) + "/" + l));
    std::vector<uint32_t> px(1280 * 720);
    auto text = [&](int x, int y, const std::string& s, uint32_t col, int h, int w) { F.draw(px.data(), 1280, x, y, s, col, h, w); };
    auto adv = [&](const std::string& s, int h) { auto m = F.measure(s, h); return m.ok ? m.advance : host_menu::text_advance(s, h); };
    auto idx = [&](const std::string& n) { return int(std::find(labels.begin(), labels.end(), n) - labels.begin()); };
    auto render = [&](int sel, int64_t ms, bool* sc) {
        host_menu::render_menu(px.data(), 1280, labels, sel, host_menu::Focus::Games, {}, "", text, text, "原生", {}, adv, details, ms, sc);
    };
    struct Shot { const char* file; const char* jar; int64_t ms; } shots[] = {
        {"menu-cn.png", "Zombie Crisis - Citywide Outbreak.jar", 0},
        {"menu-bio-midlet1.png", "Resident Evil 3D (CAPCOM Classic).jar", 0},
        {"menu-en.png", "Age of Empires III Mobile.jar", 0},
        {"menu-url.png", "Resident Evil Degeneration.jar", 0},
        {"menu-longfile-t0.png", "Flame Side Story - Blazing Mercenaries_60FPS_Original Speed Logic_Anti-Jitter Stable.jar", 0},
        {"menu-longfile-scrolled.png", "Flame Side Story - Blazing Mercenaries_60FPS_Original Speed Logic_Anti-Jitter Stable.jar", 6000},
        {"menu-longname-t0.png", "long-cn.jar", 0},
        {"menu-longname-scrolled.png", "long-cn.jar", 9000}};
    const uint32_t bg = host_menu::color(0xf3f3f3);
    for (auto& s : shots) {
        bool sc = false; render(idx(s.jar), s.ms, &sc);
        // Detail line (y 566..620) must stay inside x 48..1232: margins keep background.
        for (int y = 566; y < 620; ++y) { for (int x = 0; x < 48; ++x) assert(px[y * 1280 + x] == bg); for (int x = 1232; x < 1280; ++x) assert(px[y * 1280 + x] == bg); }
        ppm(out + "/" + s.file + ".ppm", px);
        std::printf("SHOT %s sel=%s t=%lldms scrolling=%d margins_clean=1\n", s.file, s.jar, (long long)s.ms, sc);
    }
    // Scroll cost: 10 s of 30 Hz repaints of the scrolling menu (long file + long name).
    for (const char* j : {"Flame Side Story - Blazing Mercenaries_60FPS_Original Speed Logic_Anti-Jitter Stable.jar", "long-cn.jar"}) {
        int64_t worst = 0, total = 0; int prev = -1, moved = 0;
        for (int f = 0; f < 300; ++f) {
            const int64_t a = now_us(); bool sc = false; render(idx(j), f * 33, &sc); const int64_t d = now_us() - a;
            worst = std::max(worst, d); total += d;
            const bool lf = std::string(j) != "long-cn.jar";
            const int off = lf ? host_menu::scroll_offset(adv(j, 24), 754, f * 33) : host_menu::scroll_offset(adv(details[idx(j)], 20), 1184, f * 33);
            if (prev >= 0 && off != prev) ++moved; prev = off;
        }
        std::printf("SCROLLCOST sel=%s frames=300 width=%d avg_render_us=%lld max_render_us=%lld offset_changes=%d budget_us=33333\n", j, std::string(j) != "long-cn.jar" ? adv(j, 24) : adv(details[idx(j)], 20), (long long)(total / 300), (long long)worst, moved);
    }
    std::printf("ALL OK\n");
}
