#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main

static void check(bool ok, const char* label) {
    if (!ok) { std::cerr << "FAIL " << label << "\n"; std::exit(1); }
}
#include "array_native_checks.h"
#include "zip_native_checks.h"

int main(int argc, char** argv) {
    VMS_CL(g_classpath) = {"build/test-classes", "romfs/classes"};
    VMS_DIAG(g_log_draw) = false;
    if (argc != 2) return 2;
    std::string name = argv[1];
    VMS_PLAT(g_rms_suite) = rms_class_identity(name, VMS_CL(g_classpath));
    if (name == "YieldFrameChecks") {
        int c=ensure_class(name);
        auto invoke=[&](const char* m){return run(find_method(c,m,"()I",false),{}).i;};
        check(invoke("setup")==1,"yield frame setup");
        thread_resume();
        check(invoke("verify")==1,"yield continues within the same host slice");
        check(VMS_RS(g_repaint) && VMS_EXEC(g_threads).size()==1 && VMS_EXEC(g_threads).front().state==ThreadState::Sleeping &&
              VMS_EXEC(g_threads).front().wake>monotonic_ms(),"frame request retained and sleep deadline honored");
        stop_thread();
        check(invoke("setupYielding")==2,"two infinite-yield workers");
        int64_t start=monotonic_ms();thread_resume();
        check(monotonic_ms()-start<100,"yield loop respects soft host slice budget");
        check(invoke("verifyYielding")==2,"round-robin fairness across yielding peers");
        int64_t until=monotonic_ms()+3000;
        while(VMS_EXEC(g_thread_live) && monotonic_ms()<until)thread_resume();
        check(!VMS_EXEC(g_thread_live),"yield workers terminate normally");
        // Exercise actual Java bytecode and native Thread.yield calls. A
        // generously sized slice makes two yields an observable scheduling
        // boundary rather than an accidental wall-clock timeout.
        stop_thread();
        auto field=[&](const char* key) { return VMS_CL(g_classes)[c].statics[find_field(c,key,"I")->slot].i; };
        auto setup_handoff=[&](int scenario) {
            return run(find_method(c,"setupHandoff","(I)I",false),{Value::of_i(scenario)}).i;
        };
        const int original_slice=VMS_EXEC(g_slice_ms);
        VMS_EXEC(g_slice_ms)=100;
        VMS_RS(g_fb).assign(size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,0);
        for(int cap:{30,60}) {
            stop_thread();VMS_EXEC(g_speed_cap)=cap;speed_sync_clock();
            check(invoke("setup")==1,"capped single-yield worker setup");
            display_visibility();thread_resume();
            check(invoke("verify")==1 && !VMS_EXEC(g_repaint_yield_handoff),
                  "one repaint-owner yield still continues within the same slice at 30/60 cap");
            stop_thread();VMS_EXEC(g_speed_cap)=cap;speed_sync_clock();
            check(setup_handoff(0)==1,"repaint wait worker setup");
            display_visibility();
            thread_resume();
            check(field("phase")==1 && field("turns")==2 && field("paints")==0 && VMS_RS(g_repaint),
                  "second same-PC repaint-owner yield returns to host before paint at 30/60 cap");
            check(VMS_EXEC(g_repaint_yield_handoff) && !VMS_EXEC(g_running_thread) && VMS_EXEC(g_run_depth)==0 && VMS_EXEC(g_pump_depth)==0,
                  "repaint handoff unwinds the worker scheduler to the host");
            check(VMS_EXEC(g_threads).size()==1 && VMS_EXEC(g_threads).front().state==ThreadState::Runnable,
                  "handoff preserves runnable Java continuation rather than sleeping it");
            host_events();
            check(field("paints")==1 && field("paintOnHost")==1 && field("callbackOnHost")==1 &&
                  field("callbackSawPaint")==1 && VMS_EXEC(g_serial_calls).empty(),
                  "host paints pending repaint before callback with host currentThread identity");
            thread_resume();
            check(field("phase")==2 && field("turns")==2 && VMS_EXEC(g_threads).size()==1 &&
                  VMS_EXEC(g_threads).front().state==ThreadState::Sleeping,
                  "paint-ready worker resumes past the same-PC loop and reaches its real sleep");
        }
        for(int cap:{30,60}) {
            stop_thread();VMS_EXEC(g_speed_cap)=cap;speed_sync_clock();
            check(setup_handoff(0)==1,"fresh certified group setup");display_visibility();
            auto& learner=VMS_EXEC(g_threads).back().auto_fps;
            learner.drawing(1000000,1000000/cap,1);
            for(int n=0;n<5;++n){learner.gate_period(50000);learner.drawing(1050000+n*50000,1000000/cap,1);}
            learner.gate_period(50000);
            check(learner.grouped_update() && !learner.helper_drawing(),"certified group has no first draw yet");
            thread_resume();
            check(VMS_EXEC(g_repaint_yield_handoff) && field("phase")==1 && field("turns")==2,
                "fresh certified group first repaint yields to host without waiting for the slice");
            host_events();check(field("paints")==1 && field("paintOnHost")==1,"fresh group paint executed by host");
        }
        for(int scenario=1;scenario<=7;++scenario) {
            stop_thread();VMS_EXEC(g_speed_cap)=scenario==7?0:60;speed_sync_clock();
            check(setup_handoff(scenario)==(scenario==4?2:1),"negative repaint handoff fixture setup");
            display_visibility();
            if(scenario==6) {
                auto& grouped=VMS_EXEC(g_threads).back();
                grouped.auto_fps.drawing(1000000,16667,1);
                for(int n=0;n<5;++n) {
                    grouped.auto_fps.gate_period(50000);
                    grouped.auto_fps.drawing(1050000+n*50000,16667,1);
                }
                check(grouped.auto_fps.grouped_update(),"verified grouped update fixture");
            }
            thread_resume();
            const char* labels[]={"", "monitor ownership prevents repaint yield handoff",
                "hidden Canvas repaint cannot trigger visible paint handoff",
                "repeated repaint resets yield evidence instead of handing off",
                "foreign owner yields cannot hand off another worker's repaint",
                "distinct yield bytecode sites cannot combine handoff evidence",
                "grouped update keeps its repaint helper yields inside the slice",
                "Native mode retains ordinary same-slice yield scheduling"};
            check(field("phase")==2 && field("turns")== (scenario==5?8:4) &&
                  field("paints")==0 && !VMS_EXEC(g_repaint_yield_handoff),labels[scenario]);
            check(VMS_RS(g_repaint) && !VMS_EXEC(g_serial_calls).empty(),
                  "ineligible handoff retains pending host paint and callback");
        }
        stop_thread();VMS_EXEC(g_slice_ms)=original_slice;
        std::cout << "PASS YieldFrameChecks same-frame fairness, repaint wait handoff at 30/60, host identity and seven exclusions\n";
    } else if (name == "PaintSchedulingChecks") {
        int c = ensure_class(name);
        auto invoke = [&](const char* method) { return run(find_method(c,method,"()I",false),{}).i; };
        auto field = [&](const char* key, const char* desc="I") { return VMS_CL(g_classes)[c].statics[find_field(c,key,desc)->slot].i; };
        check(invoke("setup") == 1,"paint fixture setup");
        int cuts = VMS_EXEC(g_cuts);
        repaint_current();
        check(field("paints") == 1 && field("checksum") == 700000 && field("workerDone") == 0 && VMS_EXEC(g_cuts) > cuts,
              "budget does not invalidate resources inside paint");
        thread_resume();
        check(field("workerDone") == 1,"updater resumes after paint");
        check(invoke("syncService") == 4 && !VMS_RS(g_repaint),"service coalescing, no pending, hidden and recursive requests");
        check(invoke("setupBlocking") == 1,"blocking paint setup");
        repaint_current();
        check(field("workerDone") == 1,"explicit paint sleep permits worker progress");
        check(invoke("setupWorkerService") == 1,"worker service setup");
        int64_t until = monotonic_ms()+3000;
        while (VMS_EXEC(g_thread_live) && monotonic_ms()<until) thread_resume();
        check(!VMS_EXEC(g_thread_live) && field("afterService","Z") == 1,"native worker continuation waits for paint");
        check(invoke("setupWorkerException") == 1,"worker paint exception setup");
        until = monotonic_ms()+3000;
        while (VMS_EXEC(g_thread_live) && monotonic_ms()<until) thread_resume();
        check(!VMS_EXEC(g_thread_live) && field("afterService","Z") == 1,"paint exception crosses native continuation to Java catch");
        check(VMS_RS(g_paint_depth) == 0 && VMS_EXEC(g_run_depth) == 0,"paint and run scopes balanced");
        std::cout << "PASS PaintSchedulingChecks 11 frame lifetime, service, blocking and exception\n";
    } else if (name == "PngHeaderChecks") {
        std::vector<uint8_t> pixels={255,0,0,0,255,0};
        const char* path="build/png-header-fixture.png";
        write_png(path,pixels,2,1);
        std::vector<uint8_t> bytes;check(read_whole(path,bytes),"PNG fixture");std::remove(path);
        size_t pos=8;
        while(pos+12<bytes.size() && std::string((const char*)&bytes[pos+4],4)!="IDAT")pos+=12+ru32(&bytes[pos]);
        check(pos+12<bytes.size(),"IDAT found");
        auto checksum=[&]() {
            size_t n=ru32(&bytes[pos]);uint32_t crc=crc32(bytes.data()+pos+4,n+4);
            for(int i=0;i<4;++i)bytes[pos+8+n+i]=(uint8_t)(crc>>(24-8*i));
        };
        auto decoded=[&]() {
            std::vector<uint8_t> rgb,alpha;int w=0,h=0;std::string error;
            bool ok=decode_png(bytes,rgb,alpha,w,h,error);
            if(ok)check(w==2 && h==1 && rgb==pixels && alpha==std::vector<uint8_t>(2,255),"PNG exact pixels");
            return ok;
        };
        for(int window=0;window<8;++window) {
            bytes[pos+8]=(uint8_t)((window<<4)|8);
            bytes[pos+9]=(uint8_t)((31-((bytes[pos+8]<<8)%31))%31);checksum();
            check(decoded(),"valid PNG zlib window");
        }
        bytes[pos+9]^=1;checksum();check(!decoded(),"invalid zlib header checksum");
        bytes[pos+8]=0x88;bytes[pos+9]=(uint8_t)((31-((0x88<<8)%31))%31);checksum();check(!decoded(),"oversized zlib window rejected");
        bytes[pos+8]=0x78;bytes[pos+9]=0x20;
        bytes[pos+9]+=(uint8_t)((31-(((0x78<<8)|0x20)%31))%31);checksum();check(!decoded(),"PNG preset dictionary rejected");
        std::cout << "PASS PngHeaderChecks eight zlib windows and invalid headers\n";
    } else if (name == "ZipNativeChecks") {
        run_zip_native_checks();
    } else if (name == "ArrayNativeChecks") {
        run_array_native_checks();
    } else if (name == "ThrowableGcChecks") {
        int c=ensure_class(name);
        auto invoke=[&](const char* m) {return run(find_method(c,m,"()I",false),{}).i;};
        check(invoke("setup")==1,"capture saved trace");
        gc_collect("retained exception trace");
        check(VMS_DIAG(g_exception_traces).size()==1 && invoke("verify")==1,"trace survives full GC with its object");
        check(invoke("release")==1,"release saved exception");
        gc_collect("exception trace reclaimed");
        check(VMS_DIAG(g_exception_traces).empty(),"trace metadata does not keep dead exceptions alive");
        for(size_t i=0, n=VMS_HEAP(g_free).size();i<n;++i) {
            int ref=alloc_obj(ensure_class("java/lang/Object"));
            check(!VMS_DIAG(g_exception_traces).count(ref),"reused object id has no stale exception trace");
        }
        std::cout << "PASS ThrowableGcChecks saved trace, full GC, reclamation, reused ids\n";
    } else if (name == "ThreadGcChecks") {
        int c=ensure_class(name);
        auto invoke=[&](const char* method) { return run(find_method(c,method,"()I",false),{}).i; };
        auto field=[&](const char* key) { return VMS_CL(g_classes)[c].statics[find_field(c,key,"I")->slot].i; };
        auto drain=[&]() {
            int64_t end=monotonic_ms()+5000;
            while(VMS_EXEC(g_thread_live) && monotonic_ms()<end) thread_resume();
            check(!VMS_EXEC(g_thread_live),"workers terminate before deadline");
        };
        check(invoke("setup")==3,"three unreferenced Java workers started");
        gc_collect("enqueued thread roots");
        int64_t end=monotonic_ms()+5000;
        while(field("parked")!=3 && monotonic_ms()<end) thread_resume();
        check(field("parked")==3,"all workers park");
        gc_collect("parked thread roots");
        size_t parked_bytes=VMS_HEAP(g_array_payload_live);
        int waiting=0;
        Method* interrupt=find_method(ensure_class("java/lang/Thread"),"interrupt","()V",false);
        for(auto& t:VMS_EXEC(g_threads)) if(t.state==ThreadState::Waiting) {
            ++waiting; run(interrupt,{Value::of_ref(t.ref)});
        }
        check(waiting==3,"all waiting contexts retained");
        drain();
        check(field("completed")==3 && field("failures")==0,"arrays, aliases, locks and interrupt state survive GC");
        gc_collect("finished thread roots released");
        check(parked_bytes>=VMS_HEAP(g_array_payload_live)+3*65536,"terminated stacks release payloads");
        int cuts=VMS_EXEC(g_cuts);
        check(invoke("setupCpu")==2,"two CPU workers started");
        drain();
        check(field("cpuDone")==2 && field("failures")==0 && VMS_EXEC(g_cuts)>cuts,"instruction slices resume both CPU workers");
        check(invoke("setupThrowing")==2,"throwing worker and peer started");
        drain();
        check(field("survived")==1,"uncaught worker failure releases monitor and preserves peer");
        std::cout << "PASS ThreadGcChecks parked roots, stack release, instruction cuts, uncaught worker\n";
    } else if (name == "SchedulerChecks") {
        int c = ensure_class(name); int ref = alloc_obj(c);
        auto* init = find_method(c,"<init>","()V",false); run(init,{Value::of_ref(ref)});
        check(run(find_method(c,"runChecks","()I",false),{}).i == 1,"negative sleep");
        run(find_method(ensure_class("java/lang/Thread"),"start","()V",false),{Value::of_ref(ref)});
        thread_resume();
        Field* steps = find_field(c,"steps","I");
        check(VMS_RS(g_repaint) && VMS_CL(g_classes)[c].statics[steps->slot].i == 1,"repaint does not yield before sleep");
        auto* t=thread_context(ref);
        check(t && t->state==ThreadState::Sleeping && t->wake>monotonic_ms(),"sleep sets deadline");
        thread_resume();
        check(VMS_CL(g_classes)[c].statics[steps->slot].i == 1,"sleep never resumes early");
        t->wake=monotonic_ms();thread_resume();
        check(!VMS_EXEC(g_thread_live) && VMS_CL(g_classes)[c].statics[steps->slot].i == 2,"resume at deadline");
        std::cout << "PASS SchedulerChecks 5\n";
    } else if (name == "NativeChecks") {
        int c = ensure_class("java/lang/Object");
        Method wide; wide.class_id = c; wide.name = "category2"; wide.desc = "()I"; wide.max_stack = 8;
        // dup_x2 form 2: long,int -> int,long,int. Pop all but bottom int.
        wide.code = {9, 5, 91, 87, 88, 172};
        check(run(&wide, {}).i == 2, "dup_x2 category2");
        // dup2_x2 form 2: long,int,int -> int,int,long,int,int.
        wide.code = {9, 5, 6, 94, 88, 88, 96, 172};
        check(run(&wide, {}).i == 5, "dup2_x2 category2 lower");
        wide.code = {9, 10, 94, 88, 88, 173}; wide.desc = "()J";
        check(run(&wide, {}).l == 1, "dup2_x2 two wide");
        int sb = ensure_class("java/lang/StringBuffer");
        check(find_method(sb, "append", "(I)Ljava/lang/StringBuffer;", true), "StringBuffer exact ABI");
        check(!find_method(sb, "append", "(I)Ljava/lang/Object;", true), "strict return descriptor");
        int fl = ensure_class("java/lang/Float");
        check(run(find_method(fl,"intBitsToFloat","(I)F",false), {Value::of_i(0x3f800000)}).i == 0x3f800000, "direct native result");
        int p = ensure_class("javax/microedition/media/Player");
        check(find_method(p,"getState","()I",false), "Player state descriptor");
        check(find_method(p,"setMediaTime","(J)J",false), "Player mediaTime descriptor");
        int canvas=ensure_class("javax/microedition/lcdui/Canvas"), cref=alloc_obj(canvas);
        Method* region=find_method(canvas,"repaint","(IIII)V",false);
        check(region!=nullptr,"region repaint ABI");
        const int regions[][5]={{0,0,1,1,1},{-1,-1,2,2,1},{0,0,0,10,0},{0,0,1,-1,0},
            {240,0,1,1,0},{0,320,1,1,0},{INT32_MIN,0,INT32_MAX,1,0},{-1,0,INT32_MAX,1,1}};
        for(const auto& r:regions) {
            VMS_RS(g_repaint)=false;VMS_EXEC(g_yield)=false;
            run(region,{Value::of_ref(cref),Value::of_i(r[0]),Value::of_i(r[1]),Value::of_i(r[2]),Value::of_i(r[3])});
            check(VMS_RS(g_repaint)==(r[4]!=0) && !VMS_EXEC(g_yield),"region repaint intersection, bounds and no yield");
        }
        std::cout << "PASS NativeChecks 17\n";
    } else {
        if (name == "ConnectionAbiChecks") VMS_CL(g_jar_files)["connection-abi.bin"] = {1,35,69,103};
        int c = ensure_class(name);
        Method* m = find_method(c, "run", "()I", false);
        check(m != nullptr, "test entry");
        Value result = run(m, {});
        check(result.i > 0, "positive check result");
        if(name=="M3GChecks") {
            check(VMS_RS(g_m3g_native_triangles)>=4 && VMS_RS(g_m3g_native_pixels)>0,"native M3G raster actually handles real Java triangles");
            check(VMS_RS(g_m3g_native_vertices)>0 && VMS_RS(g_m3g_native_matrices)>0,"native geometry handles real Java field layouts");
            check(VMS_RS(g_m3g_native_backgrounds)>0 && VMS_RS(g_m3g_native_background_pixels)>0,"native background uses real Java fields");
        }
        std::cout << "PASS " << name << " " << result.i << "\n";
    }
}
