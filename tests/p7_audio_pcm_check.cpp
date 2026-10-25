// P7.3 deterministic PCM check (test side). Loads every MIDI/WAV/tone resource of a real JAR into the real engine and
// drives a seeded sequence of control calls (through the fenced VMS accessors) interleaved with renders of seeded
// frame counts through audio_render_desktop() - the same function the game pump uses. Run once with
// J2ME_P7_WORKER/J2ME_P7_AUDIO unset (inline) and once set (worker); J2ME_P7_PCM_DUMP files must be byte-identical.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
int main(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "usage: p7_audio_pcm_check jar log steps\n"); return 2; }
    VMS_PLAT(g_boot_ms) = now_ms(); VMS_PLAT(g_log).open(argv[2]); configure_runtime_diagnostics();
    std::vector<uint8_t> jar; std::string error;
    if (!read_whole(argv[1], jar) || !unzip(jar, VMS_CL(g_jar_files), error)) { std::fprintf(stderr, "jar read failed %s\n", error.c_str()); return 3; }
    std::vector<int> ids;
    for (auto& kv : VMS_CL(g_jar_files)) {
        std::string n = kv.first; for (char& c : n) c = (char)std::tolower((unsigned char)c);
        const char* type = n.size() > 4 && (n.ends_with(".mid") || n.ends_with(".midi")) ? "audio/midi" : (n.ends_with(".wav") ? "audio/x-wav" : nullptr);
        if (!type) continue;
        std::string err; int id = VMS_PLAT(g_audio).load(kv.second, type, err);
        std::printf("LOAD %s type=%s id=%d %s\n", kv.first.c_str(), type, id, err.c_str());
        if (id > 0) ids.push_back(id);
    }
    if (ids.empty()) { std::fprintf(stderr, "no audio resources\n"); return 4; }
    uint32_t rng = 12345; auto next = [&] { rng = rng * 1664525u + 1013904223u; return rng >> 8; };
    const int steps = std::atoi(argv[3]); uint64_t frames_total = 0, queries = 0, endhash = 1469598103934665603ull;
    for (int i = 0; i < steps; ++i) {
        const int h = ids[next() % ids.size()];
        switch (next() % 10) {
            case 0: case 1: VMS_PLAT(g_audio).start(h); break;
            case 2: VMS_PLAT(g_audio).stop(h); break;
            case 3: VMS_PLAT(g_audio).volume(h, int(next() % 101)); break;
            case 4: VMS_PLAT(g_audio).seek(h, int64_t(next() % 3000000)); break;
            case 5: VMS_PLAT(g_audio).loop(h, int(next() % 3) == 0 ? -1 : 1 + int(next() % 3)); break;
            case 6: VMS_PLAT(g_audio).playTone(60 + int(next() % 24), 50 + int(next() % 300), 80); break;
            case 7: queries += uint64_t(VMS_PLAT(g_audio).time(h) >= 0) + uint64_t(VMS_PLAT(g_audio).state(h)); break;
            case 8: { int eh = 0; const bool pe = p7_pending_ends(); const bool got = p7_poll_end(eh); // fast-path queries (exact)
                      endhash = (endhash ^ (uint64_t(pe) | uint64_t(got) << 1 | uint64_t(got ? eh : 0) << 2)) * 1099511628211ull; break; }
            default: break; // no engine access: renders accumulate into worker batches
        }
        const size_t frames = 1 + next() % 1500; // 0.02 ms .. 83 ms, like real slice gaps
        audio_render_desktop(frames); frames_total += frames;
    }
    p7::pool().drain(); p7audio::fence();
    if (p7audio::g_dump) std::fflush(p7audio::g_dump);
    std::printf("PCMCHECK steps=%d frames_requested=%llu frames_dumped=%llu jobs=%llu fences=%llu inline_jobs=%llu batches=%llu frames_worker=%llu frames_inline_flush=%llu queries=%llu endhash=%016llx fast_hits=%llu audio=%d\n", steps,
                (unsigned long long)frames_total, (unsigned long long)p7audio::g_dump_frames.load(), (unsigned long long)p7audio::g_jobs,
                (unsigned long long)p7audio::g_fences, (unsigned long long)p7audio::g_inline, (unsigned long long)p7audio::g_batches, (unsigned long long)p7audio::g_frames_worker, (unsigned long long)p7audio::g_frames_inline, (unsigned long long)queries, (unsigned long long)endhash, (unsigned long long)p7audio::g_fast_hits, int(p7audio::g_enabled));
    p7::pool().shutdown(); if (p7audio::g_dump) std::fclose(p7audio::g_dump); VMS_PLAT(g_log).close();
    return frames_total == p7audio::g_dump_frames.load() ? 0 : 5;
}
