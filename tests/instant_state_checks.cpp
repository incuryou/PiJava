// g++ -std=c++17 -fno-exceptions tests/instant_state_checks.cpp build/puff.o -o build/instant_state_checks
// Run with the directory containing InstantStateCanvas/InstantStateChecks/InstantStateSlow classes.
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

static int file_failure = 0, rename_calls = 0;
static int checked_rename(const char* a, const char* b) {
    ++rename_calls;
    const bool commit = std::string(a).size() >= 4 && std::string(a).substr(std::strlen(a) - 4) == ".tmp";
    const bool rollback = std::string(a).find(".previous") != std::string::npos;
    if (file_failure >= 1 && file_failure <= 4 && commit) {
        errno = rename_calls == 1 && file_failure != 4 ? EEXIST : EIO;
        return -1;
    }
    if ((file_failure == 2 && rollback) || (file_failure == 3 && !commit && !rollback)) { errno = EIO; return -1; }
    return std::rename(a, b);
}
static int checked_close(FILE* f) { int result = std::fclose(f); return file_failure == 5 ? EOF : result; }
static size_t checked_write(const void* p, size_t w, size_t n, FILE* f) {
    return file_failure == 6 ? 0 : std::fwrite(p, w, n, f);
}
static int checked_flush(FILE* f) { int result = std::fflush(f); return file_failure == 7 ? EOF : result; }
#define INSTANT_STATE_RENAME checked_rename
#define INSTANT_STATE_CLOSE checked_close
#define INSTANT_STATE_WRITE checked_write
#define INSTANT_STATE_FLUSH checked_flush
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "runtime_stats_checks.h"
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) { return "build/instant-check.bin"; }
#include "../src/instant_state.inc"

static void legacy_write_thread(BinW& w, const SavThread& t) {
    w.i32(t.ref); w.i32(t.state);
    w.u8(t.has_wake); w.i64(t.wake_delta); w.u8(t.has_sample); w.i64(t.sample_delta); w.u8(t.has_wake_us); w.i64(t.wake_us_delta);
    w.i32(t.sleep_request); w.i32(t.monitor); w.i32(t.restore_depth); w.i32(t.join_target); w.i32(t.unhandled); w.i32(t.init_wait);
    w.u8(t.reacquire); w.u8(t.interrupt_exception); w.u8(t.begun);
    w.u32(uint32_t(t.frames.size()));
    for (auto& fr : t.frames) write_frame(w, fr);
}

static bool legacy_encode_state(const SavState& s, std::string& out, int version) {
    BinW w;
    w.b.append(version == 1 ? "J2S1" : "J2S2", 4);
    w.str(s.jar);
    w.u32(uint32_t(s.heap.size()));
    for (auto& o : s.heap) {
        w.str(o.cls); w.vals(o.fields); w.str(o.text); w.str(o.array_desc); w.u8(o.is_array ? 1 : 0); std::vector<SavValue> elems;
        size_t width = o.is_array ? sav_array_width(o.array_desc) : 0;
        for (size_t i = 0; i < o.array_length; ++i) elems.push_back(sav_value(sav_array_value(o.array_data.data() + i * width, o.array_desc, width)));
        w.vals(elems);
        w.raw(o.pixels.data(), o.pixels.size()); w.raw(o.alpha.data(), o.alpha.size()); w.i32(o.width); w.i32(o.height);
    }
    w.u32(uint32_t(s.free_list.size())); for (int r : s.free_list) w.i32(r);
    w.u32(uint32_t(s.classes.size()));
    for (auto& c : s.classes) { w.str(c.name); w.i32(c.init_state); w.i32(c.init_error); w.vals(c.statics); }
    w.u8(s.has_main ? 1 : 0); if (s.has_main) legacy_write_thread(w, s.main);
    w.u32(uint32_t(s.threads.size())); for (auto& t : s.threads) legacy_write_thread(w, t);
    w.u32(uint32_t(s.monitors.size()));
    for (auto& m : s.monitors) { w.i32(m.ref); w.i32(m.owner); w.i32(m.depth); w.u32(uint32_t(m.waiters.size())); for (int id : m.waiters) w.i32(id); }
    w.u32(uint32_t(s.class_objects.size())); for (auto& kv : s.class_objects) { w.str(kv.first); w.i32(kv.second); }
    auto pairs = [&](const std::vector<std::pair<int, int>>& xs) { w.u32(uint32_t(xs.size())); for (auto& kv : xs) { w.i32(kv.first); w.i32(kv.second); } };
    pairs(s.gfx_color); pairs(s.gfx_target); pairs(s.gfx_tx); pairs(s.gfx_ty); pairs(s.offscreen);
    w.u32(uint32_t(s.clips.size()));
    for (auto& c : s.clips) { w.i32(c.ref); w.i32(c.x); w.i32(c.y); w.i32(c.w); w.i32(c.h); w.u8(c.on ? 1 : 0); }
    w.i32(s.midlet); w.i32(s.current); w.i32(s.display); w.i32(s.shown); w.i32(s.alert_return);
    w.u8(s.has_alert); w.i64(s.alert_deadline_delta); w.i64(s.alert_remaining);
    w.str(s.rms); w.raw(s.fb.data(), s.fb.size());
    w.u32(uint32_t(s.serial.size())); for (int r : s.serial) w.i32(r);
    if (version >= 2) {
    w.u32(uint32_t(s.audio.size()));
    for (auto& p : s.audio) {
        w.i32(p.handle); w.i32(p.state); w.i32(p.volume); w.i32(p.loops); w.i32(p.remaining);
        w.i64(int64_t(p.frame)); w.u8(p.midi ? 1 : 0); w.u32(p.pcmRate); w.i32(p.pcmChannels);
        w.raw(reinterpret_cast<const uint8_t*>(p.pcm.data()), p.pcm.size() * sizeof(int16_t));
        w.u32(uint32_t(p.events.size()));
        for (auto& e : p.events) { w.i64(int64_t(e.at)); w.u32(e.order); w.u32(e.tempo); w.u8(e.status); w.u8(e.a); w.u8(e.b); }
    }
    }
    out.swap(w.b); return true;
}

static void clean_file(const std::string& p) {
    for (const auto& x : {p, p + ".tmp", p + ".previous"}) std::remove(x.c_str());
}
static void file_checks() {
    const std::string path = "build/instant-file-check.bin";
    std::string bytes;
    for (int failure = 1; failure <= 7; ++failure) {
        file_failure = 0; rename_calls = 0; clean_file(path);
        assert(instant_state_file::save(path, "old complete snapshot"));
        file_failure = failure; rename_calls = 0;
        assert(!instant_state_file::save(path, "replacement snapshot"));
        file_failure = 0;
        assert(instant_state_file::load(path, bytes) && bytes == "old complete snapshot");
        assert(instant_state_file::status(path + ".tmp") == instant_state_file::Status::missing);
        // The second save repairs an interrupted journal before replacing it.
        assert(instant_state_file::save(path, "new complete snapshot"));
        assert(instant_state_file::load(path, bytes) && bytes == "new complete snapshot");
    }
    assert(std::rename(path.c_str(), (path + ".previous").c_str()) == 0);
    assert(instant_state_file::load(path, bytes) && bytes == "new complete snapshot");
    { std::ofstream bad(path); bad << "invalid primary"; }
    assert(instant_state_file::load(path, bytes) && bytes == "invalid primary");
    clean_file(path);
    const std::string deletion = instant_state_path("fixture.jar");
    clean_file(deletion);
    assert(instant_state_file::save(deletion, "complete snapshot"));
    // An unreadable recovery record must prevent deleting the primary.
    assert(mkdir((deletion + ".previous").c_str(), 0700) == 0);
    assert(!delete_instant_state("fixture.jar"));
    assert(instant_state_file::load(deletion, bytes) && bytes == "complete snapshot");
    assert(rmdir((deletion + ".previous").c_str()) == 0);
    assert(delete_instant_state("fixture.jar"));
    assert(instant_state_file::status(deletion) == instant_state_file::Status::missing);
}
static void canvas_fixture() {
    stop_thread(); VMS_CL(g_classes).clear(); VMS_CL(g_class_index).clear(); VMS_HEAP(g_heap).assign(1, Obj{}); VMS_HEAP(g_free).clear();
    VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w)) * VMS_RS(g_screen_h) * 3, 0);
    int c = ensure_class("InstantStateCanvas");
    VMS_RS(g_current) = alloc_obj(c);
    run(find_method(c, "<init>", "()V", false), {Value::of_ref(VMS_RS(g_current))});
    VMS_EXEC(g_midlet) = alloc_obj(ensure_class("java/lang/Object"));
    VMS_EXEC(g_midlet_state) = MidletState::Active;
}
static void init_checks(int version) {
    canvas_fixture();
    int c = ensure_class("InstantStateChecks");
    run(find_method(c, "setup", "()I", false), {});
    thread_resume();
    int before = VMS_CL(g_class_index).at("InstantStateSlow");
    assert(VMS_CL(g_classes)[before].init_state == 1 && VMS_CL(g_classes)[before].init_owner);
    SavState saved; assert(capture_state(saved, "fixture.jar"));
    saved.version=version;
    std::string bytes;
    assert(version >= 3 ? encode_state(saved, bytes) : legacy_encode_state(saved, bytes, version));
    SavState decoded; assert(decode_state(bytes, decoded));
    assert(decoded.version == version);
    SavState invalid = decoded;
    invalid.threads[0].frames[0].pc = INT32_MAX;
    const int old_root = VMS_EXEC(g_midlet); const size_t old_heap = VMS_HEAP(g_heap).size();
    assert(!apply_state(invalid) && VMS_EXEC(g_midlet) == old_root && VMS_HEAP(g_heap).size() == old_heap);
    seed_runtime_stats();
    const auto before_restore_generation=VMS_EXEC(g_runtime_profile).generation;
    assert(apply_state(decoded));
    assert_runtime_stats_reset();
    assert(VMS_EXEC(g_runtime_profile).generation>before_restore_generation);
    int after = VMS_CL(g_class_index).at("InstantStateSlow");
    assert(VMS_CL(g_classes)[after].init_state == 1 && VMS_CL(g_classes)[after].init_owner);
    bool waiter = false;
    for (auto& t : VMS_EXEC(g_threads)) {
        if (t.state == ThreadState::Initializing) { assert(t.init_wait == after); waiter = true; }
        if (t.state == ThreadState::Sleeping) { t.wake = monotonic_ms(); t.wake_us = 0; }
    }
    assert(waiter);
    for (int i = 0; i < 6; ++i) thread_resume();
    assert(VMS_CL(g_classes)[after].init_state == 2 && !VMS_CL(g_classes)[after].init_owner);
    for (auto& t : VMS_EXEC(g_threads)) assert(t.state != ThreadState::Initializing);
    auto* finished = find_field(VMS_CL(g_class_index).at("InstantStateChecks"), "finished", "I");
    assert(finished && VMS_CL(g_classes)[finished->owner].statics[finished->slot].i == 2);
    // An old file without a uniquely identifiable initializer must be rejected.
    if (version < 3) {
        for (auto& t : decoded.threads) for (auto& f : t.frames) if (f.init_class == "InstantStateSlow") f.init_class.clear();
        int root = VMS_EXEC(g_midlet); size_t heap = VMS_HEAP(g_heap).size();
        assert(!apply_state(decoded) && VMS_EXEC(g_midlet) == root && VMS_HEAP(g_heap).size() == heap);
    }
}
static void array_checks() {
    canvas_fixture();
    const std::vector<std::string> descs = {"[Z", "[B", "[C", "[S", "[I", "[F", "[J", "[D", "[Ljava/lang/Object;", "[[I"};
    std::vector<int> refs;
    std::vector<std::vector<Value>> expected;
    for (auto& desc : descs) {
        int r = alloc_array(desc, 7); refs.push_back(r);
        std::vector<Value> values;
        for (size_t i = 0; i < 7; ++i) {
            Value v = sav_array_refs(desc) ? Value::of_ref(i % 2 ? VMS_EXEC(g_midlet) : 0) :
                sav_array_width(desc) == 8 ? Value::of_l(int64_t(UINT64_C(0xfff1234500000000) + i)) : Value::of_i(int32_t(0xfff12345u + uint32_t(i)));
            VMS_HEAP(g_heap)[r].elems.set(i, v); values.push_back(VMS_HEAP(g_heap)[r].elems.get(i));
        }
        expected.push_back(values);
    }
    VMS_RS(g_shown)=VMS_RS(g_current);
    dispatch_key(53,false);
    assert(game_canvas_state(VMS_RS(g_current),"keyHeld")->i == (1 << 8));
    int large = alloc_array("[B", 262144); VMS_HEAP(g_heap)[large].elems.set(262143, Value::of_i(-91));
    SavState saved; assert(capture_state(saved, "fixture.jar"));
    assert(saved.heap[large].elems.empty() && saved.heap[large].array_data.size() == 262144);
    // The 0.2.17 GameCanvas/Vector additions move subclass field slots. A
    // schema-less old state must be refused, rather than assigning by index.
    int gc = ensure_class("javax/microedition/lcdui/game/GameCanvas");
    if (Field* added = find_field(gc, "suppressKeyEvents", "Z")) {
        SavState old_layout = saved; old_layout.version = 2;
        old_layout.heap[VMS_RS(g_current)].fields.erase(old_layout.heap[VMS_RS(g_current)].fields.begin() + added->slot);
        size_t size = VMS_HEAP(g_heap).size(); int root = VMS_EXEC(g_midlet);
        assert(!apply_state(old_layout) && VMS_EXEC(g_midlet) == root && VMS_HEAP(g_heap).size() == size);
    }
    int vector = ensure_class("java/util/Vector");
    if (Field* added = find_field(vector, "capacityIncrement", "I")) {
        int object = alloc_obj(vector); SavState old_layout;
        assert(capture_state(old_layout, "fixture.jar")); old_layout.version = 1;
        old_layout.heap[object].fields.erase(old_layout.heap[object].fields.begin() + added->slot);
        size_t size = VMS_HEAP(g_heap).size(); int root = VMS_EXEC(g_midlet);
        assert(!apply_state(old_layout) && VMS_EXEC(g_midlet) == root && VMS_HEAP(g_heap).size() == size);
    }
    for (int version : {1, 2, 3, 4}) {
        saved.version=version;
        std::string bytes;
        assert(version >= 3 ? encode_state(saved, bytes) : legacy_encode_state(saved, bytes, version));
        if (version == 3) {
            size_t payload = saved.fb.size();
            for (const auto& object : saved.heap)
                payload += object.array_data.size() + object.pixels.size() + object.alpha.size();
            // GameCanvas now allocates its real backing Image in the ctor.
            // Count that image separately from compact array payloads.
            assert(bytes.size() >= payload && bytes.size() < payload + 32768);
        }
        SavState decoded; assert(decode_state(bytes, decoded)); assert(apply_state(decoded));
        assert(game_canvas_state(VMS_RS(g_current),"keyHeld")->i == 0 && game_canvas_state(VMS_RS(g_current),"keyLatched")->i == 0);
        for (size_t a = 0; a < refs.size(); ++a) for (size_t i = 0; i < 7; ++i) {
            Value got = VMS_HEAP(g_heap)[refs[a]].elems.get(i), want = expected[a][i];
            assert(got.i == want.i && got.l == want.l && got.ref == want.ref && got.is_long == want.is_long);
        }
        assert(VMS_HEAP(g_heap)[large].elems.get(262143).i == -91);
    }
    std::string bytes; assert(encode_state(saved, bytes));
    // Truncation at each format section, trailing junk and a hostile heap count.
    for (size_t n = 0; n < bytes.size(); n += std::max(size_t(1), bytes.size() / 100)) {
        SavState bad; assert(!decode_state(bytes.substr(0, n), bad));
    }
    SavState bad; assert(!decode_state(bytes + "junk", bad));
    std::string hostile = bytes;
    size_t heap_count = 8 + saved.jar.size() + (saved.version>=4?16:0);
    for (size_t i = 0; i < 4; ++i) hostile[heap_count + i] = char(255);
    assert(!decode_state(hostile, bad));
    hostile=bytes;
    const size_t clock_offset=8+saved.jar.size();
    for(size_t i=0;i<8;++i)hostile[clock_offset+i]=char(255);
    assert(!decode_state(hostile,bad));
    const int root = VMS_EXEC(g_midlet); const size_t heap_size = VMS_HEAP(g_heap).size();
    auto reject = [&](SavState& malformed) { assert(!apply_state(malformed)); assert(VMS_EXEC(g_midlet) == root && VMS_HEAP(g_heap).size() == heap_size); };
    bad = saved; bad.current = int(saved.heap.size()); reject(bad);
    bad = saved; bad.game_millis=INT64_MAX; reject(bad);assert(!encode_state(bad,bytes));
    bad = saved; bad.clock_scale=17; reject(bad);assert(!encode_state(bad,bytes));
    bad = saved; bad.clock_scale=NAN; reject(bad);assert(!encode_state(bad,bytes));
    bad = saved; bad.heap[large].array_data.pop_back(); reject(bad);
    bad = saved; bad.heap[large].array_desc = "[Q"; reject(bad);
    bad = saved; bad.free_list = {root}; reject(bad);
    bad = saved; bad.main.state = 99; reject(bad);
    bad = saved; bad.heap[root].fields.push_back(SavValue{}); reject(bad);
    bad.version = 1; reject(bad);
    bad = saved; bad.classes[0].instance_schema += "different slot identity"; reject(bad);
    bad = saved; bad.classes[0].init_state = 1; bad.classes[0].init_owner = 999; reject(bad);
    bad = saved; bad.classes[0].name = "MissingStateClass"; reject(bad);
    // Preparing a previously unseen class with a ConstantValue string must
    // neither allocate live objects nor leave metadata behind after rejection.
    const size_t class_count = VMS_CL(g_classes).size();
    const int alloc_count = VMS_HEAP(g_alloc_since);
    assert(!VMS_CL(g_class_index).count("InstantStateConstant"));
    bad = saved; bad.classes[0].name = "InstantStateConstant"; reject(bad);
    assert(VMS_CL(g_classes).size() == class_count && VMS_HEAP(g_alloc_since) == alloc_count);
    assert(!VMS_CL(g_class_index).count("InstantStateConstant"));
    // The on-disk jar identity is checked before applying it.
    assert(instant_state_file::save(instant_state_path("other.jar"), bytes));
    assert(!restore_instant_state("other.jar") && VMS_EXEC(g_midlet) == root && VMS_HEAP(g_heap).size() == heap_size);
    clean_file(instant_state_path("other.jar"));
    // Same path with changed JAR bytes must not apply old object/field state.
    const std::string jar = "build/instant-identity-fixture.jar";
    { std::ofstream file(jar, std::ios::binary); file << "original jar contents"; }
    std::vector<uint8_t> jar_bytes; assert(read_whole(jar, jar_bytes));
    saved.jar = jar; saved.rms = rms_jar_identity(jar_bytes.data(), jar_bytes.size());
    assert(encode_state(saved, bytes));
    assert(instant_state_file::save(instant_state_path(jar), bytes));
    { std::ofstream file(jar, std::ios::binary); file << "changed jar contents"; }
    assert(!restore_instant_state(jar) && VMS_EXEC(g_midlet) == root && VMS_HEAP(g_heap).size() == heap_size);
    { std::ofstream file(jar, std::ios::binary); file << "original jar contents"; }
    assert(restore_instant_state(jar));
    assert(std::rename(instant_state_path(jar).c_str(), (instant_state_path(jar) + ".previous").c_str()) == 0);
    assert(restore_instant_state(jar));
    assert(delete_instant_state(jar));
    assert(instant_state_file::status(instant_state_path(jar) + ".previous") == instant_state_file::Status::missing);
    std::remove(jar.c_str());
    std::cout << "PASS compact snapshot bytes=" << bytes.size() << " byte-array-copy=" << saved.heap[large].array_data.size() << "\n";
}
int main(int argc, char** argv) {
    assert(argc == 2);
    VMS_CL(g_classpath) = {argv[1], "romfs/classes"}; VMS_DIAG(g_log_draw) = false;
    file_checks();
    init_checks(4); init_checks(3); init_checks(2); init_checks(1);
    array_checks();
    std::cout << "PASS instant state initialization, legacy migration, all array kinds, corruption and commit recovery\n";
}
