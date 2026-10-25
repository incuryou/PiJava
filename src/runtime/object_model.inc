// Java handles, packed values, class/method layout and cooperative call frames.
// Included once by the runtime composition unit; not a standalone TU.
struct Obj {
    bool indexed_key = false; // Native derived indexes never become Java roots.
    int cls = -1;
    std::vector<Value> fields;
    ArrayStorage elems;
    std::string text, array_desc;
    bool is_array = false;
    std::vector<uint8_t> pixels, alpha;
    int width = 0, height = 0;
    // P2.5 (never serialized): slot generation survives Obj{} resets; image versions are clock stamps.
    uint32_t generation = 0;
    uint64_t image_layout_version = 0, image_content_version = 0;
};
struct Handler { int start = 0, end = 0, pc = 0, catch_cp = 0; };
struct Field { std::string name, desc; uint16_t flags = 0; int slot = -1, owner = -1; bool is_static = false; int const_idx = 0; };
// One slot per bytecode. The first execution resolves the constant pool;
// later executions at this pc use the stored field or method.
struct QuickSite {
    uint8_t kind = 0;      // 1 field, 3 invokestatic/special, 4 invokevirtual/interface
    uint8_t argc = 0;
    uint8_t flags = 0;     // bit 0: returns void; bits 1..4: stream intrinsic
    uint8_t intrinsic = 0; // 1 drawImage, 2 drawRegion, 3 fillRect
    uint8_t library_intrinsic = 0; // Verified bundled class-library leaf methods.
    uint8_t residual_sleep = 0; // 0 unchecked, 1 rejected, 2 clock in remainder sleep.
    uint8_t frame_wait = 0; // 0 unchecked/eligible, 1 structurally not a frame remainder wait.
    int a = 0, b = 0;
    int rechecked_deadline = -1; // structural cache, 0 rejected, >0 pattern index
    int compensated_sleep = -1;
    int cached_elapsed = -1; // -1 unchecked, 0 rejected, >0 pattern index
    int time_gate = -1; // -1 unchecked, 0 ordinary clock read, >0 pattern index
    int vclass = -1, va = 0, vb = 0;
};
struct Method {
    std::string name, desc;
    uint16_t flags = 0, max_locals = 0, max_stack = 0;
    std::vector<uint8_t> code;
    std::vector<Handler> handlers;
    std::vector<QuickSite> quick;
    std::vector<TimeGatePattern> time_gates;
    std::vector<RecheckedDeadlinePattern> rechecked_deadlines;
    bool has_rechecked_deadline=false;
    std::vector<CachedElapsedPattern> cached_elapsed_gates;
    std::vector<CompensatedSleepPattern> compensated_sleep_patterns;
    bool native = false;
    int class_id = -1;
    MethodCpuCounters cpu_profile;
    uint8_t read_leaf_kind=0; // derived eligibility; never serialized
    uint64_t read_leaf_calls=0,read_leaf_fallbacks=0;
    jit236::Site jit_site; // nonserialized, epoch-checked native code cache ticket
    std::vector<uint8_t> jitr_skip; uint64_t jitr_skip_epoch = 0; // P6.8: nonserialized per-pc 'region JIT: never try again' bits, valid only for jitr_skip_epoch
};
struct CpEntry { uint8_t tag = 0; int a = 0, b = 0; std::string utf; int64_t l = 0; };
// Derived lookup only. Strong ownership is the ordinary Java static array in
// j2me/internal/StringLiterals, so J2S5 needs no new fields or record layout.
struct LiteralCache {
    uint64_t epoch = 0;
    int roots = 0;
    std::unordered_map<std::string, int> refs;
};
struct Class {
    // Private M3G metadata only: no Java roots, never serialized.
    std::vector<std::pair<int,int>> m3g_slots238;
    uint8_t trusted_library = 0;
    int library_value_slot = -1, library_keys_slot = -1, library_count_slot = -1;
    std::string name;
    NativeClassId native_id = NativeClassId::Unresolved;
    int super_id = -1;
    std::vector<int> interfaces;
    std::vector<Field> fields;
    std::vector<Method> methods;
    std::vector<CpEntry> cp;
    int instance_fields = 0;
    std::vector<Value> statics;
    int init_state = 0, init_error = 0; // prepared / initializing / initialized / erroneous
    void* init_owner = nullptr;
    Method init_stub;
    uint16_t flags = 0;
};
 VMS_HEAP_ACCESSOR(g_frame_buffer_pool, FrameBufferPool<Value>)
struct Frame {
    Method* method = nullptr;
    std::vector<Value> locals, stack;
    int pc = 0, op_pc = 0;
    int sync_monitor = 0, init_class = -1;
    bool sync_acquired = false, entry_checked = false, return_object = false, init_super_pending = false;
    Value constructed;
    Frame() = default;
    Frame(const Frame&) = default;
    Frame& operator=(const Frame&) = default;
    Frame(Frame&&) noexcept = default;
    Frame& operator=(Frame&&) noexcept = default;
#if J2ME_OPT_FRAME_POOL
    ~Frame() { VMS_HEAP(g_frame_buffer_pool).release(locals); VMS_HEAP(g_frame_buffer_pool).release(stack); }
#endif
};
