#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

static int checks = 0;
static void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { std::cerr << "FAIL GameCanvasGcChecks " << label << "\n"; std::exit(1); }
}
static Value call(int self, const char* name, const char* desc, std::vector<Value> args = {}) {
    args.insert(args.begin(), Value::of_ref(self));
    return run(find_method(VMS_HEAP(g_heap)[self].cls, name, desc, true), args);
}
static int canvas(bool suppress = true) {
    int cid = ensure_class("GameCanvasChecks");
    int ref = alloc_obj(cid);
    run(find_method(cid, "<init>", "(Z)V", false), {Value::of_ref(ref), Value::of_i(suppress)});
    return ref;
}
static void show(int ref) { VMS_RS(g_current) = ref; display_visibility(); VMS_RS(g_repaint) = false; }
static int field(int ref, const char* name) {
    return VMS_HEAP(g_heap)[ref].fields[find_field(VMS_HEAP(g_heap)[ref].cls, name, "I")->slot].i;
}
static int poll(int ref) { return call(ref, "getKeyStates", "()I").i; }
static void flush(int ref, int x, int y, int w, int h) {
    call(ref, "flushGraphics", "(IIII)V", {Value::of_i(x), Value::of_i(y), Value::of_i(w), Value::of_i(h)});
}
static int pixel(int x, int y) { return get_px(VMS_RS(g_fb), VMS_RS(g_screen_w), VMS_RS(g_screen_h), x, y) & 0xffffff; }
static int image_pixel(int image, int x, int y) {
    return get_px(VMS_HEAP(g_heap)[image].pixels, VMS_HEAP(g_heap)[image].width, VMS_HEAP(g_heap)[image].height, x, y) & 0xffffff;
}

int main(int argc, char** argv) {
    VMS_CL(g_classpath) = {argc > 1 ? argv[1] : "build/test-classes", "romfs/classes"};
    VMS_DIAG(g_log_draw) = false;
    VMS_RS(g_screen_w) = 12; VMS_RS(g_screen_h) = 9;
    launch_class("GameCanvasBootChecks");
    check(VMS_RS(g_current) && pixel(0, 0) == 0x123456 && pixel(11, 8) == 0x123456,
          "first launch inherits concrete paint and displays complete buffer");
    check(find_method(VMS_HEAP(g_heap)[VMS_RS(g_current)].cls, "paint", "(Ljavax/microedition/lcdui/Graphics;)V", true)->class_id ==
          ensure_class("javax/microedition/lcdui/game/GameCanvas"), "subclass has no paint override");
    lifecycle_destroy();
    VMS_EXEC(g_midlet_state) = MidletState::Active; VMS_EXEC(g_notify_destroyed) = false; VMS_PLAT(g_host_paused) = false;

    int visible = canvas(); show(visible);
    int image = game_buffer(visible);
    check(VMS_HEAP(g_heap)[image].pixels.size() == 12 * 9 * 3 &&
          std::all_of(VMS_HEAP(g_heap)[image].pixels.begin(), VMS_HEAP(g_heap)[image].pixels.end(), [](uint8_t p) { return p == 255; }),
          "constructor creates white buffer at screen dimensions");
    check(call(visible, "getWidth", "()I").i == 12 && call(visible, "getHeight", "()I").i == 9,
          "reported canvas dimensions match buffer");
    int gfx = call(visible, "getGraphics", "()Ljavax/microedition/lcdui/Graphics;").ref;
    int second = call(visible, "getGraphics", "()Ljavax/microedition/lcdui/Graphics;").ref;
    check(gfx != second && VMS_RS(g_gfx_target)[gfx] == image && VMS_RS(g_gfx_target)[second] == image,
          "each getGraphics creates a distinct graphics sharing the same buffer");
    check(VMS_RS(g_gfx_color)[gfx] == 0 && VMS_RS(g_gfx_tx)[gfx] == 0 && VMS_RS(g_gfx_ty)[gfx] == 0 &&
          VMS_RS(g_clip)[gfx].w == 12 && VMS_RS(g_clip)[gfx].h == 9, "graphics defaults to black with full buffer clip and zero translation");

    // Default paint must honor the supplied Graphics, including translation
    // and clipping, rather than unconditionally flushing the display.
    int ic = ensure_class("javax/microedition/lcdui/Image");
    int dst = run(find_method(ic, "createImage", "(II)Ljavax/microedition/lcdui/Image;", false),
                  {Value::of_i(12), Value::of_i(9)}).ref;
    int dstgfx = call(dst, "getGraphics", "()Ljavax/microedition/lcdui/Graphics;").ref;
    VMS_HEAP(g_heap)[image].pixels.assign(12 * 9 * 3, 0x71);
    VMS_HEAP(g_heap)[dst].pixels.assign(12 * 9 * 3, 0x22);
    call(dstgfx, "translate", "(II)V", {Value::of_i(2), Value::of_i(1)});
    call(dstgfx, "setClip", "(IIII)V", {Value::of_i(0), Value::of_i(0), Value::of_i(1), Value::of_i(1)});
    call(visible, "paint", "(Ljavax/microedition/lcdui/Graphics;)V", {Value::of_ref(dstgfx)});
    check(image_pixel(dst, 2, 1) == 0x717171 && image_pixel(dst, 3, 1) == 0x222222 &&
          image_pixel(dst, 0, 0) == 0x222222,
          "default paint respects destination translation and clip");
    check(run(find_method(ensure_class("GameCanvasChecks"), "nullPaintCheck", "(LGameCanvasChecks;)I", false),
              {Value::of_ref(visible)}).i == 1, "default paint rejects null Graphics with Java NullPointerException");

    VMS_RS(g_fb).assign(12 * 9 * 3, 0); VMS_RS(g_fb_dirty) = false;
    flush(visible, 0, 0, 1, 1);
    check(pixel(0, 0) == 0x717171 && pixel(1, 0) == 0 && pixel(0, 1) == 0,
          "one-pixel flush leaves region exterior unchanged");
    check(VMS_RS(g_fb_dirty), "visible nonempty flush marks presentation dirty");
    VMS_RS(g_fb).assign(12 * 9 * 3, 0);
    flush(visible, -2, -3, 3, 4);
    check(pixel(0, 0) == 0x717171 && pixel(1, 0) == 0 && pixel(0, 1) == 0,
          "negative origin is clipped to canvas");
    VMS_RS(g_fb).assign(12 * 9 * 3, 0);
    flush(visible, 11, 8, INT32_MAX, INT32_MAX);
    check(pixel(11, 8) == 0x717171 && pixel(10, 8) == 0 && pixel(11, 7) == 0,
          "large positive rectangle endpoints do not overflow");
    VMS_RS(g_fb_dirty) = false;
    auto prior = VMS_RS(g_fb);
    flush(visible, 0, 0, 0, 4); flush(visible, 0, 0, 4, -1);
    flush(visible, INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX);
    flush(visible, INT32_MAX, 0, 1, 1);
    check(VMS_RS(g_fb) == prior && !VMS_RS(g_fb_dirty), "empty and nonintersecting flushes are no-ops");
    int hidden = canvas(); VMS_HEAP(g_heap)[game_buffer(hidden)].pixels.assign(12 * 9 * 3, 0x44);
    call(hidden, "flushGraphics", "()V");
    check(VMS_RS(g_fb) == prior && !VMS_RS(g_fb_dirty), "hidden canvas cannot overwrite visible framebuffer");
    call(visible, "flushGraphics", "()V");
    check(pixel(11, 8) == 0x717171 && pixel(0, 0) == 0x717171, "full flush covers complete canvas");
    check(VMS_HEAP(g_heap)[image].pixels[0] == 0x71, "flush preserves buffer contents");
    VMS_PLAT(g_host_paused) = true; VMS_RS(g_fb_dirty) = false; prior = VMS_RS(g_fb);
    call(visible, "flushGraphics", "()V");
    check(VMS_RS(g_fb) == prior && !VMS_RS(g_fb_dirty), "paused canvas cannot submit");
    VMS_PLAT(g_host_paused) = false;

    dispatch_key(53, false);
    check(field(visible, "presses") == 0 && poll(visible) == (1 << 8), "suppressed FIRE callbacks retain polling");
    check(poll(visible) == (1 << 8), "held key stays set after consuming latch");
    dispatch_key(53, true);
    check(field(visible, "releases") == 0 && poll(visible) == 0, "suppressed release clears held key without callback");
    dispatch_key(49, false); dispatch_key(49, true);
    check(poll(visible) == (1 << 9) && poll(visible) == 0, "short GAME_A tap is latched exactly once");
    const int codes[] = {-1, -2, -3, -4, -5, 49, 51, 55, 57};
    const int actions[] = {1, 6, 2, 5, 8, 9, 10, 11, 12};
    for (int i = 0; i < 9; ++i) {
        dispatch_key(codes[i], false); dispatch_key(codes[i], true);
        check(poll(visible) == (1 << actions[i]) && poll(visible) == 0,
              "directional and numeric game actions map to correct latched bit");
    }
    dispatch_key(48, false); dispatch_key(48, true);
    check(field(visible, "presses") == 1 && field(visible, "releases") == 1 && poll(visible) == 0,
          "non-game key callbacks are never suppressed");
    check(poll(hidden) == 0, "hidden GameCanvas cannot poll current canvas keys");
    dispatch_key(50, false); show(hidden); show(visible);
    check(poll(visible) == 0, "showing canvas clears previous held and latched history");
    dispatch_key(50, true); dispatch_key(50, false);
    check(poll(visible) == (1 << 1), "release then new press after show restores held key");
    dispatch_key(50, true);
    int normal = canvas(false); show(normal);
    dispatch_key(53, false); dispatch_key(53, true);
    check(field(normal, "presses") == 1 && field(normal, "releases") == 1 && poll(normal) == (1 << 8),
          "unsuppressed GameCanvas supports callbacks and short-tap polling together");

    dispatch_key(53, false); show(visible);
    int releases_before=field(visible,"releases");
    release_game_keys();
    check(field(visible,"releases")==releases_before && poll(visible)==0,
          "pause release honors suppression after switching from an unsuppressed canvas");
    dispatch_key(53,false); release_game_keys();
    check(poll(visible)==0,"host release clears suppressed held and latched input");

    // Native reference graph: a live Canvas retains its buffer, but its cache
    // entry is not a root. A separately retained Graphics owns the Image even
    // after the Canvas disappears.
    show(visible); image = game_buffer(visible);
    gc_collect("gamecanvas live buffer");
    check(VMS_HEAP(g_heap)[image].cls >= 0 && VMS_RS(g_offscreen)[visible] == image, "live Canvas retains its buffer across GC");
    gfx = call(visible, "getGraphics", "()Ljavax/microedition/lcdui/Graphics;").ref;
    int owner = ensure_class("GameCanvasChecks");
    Field* kept = find_field(owner, "keptGraphics", "Ljavax/microedition/lcdui/Graphics;");
    VMS_CL(g_classes)[kept->owner].statics[kept->slot] = Value::of_ref(gfx);
    int dead = visible;
    VMS_RS(g_current) = VMS_RS(g_shown) = VMS_RS(g_display) = VMS_EXEC(g_midlet) = 0;
    gc_collect("graphics retains buffer without canvas");
    check(VMS_HEAP(g_heap)[dead].cls < 0 && !VMS_RS(g_offscreen).count(dead), "dead Canvas is collected and its offscreen key removed");
    check(VMS_HEAP(g_heap)[gfx].cls >= 0 && VMS_HEAP(g_heap)[image].cls >= 0 && VMS_RS(g_gfx_target)[gfx] == image,
          "live Graphics retains target Image after Canvas is collected");
    call(gfx, "setColor", "(I)V", {Value::of_i(0xabcdef)});
    call(gfx, "fillRect", "(IIII)V", {Value::of_i(0), Value::of_i(0), Value::of_i(1), Value::of_i(1)});
    check(image_pixel(image, 0, 0) == 0xabcdef, "retained Graphics remains writable after GC");
    int reused = 0;
    const size_t available = VMS_HEAP(g_free).size();
    for (size_t i = 0; i < available; ++i) {
        int ref = alloc_obj(owner);
        if (ref == dead) { reused = ref; break; }
    }
    check(reused != 0, "dead Canvas object ID can be reused");
    int replacement = game_buffer(reused);
    check(replacement != image && VMS_HEAP(g_heap)[replacement].pixels[0] == 255,
          "reused Canvas ID receives a new white buffer instead of old target");
    VMS_CL(g_classes)[kept->owner].statics[kept->slot] = Value{};
    gc_collect("unreachable graphics and buffers");
    check(VMS_HEAP(g_heap)[gfx].cls < 0 && VMS_HEAP(g_heap)[image].cls < 0 && !VMS_RS(g_gfx_target).count(gfx),
          "unreachable Graphics and Image are reclaimed with binding cleanup");
    check(VMS_HEAP(g_heap)[replacement].cls < 0 && !VMS_RS(g_offscreen).count(reused), "unreachable replacement Canvas and buffer are reclaimed together");
    image = run(find_method(ic, "createImage", "(II)Ljavax/microedition/lcdui/Image;", false),
                {Value::of_i(3), Value::of_i(2)}).ref;
    gfx = call(image, "getGraphics", "()Ljavax/microedition/lcdui/Graphics;").ref;
    VMS_CL(g_classes)[kept->owner].statics[kept->slot] = Value::of_ref(gfx);
    gc_collect("standalone Graphics owns image");
    check(VMS_HEAP(g_heap)[gfx].cls >= 0 && VMS_HEAP(g_heap)[image].cls >= 0 && VMS_RS(g_gfx_target)[gfx] == image,
          "Image.getGraphics alone keeps its standalone target alive");
    call(gfx, "fillRect", "(IIII)V", {Value::of_i(0), Value::of_i(0), Value::of_i(3), Value::of_i(2)});
    check(image_pixel(image, 2, 1) == 0, "standalone retained Graphics draws after GC");
    VMS_CL(g_classes)[kept->owner].statics[kept->slot] = Value{};
    gc_collect("standalone native reference released");
    check(VMS_HEAP(g_heap)[gfx].cls < 0 && VMS_HEAP(g_heap)[image].cls < 0 && !VMS_RS(g_gfx_target).count(gfx),
          "standalone target becomes collectible when its Graphics is unreachable");
    std::cout << "PASS GameCanvasGcChecks " << checks << " lifecycle, paint, region, input and native-reference checks\n";
}
