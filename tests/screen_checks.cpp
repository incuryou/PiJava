#define main emulator_main
#ifndef CHECK_VM
#define CHECK_VM "../src/j2mevm.cpp"
#endif
#include CHECK_VM
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) { return "build/screen-state.bin"; }
#ifndef CHECK_STATE
#define CHECK_STATE "../src/instant_state.inc"
#endif
#include CHECK_STATE
static void check(bool b,const char* m){if(!b){std::cerr<<"FAIL "<<m<<'\n';std::exit(1);}}
static Value call(const char* n,const char* d="()V",std::vector<Value> a={}){
    Method* m=find_method(ensure_class("ScreenChecks"),n,d,false);check(m,"method");return run(m,a);
}
static int field(const char* n){int c=ensure_class("ScreenChecks");return VMS_CL(g_classes)[c].statics[find_field(c,n,"I")->slot].i;}
static std::string events(){int c=ensure_class("ScreenChecks");return read_string(VMS_CL(g_classes)[c].statics[find_field(c,"events","Ljava/lang/String;")->slot].ref);}
static SavState capture(){VMS_EXEC(g_midlet)=alloc_obj(ensure_class("java/lang/Object"));VMS_EXEC(g_midlet_state)=MidletState::Active;SavState s;check(capture_state(s,"screen-fixture.jar"),"capture");return s;}
int main(int argc,char** argv){
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;check(sizeof(Value)==16,"Value ABI");
#ifdef BASELINE_PROBE
    VMS_RS(g_screen_w)=176;VMS_RS(g_screen_h)=208;VMS_RS(g_fb).assign(176*208*3,0);
    int result=call("setup","(II)I",{Value::of_i(176),Value::of_i(208)}).i;
    check(result==-1,"old Canvas reports hardcoded dimensions");
    VMS_RS(g_screen_w)=240;VMS_RS(g_screen_h)=320;VMS_RS(g_fb).assign(240*320*3,0);check(call("setup","(II)I",{Value::of_i(240),Value::of_i(320)}).i==1,"baseline fixture");
    display_visibility();SavState state=capture();std::string bytes;check(encode_state(state,bytes),"old encode");
    check(argc==2,"baseline output");std::ofstream f(argv[1],std::ios::binary);f.write(bytes.data(),bytes.size());f.close();check(bool(f),"old J2S5 output");
    std::cout<<"BASELINE_REPRODUCED Canvas/Displayable disagree with 176x208 GameCanvas\n";return 17;
#else
    const std::string base="build/selfauthored-screen.jar";
    for(auto dims:{std::pair<int,int>{240,320},{176,208},{320,240},{640,640}}){
        {std::ofstream f(base+".screen");f<<dims.first<<'x'<<dims.second<<'\n';}
        select_screen_sidecar(base);check(VMS_RS(g_screen_w)==dims.first&&VMS_RS(g_screen_h)==dims.second,"actual sidecar selected");
        VMS_RS(g_fb).assign(size_t(dims.first)*dims.second*3,0);
        check(call("setup","(II)I",{Value::of_i(dims.first),Value::of_i(dims.second)}).i==1,"Canvas GameCanvas Form dimensions and graphics clip");
        display_visibility();VMS_RS(g_repaint)=false;repaint_current();
        const std::string expected=dims.first==240&&dims.second==320?"vp":"svp";
        check(events()==expected,"sizeChanged before showNotify/paint; default order retained");
        int n=field("changes");call("toggle");display_visibility();check(field("changes")==n&&field("paints")==1&&events()==expected,"fullscreen same area has no fake callback/repaint");
        gc_collect("screen metadata remains on Java object");
        SavState state=capture(),decoded;std::string bytes;check(encode_state(state,bytes)&&bytes.substr(0,4)=="J2S5"&&decode_state(bytes,decoded)&&apply_state(decoded),"new size/fullscreen J2S5 roundtrip");
        check(call("verify","()I").i==dims.first*1000+dims.second,"restored dimensions");
        check(screen_meta(VMS_HEAP(g_heap)[VMS_RS(g_current)].text).full,"fullscreen value restored");
        const int live=VMS_RS(g_current);const auto pixels=VMS_RS(g_fb);
        std::ofstream statefile(base+".state",std::ios::binary);statefile.write(bytes.data(),bytes.size());statefile.close();
        // Equal area swapped dimensions must also be rejected before heap apply.
        VMS_RS(g_screen_w)=dims.first==240?320:240;VMS_RS(g_screen_h)=dims.first==240?240:320;
        check(!apply_state(decoded)&&VMS_RS(g_current)==live&&VMS_RS(g_fb)==pixels,"mismatch rejected without changing live roots/pixels");
        std::ifstream kept(base+".state",std::ios::binary);std::string reread(std::istreambuf_iterator<char>(kept),{});check(reread==bytes,"rejected input file retained");
        VMS_RS(g_screen_w)=dims.first;VMS_RS(g_screen_h)=dims.second;
        call("hide");display_visibility();check(events().back()=='h',"hide notified");
    }
    {std::ofstream f(base+".screen");f<<"-1x9999999999999999999999999999999999999";}
    select_screen_sidecar(base);check(VMS_RS(g_screen_w)==240&&VMS_RS(g_screen_h)==320,"invalid profile default fallback");
    std::remove((base+".screen").c_str());select_screen_sidecar(base);check(VMS_RS(g_screen_w)==240&&VMS_RS(g_screen_h)==320,"missing sidecar fallback");
    VMS_RS(g_fb).assign(240*320*3,0);check(call("setup","(II)I",{Value::of_i(240),Value::of_i(320)}).i==1,"redirect fixture");
    call("redirectSetup");VMS_RS(g_screen_w)=176;VMS_RS(g_screen_h)=208;display_visibility();check(events()=="s"&&VMS_RS(g_current)==0&&VMS_RS(g_shown)==0,"sizeChanged redirects current without stale showNotify");
    for(int mode:{2,3}) {
        VMS_EXEC(g_notify_paused)=VMS_EXEC(g_notify_destroyed)=false;VMS_EXEC(g_midlet_state)=MidletState::Active;
        VMS_RS(g_screen_w)=240;VMS_RS(g_screen_h)=320;
        check(call("setup","(II)I",{Value::of_i(240),Value::of_i(320)}).i==1,"lifecycle-size fixture");
        int c=ensure_class("ScreenChecks");VMS_EXEC(g_midlet)=VMS_CL(g_classes)[c].statics[find_field(c,"app","LScreenChecks$App;")->slot].ref;
        VMS_CL(g_classes)[c].statics[find_field(c,"redirect","I")->slot]=Value::of_i(mode);
        VMS_RS(g_screen_w)=176;VMS_RS(g_screen_h)=208;display_visibility();
        check(events()=="s"&&VMS_RS(g_shown)==0,"sizeChanged pause/destroy does not show or hide an unshown screen");
        repaint_current();check(field("paints")==0&&events()=="s","no paint before pending sizeChanged lifecycle request is processed");
        if(mode==2) {
            check(VMS_EXEC(g_notify_paused),"pause request retained");host_events();check(VMS_EXEC(g_midlet_state)==MidletState::Paused&&events()=="s","pending pause processed without fake visibility");
            VMS_CL(g_classes)[c].statics[find_field(c,"redirect","I")->slot]=Value::of_i(0);
            VMS_EXEC(g_midlet_state)=MidletState::Active;display_visibility();check(events()=="sv","resume shows once without duplicate sizeChanged");
            call("hide");display_visibility();
        } else {check(VMS_EXEC(g_notify_destroyed),"destroy request retained");host_events();check(VMS_EXEC(g_midlet_state)==MidletState::Destroyed&&events()=="s","pending destroy processed without fake visibility");}
    }
    VMS_EXEC(g_notify_paused)=VMS_EXEC(g_notify_destroyed)=false;
    VMS_RS(g_screen_w)=240;VMS_RS(g_screen_h)=320;
    if(argc==2){std::ifstream f(argv[1],std::ios::binary);std::string bytes(std::istreambuf_iterator<char>(f),{});SavState old;check(decode_state(bytes,old)&&apply_state(old),"actual old default J2S5 accepted");}
    std::cout<<"PASS ScreenChecks: logical sizes, callback order/reentry, fullscreen, GC, J2S5 and rejection retention\n";
#endif
}
