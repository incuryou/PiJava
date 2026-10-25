static void command_gc_hook();
#define J2ME_TEST_REENTRY_GC_HOOK() command_gc_hook()
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
#include "../src/host_keymap.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&){return "unused-canvas-command-state";}
#include "../src/instant_state.inc"
static bool force_command_gc=false;
static void command_gc_hook(){if(force_command_gc){force_command_gc=false;gc_collect("command dispatch reentry");}}

static int checks=0,failures=0;
static void check(bool ok,const char* label) {
    ++checks;if(!ok){++failures;std::cerr<<"FAIL CanvasCommandChecks "<<label<<"\n";}
}
static Value& stat(const char* name,const char* desc="I") {
    Field* f=find_field(ensure_class("CanvasCommandChecks"),name,desc);
    return VMS_CL(g_classes)[f->owner].statics[f->slot];
}
static int value(const char* name){return stat(name).i;}
static void configure(int kind=0,int count=2,bool listener=true) {
    run(find_method(ensure_class("CanvasCommandChecks"),"configure","(IIZ)V",false),
        {Value::of_i(kind),Value::of_i(count),Value::of_i(listener)});
    display_visibility();
}
static void tap(int key){dispatch_key(key,false);dispatch_key(key,true);}
static void command_switch_to(int kind) {
    release_game_keys();configure();const int old=VMS_RS(g_current);
    RootScope roots(ReentrySite::DispatchKey,{&old});configure(kind);
    stat("next","Ljavax/microedition/lcdui/Displayable;")=Value::of_ref(VMS_RS(g_current));
    VMS_RS(g_current)=old;display_visibility();stat("mode")=Value::of_i(1);
}
static void tracker_capture_checks() {
    const auto emit=[](int key,bool release){dispatch_key(key,release);};
    host_keymap::Config config;config.automatic=false;config.bindings=host_keymap::empty_bindings();
    host_keymap::assign(config,false,host_keymap::SoftL,0,host_keymap::X);
    host_keymap::assign(config,false,host_keymap::SoftL,1,host_keymap::Y);
    const uint32_t x=1u<<host_keymap::X,y=1u<<host_keymap::Y;
    for(int kind:{1,2,3,0,4}) {
        command_switch_to(kind);host_keymap::Tracker tracker;uint32_t blocked=0;
        const auto step=[&](uint32_t held){blocked&=held;tracker.update(host_keymap::codes(config,false,held&~blocked),emit);};
        step(x);display_visibility();
        host_freeze236(true);tracker.clear();blocked|=x;
        // Physical release happens inside the host menu. No Tracker update or
        // dispatch_key release is emitted; close_keymap clears Tracker again.
        blocked&=0;tracker.clear();host_freeze236(false);
        if(kind==0)run(find_method(ensure_class("javax/microedition/lcdui/Displayable"),"setCommandListener","(Ljavax/microedition/lcdui/CommandListener;)V",false),{Value::of_ref(VMS_RS(g_current)),Value::of_ref(0)});
        stat("actions")=Value::of_i(0);stat("presses")=Value::of_i(0);stat("releases")=Value::of_i(0);
        step(x|y);step(y);step(0);
        const bool plain=kind!=4;
        std::cout<<"tracker reset kind="<<kind<<" presses="<<value("presses")<<" releases="<<value("releases")<<" pressed_keys="<<VMS_PLAT(g_pressed_keys).size()<<"\n";
        check((!plain||(value("presses")==1&&value("releases")==1))&&VMS_PLAT(g_pressed_keys).empty()&&VMS_PLAT(g_command_keys).empty(),"new host press after silent Tracker clear retains complete release and empty held state");
    }
    command_switch_to(2);host_keymap::Tracker held;
    held.update({-6},emit);display_visibility();held.update({-6},emit);held.update({},emit);
    check(value("presses")==0&&value("releases")==0&&VMS_PLAT(g_pressed_keys).empty(),"same held host press across screen switch keeps captured late release");
    command_switch_to(2);dispatch_key(-6,false);display_visibility();host_freeze236(true);
    dispatch_key(-6,false);host_freeze236(false);dispatch_key(-6,true);
    check(value("presses")==0&&value("releases")==0&&VMS_PLAT(g_command_keys).empty(),"ignored paused press does not retire genuine old release capture");
    configure();dispatch_key(-6,false);dispatch_key(-6,false);dispatch_key(-6,true);
    check(value("actions")==2&&value("presses")==0&&value("releases")==0&&VMS_PLAT(g_command_keys).empty(),"repeated explicit command press renews capture without leaking release");
    configure(0,4);tap(-6);host_keymap::Tracker raw;raw.update({1},emit);
    configure(2);release_game_keys();raw.clear();raw.update({1},emit);
    check(VMS_PLAT(g_pressed_keys).count(-1)==1,"new raw press retains FullCanvas direction adaptation");
    raw.update({},emit);
    check(value("presses")==1&&value("releases")==1&&VMS_PLAT(g_pressed_keys).empty(),"stale raw menu key capture cannot consume adapted FullCanvas release");
}
int main(int argc,char** argv) {
    VMS_CL(g_classpath)={argc>1?argv[1]:"build/test-classes","romfs/classes"};
    VMS_DIAG(g_log_draw)=false;
    launch_class("CanvasCommandChecks");
    if(argc>2&&std::string(argv[2])=="throw") {
        stat("mode")=Value::of_i(4);tap(-6);
        std::cerr<<"FAIL expected uncaught command callback exception\n";return 2;
    }
    tap(-6);
    std::cout<<"ordinary overridden keyPressed: commandAction="<<value("actions")<<" expected=1\n";
    check(value("actions")==1&&value("lastType")==4,"left softkey reaches registered CommandListener");
    check(value("presses")==0&&value("releases")==0,"consumed command excludes both Canvas key callbacks");
    tap(-7);check(value("actions")==2&&value("lastType")==7,"right softkey retains EXIT binding");
    check(value("presses")==0&&value("releases")==0,"duplicate addCommand does not duplicate callbacks");
    configure();for(int key:{-5,8,53,-1,-2,-3,-4,49,50,56,48})tap(key);
    check(value("actions")==0&&value("presses")==11&&value("releases")==11,"ordinary FIRE numeric and direction keys stay Canvas callbacks");
    for(int kind:{1,2,3}) {
        configure(kind);tap(-6);tap(-7);tap(53);
        check(value("actions")==0&&value("presses")==3&&value("releases")==3,"fullscreen FullCanvas and GameCanvas retain key callbacks");
    }
    configure(0,0);tap(-6);tap(-7);
    check(value("actions")==0&&value("presses")==2&&value("releases")==2,"no commands retains softkey callbacks");
    configure(0,2,false);tap(-6);tap(-7);
    check(value("actions")==0&&value("presses")==2&&value("releases")==2,"no listener retains softkey callbacks");
    configure(0,1);tap(-7);tap(-6);
    check(value("actions")==1&&value("presses")==1&&value("releases")==1,"unbound softkey retains paired callbacks");
    configure(0,4);tap(-6);
    check(value("actions")==0&&value("presses")==0,"three plus commands open existing menu");
    tap(-2);tap(-6);
    check(value("actions")==1&&value("lastType")==4&&value("presses")==0&&value("releases")==0,"menu navigation and selection consume matching releases");
    tap(53);check(value("presses")==1&&value("releases")==1,"closed menu immediately returns FIRE to Canvas");
    tap(-6);tap(-7);tap(-1);
    check(value("actions")==1&&value("presses")==2&&value("releases")==2,"menu cancel restores ordinary key callbacks");
    configure(4);tap(-6);check(value("actions")==1,"Form retains its existing command handling");
    configure();VMS_RS(g_shown)=0;tap(-6);
    check(value("actions")==0&&value("presses")==1&&value("releases")==1,"not shown Canvas does not intercept command keys");
    configure();VMS_PLAT(g_host_paused)=true;tap(-6);VMS_PLAT(g_host_paused)=false;
    check(value("actions")==0&&value("presses")==0,"paused display does not dispatch commands");
    configure();const auto gc_before=VMS_HEAP(g_gc_cycles);force_command_gc=true;tap(-6);
    check(value("actions")==1&&value("releases")==0&&VMS_HEAP(g_gc_cycles)>gc_before,"command dispatch survives garbage collection at VM reentry");
    {
        configure();const int old=VMS_RS(g_current);int next=0;
        RootScope roots(ReentrySite::RunArgs,{&old,&next});configure();next=VMS_RS(g_current);
        stat("next","Ljavax/microedition/lcdui/Displayable;")=Value::of_ref(next);
        VMS_RS(g_current)=old;display_visibility();stat("mode")=Value::of_i(1);tap(-6);
        check(value("actions")==1&&VMS_RS(g_current)==next&&value("presses")==0&&value("releases")==0,"command callback switch does not leak release to new Canvas");
    }
    configure();dispatch_key(-6,false);release_game_keys();
    check(value("releases")==0,"pause cleanup does not synthesize command key release");
    dispatch_key(-6,true);check(value("releases")==0,"physical release remains consumed after cleanup");
    tracker_capture_checks();
    configure();dispatch_key(-6,false);
    SavState saved,decoded;std::string bytes;
    const bool restored=capture_state(saved,"canvas-commands.jar")&&encode_state(saved,bytes)&&decode_state(bytes,decoded)&&apply_state(decoded);
    check(restored,"command press snapshot round trips through production restore");
    configure(0,0);tap(-6);
    check(value("presses")==1&&value("releases")==1,"restore clears command host key history before unbound softkey");
    configure();stat("mode")=Value::of_i(2);tap(-6);
    check(value("actions")==1&&value("presses")==0&&value("releases")==0&&VMS_EXEC(g_notify_destroyed),"destroying callback leaves no Canvas callbacks");
    std::cout<<(failures?"FAIL ":"PASS ")<<"CanvasCommandChecks "<<checks<<" checks failures="<<failures<<"\n";
    return failures?1:0;
}
