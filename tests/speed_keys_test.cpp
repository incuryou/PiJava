// P8.3 unit/scenario test for speed_keys.h (desktop). Usage: speed_keys_test <scratch_dir> <jar_dir>
#include "../src/speed_keys.h"
#include <chrono>
#include <cstdlib>
#include <dirent.h>
#include <iostream>
#include <vector>
static int fails = 0;
#define CHECK(c) do { if (c) std::cout << "PASS " #c "\n"; else { std::cout << "FAIL " #c " line " << __LINE__ << "\n"; ++fails; } } while (0)
static std::string slurp(const std::string& p) { std::ifstream i(p.c_str(), std::ios::binary); std::string s((std::istreambuf_iterator<char>(i)), {}); return s; }
static void put(const std::string& p, const std::string& d) { std::ofstream o(p.c_str(), std::ios::binary | std::ios::trunc); o << d; }
static double ms_since(std::chrono::steady_clock::time_point t) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count(); }
int main(int argc, char** argv) {
    if (argc < 3) return 2;
    const std::string W = argv[1], J = argv[2];
    std::system(("rm -rf '" + W + "' && mkdir -p '" + W + "/j2me/games'").c_str());
    const std::string D = W + "/j2me";
    std::system(("cp '" + J + "/ArtOfWar2-60fps.jar' '" + D + "/'").c_str());
    std::system(("cp '" + J + "/city.jar' '" + D + "/'").c_str());
    const std::string aow_root = D + "/ArtOfWar2-60fps.jar", aow_games = D + "/games/ArtOfWar2-60fps.jar", city = D + "/city.jar";
    // 1) old-format speed.cfg migration (+ missing jar preserved)
    const std::string old_cfg = "J2ME_SPEED 1\n" + aow_root + "\t60\n" + city + "\t30\n" + D + "/gone.jar\t60\n";
    put(D + "/speed.cfg", old_cfg);
    { speed_keys::State s; s.dir = D; std::map<std::string, int> sp;
      CHECK(host_speed::load(s.cfg(), sp) && sp.size() == 3);
      speed_keys::load_cache(s);
      auto r = speed_keys::migrate(s, sp); speed_keys::save_cache(s);
      std::cout << "INFO migrate migrated=" << r.migrated << " kept=" << r.kept << " bak=" << r.bak_written << "\n";
      CHECK(r.migrated == 2 && r.kept == 1 && r.rewritten && r.bak_written && r.save_ok);
      CHECK(slurp(s.bak()) == old_cfg);
      CHECK(sp.count(D + "/gone.jar") && sp[D + "/gone.jar"] == 60);
      CHECK(speed_keys::lookup(s, sp, aow_root) == 60 && speed_keys::lookup(s, sp, city) == 30);
      CHECK(s.hashed == 2 && s.hits == 0); // cache miss path
    }
    const std::string after1 = slurp(D + "/speed.cfg");
    CHECK(after1.find(aow_root) == std::string::npos && after1.find("sha256:") != std::string::npos);
    // 2) reload: cache hit, nothing re-hashed, no rewrite
    { speed_keys::State s; s.dir = D; std::map<std::string, int> sp; host_speed::load(s.cfg(), sp); speed_keys::load_cache(s);
      auto r = speed_keys::migrate(s, sp);
      CHECK(r.migrated == 0 && !r.rewritten && r.kept == 1);
      CHECK(speed_keys::lookup(s, sp, aow_root) == 60 && s.hashed == 0 && s.hits == 1); }
    // 3) bak written once: a later rewrite keeps the original bak
    { speed_keys::State s; s.dir = D; std::map<std::string, int> sp; host_speed::load(s.cfg(), sp); speed_keys::load_cache(s);
      sp[speed_keys::key_for(s, city)] = 60; bool bw = false; CHECK(speed_keys::rewrite(s, sp, &bw)); CHECK(!bw);
      CHECK(slurp(s.bak()) == old_cfg); }
    // 4) cache miss when mtime/size changes (touch -> mtime)
    { std::system(("touch -d '2001-01-01' '" + city + "'").c_str());
      speed_keys::State s; s.dir = D; speed_keys::load_cache(s); speed_keys::jar_sha(s, city); CHECK(s.hashed == 1 && s.hits == 0); speed_keys::save_cache(s); }
    // 5) missing jar
    { speed_keys::State s; s.dir = D; std::map<std::string, int> sp; CHECK(speed_keys::key_for(s, D + "/nope.jar").empty()); CHECK(speed_keys::lookup(s, sp, D + "/nope.jar") == 0); }
    // 6) move aow root -> games/, speed follows
    std::system(("mv '" + aow_root + "' '" + aow_games + "'").c_str());
    { speed_keys::State s; s.dir = D; std::map<std::string, int> sp; host_speed::load(s.cfg(), sp); speed_keys::load_cache(s); speed_keys::migrate(s, sp);
      CHECK(speed_keys::lookup(s, sp, aow_games) == 60); std::cout << "INFO moved aow hashed=" << s.hashed << "\n"; speed_keys::save_cache(s); }
    // 7) timing: 22 jars (6 real jars copied to reach 22)
    const std::string T = W + "/t22"; std::system(("mkdir -p '" + T + "'").c_str());
    std::vector<std::string> src; DIR* d = opendir(J.c_str()); for (dirent* e; d && (e = readdir(d));) { std::string n = e->d_name; if (n.size() > 4 && n.substr(n.size() - 4) == ".jar") src.push_back(J + "/" + n); } if (d) closedir(d);
    std::vector<std::string> jars; uint64_t bytes = 0;
    for (int i = 0; i < 22; ++i) { std::string p = T + "/g" + std::to_string(i) + ".jar"; std::system(("cp '" + src[i % src.size()] + "' '" + p + "'").c_str()); jars.push_back(p); struct stat st; stat(p.c_str(), &st); bytes += st.st_size; }
    std::system("sync; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null");
    { speed_keys::State s; s.dir = T; auto t = std::chrono::steady_clock::now(); for (auto& p : jars) speed_keys::jar_sha(s, p); speed_keys::save_cache(s);
      std::cout << "TIME first_launch jars=22 bytes=" << bytes << " hashed=" << s.hashed << " ms=" << ms_since(t) << "\n"; CHECK(s.hashed == 22); }
    { auto t = std::chrono::steady_clock::now(); speed_keys::State s; s.dir = T; speed_keys::load_cache(s); for (auto& p : jars) speed_keys::jar_sha(s, p);
      std::cout << "TIME cache_hit jars=22 hits=" << s.hits << " hashed=" << s.hashed << " ms=" << ms_since(t) << "\n"; CHECK(s.hits == 22 && s.hashed == 0); }
    std::cout << (fails ? "RESULT FAIL " : "RESULT PASS ") << fails << "\n";
    return fails ? 1 : 0;
}
