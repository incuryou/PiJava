#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

// Desktop replay is a bounded smoke check, not evidence of full game compatibility.
int main(int argc, char** argv) {
    if (argc != 5 && argc != 6) {
        std::cerr << "usage: game_replay jarpath logpath pngpath seconds [petking|robot|zxx]\n";
        return 2;
    }
    int seconds = std::atoi(argv[4]);
    if (seconds <= 0 || seconds > 3600 || (argc == 6 && std::string(argv[5]) != "petking" && std::string(argv[5]) != "robot" && std::string(argv[5]) != "zxx")) {
        std::cerr << "seconds must be 1..3600; optional replay mode is petking, robot or zxx\n";
        return 2;
    }
    const bool petking = argc == 6 && std::string(argv[5]) == "petking";
    const bool robot = argc == 6 && std::string(argv[5]) == "robot";
    const bool zxx = argc == 6 && std::string(argv[5]) == "zxx";
    const std::string pngpath = argv[3];
    std::string prefix = pngpath;
    if (prefix.size() >= 4 && prefix.substr(prefix.size() - 4) == ".png") prefix.resize(prefix.size() - 4);
    VMS_PLAT(g_boot_ms) = now_ms();
    VMS_PLAT(g_log).open(argv[2], std::ios::out | std::ios::trunc);
    if (!VMS_PLAT(g_log)) { std::cerr << "cannot open replay log\n"; return 2; }
    VMS_CL(g_classpath) = {"romfs/classes"};
    VMS_DIAG(g_log_draw) = true;
    log_line(std::string("REPLAY BOOT ") + kVer + " jar=" + argv[1] +
             " seconds=" + std::to_string(seconds) + " mode=" + (petking ? "petking" : robot ? "robot" : zxx ? "zxx" : "observe"));
    std::vector<uint8_t> bytes;
    if (!read_whole(argv[1], bytes)) die("REPLAY cannot read jar");
    std::string error;
    if (!unzip(bytes, VMS_CL(g_jar_files), error)) die("REPLAY unzip: " + error);
    load_props();
    std::string midlet = manifest_midlet();
    if (midlet.empty()) midlet = find_midlet_class();
    if (midlet.empty()) die("REPLAY no MIDlet class");
    for (char& character : midlet) if (character == '.') character = '/';
    VMS_RS(g_fb).assign((size_t) VMS_RS(g_screen_w) * VMS_RS(g_screen_h) * 3, 0);
    log_line("REPLAY launch=" + midlet);
    launch_class(midlet, rms_jar_identity(bytes.data(), bytes.size()));

    bool music = false, newgame = false;
    int story = 0;
    int64_t story_at = 0;
    int held = 0;
    int held_mask = 0;
    int64_t release_at = 0;
    int64_t begin = monotonic_ms();
    int64_t deadline = begin + (int64_t) seconds * 1000;
    auto screenshot = [&](const std::string& stage) {
        std::string path = prefix + "-" + stage + ".png";
        write_png(path, VMS_RS(g_fb), VMS_RS(g_screen_w), VMS_RS(g_screen_h));
        log_line("REPLAY screenshot stage=" + stage + " path=" + path);
    };
    auto press = [&](int code, const std::string& stage) {
        screenshot(stage);
        log_line("REPLAY press stage=" + stage + " code=" + std::to_string(code));
        held = code;
        held_mask = code == 53 ? 256 : 0; // MIDP GameCanvas FIRE_PRESSED.
        VMS_PLAT(g_key_states) |= held_mask;
        dispatch_key(code, false);
        release_at = monotonic_ms() + 100;
    };
    while (monotonic_ms() < deadline) {
        int64_t tick = monotonic_ms();
        host_events();
        if (VMS_EXEC(g_thread_live)) thread_resume();
        host_events();
        if (VMS_RS(g_repaint)) { VMS_RS(g_repaint) = false; repaint_current(); }
        if (held && monotonic_ms() >= release_at) {
            log_line("REPLAY release code=" + std::to_string(held));
            VMS_PLAT(g_key_states) &= ~held_mask;
            dispatch_key(held, true);
            held = 0;
            held_mask = 0;
        }
        if (robot && !held) {
            if (!music && VMS_RS(g_drawacc).textblob.find("背景音乐") != std::string::npos) {
                music = true; story_at = monotonic_ms() + 2700;
                press(53, "music-on");
            } else if (music && !newgame && monotonic_ms() >= story_at) {
                newgame = true; press(53, "intro-advance");
            }
        }
        if (zxx && !held && !music && VMS_RS(g_drawacc).textblob.find("开启音乐吗") != std::string::npos) {
            music = true;
            press(-6, "music-on");
        }
        if (petking && !held) {
            if (!music && VMS_RS(g_drawacc).textblob.find("是否开启游戏音乐") != std::string::npos) {
                music = true;
                press(-6, "music-on");
            } else if (music && !newgame && VMS_RS(g_drawacc).textblob.find("新游戏") != std::string::npos) {
                newgame = true;
                press(53, "new-game");
            } else if (newgame && story < 8 && monotonic_ms() >= story_at
                    && VMS_RS(g_drawacc).textblob.find("按0键继续") != std::string::npos) {
                story++;
                story_at = monotonic_ms() + 1500;
                press(48, "story-" + std::to_string(story));
            }
        }
        int64_t left = 16 - (monotonic_ms() - tick);
        if (left > 0) std::this_thread::sleep_for(std::chrono::milliseconds(left));
    }
    if (held) {
        VMS_PLAT(g_key_states) &= ~held_mask;
        dispatch_key(held, true);
        log_line("REPLAY final release code=" + std::to_string(held));
    }
    write_png(pngpath, VMS_RS(g_fb), VMS_RS(g_screen_w), VMS_RS(g_screen_h));
    log_line("REPLAY final screenshot=" + pngpath + " elapsed_ms=" + std::to_string(monotonic_ms() - begin) +
             " music=" + (music ? "1" : "0") + " newgame=" + (newgame ? "1" : "0") +
             " story=" + std::to_string(story) + " thread_live=" + (VMS_EXEC(g_thread_live) ? "1" : "0"));
    log_line("REPLAY MEM array_bytes=" + std::to_string(VMS_HEAP(g_array_payload_live)) +
             " array_peak=" + std::to_string(VMS_HEAP(g_array_payload_peak)) + " gc_cycles=" + std::to_string(VMS_HEAP(g_gc_cycles)));
    log_line("REPLAY bounded timeout completed normally; short smoke check only, not a full game validation");
    VMS_PLAT(g_log).flush();
    return 0;
}
