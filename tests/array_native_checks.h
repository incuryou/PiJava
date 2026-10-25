// Included after the VM and check() by runtime_checks.cpp.
static void run_array_native_checks() {
    const char* types[] = {"[B", "[Z", "[C", "[S", "[I", "[F", "[J", "[D", "[Ljava/lang/Object;", "[[I"};
    const size_t widths[] = {1,1,2,2,4,4,8,8,4,4};
    for (size_t i = 0; i < 10; ++i) {
        ArrayStorage a;
        check(a.reset(types[i], 1048576), "allocate compact array");
        check(a.payload_bytes() == 1048576 * widths[i], "exact compact payload");
        check(a.capacity_bytes() >= a.payload_bytes(), "capacity records allocation");
        check(a.references() == (i >= 8), "reference kind");
        Value zero = a.get(1048575);
        check(i >= 6 && i < 8 ? (zero.is_long && zero.l == 0) : (!zero.is_long && zero.i == 0 && zero.ref == 0), "typed default zero");
    }
    ArrayStorage a,b;
    check(a.reset("[B",4), "byte fixture"); a.set(0,Value::of_i(-128));a.set(1,Value::of_i(255));
    check(a.get(0).i == -128 && a.get(1).i == -1,"sign extend byte");
    check(!a.reset("[V",4) && a.size() == 4 && a.get(0).i == -128,"invalid reset preserves contents");
    check(!a.reset("[I",(size_t)INT32_MAX+1),"reject oversized array");
    check(!a.reset("[Ljava//lang/Object;",2),"reject malformed descriptor");
    check(b.reset("[Z",4) && !b.copy_from(a,0,0,4),"primitive kind mismatch");
    b.set(0,Value::of_i(2));b.set(1,Value::of_i(3));
    check(b.get(0).i == 0 && b.get(1).i == 1,"boolean low bit");
    check(a.copy_from(a,0,4,0),"empty copy at end");
    check(!a.copy_from(a,0,SIZE_MAX,1) && !a.copy_from(a,SIZE_MAX,0,0),"copy range overflow");

    int object = ensure_class("java/lang/Object");
    int orphan = alloc_obj(object);
    int primitive = alloc_array("[I",4096);
    VMS_HEAP(g_heap)[primitive].elems.set(0,Value::of_i(orphan));
    VMS_RS(g_current) = primitive;
    uint64_t before = VMS_HEAP(g_gc_array_refs_scanned);
    gc_collect("primitive-root-check");
    check(VMS_HEAP(g_gc_array_refs_scanned) == before,"primitive payload never traced as references");
    check(std::find(VMS_HEAP(g_free).begin(),VMS_HEAP(g_free).end(),orphan) != VMS_HEAP(g_free).end(),"integer looking like reference does not retain object");
    check(VMS_HEAP(g_heap)[primitive].is_array && VMS_HEAP(g_heap)[primitive].elems.get(0).i == orphan,"primitive root retained");
    check(VMS_HEAP(g_array_payload_live) == 4096*4,"live compact payload accounting");

    int refs = alloc_array("[Ljava/lang/Object;",2);
    int child = alloc_obj(object);
    int nested = alloc_array("[J",3);
    VMS_HEAP(g_heap)[nested].elems.set(2,Value::of_l(INT64_MIN));
    VMS_HEAP(g_heap)[refs].elems.set(0,Value::of_ref(child));
    VMS_HEAP(g_heap)[refs].elems.set(1,Value::of_ref(nested));
    VMS_RS(g_current) = refs;
    before = VMS_HEAP(g_gc_array_refs_scanned);
    gc_collect("reference-root-check");
    check(VMS_HEAP(g_gc_array_refs_scanned) == before+2,"only reference elements traced");
    check(VMS_HEAP(g_heap)[child].cls == object && VMS_HEAP(g_heap)[nested].is_array,"transitive reference roots retained");
    check(VMS_HEAP(g_heap)[nested].elems.get(2).l == INT64_MIN,"wide payload survives GC");
    check(VMS_HEAP(g_array_payload_live) == 2*4+3*8,"reclaimed primitive payload deducted");
    uint64_t peak = VMS_HEAP(g_array_payload_peak);
    VMS_RS(g_current) = 0;
    gc_collect("release-array-roots");
    check(VMS_HEAP(g_array_payload_live) == 0 && VMS_HEAP(g_array_payload_peak) == peak,"payload drops, peak retained");
    int reused = alloc_array("[D",2);
    check(VMS_HEAP(g_heap)[reused].elems.get(0).is_long && VMS_HEAP(g_heap)[reused].elems.get(0).l == 0,"recycled slot reinitializes type and payload");
    gc_collect("recycled-unrooted-array");
    check(VMS_HEAP(g_array_payload_live) == 0,"recycled allocation accounting");
    // Few large allocations must trigger the safe-point collector too.
    alloc_array("[B",4*1024*1024);
    check(VMS_HEAP(g_alloc_since) < 256,"large-array fixture below object trigger");
    uint64_t cycles=VMS_HEAP(g_gc_cycles); gc_safepoint(GcPoint::Threads); // S6: was maybe_gc("array-byte-threshold")
    check(VMS_HEAP(g_gc_cycles) == cycles+1 && VMS_HEAP(g_array_payload_live) == 0,"byte allocation triggers safe-point GC");
    // Native image and resource paths use packed int[] / byte[] as well.
    int imageClass=ensure_class("javax/microedition/lcdui/Image");
    int rgb=alloc_array("[I",4);
    const int colors[]={0x123456,0xabcdef,0x445566,0x778899};
    for(int i=0;i<4;++i)VMS_HEAP(g_heap)[rgb].elems.set(i,Value::of_i(colors[i]));
    int picture=run(find_method(imageClass,"createRGBImage","([IIIZ)Ljavax/microedition/lcdui/Image;",false),
        {Value::of_ref(rgb),Value::of_i(2),Value::of_i(2),Value::of_i(0)}).ref;
    check(picture && VMS_HEAP(g_heap)[picture].width==2,"packed int[] creates RGB image");
    int pixels=alloc_array("[I",8);
    run(find_method(imageClass,"getRGB","([IIIIIII)V",false),
        {Value::of_ref(picture),Value::of_ref(pixels),Value::of_i(2),Value::of_i(2),Value::of_i(0),Value::of_i(0),Value::of_i(2),Value::of_i(2)});
    for(int i=0;i<4;++i)check((VMS_HEAP(g_heap)[pixels].elems.get(2+i).i&0xffffff)==colors[i],"packed int[] receives getRGB at offset");
    check(VMS_HEAP(g_heap)[pixels].elems.get(0).i==0 && VMS_HEAP(g_heap)[pixels].elems.get(7).i==0,"getRGB preserves outside range");
    // createRGBImage is immutable by MIDP contract; draw onto a separate mutable surface.
    int surface=run(find_method(imageClass,"createImage","(II)Ljavax/microedition/lcdui/Image;",false),
        {Value::of_i(2),Value::of_i(2)}).ref;
    int graphics=run(find_method(imageClass,"getGraphics","()Ljavax/microedition/lcdui/Graphics;",false),{Value::of_ref(surface)}).ref;
    int graphicsClass=ensure_class("javax/microedition/lcdui/Graphics");
    VMS_HEAP(g_heap)[rgb].elems.set(0,Value::of_i(0x998877));
    run(find_method(graphicsClass,"drawRGB","([IIIIIIIZ)V",false),
        {Value::of_ref(graphics),Value::of_ref(rgb),Value::of_i(0),Value::of_i(2),Value::of_i(0),Value::of_i(0),Value::of_i(2),Value::of_i(2),Value::of_i(0)});
    check((get_px(VMS_HEAP(g_heap)[surface].pixels,2,2,0,0)&0xffffff)==0x998877,"drawRGB reads compact source");
    std::vector<uint8_t> resource={0,1,127,128,255};
    int stream=stream_from(resource);
    Method* read=find_method(VMS_HEAP(g_heap)[stream].cls,"read","()I",true);
    for(auto byte:resource)check(run(read,{Value::of_ref(stream)}).i==byte,"resource stream preserves unsigned byte reads");
    check(run(read,{Value::of_ref(stream)}).i==-1,"resource stream EOF");
    std::cout << "PASS ArrayNativeChecks compact widths, raw storage, GC roots, byte trigger, image and resource natives\n";
}
