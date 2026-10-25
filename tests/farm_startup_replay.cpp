#define main emulator_main
#include FARM_VM_SOURCE
#undef main

// Optional external-JAR regression. The Python runner builds a copy of the VM
// with short interpreter slices to expose the device startup race on fast hosts.
int main(int argc, char** argv) {
    if (argc != 4) return 2;
    const std::string mode = argv[3];
    const bool baseline = mode.find("baseline") == 0;
    const bool inject_key = mode.find("paint") == std::string::npos;
    VMS_PLAT(g_boot_ms) = now_ms(); VMS_PLAT(g_log).open(argv[2]); VMS_CL(g_classpath) = {"romfs/classes"};
    VMS_DIAG(g_log_draw) = true; VMS_RS(g_fb).assign(VMS_RS(g_screen_w)*VMS_RS(g_screen_h)*3,0);
    std::vector<uint8_t> bytes; std::string error;
    if (!read_whole(argv[1],bytes) || !unzip(bytes,VMS_CL(g_jar_files),error)) return 2;
    load_props();
    const auto identity = rms_jar_identity(bytes.data(),bytes.size());
    launch_class("Midlet",baseline ? "farm-startup-baseline-test" : identity);
    // The displayed splash has no e array. Both input edges must be retained
    // while loading, then delivered to the initialized game in their order.
    if (inject_key) { dispatch_key(53,false); dispatch_key(53,true); }
    if (!baseline && inject_key) {
        // A pause must cancel startup input, while keeping readiness protection.
        release_game_keys();
        if (!VMS_EXEC(g_farm_startup).waiting() || !VMS_EXEC(g_farm_startup).keys.empty()) return 1;
        dispatch_key(53,false); dispatch_key(53,true);
    }
    bool retained_frame = false;
    const int64_t end = monotonic_ms()+20000;
    while (monotonic_ms()<end) {
        // One runnable Java slice per event turn makes the first screen request
        // observable before the new screen's loading worker gets its first turn.
        host_events(); if(VMS_EXEC(g_thread_live))pump_one_thread(); host_events();
        if(VMS_RS(g_repaint)) {
            const auto before = VMS_RS(g_fb);
            const bool deferring = VMS_EXEC(g_farm_startup).waiting() && farm_startup_defer_paint();
            VMS_RS(g_repaint)=false;repaint_current();
            if (deferring) {
                if(VMS_RS(g_fb) != before) return 1;
                retained_frame = true;
            }
        }
        if(VMS_EXEC(g_farm_startup).state==FarmStartup::State::Ready) {
            // Caught RMS-not-found exceptions retain traces until GC; only an
            // unhandled exception is a startup failure.
            if(!VMS_EXEC(g_farm_startup).keys.empty() || VMS_PLAT(g_pressed_keys).count(53) || VMS_EXEC(g_main_thread).unhandled || VMS_EXEC(g_run_depth) || VMS_RS(g_paint_depth)) return 1;
            if (!inject_key && !retained_frame) return 1;
            const int paints = VMS_RS(g_game_frames);
            for(int i=0;i<4;++i) {
                dispatch_key(53,false);dispatch_key(53,true);
                if(VMS_EXEC(g_thread_live))pump_one_thread();
                host_events();repaint_current();
                if(VMS_EXEC(g_farm_startup).state!=FarmStartup::State::Ready || VMS_PLAT(g_pressed_keys).count(53) || VMS_EXEC(g_main_thread).unhandled) return 1;
            }
            if(VMS_RS(g_game_frames)<=paints) return 1;
            if(mode=="after-ready-failure") {
                // Inject a later object-state fault, without changing game code,
                // to prove ordinary Java errors remain visible after readiness.
                auto* settings = find_field(VMS_HEAP(g_heap)[VMS_RS(g_current)].cls,"e","[B");
                VMS_HEAP(g_heap)[VMS_RS(g_current)].fields[settings->slot] = Value::of_ref(0);
                dispatch_key(53,false);
                return 1;
            }
            log_line("PASS HappyFarm startup: short slices, ready resources, repeated FIRE/release, actual paints, preserved transition framebuffer");
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    log_line("FAIL HappyFarm startup readiness watchdog"); return 1;
}
