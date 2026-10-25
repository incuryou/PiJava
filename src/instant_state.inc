// c5: instant_state.inc is compiled by the Switch host AND by desktop test harnesses that supply their own
// host globals (g_menu_settings, g_launched_path). Switch-only platform state goes through J2ME_ISP().
#if defined(__SWITCH__)
#define J2ME_ISP(n) VMS_PLAT(n)
#define J2ME_ISP_DECL_LAUNCHED_PATH  VMS_PLAT_ACCESSOR(g_launched_path, std::string)
#else
#define J2ME_ISP(n) n
#define J2ME_ISP_DECL_LAUNCHED_PATH static std::string g_launched_path;
#endif
#include "instant_state_file.h"

// Instant state. Host bezel, FPS and speed stay outside this file and keep working after restore.
struct SavValue { int32_t i = 0, ref = 0; int64_t l = 0; uint8_t is_long = 0; };
struct SavFrame {
    std::string cls, name, desc, init_class;
    bool stub = false, sync_acquired = false, entry_checked = false, return_object = false, init_super_pending = false;
    int pc = 0, op_pc = 0, sync_monitor = 0;
    SavValue constructed;
    std::vector<SavValue> locals, stack;
};
struct SavThread {
    int ref = 0, sleep_request = 0, monitor = 0, restore_depth = 0, join_target = 0, unhandled = 0, init_wait = -1;
    std::string init_wait_class;
    int state = 0;
    int64_t wake_delta = 0, sample_delta = 0, wake_us_delta = 0;
    bool has_wake = false, has_sample = false, has_wake_us = false;
    bool reacquire = false, interrupt_exception = false, begun = false;
    std::vector<SavFrame> frames;
};
struct SavObj {
    std::string cls, text, array_desc;
    bool is_array = false;
    std::vector<SavValue> fields, elems; // elems is used only for legacy input.
    std::vector<uint8_t> array_data;
    uint32_t array_length = 0;
    std::vector<uint8_t> pixels, alpha;
    int width = 0, height = 0;
};
struct SavClass { std::string name, instance_schema, static_schema; int init_state = 0, init_error = 0, init_owner = -1; std::vector<SavValue> statics; };
struct SavMonitor { int ref = 0, owner = -1, depth = 0; std::vector<int> waiters; };
struct SavState {
    int version = 5;  // J2S5 (P2.1-c4): deterministic raw bytes; J2S1..J2S4 are rejected
    int64_t game_millis = 0;
    double clock_scale = 1.0;
    std::string jar;
    std::vector<SavObj> heap;
    std::vector<int> free_list;
    std::vector<SavClass> classes;
    std::vector<SavThread> threads;
    SavThread main;
    bool has_main = false;
    std::vector<SavMonitor> monitors;
    std::vector<std::pair<std::string, int>> class_objects;
    std::vector<std::pair<int, int>> gfx_color, gfx_target, gfx_tx, gfx_ty, offscreen;
    struct SavClip { int ref, x, y, w, h; bool on; };
    std::vector<SavClip> clips;
    int midlet = 0, current = 0, display = 0, shown = 0, alert_return = 0;
    int64_t alert_deadline_delta = 0, alert_remaining = -1;
    bool has_alert = false;
    std::string rms;
    std::vector<uint8_t> fb;
    std::vector<int> serial;
    std::vector<j2me_audio::Engine::SavedPlayer> audio;
};

static SavValue sav_value(const Value& v) { return {v.i, v.ref, v.l, uint8_t(v.is_long ? 1 : 0)}; }
static Value unsav_value(const SavValue& v) { Value x; x.i = v.i; x.ref = v.ref; x.l = v.l; x.is_long = v.is_long != 0; return x; }

struct BinW {
    std::string b;
    void u8(uint8_t v) { b.push_back(char(v)); }
    void u32(uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(char((v >> (8 * i)) & 255)); }
    void i32(int32_t v) { u32(uint32_t(v)); }
    void i64(int64_t v) { uint64_t u = uint64_t(v); for (int i = 0; i < 8; ++i) b.push_back(char((u >> (8 * i)) & 255)); }
    void str(const std::string& s) { u32(uint32_t(s.size())); b.append(s); }
    void raw(const uint8_t* p, size_t n) { u32(uint32_t(n)); if (n) b.append(reinterpret_cast<const char*>(p), n); }
    void val(const SavValue& v) { i32(v.i); i32(v.ref); i64(v.l); u8(v.is_long); }
    void vals(const std::vector<SavValue>& xs) { u32(uint32_t(xs.size())); for (auto& v : xs) val(v); }
};
struct BinR {
    const uint8_t* p; const uint8_t* e; bool ok = true;
    bool need(size_t n) { if (!ok || n > size_t(e - p)) { ok = false; return false; } return true; }
    uint8_t u8() { if (!need(1)) return 0; return *p++; }
    uint32_t u32() { if (!need(4)) return 0; uint32_t v = 0; for (int i = 0; i < 4; ++i) v |= uint32_t(p[i]) << (8 * i); p += 4; return v; }
    int32_t i32() { return int32_t(u32()); }
    int64_t i64() { if (!need(8)) return 0; uint64_t v = 0; for (int i = 0; i < 8; ++i) v |= uint64_t(p[i]) << (8 * i); p += 8; return int64_t(v); }
    std::string str() { uint32_t n = u32(); if (!need(n)) return {}; std::string s(reinterpret_cast<const char*>(p), n); p += n; return s; }
    std::vector<uint8_t> raw() { uint32_t n = u32(); if (!need(n)) return {}; std::vector<uint8_t> s(p, p + n); p += n; return s; }
    uint32_t count(uint32_t limit, size_t minimum) {
        uint32_t n = u32();
        if (n > limit || (minimum && n > size_t(e - p) / minimum)) { ok = false; return 0; }
        return n;
    }
    SavValue val() { SavValue v; v.i = i32(); v.ref = i32(); v.l = i64(); v.is_long = u8(); if (v.is_long > 1) ok = false; return v; }
    std::vector<SavValue> vals() { uint32_t n = u32(); std::vector<SavValue> xs; if (n > 10000000 || !need(size_t(n) * 17)) { ok = false; return xs; } xs.reserve(n); for (uint32_t i = 0; i < n && ok; ++i) xs.push_back(val()); return xs; }
};

// Stable slot identities catch superclass additions and same-sized field
// reorderings before any live objects are replaced.
static std::string sav_class_schema(const Class& cls, bool statics) {
    std::vector<std::string> slots(statics ? cls.statics.size() : size_t(cls.instance_fields));
    const Class* c = &cls;
    while (c) {
        for (auto& f : c->fields) if (f.is_static == statics && f.slot >= 0 && size_t(f.slot) < slots.size()) {
            BinW key; key.str(c->name); key.str(f.name); key.str(f.desc);
            slots[f.slot] = std::move(key.b);
        }
        if (statics || c->super_id < 0) break;
        c = &VMS_CL(g_classes)[c->super_id];
    }
    BinW schema; schema.u32(uint32_t(slots.size()));
    for (auto& key : slots) schema.str(key);
    return schema.b;
}
static int sav_thread_index(JavaThreadContext* t) {
    if (!t) return -1;
    if (t == &VMS_EXEC(g_main_thread)) return 0;
    int i = 1;
    for (auto& th : VMS_EXEC(g_threads)) { if (&th == t) return i; ++i; }
    return -1;
}
static SavFrame sav_frame(const Frame& fr) {
    SavFrame s;
    if (fr.method) {
        s.cls = VMS_CL(g_classes)[fr.method->class_id].name;
        s.name = fr.method->name; s.desc = fr.method->desc;
        s.stub = fr.method == &VMS_CL(g_classes)[fr.method->class_id].init_stub;
    }
    s.pc = fr.pc; s.op_pc = fr.op_pc; s.sync_monitor = fr.sync_monitor;
    if (fr.init_class >= 0 && fr.init_class < (int)VMS_CL(g_classes).size()) s.init_class = VMS_CL(g_classes)[fr.init_class].name;
    s.sync_acquired = fr.sync_acquired; s.entry_checked = fr.entry_checked;
    s.return_object = fr.return_object; s.init_super_pending = fr.init_super_pending;
    s.constructed = sav_value(fr.constructed);
    for (auto& v : fr.locals) s.locals.push_back(sav_value(v));
    for (auto& v : fr.stack) s.stack.push_back(sav_value(v));
    return s;
}
static bool save_clock_frozen() { const auto& c = VMS_EXEC(g_save_clock); return c.valid && c.exec_epoch == c.anchor_epoch; }
static int64_t save_now_ms() { return save_clock_frozen() ? VMS_EXEC(g_save_clock).ms : monotonic_ms(); }
static int64_t save_now_us() { return save_clock_frozen() ? VMS_EXEC(g_save_clock).us : paint_clock_us(); }
static SavThread sav_thread(const JavaThreadContext& t) {
    SavThread s;
    s.ref = t.ref; s.state = int(t.state);
    const int64_t now = save_now_ms();
    const int64_t now_us = save_now_us();
    // J2S5: a delta is only meaningful when its flag is set; unset deadlines are written as 0 (no "now" leak).
    s.has_wake = t.wake != 0; s.wake_delta = s.has_wake ? t.wake - now : 0;
    s.has_sample = t.sleep_sample_due != 0; s.sample_delta = s.has_sample ? t.sleep_sample_due - now : 0;
    s.has_wake_us = t.wake_us != 0; s.wake_us_delta = s.has_wake_us ? t.wake_us - now_us : 0;
    s.sleep_request = t.sleep_request; s.monitor = t.monitor; s.restore_depth = t.restore_depth;
    s.join_target = t.join_target; s.unhandled = t.unhandled; s.init_wait = t.init_wait;
    if (t.init_wait >= 0 && t.init_wait < (int)VMS_CL(g_classes).size()) s.init_wait_class = VMS_CL(g_classes)[t.init_wait].name;
    s.reacquire = t.reacquire; s.interrupt_exception = t.interrupt_exception; s.begun = t.begun;
    for (auto& fr : t.frames) s.frames.push_back(sav_frame(fr));
    return s;
}
static void write_frame(BinW& w, const SavFrame& fr) {
    w.str(fr.cls); w.str(fr.name); w.str(fr.desc); w.u8(fr.stub ? 1 : 0);
    w.i32(fr.pc); w.i32(fr.op_pc); w.i32(fr.sync_monitor); w.str(fr.init_class);
    w.u8(fr.sync_acquired); w.u8(fr.entry_checked); w.u8(fr.return_object); w.u8(fr.init_super_pending);
    w.val(fr.constructed); w.vals(fr.locals); w.vals(fr.stack);
}
static void write_thread(BinW& w, const SavThread& t) {
    w.i32(t.ref); w.i32(t.state);
    w.u8(t.has_wake); w.i64(t.wake_delta); w.u8(t.has_sample); w.i64(t.sample_delta); w.u8(t.has_wake_us); w.i64(t.wake_us_delta);
    w.i32(t.sleep_request); w.i32(t.monitor); w.i32(t.restore_depth); w.i32(t.join_target); w.i32(t.unhandled); w.str(t.init_wait_class);
    w.u8(t.reacquire); w.u8(t.interrupt_exception); w.u8(t.begun);
    w.u32(uint32_t(t.frames.size()));
    for (auto& fr : t.frames) write_frame(w, fr);
}
static SavFrame read_frame(BinR& r) {
    SavFrame fr;
    fr.cls = r.str(); fr.name = r.str(); fr.desc = r.str(); fr.stub = r.u8() != 0;
    fr.pc = r.i32(); fr.op_pc = r.i32(); fr.sync_monitor = r.i32(); fr.init_class = r.str();
    fr.sync_acquired = r.u8(); fr.entry_checked = r.u8(); fr.return_object = r.u8(); fr.init_super_pending = r.u8();
    fr.constructed = r.val(); fr.locals = r.vals(); fr.stack = r.vals();
    return fr;
}
static SavThread read_thread(BinR& r, int version) {
    SavThread t;
    t.ref = r.i32(); t.state = r.i32();
    t.has_wake = r.u8(); t.wake_delta = r.i64(); t.has_sample = r.u8(); t.sample_delta = r.i64(); t.has_wake_us = r.u8(); t.wake_us_delta = r.i64();
    t.sleep_request = r.i32(); t.monitor = r.i32(); t.restore_depth = r.i32(); t.join_target = r.i32(); t.unhandled = r.i32();
    if (version >= 3) t.init_wait_class = r.str(); else t.init_wait = r.i32();
    t.reacquire = r.u8(); t.interrupt_exception = r.u8(); t.begun = r.u8();
    uint32_t n = r.count(100000, 43);
    for (uint32_t i = 0; i < n && r.ok; ++i) t.frames.push_back(read_frame(r));
    return t;
}

static size_t sav_array_width(const std::string& desc) {
    ArrayStorage check;
    if (!check.reset(desc, 0)) return 0;
    if (desc[1] == '[' || desc[1] == 'L') return 4;
    switch (desc[1]) {
    case 'B': case 'Z': return 1;
    case 'S': case 'C': return 2;
    case 'I': case 'F': return 4;
    case 'J': case 'D': return 8;
    default: return 0;
    }
}
static bool sav_array_refs(const std::string& desc) { return desc.size() > 1 && (desc[1] == '[' || desc[1] == 'L'); }
static void sav_array_append(std::vector<uint8_t>& bytes, const Value& v, const std::string& desc, size_t width) {
    uint64_t bits = sav_array_refs(desc) ? uint32_t(v.ref) : width == 8 ? uint64_t(v.l) : uint32_t(v.i);
    if (desc == "[Z") bits &= 1;
    for (size_t k = 0; k < width; ++k) bytes.push_back(uint8_t(bits >> (8 * k)));
}
static Value sav_array_value(const uint8_t* bytes, const std::string& desc, size_t width) {
    uint64_t bits = 0;
    for (size_t k = 0; k < width; ++k) bits |= uint64_t(bytes[k]) << (8 * k);
    if (sav_array_refs(desc)) return Value::of_ref(int32_t(bits));
    if (width == 8) return Value::of_l(int64_t(bits));
    return Value::of_i(int32_t(bits)); // ArrayStorage.set handles Java narrowing.
}

static bool capture_state(SavState& s, const std::string& jar) {
    if(VMS_RS(g_current)>0&&size_t(VMS_RS(g_current))<VMS_HEAP(g_heap).size()) {
        ScreenMeta m=screen_meta(VMS_HEAP(g_heap)[VMS_RS(g_current)].text);
        if(m.w!=VMS_RS(g_screen_w)||m.h!=VMS_RS(g_screen_h))m.notified=false;
        m.w=VMS_RS(g_screen_w);m.h=VMS_RS(g_screen_h);screen_meta_set(VMS_RS(g_current),m);
    }
    s = SavState{};
    s.jar = jar;
    speed_sync_clock();
    s.game_millis = speed_period_us() || VMS_EXEC(g_game_clock_inherited)?VMS_EXEC(g_game_clock).value_us(save_now_us())/1000:now_ms();
    s.clock_scale = speed_period_us()?VMS_EXEC(g_game_clock).scale:1.0;
    s.heap.resize(VMS_HEAP(g_heap).size());
    for (size_t i = 0; i < VMS_HEAP(g_heap).size(); ++i) {
        const Obj& o = VMS_HEAP(g_heap)[i];
        SavObj& d = s.heap[i];
        if (o.cls >= 0 && o.cls < (int)VMS_CL(g_classes).size()) d.cls = VMS_CL(g_classes)[o.cls].name;
        for (auto& v : o.fields) d.fields.push_back(sav_value(v));
        d.text = o.text; d.array_desc = o.array_desc; d.is_array = o.is_array;
        if (o.is_array) {
            d.array_length = uint32_t(o.elems.size());
            const size_t width = sav_array_width(o.array_desc);
            if (!width) return false;
            d.array_data.reserve(o.elems.payload_bytes());
            for (size_t k = 0; k < o.elems.size(); ++k) sav_array_append(d.array_data, o.elems.get(k), o.array_desc, width);
        }
        d.pixels = o.pixels; d.alpha = o.alpha; d.width = o.width; d.height = o.height;
    }
    s.free_list = VMS_HEAP(g_free);
    for (auto& c : VMS_CL(g_classes)) {
        SavClass sc; sc.name = c.name; sc.instance_schema = sav_class_schema(c, false); sc.static_schema = sav_class_schema(c, true); sc.init_state = c.init_state; sc.init_error = c.init_error; sc.init_owner = sav_thread_index(static_cast<JavaThreadContext*>(c.init_owner));
        for (auto& v : c.statics) sc.statics.push_back(sav_value(v));
        s.classes.push_back(std::move(sc));
    }
    // J2S5: classes are written sorted by name (restore re-loads them in a different order; apply matches by name).
    std::sort(s.classes.begin(), s.classes.end(), [](const SavClass& x, const SavClass& y) { return x.name < y.name; });
    s.has_main = true; s.main = sav_thread(VMS_EXEC(g_main_thread));
    for (auto& t : VMS_EXEC(g_threads)) s.threads.push_back(sav_thread(t));
    for (auto& kv : VMS_EXEC(g_monitors)) {
        SavMonitor m; m.ref = kv.first; m.owner = sav_thread_index(kv.second.owner); m.depth = kv.second.depth;
        for (auto* w : kv.second.waiters) m.waiters.push_back(sav_thread_index(w));
        s.monitors.push_back(std::move(m));
    }
    // J2S5: hash-container tables are written sorted by key (deterministic raw bytes).
    std::sort(s.monitors.begin(), s.monitors.end(), [](const SavMonitor& a, const SavMonitor& b) { return a.ref < b.ref; });
    for (auto& kv : VMS_CL(g_class_objects)) s.class_objects.push_back(kv);
    for (auto& kv : VMS_RS(g_gfx_color)) s.gfx_color.push_back(kv);
    for (auto& kv : VMS_RS(g_gfx_target)) s.gfx_target.push_back(kv);
    for (auto& kv : VMS_RS(g_gfx_tx)) s.gfx_tx.push_back(kv);
    for (auto& kv : VMS_RS(g_gfx_ty)) s.gfx_ty.push_back(kv);
    for (auto& kv : VMS_RS(g_offscreen)) s.offscreen.push_back(kv);
    for (auto& kv : VMS_RS(g_clip)) s.clips.push_back({kv.first, kv.second.x, kv.second.y, kv.second.w, kv.second.h, kv.second.on});
    std::sort(s.class_objects.begin(), s.class_objects.end());
    for (auto* xs : {&s.gfx_color, &s.gfx_target, &s.gfx_tx, &s.gfx_ty, &s.offscreen}) std::sort(xs->begin(), xs->end());
    std::sort(s.clips.begin(), s.clips.end(), [](const SavState::SavClip& a, const SavState::SavClip& b) { return a.ref < b.ref; });
    s.midlet = VMS_EXEC(g_midlet); s.current = VMS_RS(g_current); s.display = VMS_RS(g_display); s.shown = VMS_RS(g_shown); s.alert_return = VMS_RS(g_alert_return);
    s.has_alert = VMS_RS(g_alert_deadline) != 0; s.alert_deadline_delta = s.has_alert ? VMS_RS(g_alert_deadline) - save_now_ms() : 0; s.alert_remaining = VMS_RS(g_alert_remaining);
    s.rms = VMS_PLAT(g_rms_suite); s.fb = VMS_RS(g_fb);
    for (int r : VMS_EXEC(g_serial_calls)) s.serial.push_back(r);
    for (auto& player : VMS_PLAT(g_audio).capture()) if (!VMS_PLAT(g_tones).count(player.handle)) s.audio.push_back(std::move(player));
    return true;
}
static bool encode_state(const SavState& s, std::string& out) {
    if(s.version>=4 && (s.game_millis<0 || s.game_millis>INT64_MAX/1000 || !std::isfinite(s.clock_scale) || s.clock_scale<1 || s.clock_scale>16))return false;
    BinW w;
    if(s.version!=5)return false;
    w.b.append("J2S5", 4);
    w.str(s.jar);
    if(s.version>=4) {w.i64(s.game_millis);w.i64(int64_t(s.clock_scale*1000000));}
    w.u32(uint32_t(s.heap.size()));
    for (auto& o : s.heap) {
        w.str(o.cls); w.vals(o.fields); w.str(o.text); w.str(o.array_desc); w.u8(o.is_array ? 1 : 0);
        if (!o.elems.empty()) { // Migration/testing of an old expanded snapshot.
            std::vector<uint8_t> data;
            size_t width = sav_array_width(o.array_desc);
            if (!width) return false;
            for (auto& v : o.elems) sav_array_append(data, unsav_value(v), o.array_desc, width);
            w.u32(uint32_t(o.elems.size())); w.raw(data.data(), data.size());
        } else { w.u32(o.array_length); w.raw(o.array_data.data(), o.array_data.size()); }
        w.raw(o.pixels.data(), o.pixels.size()); w.raw(o.alpha.data(), o.alpha.size()); w.i32(o.width); w.i32(o.height);
    }
    w.u32(uint32_t(s.free_list.size())); for (int r : s.free_list) w.i32(r);
    w.u32(uint32_t(s.classes.size()));
    for (auto& c : s.classes) { w.str(c.name); w.str(c.instance_schema); w.str(c.static_schema); w.i32(c.init_state); w.i32(c.init_error); w.i32(c.init_owner); w.vals(c.statics); }
    w.u8(s.has_main ? 1 : 0); if (s.has_main) write_thread(w, s.main);
    w.u32(uint32_t(s.threads.size())); for (auto& t : s.threads) write_thread(w, t);
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
    w.u32(uint32_t(s.audio.size()));
    for (auto& p : s.audio) {
        w.i32(p.handle); w.i32(p.state); w.i32(p.volume); w.i32(p.loops); w.i32(p.remaining);
        w.i64(int64_t(p.frame)); w.u8(p.midi ? 1 : 0); w.u32(p.pcmRate); w.i32(p.pcmChannels);
        w.raw(reinterpret_cast<const uint8_t*>(p.pcm.data()), p.pcm.size() * sizeof(int16_t));
        w.u32(uint32_t(p.events.size()));
        for (auto& e : p.events) { w.i64(int64_t(e.at)); w.u32(e.order); w.u32(e.tempo); w.u8(e.status); w.u8(e.a); w.u8(e.b); }
    }
    out.swap(w.b); return true;
}
static bool decode_state(const std::string& bytes, SavState& s) {
    if (bytes.size() < 4) return false;
    if (bytes.compare(0, 4, "J2S5") != 0) {
        // C10: no compatibility with earlier formats. Old saves are refused cleanly (file kept, VM untouched).
        if (bytes.compare(0, 3, "J2S") == 0 && bytes[3] >= '1' && bytes[3] <= '4')
            log_line("SAVE rejected: savestate format " + bytes.substr(0, 4) + " is from an older build; this build reads J2S5 only");
        return false;
    }
    const int version = 5;
    const bool audio_block = version >= 2;
    s = SavState{}; s.version = version;
    BinR r{reinterpret_cast<const uint8_t*>(bytes.data()) + 4, reinterpret_cast<const uint8_t*>(bytes.data()) + bytes.size()};
    s.jar = r.str();
    if(version>=4) {s.game_millis=r.i64();s.clock_scale=double(r.i64())/1000000; if(s.game_millis<0 || s.game_millis>INT64_MAX/1000 || s.clock_scale<1 || s.clock_scale>16)r.ok=false;}
    uint32_t nheap = r.count(2000000, version >= 3 ? 41 : 37);
    if (nheap > 2000000) return false;
    s.heap.resize(nheap);
    for (uint32_t i = 0; i < nheap && r.ok; ++i) {
        auto& o = s.heap[i];
        o.cls = r.str(); o.fields = r.vals(); o.text = r.str(); o.array_desc = r.str(); o.is_array = r.u8() != 0;
        if (version >= 3) { o.array_length = r.u32(); o.array_data = r.raw(); }
        else {
            auto elems = r.vals();
            o.array_length = uint32_t(elems.size());
            if (!elems.empty() || o.is_array) {
                size_t width = sav_array_width(o.array_desc);
                if (!o.is_array || !width) r.ok = false;
                else for (auto& v : elems) sav_array_append(o.array_data, unsav_value(v), o.array_desc, width);
            }
        }
        o.pixels = r.raw(); o.alpha = r.raw(); o.width = r.i32(); o.height = r.i32();
    }
    uint32_t nfree = r.count(2000000, 4);
    for (uint32_t i = 0; i < nfree && r.ok; ++i) s.free_list.push_back(r.i32());
    uint32_t nclass = r.count(100000, 16);
    for (uint32_t i = 0; i < nclass && r.ok; ++i) {
        SavClass c; c.name = r.str(); if (version >= 3) { c.instance_schema = r.str(); c.static_schema = r.str(); } c.init_state = r.i32(); c.init_error = r.i32(); if (version >= 3) c.init_owner = r.i32(); c.statics = r.vals(); s.classes.push_back(std::move(c));
    }
    s.has_main = r.u8() != 0; if (s.has_main) s.main = read_thread(r, version);
    uint32_t nthread = r.count(100000, 64);
    for (uint32_t i = 0; i < nthread && r.ok; ++i) s.threads.push_back(read_thread(r, version));
    uint32_t nmon = r.count(2000000, 16);
    for (uint32_t i = 0; i < nmon && r.ok; ++i) {
        SavMonitor m; m.ref = r.i32(); m.owner = r.i32(); m.depth = r.i32(); uint32_t nw = r.count(100000, 4);
        for (uint32_t k = 0; k < nw && r.ok; ++k) m.waiters.push_back(r.i32());
        s.monitors.push_back(std::move(m));
    }
    uint32_t nco = r.count(2000000, 8);
    for (uint32_t i = 0; i < nco && r.ok; ++i) s.class_objects.push_back({r.str(), r.i32()});
    auto pairs = [&](std::vector<std::pair<int, int>>& xs) {
        uint32_t n = r.count(2000000, 8); for (uint32_t i = 0; i < n && r.ok; ++i) xs.push_back({r.i32(), r.i32()});
    };
    pairs(s.gfx_color); pairs(s.gfx_target); pairs(s.gfx_tx); pairs(s.gfx_ty); pairs(s.offscreen);
    uint32_t nclip = r.count(2000000, 21);
    for (uint32_t i = 0; i < nclip && r.ok; ++i) {
        SavState::SavClip c; c.ref = r.i32(); c.x = r.i32(); c.y = r.i32(); c.w = r.i32(); c.h = r.i32(); c.on = r.u8() != 0; s.clips.push_back(c);
    }
    s.midlet = r.i32(); s.current = r.i32(); s.display = r.i32(); s.shown = r.i32(); s.alert_return = r.i32();
    s.has_alert = r.u8() != 0; s.alert_deadline_delta = r.i64(); s.alert_remaining = r.i64();
    s.rms = r.str(); s.fb = r.raw();
    uint32_t nserial = r.count(2000000, 4);
    for (uint32_t i = 0; i < nserial && r.ok; ++i) s.serial.push_back(r.i32());
    if (audio_block) {
        uint32_t naudio = r.u32();
        if (naudio > 64) r.ok = false;
        for (uint32_t i = 0; i < naudio && r.ok; ++i) {
            j2me_audio::Engine::SavedPlayer p;
            p.handle = r.i32(); p.state = r.i32(); p.volume = r.i32(); p.loops = r.i32(); p.remaining = r.i32();
            p.frame = uint64_t(r.i64()); p.midi = r.u8() != 0; p.pcmRate = r.u32(); p.pcmChannels = r.i32();
            auto pcm = r.raw();
            if (pcm.size() % 2) r.ok = false;
            else {
                p.pcm.resize(pcm.size() / 2);
                if (!p.pcm.empty()) std::memcpy(p.pcm.data(), pcm.data(), pcm.size());
            }
            uint32_t ne = r.u32();
            if (ne > 100000) r.ok = false;
            p.events.reserve(std::min<uint32_t>(ne, 100000));
            for (uint32_t k = 0; k < ne && r.ok; ++k) {
                j2me_audio::Engine::SavedEvent e;
                e.at = uint64_t(r.i64()); e.order = r.u32(); e.tempo = r.u32(); e.status = r.u8(); e.a = r.u8(); e.b = r.u8();
                p.events.push_back(e);
            }
            s.audio.push_back(std::move(p));
        }
    }
    return r.ok && r.p == r.e;
}
// Legacy class IDs refer to the saved class sequence, never today's load order.
static std::string sav_wait_name(const SavState& s, const SavThread& t) {
    if (s.version >= 3) return t.init_wait_class;
    return t.init_wait >= 0 && size_t(t.init_wait) < s.classes.size() ? s.classes[t.init_wait].name : std::string{};
}
static int sav_owner(const SavState& s, const SavClass& c) {
    if (s.version >= 3) return c.init_owner;
    int owner = -1;
    auto inspect = [&](const SavThread& t, int id) {
        for (auto& f : t.frames) if (f.init_class == c.name) {
            if (owner >= 0 && owner != id) owner = -2;
            else if (owner != -2) owner = id;
        }
    };
    if (s.has_main) inspect(s.main, 0);
    for (size_t i = 0; i < s.threads.size(); ++i) inspect(s.threads[i], int(i + 1));
    return owner;
}
// Load fresh metadata in a private heap. ConstantValue strings may allocate
// while preparing classes, and the live class cache may belong to another JAR.
// Swapping deques preserves the live Method pointers held by suspended frames.
struct SavPreflightScope {
    std::deque<Class> classes;
    std::unordered_map<std::string, int> class_index;
    std::deque<Obj> heap;
    std::vector<int> free_list;
    int alloc_since = VMS_HEAP(g_alloc_since);
    SavPreflightScope() {
        VMS_CL(g_classes).swap(classes); VMS_CL(g_class_index).swap(class_index);
        VMS_HEAP(g_heap).swap(heap); VMS_HEAP(g_free).swap(free_list); resver::bump_epoch(); p7::pool().epoch_clear(resver::g_epoch);
        VMS_HEAP(g_heap).emplace_back();
    }
    ~SavPreflightScope() {
        VMS_CL(g_classes).swap(classes); VMS_CL(g_class_index).swap(class_index);
        VMS_HEAP(g_heap).swap(heap); VMS_HEAP(g_free).swap(free_list); resver::bump_epoch(); p7::pool().epoch_clear(resver::g_epoch);
        VMS_HEAP(g_alloc_since) = alloc_since;
    }
};
// Preflight finishes before stop_thread or replacing any live objects/stacks.
static bool validate_state(const SavState& s,bool use_saved_size=false) {
    SavPreflightScope scope;
    auto reject = [](const std::string& why) { log_line(std::string("SAVE invalid state: ") + why); return false; };
    if(s.current>0&&size_t(s.current)<s.heap.size()) {
        const ScreenMeta m=screen_meta(s.heap[s.current].text);
        if(!logical_screen::valid(m.w,m.h)||(!use_saved_size&&(m.w!=VMS_RS(g_screen_w)||m.h!=VMS_RS(g_screen_h)))||
           s.fb.size()!=size_t(m.w)*m.h*3)return reject("logical canvas size mismatch; use the saved game's .screen setting (legacy default 240x320)");
    }
    if(s.version>=4 && (s.game_millis<0 || s.game_millis>INT64_MAX/1000 || !std::isfinite(s.clock_scale) || s.clock_scale<1 || s.clock_scale>16))return reject("game clock");
    if (s.heap.empty() || !s.has_main || s.heap[0].is_array || !s.heap[0].cls.empty()) return reject("heap/main");
    std::unordered_set<int> free;
    for (int r : s.free_list) if (r <= 0 || size_t(r) >= s.heap.size() || !free.insert(r).second ||
        !s.heap[r].cls.empty() || s.heap[r].is_array) return reject("free list");
    auto ref = [&](int r) { return r == 0 || (r > 0 && size_t(r) < s.heap.size() && !free.count(r) &&
        (s.heap[r].is_array || !s.heap[r].cls.empty())); };
    auto vals = [&](const std::vector<SavValue>& vs) { for (auto& v : vs) if (v.is_long > 1 || !ref(v.ref)) return false; return true; };
    auto tid = [&](int id) { return id >= 0 && size_t(id) <= s.threads.size(); };
    auto name = [&](const std::string& n) {
        if (n.empty() || n.size() > 65535 || n.find('\0') != std::string::npos || n.find('.') != std::string::npos) return false;
        return load_class_file(n, false) >= 0;
    };
    if (!s.midlet || !s.current || !ref(s.midlet) || !ref(s.current) || !ref(s.display) || !ref(s.shown) || !ref(s.alert_return)) return reject("display roots");
    for (int r : {s.midlet, s.current, s.display, s.shown, s.alert_return})
        if (r && s.heap[r].cls.empty()) return reject("display root is not an object");
    std::unordered_map<std::string, const SavClass*> classes;
    for (auto& c : s.classes) {
        if (!classes.emplace(c.name, &c).second || c.init_state < 0 || c.init_state > 3 || !ref(c.init_error) || !vals(c.statics)) return reject("class metadata");
        if (c.init_state == 1 && !tid(sav_owner(s, c))) return reject("initialization owner");
        if (s.version >= 3 && c.init_state != 1 && c.init_owner != -1) return reject("unexpected initialization owner");
    }
    for (auto& o : s.heap) {
        if (!vals(o.fields)) return reject("object fields");
        if (!o.cls.empty()) {
            if (!name(o.cls)) return reject("missing object class");
            if (size_t(VMS_CL(g_classes)[VMS_CL(g_class_index).at(o.cls)].instance_fields) != o.fields.size()) return reject(std::string(s.version < 3 ? "legacy field layout mismatch: " : "instance field layout mismatch: ") + o.cls);
        }
        if (o.width < 0 || o.height < 0 || (!o.pixels.empty() && (uint64_t(o.width) * uint64_t(o.height) * 3 != o.pixels.size())) ||
            (!o.alpha.empty() && uint64_t(o.width) * uint64_t(o.height) != o.alpha.size())) return reject("image dimensions");
        if (o.is_array) {
            size_t width = sav_array_width(o.array_desc);
            size_t count = o.elems.empty() ? o.array_length : o.elems.size();
            if (!width || count > uint32_t(INT32_MAX) || (!o.elems.empty() ? !vals(o.elems) : uint64_t(count) * width != o.array_data.size())) return reject("array payload");
            if (o.elems.empty() && sav_array_refs(o.array_desc))
                for (size_t i = 0; i < count; ++i) if (!ref(sav_array_value(o.array_data.data() + i * width, o.array_desc, width).ref)) return reject("array reference");
        } else if (o.array_length || !o.array_data.empty() || !o.elems.empty()) return reject("non-array payload");
    }
    for (auto& c : s.classes) {
        if (!name(c.name)) return reject("missing class: " + c.name);
        if (VMS_CL(g_classes)[VMS_CL(g_class_index).at(c.name)].statics.size() != c.statics.size()) return reject(std::string(s.version < 3 ? "legacy static layout mismatch: " : "static layout mismatch: ") + c.name);
        const auto& loaded = VMS_CL(g_classes)[VMS_CL(g_class_index).at(c.name)];
        if (s.version >= 3 && (sav_class_schema(loaded, false) != c.instance_schema || sav_class_schema(loaded, true) != c.static_schema)) return reject("class field schema changed: " + c.name);
        if (c.name == kLiteralRootsClass) {
            const Field* field = find_field(VMS_CL(g_class_index).at(c.name), "roots", "[Ljava/lang/String;");
            if (!field || size_t(field->slot) >= c.statics.size()) return reject("literal root field");
            const int root = c.statics[field->slot].ref;
            if (root) {
                const auto& array = s.heap[root];
                if (!array.is_array || array.array_desc != "[Ljava/lang/String;") return reject("literal root array");
                std::unordered_set<std::string> texts;
                bool ended = false;
                const size_t count = array.elems.empty() ? array.array_length : array.elems.size();
                for (size_t i = 0; i < count; ++i) {
                    const int r = array.elems.empty() ? sav_array_value(array.array_data.data() + i * 4, array.array_desc, 4).ref : array.elems[i].ref;
                    if (!r) { ended = true; continue; }
                    if (ended || s.heap[r].cls != "java/lang/String" || !texts.insert(s.heap[r].text).second)
                        return reject("literal root entry");
                }
            }
        }
    }
    auto thread = [&](const SavThread& t, int id) {
        if (t.state < int(ThreadState::Runnable) || t.state > int(ThreadState::Terminated) || !ref(t.ref) || !ref(t.monitor) || !ref(t.join_target) || !ref(t.unhandled) || t.restore_depth < 0) return false;
        auto java_thread = [&](int r) {
            if (!r) return true;
            if (s.heap[r].cls.empty()) return false;
            return find_field(VMS_CL(g_class_index).at(s.heap[r].cls), "vmState", "I") != nullptr;
        };
        if ((id != 0 && !t.ref) || !java_thread(t.ref) || !java_thread(t.join_target)) return false;
        const std::string waiting = sav_wait_name(s, t);
        if (t.state == int(ThreadState::Initializing)) {
            auto c = classes.find(waiting);
            if (c == classes.end() || c->second->init_state != 1 || sav_owner(s, *c->second) == id) return false;
        } else if (!waiting.empty() || (s.version < 3 && t.init_wait != -1)) return false;
        for (auto& f : t.frames) {
            if (!name(f.cls) || f.name.empty() || !ref(f.sync_monitor) || f.constructed.is_long > 1 || !ref(f.constructed.ref) || !vals(f.locals) || !vals(f.stack)) return false;
            int cid = VMS_CL(g_class_index).at(f.cls);
            Method* m = f.stub ? nullptr : find_method(cid, f.name, f.desc, true);
            if (f.stub && (f.name != "<clinit>" || f.desc != "()V" || f.init_class != f.cls || find_method(cid, "<clinit>", "()V", false))) return false;
            if (!f.stub && !m) return false;
            size_t code_size = f.stub ? 1 : m->code.size();
            if (f.pc < 0 || (code_size ? size_t(f.pc) >= code_size : f.pc != 0) || f.op_pc < 0 || (code_size ? size_t(f.op_pc) >= code_size : f.op_pc != 0) ||
                (!f.stub && f.locals.size() < size_t(std::max(0, int(m->max_locals))))) return false;
            if (!f.init_class.empty()) {
                auto c = classes.find(f.init_class);
                if (c == classes.end() || c->second->init_state != 1 || sav_owner(s, *c->second) != id) return false;
            } else if (f.init_super_pending) return false;
        }
        return true;
    };
    if (!thread(s.main, 0)) return reject("main thread");
    for (size_t i = 0; i < s.threads.size(); ++i) if (!thread(s.threads[i], int(i + 1))) return reject("worker thread");
    for (auto& c : s.classes) if (c.init_state == 1) {
        int id = sav_owner(s, c);
        const auto& t = id == 0 ? s.main : s.threads[id - 1];
        bool found = false;
        for (auto& f : t.frames) if (f.init_class == c.name) found = true;
        if (!found || t.state == int(ThreadState::Terminated)) return reject("missing initializer frame");
    }
    std::unordered_set<int> monitors;
    for (auto& m : s.monitors) {
        if (!m.ref || !ref(m.ref) || !monitors.insert(m.ref).second || m.depth < 0 ||
            (m.owner == -1 ? m.depth != 0 : (!tid(m.owner) || m.depth == 0))) return reject("monitor");
        for (int id : m.waiters) if (!tid(id)) return reject("monitor waiter");
    }
    for (auto& p : s.class_objects) if (!name(p.first) || !ref(p.second)) return reject("class object");
    auto pairs = [&](const std::vector<std::pair<int, int>>& ps, bool targets) {
        for (auto& p : ps) if (!p.first || !ref(p.first) || (targets && !ref(p.second))) return false;
        return true;
    };
    if (!pairs(s.gfx_color, false) || !pairs(s.gfx_tx, false) || !pairs(s.gfx_ty, false) || !pairs(s.gfx_target, true) || !pairs(s.offscreen, true)) return reject("graphics binding");
    for (auto& c : s.clips) if (!ref(c.ref)) return reject("clip");
    for (int r : s.serial) if (!r || !ref(r)) return reject("serial callback");
    return true;
}

static bool apply_frame(Frame& fr, const SavFrame& s) {
    if (s.cls.empty() || s.name.empty()) return false;
    int cid = ensure_class(s.cls);
    if (s.stub) {
        auto& m = VMS_CL(g_classes)[cid].init_stub;
        m.class_id = cid; m.name = "<clinit>"; m.desc = "()V"; m.code = {177};
        fr.method = &m;
    }
    else fr.method = find_method(cid, s.name, s.desc, true);
    if (!fr.method) return false;
    fr.pc = s.pc; fr.op_pc = s.op_pc; fr.sync_monitor = s.sync_monitor;
    fr.init_class = s.init_class.empty() || !VMS_CL(g_class_index).count(s.init_class) ? -1 : VMS_CL(g_class_index)[s.init_class];
    fr.sync_acquired = s.sync_acquired; fr.entry_checked = s.entry_checked;
    fr.return_object = s.return_object; fr.init_super_pending = s.init_super_pending;
    fr.constructed = unsav_value(s.constructed);
    fr.locals.clear(); for (auto& v : s.locals) fr.locals.push_back(unsav_value(v));
    fr.stack.clear(); for (auto& v : s.stack) fr.stack.push_back(unsav_value(v));
    return true;
}
static bool apply_thread(JavaThreadContext& t, const SavThread& s, const std::string& wait) {
    const int64_t now = VMS_EXEC(g_save_clock).ms, now_us = VMS_EXEC(g_save_clock).us;  // one restore instant
    t = JavaThreadContext{};
    t.ref = s.ref; t.state = ThreadState(s.state);
    auto deadline = [](int64_t clock, int64_t delta) { return delta > INT64_MAX - clock ? INT64_MAX : clock + delta; };
    t.wake = s.has_wake ? deadline(now, s.wake_delta) : 0;
    t.sleep_sample_due = s.has_sample ? deadline(now, s.sample_delta) : 0;
    t.wake_us = s.has_wake_us ? deadline(now_us, s.wake_us_delta) : 0;
    t.sleep_request = s.sleep_request; t.monitor = s.monitor; t.restore_depth = s.restore_depth;
    t.join_target = s.join_target; t.unhandled = s.unhandled; t.init_wait = wait.empty() ? -1 : VMS_CL(g_class_index).at(wait);
    t.reacquire = s.reacquire; t.interrupt_exception = s.interrupt_exception; t.begun = s.begun;
    t.executing = false;
    for (auto& frs : s.frames) {
        Frame fr;
        if (!apply_frame(fr, frs)) return false;
        t.frames.push_back(std::move(fr));
    }
    return true;
}
// Only legacy-default (240x320, including tagless J2S5) sessions migrate.
// A current explicit setting or verified source-size hint can repair the old
// host default; unknown games and already nondefault saves keep their geometry.
// This changes live host surfaces only. The J2S5 file and game-owned Images,
// Java fields, stacks and RMS are not rewritten.
static void restore_screen_fit(const ScreenMeta& saved,const canvas_resolution::Choice& selected) {
    if(saved.w!=240||saved.h!=320||!logical_screen::valid(selected.w,selected.h)||
       (selected.w==saved.w&&selected.h==saved.h))return;
    auto resized=[&](const std::vector<uint8_t>& src,int channels,uint8_t fill){
        std::vector<uint8_t> dst(size_t(selected.w)*selected.h*channels,fill);
        const int width=std::min(saved.w,selected.w),height=std::min(saved.h,selected.h);
        for(int y=0;y<height;++y)std::memcpy(dst.data()+size_t(y)*selected.w*channels,
            src.data()+size_t(y)*saved.w*channels,size_t(width)*channels);
        return dst;
    };
    VMS_RS(g_fb)=resized(VMS_RS(g_fb),3,0);
    std::unordered_set<int> buffers;
    for(const auto& kv:VMS_RS(g_offscreen)) {
        const int ref=kv.second;
        if(ref<=0||size_t(ref)>=VMS_HEAP(g_heap).size()||buffers.count(ref))continue;
        Obj& image=VMS_HEAP(g_heap)[ref];
        if(image.width!=saved.w||image.height!=saved.h||image.pixels.size()!=size_t(saved.w)*saved.h*3)continue;
        image.pixels=resized(image.pixels,3,255);
        if(!image.alpha.empty())image.alpha=resized(image.alpha,1,255);
        image.width=selected.w;image.height=selected.h;
        if(const Field* f=find_field(image.cls,"width","I"))image.fields[f->slot]=Value::of_i(selected.w);
        if(const Field* f=find_field(image.cls,"height","I"))image.fields[f->slot]=Value::of_i(selected.h);
        image_layout_dirty(ref);buffers.insert(ref);
    }
    // Retained GameCanvas Graphics otherwise keep their old default clip and
    // truncate a larger canvas. Preserve every explicitly narrowed clip.
    for(const auto& kv:VMS_RS(g_gfx_target))if(kv.second==0||buffers.count(kv.second)) {
        auto it=VMS_RS(g_clip).find(kv.first);if(it==VMS_RS(g_clip).end())continue;
        Clip& c=it->second;
        if(c.on&&c.x==0&&c.y==0&&c.w==saved.w&&c.h==saved.h){c.w=selected.w;c.h=selected.h;}
    }
    VMS_RS(g_screen_w)=selected.w;VMS_RS(g_screen_h)=selected.h;
    // Keep the saved DSB1 dimensions until the next ordinary host_events turn.
    // display_visibility then sends sizeChanged once under its normal reentry
    // guards, before painting; no Java callback runs inside state application.
    VMS_RS(g_repaint)=true;VMS_RS(g_fb_dirty)=true;
    log_line("CANVAS migrated="+std::to_string(saved.w)+"x"+std::to_string(saved.h)+
             " size="+std::to_string(selected.w)+"x"+std::to_string(selected.h)+
             " source="+selected.source+" boundary=legacy_default_live_only");
}
// Rebind saved Java rate owners before the first restored STARTED audio block.
// This scans existing objects only: no class initialization, allocation or Java calls.
static void restore_media_rates() {
    int controlClass=-1,playerClass=-1;
    for(const auto& entry:VMS_CL(g_class_index)) {
        if(entry.first=="javax/microedition/media/RateControlImpl") controlClass=entry.second;
        else if(entry.first=="javax/microedition/media/NativePlayer") playerClass=entry.second;
    }
    if(controlClass<0 || playerClass<0) return;
    if(VMS_CL(g_classes)[controlClass].init_state!=2) return;
    auto slot=[](int cls,const char* name,const char* desc,bool isStatic) {
        for(const auto& field:VMS_CL(g_classes)[cls].fields)
            if(field.name==name && field.desc==desc && field.is_static==isStatic) return field.slot;
        return -1;
    };
    int registrySlot=slot(controlClass,"controls","[Ljavax/microedition/media/RateControlImpl;",true);
    int ownerSlot=slot(controlClass,"owner","Ljavax/microedition/media/NativePlayer;",false);
    int rateSlot=slot(controlClass,"rate","I",false);
    int handleSlot=slot(playerClass,"handle","I",false),stateSlot=slot(playerClass,"state","I",false);
    const auto& statics=VMS_CL(g_classes)[controlClass].statics;
    if(registrySlot<0 || size_t(registrySlot)>=statics.size() || ownerSlot<0 || rateSlot<0 || handleSlot<0 || stateSlot<0) return;
    const int registry=statics[size_t(registrySlot)].ref;
    auto valid=[](int ref){return ref>0 && size_t(ref)<VMS_HEAP(g_heap).size();};
    if(!valid(registry) || !VMS_HEAP(g_heap)[registry].is_array || VMS_HEAP(g_heap)[registry].array_desc!="[Ljavax/microedition/media/RateControlImpl;") return;
    const auto& entries=VMS_HEAP(g_heap)[registry].elems;
    for(size_t i=0;i<entries.size() && i<64;++i) {
        int ref=entries.get(i).ref;
        if(!valid(ref) || VMS_HEAP(g_heap)[ref].cls!=controlClass) continue;
        const auto& fields=VMS_HEAP(g_heap)[ref].fields;
        if(size_t(ownerSlot)>=fields.size() || size_t(rateSlot)>=fields.size()) continue;
        int owner=fields[size_t(ownerSlot)].ref;
        int rate=fields[size_t(rateSlot)].i;
        if(rate<50000 || rate>200000) continue;
        if(!valid(owner) || VMS_HEAP(g_heap)[owner].cls!=playerClass) continue;
        const auto& player=VMS_HEAP(g_heap)[owner].fields;
        if(size_t(handleSlot)>=player.size() || size_t(stateSlot)>=player.size() || player[size_t(stateSlot)].i==0) continue;
        int handle=player[size_t(handleSlot)].i;
        if(handle>0) VMS_PLAT(g_audio).rate(handle,rate);
    }
}
static bool apply_state(const SavState& s,bool use_saved_size=false) {
    if (!validate_state(s,use_saved_size)) return false;
    std::unordered_set<std::string> names;
    for (auto& c : s.classes) if (!c.name.empty()) names.insert(c.name);
    for (auto& o : s.heap) if (!o.cls.empty()) names.insert(o.cls);
    auto note_frame = [&](const SavFrame& fr) { if (!fr.cls.empty()) names.insert(fr.cls); if (!fr.init_class.empty()) names.insert(fr.init_class); };
    if (s.has_main) for (auto& fr : s.main.frames) note_frame(fr);
    for (auto& t : s.threads) for (auto& fr : t.frames) note_frame(fr);
    // Preflight passed before changing live geometry or stopping threads.
    // Apply the exact saved geometry first; the restore entry point may then
    // migrate a legacy-default host surface using a trusted current choice.
    if(use_saved_size) {
        const ScreenMeta meta=screen_meta(s.heap[s.current].text);
        VMS_RS(g_screen_w)=meta.w;VMS_RS(g_screen_h)=meta.h;
        log_line("CANVAS size="+std::to_string(meta.w)+"x"+std::to_string(meta.h)+" source=instant_state");
    }
    stop_thread();
    VMS_PLAT(g_rms_suite) = s.rms; VMS_PLAT(g_rms_import_legacy) = false;
    VMS_EXEC(g_execution_profile).reset();VMS_HEAP(g_frame_buffer_pool).clear();
    VMS_HEAP(g_gc_mark_scratch).clear();VMS_HEAP(g_gc_work_scratch).clear();
    VMS_CL(g_classes).clear(); VMS_CL(g_class_index).clear();
    VMS_HEAP(g_heap).assign(1, Obj{}); VMS_HEAP(g_free).clear(); resver::bump_epoch(); p7::pool().epoch_clear(resver::g_epoch); // P2.5 restore
    VMS_RS(g_gfx_color).clear(); VMS_RS(g_gfx_target).clear(); VMS_RS(g_gfx_tx).clear(); VMS_RS(g_gfx_ty).clear(); VMS_RS(g_clip).clear(); VMS_RS(g_offscreen).clear();
    VMS_RS(g_current) = VMS_RS(g_display) = VMS_EXEC(g_midlet) = VMS_RS(g_shown) = VMS_RS(g_alert_return) = 0;
    for (auto& name : names) ensure_class(name);
    for (auto& sc : s.classes) {
        auto it = VMS_CL(g_class_index).find(sc.name);
        if (it == VMS_CL(g_class_index).end()) continue;
        Class& c = VMS_CL(g_classes)[it->second];
        c.init_state = sc.init_state; c.init_error = sc.init_error; c.init_owner = nullptr;
        size_t n = std::min(c.statics.size(), sc.statics.size());
        for (size_t i = 0; i < n; ++i) c.statics[i] = unsav_value(sc.statics[i]);
    }
    VMS_HEAP(g_heap).assign(s.heap.size(), Obj{});
    for (size_t i = 0; i < s.heap.size(); ++i) {
        const SavObj& src = s.heap[i];
        Obj& o = VMS_HEAP(g_heap)[i];
        if (!src.cls.empty() && VMS_CL(g_class_index).count(src.cls)) o.cls = VMS_CL(g_class_index)[src.cls];
        int slots = o.cls >= 0 ? VMS_CL(g_classes)[o.cls].instance_fields : 0;
        o.fields.assign(std::max(slots, (int)src.fields.size()), Value{});
        for (size_t k = 0; k < src.fields.size() && k < o.fields.size(); ++k) o.fields[k] = unsav_value(src.fields[k]);
        o.text = src.text; o.array_desc = src.array_desc; o.is_array = src.is_array;
        if (src.is_array) {
            size_t count = src.elems.empty() ? src.array_length : src.elems.size();
            if (!o.elems.reset(src.array_desc, count)) return false;
            size_t width = sav_array_width(src.array_desc);
            for (size_t k = 0; k < count; ++k) o.elems.set(k, src.elems.empty() ? sav_array_value(src.array_data.data() + k * width, src.array_desc, width) : unsav_value(src.elems[k]));
        }
        o.pixels = src.pixels; o.alpha = src.alpha; o.width = src.width; o.height = src.height;
        if (!o.pixels.empty()) { o.image_layout_version = o.image_content_version = resver::tick(); ++resver::g_stats.image_layout; }
    }
    VMS_HEAP(g_free) = s.free_list;
    // Class preparation can create temporary literal roots before replacing
    // the heap. New J2S5 snapshots restore the ordinary saved static array;
    // legacy J2S5 has no such class and must not retain a temporary heap handle.
    VMS_CL(g_literal_cache) = LiteralCache{};
    bool saved_literal_roots = false;
    for (const auto& c : s.classes) if (c.name == kLiteralRootsClass) saved_literal_roots = true;
    if (!saved_literal_roots && VMS_CL(g_class_index).count(kLiteralRootsClass)) {
        const int cid = VMS_CL(g_class_index).at(kLiteralRootsClass);
        if (const Field* f = find_field(cid, "roots", "[Ljava/lang/String;")) VMS_CL(g_classes)[cid].statics[f->slot] = Value{};
    }
    if (s.has_main && !apply_thread(VMS_EXEC(g_main_thread), s.main, sav_wait_name(s, s.main))) return false;
    VMS_EXEC(g_threads).clear();
    for (auto& st : s.threads) { VMS_EXEC(g_threads).emplace_back(); if (!apply_thread(VMS_EXEC(g_threads).back(), st, sav_wait_name(s, st))) return false; }
    std::vector<JavaThreadContext*> ids;
    ids.push_back(&VMS_EXEC(g_main_thread));
    for (auto& t : VMS_EXEC(g_threads)) ids.push_back(&t);
    auto thread_at = [&](int id) -> JavaThreadContext* { return id >= 0 && id < (int)ids.size() ? ids[id] : nullptr; };
    for (auto& c : s.classes) if (c.init_state == 1)
        VMS_CL(g_classes)[VMS_CL(g_class_index).at(c.name)].init_owner = thread_at(sav_owner(s, c));
    VMS_EXEC(g_monitors).clear();
    for (auto& m : s.monitors) {
        Monitor mon; mon.owner = thread_at(m.owner); mon.depth = m.depth;
        for (int id : m.waiters) if (auto* t = thread_at(id)) mon.waiters.push_back(t);
        VMS_EXEC(g_monitors)[m.ref] = std::move(mon);
    }
    VMS_CL(g_class_objects).clear();
    for (auto& kv : s.class_objects) VMS_CL(g_class_objects)[kv.first] = kv.second;
    for (auto& kv : s.gfx_color) VMS_RS(g_gfx_color)[kv.first] = kv.second;
    for (auto& kv : s.gfx_target) VMS_RS(g_gfx_target)[kv.first] = kv.second;
    for (auto& kv : s.gfx_tx) VMS_RS(g_gfx_tx)[kv.first] = kv.second;
    for (auto& kv : s.gfx_ty) VMS_RS(g_gfx_ty)[kv.first] = kv.second;
    for (auto& kv : s.offscreen) VMS_RS(g_offscreen)[kv.first] = kv.second;
    for (auto& c : s.clips) VMS_RS(g_clip)[c.ref] = Clip{c.x, c.y, c.w, c.h, c.on};
    VMS_EXEC(g_midlet) = s.midlet; VMS_RS(g_current) = s.current; VMS_RS(g_display) = s.display; VMS_RS(g_shown) = s.shown; VMS_RS(g_alert_return) = s.alert_return;
    const int64_t now = VMS_EXEC(g_save_clock).ms;
    VMS_RS(g_alert_deadline) = s.has_alert ? (s.alert_deadline_delta > INT64_MAX - now ? INT64_MAX : now + s.alert_deadline_delta) : 0;
    VMS_RS(g_alert_remaining) = s.alert_remaining;
    VMS_PLAT(g_rms_suite) = s.rms;
    if (s.fb.size() == (size_t)VMS_RS(g_screen_w) * (size_t)VMS_RS(g_screen_h) * 3) VMS_RS(g_fb) = s.fb;
    else {
        VMS_RS(g_fb).assign((size_t)VMS_RS(g_screen_w) * (size_t)VMS_RS(g_screen_h) * 3, 0);
        const size_t n = std::min(VMS_RS(g_fb).size(), s.fb.size());
        if (n) std::memcpy(VMS_RS(g_fb).data(), s.fb.data(), n);
    }
    VMS_EXEC(g_serial_calls).clear();cont_new_epoch(); // restored tasks are a new epoch, their order is untracked
    for (int r : s.serial) VMS_EXEC(g_serial_calls).push_back(r);
    VMS_PLAT(g_pressed_keys).clear(); VMS_PLAT(g_command_keys).clear(); VMS_PLAT(g_key_states) = 0;
    // Host presses are not resumed. Otherwise a saved held GameCanvas key
    // survives without a matching release event after continuing the game.
    for (size_t ref = 1; ref < VMS_HEAP(g_heap).size(); ++ref) reset_game_keys(int(ref));
    VMS_HEAP(g_array_payload_live) = 0;
    for (auto& o : VMS_HEAP(g_heap)) if (o.is_array) VMS_HEAP(g_array_payload_live) += o.elems.payload_bytes();
    VMS_HEAP(g_array_payload_peak) = std::max(VMS_HEAP(g_array_payload_peak), VMS_HEAP(g_array_payload_live));
    VMS_EXEC(g_midlet_state) = MidletState::Active; VMS_PLAT(g_host_paused) = false;
    VMS_PLAT(g_host_frozen236)=false;VMS_PLAT(g_host_freeze_started236)=0; VMS_EXEC(g_notify_destroyed) = VMS_EXEC(g_notify_paused) = false;
    // Preserve the game's epoch, but relearn its rate from the restored loop.
    // Earlier releases could save an unproven 8x rate that suppresses all waits.
    VMS_EXEC(g_game_clock).start(VMS_EXEC(g_save_clock).us,s.game_millis,1.0);
    // P8.4 (P8-BUG-01): the current per-game speed (speed.cfg, already in g_speed_cap) wins; J2S5 carries no
    // speed value. game_millis is kept and the clock is re-aligned at scale 1 for the current rate.
    log_line("SPEEDRESTORE game_millis="+std::to_string(s.game_millis)+" saved_clock_scale="+std::to_string(s.clock_scale)+
             " saved_speed="+(s.clock_scale>1.0001?"capped":"unknown")+" current_speed="+host_speed::log_name(VMS_EXEC(g_speed_cap))+
             " new_clock_scale="+std::to_string(VMS_EXEC(g_game_clock).scale)+" source=speed.cfg");
    g_p84_sleep = {}; g_p84_sleep.left = 120;
    VMS_EXEC(g_game_clock_mode)=VMS_EXEC(g_speed_cap);
    VMS_EXEC(g_game_clock_inherited)=true;
    speed_reset_learning();
    VMS_EXEC(g_running_thread) = nullptr; VMS_EXEC(g_active_stack) = nullptr; VMS_HEAP(g_root_stacks).clear(); native_roots_reset();
    update_thread_live();
    VMS_RS(g_repaint) = true; VMS_RS(g_fb_dirty) = true;
    const int audio_restored = VMS_PLAT(g_audio).restore(s.audio);
    restore_media_rates();
    audio_suspend(false);
    log_line("SAVE audio players=" + std::to_string(s.audio.size()) + " restored=" + std::to_string(audio_restored) +
             " loaded=" + std::to_string(VMS_PLAT(g_audio).loadedPlayers()) + " started=" + std::to_string(VMS_PLAT(g_audio).activePlayers()));
    reset_runtime_stats();
    return VMS_EXEC(g_midlet) > 0 && VMS_RS(g_current) > 0;
}

J2ME_ISP_DECL_LAUNCHED_PATH
// P2.2-S7: snapshot freezability check (PX-2 doc 03 section 3). OBSERVE ONLY in this build: an unsafe state is
// logged as "SAVE check freezable=0 reasons=... would_refuse=1" and counted, but the save still proceeds exactly
// as before. Refusing would change what the user sees (no file, a failure note on the Y sheet), so it is held
// until the user rules on it (see docs/P2.2-S5S7-SELFCHECK-CN.md).
// (snapshot_freezable / SnapshotCheckStats live in src/p2s5_rootscope.inc so desktop builds report them too.)
static bool save_instant_state(const std::string& jar) {
    if(r4::state.active){log_line("R4 save unsupported before I/O");return false;}
    p7::pool().drain(); // P7.2: workers quiesced before J2S5 capture
    if (jar.empty() || VMS_EXEC(g_midlet) <= 0) return false;
    S0_HOOK(s0_assert_snapshot_safe());
    { std::string reasons; ++g_snap_check.checks;
      if (snapshot_freezable(reasons)) { ++g_snap_check.freezable; log_line("SAVE check freezable=1 gpu_fence=untracked"); }
      else { ++g_snap_check.would_refuse; log_line("SAVE check freezable=0 reasons="+reasons+" would_refuse=1 action=saved_anyway(observe_only)"); } }
    SavState state;
    if (!capture_state(state, jar)) return false;
    std::string bytes;
    if (!encode_state(state, bytes)) return false;
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/j2me", 0777);
    mkdir("sdmc:/switch/j2me/state", 0777);
    const std::string path = instant_state_path(jar);
    if (!instant_state_file::save(path, bytes)) return false;
    log_line("SAVE instant bytes=" + std::to_string(bytes.size()) + " heap=" + std::to_string(state.heap.size()) +
             " threads=" + std::to_string(state.threads.size()) + " audio=" + std::to_string(state.audio.size()) +
             " path=" + path +
             " bezel=" + std::to_string(J2ME_ISP(g_menu_settings).bezel) +
             " show_fps=" + std::to_string(J2ME_ISP(g_menu_settings).show_fps ? 1 : 0) +
             " speed=" + host_speed::log_name(VMS_EXEC(g_speed_cap)));
    p8_count_summary("save", false); // P8.2: HOME/close may follow; window stays open (cumulative)
    log_drain(); // P8.7: durable log caught up before HOME/kill
    return true;
}
// P2.2 (user ruling 2026-10-13): the Y-sheet save must snapshot with audio paused.
// Audio has no separate thread: the sink is pumped from this main loop, and on Switch
// audio_suspend(true) -> SwitchAudioSink::suspend -> audoutStopAudioOut, which synchronizes
// with the playing block before returning. The previous pause state is restored afterwards.
// J2S5 bytes are unaffected (the engine pause flag is not part of the state), RMS is not touched.
static bool sheet_save_instant_state(const std::string& jar) {
    if(r4::state.active)return false;
    const bool was_paused = VMS_PLAT(g_audio).paused();
    if (!was_paused) audio_suspend(true);
    const bool ok = save_instant_state(jar);
    if (!was_paused) audio_suspend(false);
    return ok;
}
static bool delete_instant_state(const std::string& jar) {
    if (jar.empty()) return false;
    const std::string path = instant_state_path(jar);
    bool removed = false;
    // Remove recovery records first: a failed deletion must not leave a journal
    // that can resurrect a primary snapshot already removed by this call.
    for (const auto& file : {path + ".previous", path + ".tmp", path}) {
        auto status = instant_state_file::status(file);
        if (status == instant_state_file::Status::error ||
            (status == instant_state_file::Status::readable && std::remove(file.c_str()) != 0)) return false;
        removed = removed || status == instant_state_file::Status::readable;
    }
    if (!removed) return false;
    log_line("SAVE delete path=" + path);
    return true;
}
static bool restore_instant_state(const std::string& jar, const std::string& expected_identity = "") {
    if(r4::state.active){log_line("R4 restore unsupported before I/O");return false;}
    p7::pool().drain(); // P7.2: workers quiesced before J2S5 restore
    std::string bytes;
    if (!instant_state_file::load(instant_state_path(jar), bytes)) return false;
    SavState state;
    if (!decode_state(bytes, state)) { log_line("SAVE decode failed"); return false; }
    // P8.6: the JAR is identified by its SHA256 (state.rms), not by its path. A JAR moved between
    // j2me/ and j2me/games/ keeps its hash, so the save loads and a SAVE jar_moved line is written.
    // A different hash is still rejected, whatever the path. J2S5 format unchanged.
    std::string identity = expected_identity;
    if (identity.empty()) {
        std::vector<uint8_t> jar_bytes;
        if (!read_whole(jar, jar_bytes)) { log_line("SAVE jar identity unavailable"); return false; }
        identity = rms_jar_identity(jar_bytes.data(), jar_bytes.size());
    }
    if (identity != state.rms) {
        log_line(state.jar != jar ? "SAVE jar mismatch reason=sha256 path_differs=1" : "SAVE jar identity mismatch");
        return false;
    }
    if (state.jar != jar) log_line("SAVE jar_moved from=" + state.jar + " to=" + jar + " sha256=" + identity);
    { auto& c = VMS_EXEC(g_save_clock); c.valid = false; c.ms = monotonic_ms(); c.us = paint_clock_us(); }  // J2S5 restore instant
    // Keep the snapshot's saved logical geometry exactly as stored. Do not
    // infer a replacement size from JAR/vendor metadata or migrate 240x320
    // surfaces automatically. Explicit .screen applies only on cold launch.
    if (!apply_state(state,true)) { log_line("SAVE apply failed"); return false; }
    log_line("CANVAS autosize_restore=disabled source=saved_snapshot");
    VMS_EXEC(g_farm_startup).start(identity,monotonic_ms());
    if(VMS_EXEC(g_farm_startup).waiting()) {
        if(farm_startup_ready()) VMS_EXEC(g_farm_startup).update(monotonic_ms(),true);
        else if(startup_defer_visibility(VMS_RS(g_current))) VMS_EXEC(g_farm_startup).pending_show=VMS_RS(g_current);
    }
    log_line("SAVE restored heap=" + std::to_string(VMS_HEAP(g_heap).size()) + " threads=" + std::to_string(VMS_EXEC(g_threads).size()) +
             " canvas=" + (VMS_RS(g_current) > 0 && VMS_HEAP(g_heap)[VMS_RS(g_current)].cls >= 0 ? VMS_CL(g_classes)[VMS_HEAP(g_heap)[VMS_RS(g_current)].cls].name : std::string("-")) +
             " bezel=" + std::to_string(J2ME_ISP(g_menu_settings).bezel) + " fps=" + std::to_string(J2ME_ISP(g_menu_settings).show_fps) +
             " speed=" + host_speed::log_name(VMS_EXEC(g_speed_cap)));
    { auto& c = VMS_EXEC(g_save_clock); c.anchor_epoch = c.exec_epoch; c.valid = true; }  // logical time stands still until the VM executes
    p8_caught_reset(); p8_count_open_continue(); // P8.2: continue sessions get a P7COUNT window too
    return true;
}
