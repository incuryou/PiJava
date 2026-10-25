#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) { return "build/compat234-owned-test-state.bin"; }
#include "../src/instant_state.inc"

static void requireMedia234(bool ok,const char* label) {
    if(!ok){std::cerr<<"FAIL "<<label<<"\n";std::exit(1);}
}
int main(int argc,char** argv) {
    VMS_CL(g_classpath)={"build/test-classes",argc>2?argv[2]:"romfs/classes"};
    configure_runtime_diagnostics();
    VMS_DIAG(g_log_draw)=false;
    std::cout<<"INFO native_jit_enabled="<<VMS_NATIVE(g_native_jit).enabled<<" available="<<VMS_NATIVE(g_native_jit).available<<"\n";
    if(argc>1) {
        int c=ensure_class(argv[1]);
        Method* method=find_method(c,argc>3?argv[3]:"run","()I",false);
        requireMedia234(method!=nullptr,"Java test entry exists");
        Value result=run(method,{});
        requireMedia234(!VMS_EXEC(g_thrown) && result.i>0,"Java test returns positive assertions without throw");
        std::cout<<"PASS "<<argv[1]<<" assertions="<<result.i<<"\n";
        stop_thread();p7::pool().shutdown();return 0;
    }
    launch_class("RateCompat234Checks","compat234-owned-fixture.jar");
    requireMedia234(VMS_EXEC(g_midlet)>0 && VMS_EXEC(g_midlet_state)==MidletState::Active &&
        VMS_RS(g_fb).size()==size_t(VMS_RS(g_screen_w))*VMS_RS(g_screen_h)*3,
        "normal host launch initializes MIDlet lifecycle and framebuffer");
    int c=ensure_class("RateCompat234Checks");
    auto integer=[&](const char* name){Value value=run(find_method(c,name,"()I",false),{});requireMedia234(!VMS_EXEC(g_thrown),name);return value.i;};
    auto time=[&](){return run(find_method(c,"time","()J",false),{}).l;};
    Value app=run(find_method(c,"setup","(I)Ljavax/microedition/midlet/MIDlet;",false),{Value::of_i(200000)});
    requireMedia234(app.ref>0 && !VMS_EXEC(g_thrown),"real player and MIDlet setup");
    VMS_EXEC(g_midlet)=app.ref;
    requireMedia234(integer("value")==200000,"configured rate");
    const int rateSlot=find_field(c,"rate","Ljavax/microedition/media/control/RateControl;")->slot;
    const int playerSlot=find_field(c,"player","Ljavax/microedition/media/Player;")->slot;
    const int controlRef=VMS_CL(g_classes)[c].statics[rateSlot].ref;
    const int controlClass=VMS_HEAP(g_heap)[controlRef].cls;
    const uint32_t controlGeneration=VMS_HEAP(g_heap)[controlRef].generation;
    const int playerRef=VMS_CL(g_classes)[c].statics[playerSlot].ref;
    const int playerClass=VMS_HEAP(g_heap)[playerRef].cls;
    const int volumeSlot=find_field(playerClass,"volume","Ljavax/microedition/media/NativePlayer$NativeVolumeControl;")->slot;
    const int volumeRef=VMS_HEAP(g_heap)[playerRef].fields[volumeSlot].ref;
    auto volumeLevel=[&](){return run(find_method(VMS_HEAP(g_heap)[volumeRef].cls,"getLevel","()I",false),{Value::of_ref(volumeRef)}).i;};
    auto setVolume=[&](int value){return run(find_method(VMS_HEAP(g_heap)[volumeRef].cls,"setLevel","(I)I",false),{Value::of_ref(volumeRef),Value::of_i(value)}).i;};
    requireMedia234(setVolume(37)==37 && volumeLevel()==37,"existing volume object works before snapshot");
    // Keep only a non-root C++ integer: the new Java registry must retain controlRef.
    VMS_CL(g_classes)[c].statics[rateSlot]=Value::of_ref(0);
    uint64_t collections=VMS_HEAP(g_gc_cycles);gc_collect("compat234-rate-registry");
    requireMedia234(VMS_HEAP(g_gc_cycles)==collections+1,"full GC actually ran");
    requireMedia234(VMS_CL(g_classes)[c].statics[rateSlot].ref==0 &&
        VMS_HEAP(g_heap)[controlRef].cls==controlClass && VMS_HEAP(g_heap)[controlRef].generation==controlGeneration,
        "registry retains control with no application control reference");
    audio_suspend(true);
    SavState saved;requireMedia234(capture_state(saved,"compat234-owned-fixture.jar"),"capture existing format");
    audio_suspend(false);
    std::string bytes;requireMedia234(encode_state(saved,bytes)&&bytes.substr(0,4)=="J2S5","J2S5 remains unchanged");
    SavState decoded;requireMedia234(decode_state(bytes,decoded),"decode snapshot");
    VMS_PLAT(g_audio).render(nullptr,4800);requireMedia234(time()==200000,"2x actual media advance");
    requireMedia234(run(find_method(controlClass,"setRate","(I)I",false),{Value::of_ref(controlRef),Value::of_i(50000)}).i==50000,"live rate change");
    requireMedia234(setVolume(65)==65,"live volume change after capture");
    requireMedia234(apply_state(decoded),"restore STARTED player");
    c=ensure_class("RateCompat234Checks");
    requireMedia234(VMS_CL(g_classes)[c].statics[rateSlot].ref==0,"restore keeps application control reference absent");
    // Do not call any Java RateControl method before this first audio render.
    requireMedia234(VMS_PLAT(g_audio).endHeadroom()==24000,"restore rate rebound before audio resumes");
    VMS_PLAT(g_audio).render(nullptr,4800);requireMedia234(time()==200000,"restored first block uses saved rate");
    requireMedia234(VMS_CL(g_classes)[c].statics[playerSlot].ref==playerRef &&
        VMS_HEAP(g_heap)[playerRef].fields[volumeSlot].ref==volumeRef && volumeLevel()==37,
        "restored original volume identity and saved level remain valid");
    requireMedia234(setVolume(41)==41 && VMS_PLAT(g_audio).capture().front().volume==41,
        "retained restored volume object still changes native mixer");
    VMS_CL(g_classes)[c].statics[rateSlot]=Value::of_ref(controlRef);
    requireMedia234(integer("value")==200000,"Java rate agrees with restored engine");
    requireMedia234(integer("close")==0 && integer("closedControl")==1,"close invalidates retained control");
    requireMedia234(VMS_PLAT(g_audio).loadedPlayers()==0,"close releases native audio");
    app=run(find_method(c,"setup","(I)Ljavax/microedition/midlet/MIDlet;",false),{Value::of_i(50000)});
    VMS_EXEC(g_midlet)=app.ref;stop_thread();
    requireMedia234(VMS_PLAT(g_audio).loadedPlayers()==0 && integer("closedControl")==1,"resetAll releases registry and control owner");
    // A valid metadata-free snapshot uses the old NativePlayer field layout and
    // an empty rate registry; it is a synthetic legacy boundary, not an old archive.
    app=run(find_method(c,"setup","(I)Ljavax/microedition/midlet/MIDlet;",false),{Value::of_i(100000)});
    VMS_EXEC(g_midlet)=app.ref;
    VMS_CL(g_classes)[c].statics[rateSlot]=Value::of_ref(0);
    const int restoredControlClass=VMS_CL(g_class_index).at("javax/microedition/media/RateControlImpl");
    int registrySlot=find_field(restoredControlClass,"controls","[Ljavax/microedition/media/RateControlImpl;")->slot;
    int registry=VMS_CL(g_classes)[restoredControlClass].statics[registrySlot].ref;
    for(size_t i=0;i<VMS_HEAP(g_heap)[registry].elems.size();++i)VMS_HEAP(g_heap)[registry].elems.set(i,Value::of_ref(0));
    gc_collect("compat234-no-rate-metadata");
    audio_suspend(true);SavState legacy;requireMedia234(capture_state(legacy,"compat234-owned-fixture.jar"),"capture metadata-free snapshot");audio_suspend(false);
    for(const auto& object:legacy.heap) requireMedia234(object.cls!="javax/microedition/media/RateControlImpl","snapshot has no rate control objects");
    requireMedia234(apply_state(legacy),"restore metadata-free player snapshot");
    c=ensure_class("RateCompat234Checks");
    requireMedia234(VMS_PLAT(g_audio).endHeadroom()==48000,"metadata-free restored player defaults to 1x");
    VMS_PLAT(g_audio).render(nullptr,4800);requireMedia234(time()==100000,"metadata-free first audio block advances at 1x");
    requireMedia234(integer("close")==0,"metadata-free player still closes normally");
    std::cout<<"PASS registry-only rate GC/restore, original Volume identity, unchanged J2S5, metadata-free 1x, close and resetAll\n";
    stop_thread();p7::pool().shutdown();
}
