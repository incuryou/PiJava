// Desktop diagnostic frontend; includes no Horizon headers.
int main(int argc, char** argv) {
    VMS_PLAT(g_boot_ms) = now_ms();
    std::string classpath, main_class = "Hello", out = "title.png", log_path = "j2me-vm.log", jar, rms_identity;
    std::string test_profile; if(const char* p=std::getenv("J2ME_TEST_PROFILE"))test_profile=p;
    bool import_legacy = false, explicit_size=false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--trace") { std::cerr << "--trace is not supported; use --log for runtime diagnostics\n"; return 2; }
        else if (a == "--test-profile" && i+1<argc)test_profile=argv[++i];
        else if (a == "--test-profile") {std::cerr<<"Missing --test-profile path\n";return 2;}
        else if (a == "--import-legacy-rms") import_legacy = true;
        else if (a == "--cp" && i + 1 < argc) classpath = argv[++i];
        else if (a == "--log" && i + 1 < argc) log_path = argv[++i];
        else if (a == "-o" && i + 1 < argc) out = argv[++i];
        else if (a == "--jar" && i + 1 < argc) jar = argv[++i];
        else if (a == "--size") {
            int w=0,h=0;
            if(i+1>=argc||!logical_screen::parse(argv[++i],w,h)) {std::cerr<<"Invalid --size; use WIDTHxHEIGHT, each 16..640\n";return 2;}
            VMS_RS(g_screen_w)=w;VMS_RS(g_screen_h)=h;explicit_size=true;
        } else main_class = a;
    }
    if(!r4_configure(test_profile))return 2;
    if(r4::state.active&&import_legacy){std::cerr<<"R4 forbids RMS import\n";r4::finish("REJECTED","RMS_IMPORT");return 2;}
    if (classpath.empty() && jar.empty()) die("usage");
    if (!jar.empty()) {
        std::vector<uint8_t> bytes; if (!read_whole(jar, bytes)) die("cannot read jar");
        rms_identity = rms_jar_identity(bytes.data(), bytes.size());
        std::string err; if (!unzip(bytes, VMS_CL(g_jar_files), err)) die(err);
        load_props();
        load_sidecar_jad(jar);
        main_class = manifest_midlet(); if (main_class.empty()) die("no MIDlet-1");
        for (char& c : main_class) if (c == '.') c = '/';
    }
    log_open(log_path.c_str(), true);
    configure_runtime_diagnostics();
    if(!explicit_size)select_screen_sidecar(jar,rms_identity);
    if (!classpath.empty()) VMS_CL(g_classpath).push_back(classpath);
    VMS_RS(g_fb).assign((size_t)VMS_RS(g_screen_w) * VMS_RS(g_screen_h) * 3, 0);
    log_line(std::string("BOOT ") + kVer + " " + main_class);
    launch_class(main_class, rms_identity, import_legacy);
    if(r4::state.active){r4_run_loop();p7::pool().shutdown();return 0;}
    const int64_t diagnostic_started_us=paint_clock_us();
    if (const char* ks = std::getenv("J2ME_KEYS")) {
        int code = 0; bool num = false;
        for (const char* p = ks;; ++p) {
            if (*p >= '0' && *p <= '9') { code = code * 10 + (*p - '0'); num = true; }
            else {
                if (num) { if (code >= 0 && code < 31) VMS_PLAT(g_key_states) |= 1 << code; dispatch_key(code, false); }
                code = 0; num = false;
                if (*p == 0) break;
            }
        }
    }
    if (const char* env = std::getenv("J2ME_FRAMES")) {
        int n = std::atoi(env);
        for (int i = 0; i < n; ++i) {
            int64_t begin = monotonic_ms();
            if (!sched_legacy()) {
                // P2.6: same scheduler core as Switch; this frontend supplies only sleeps and a 16 ms "vsync".
                host_lifecycle_poll242();
                sched_game_work(false, [] {}, [](int64_t ms) { { P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::milliseconds(ms)); } });
                sched_present_turn(true, false, false, 3, [&] {
                    int64_t left = 16 - (monotonic_ms() - begin);
                    if (left > 0) { P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::milliseconds(left)); }
                });
                continue;
            }
            host_events();
            if (VMS_EXEC(g_thread_live)) thread_resume();
            host_events();
            if (VMS_RS(g_repaint)) { VMS_RS(g_repaint) = false; repaint_current(); }
            int64_t left = 16 - (monotonic_ms() - begin);
            if (left > 0) { P7_SCOPE(Idle); std::this_thread::sleep_for(std::chrono::milliseconds(left)); }
        }
    }
    log_line("MEM array_bytes=" + std::to_string(VMS_HEAP(g_array_payload_live)) + " array_peak=" + std::to_string(VMS_HEAP(g_array_payload_peak)) +
             " gc_cycles=" + std::to_string(VMS_HEAP(g_gc_cycles)) + " gc_array_refs_scanned=" + std::to_string(VMS_HEAP(g_gc_array_refs_scanned)));
    report_optimization_window(paint_clock_us()-diagnostic_started_us,VMS_EXEC(g_runtime_profile).take());
    write_png(out, VMS_RS(g_fb), VMS_RS(g_screen_w), VMS_RS(g_screen_h));
    lifecycle_destroy();
    p7::pool().shutdown(); return 0;
}
