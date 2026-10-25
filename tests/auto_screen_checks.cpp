#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) { return "build/auto-screen.sav"; }
#include "../src/instant_state.inc"
static void check(bool value,const char* message) {
    if(!value){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}
}
static void dims(int w,int h,const char* message) {
    check(VMS_RS(g_screen_w)==w&&VMS_RS(g_screen_h)==h,message);
}
static void restore_fit_checks() {
    const std::string jar="build/restore-fit-fixture.jar";
    std::remove((jar+".screen").c_str());
    VMS_RS(g_screen_w)=240;VMS_RS(g_screen_h)=320;
    VMS_RS(g_fb).resize(240*320*3);
    for(size_t i=0;i<VMS_RS(g_fb).size();++i)VMS_RS(g_fb)[i]=uint8_t(i*17+i/720);
    Method* setup=find_method(ensure_class("ScreenChecks"),"setup","(II)I",false);
    check(setup&&run(setup,{Value::of_i(240),Value::of_i(320)}).i==1,"restore-fit setup");display_visibility();
    VMS_EXEC(g_midlet)=alloc_obj(ensure_class("java/lang/Object"));VMS_EXEC(g_midlet_state)=MidletState::Active;
    VMS_PLAT(g_rms_suite)="e808c5b9878639757edcf67d330536a7b95f553790c2cf23c7d267df06ecb6b8";
    check(!VMS_RS(g_offscreen).empty(),"fixture has host GameCanvas buffer");
    const int buffer=VMS_RS(g_offscreen).begin()->second;
    auto& image=VMS_HEAP(g_heap)[buffer];
    for(size_t i=0;i<image.pixels.size();++i)image.pixels[i]=uint8_t(i*11+i/720);
    int graphics=0;for(auto kv:VMS_RS(g_gfx_target))if(kv.second==buffer)graphics=kv.first;
    check(graphics>0,"fixture has host buffer graphics");
    image.alpha.resize(240*320);for(size_t i=0;i<image.alpha.size();++i)image.alpha[i]=uint8_t(i*13+i/240);
    const int subclip_graphics=alloc_obj(ensure_class("javax/microedition/lcdui/Graphics"));
    gfx_bind(subclip_graphics,buffer,0);VMS_RS(g_clip)[subclip_graphics]=Clip{5,7,53,61,true};
    const int ordinary=alloc_obj(ensure_class("javax/microedition/lcdui/Image"));
    VMS_HEAP(g_heap)[ordinary].width=17;VMS_HEAP(g_heap)[ordinary].height=19;
    VMS_HEAP(g_heap)[ordinary].pixels.assign(17*19*3,61);
    SavState original;check(capture_state(original,jar),"capture legacy-default fixture");
    auto write=[&](const SavState& state){std::string bytes;check(encode_state(state,bytes),"encode migration fixture");std::ofstream f(instant_state_path(""),std::ios::binary);f.write(bytes.data(),bytes.size());check(bool(f),"write owned migration fixture");return bytes;};
    auto retained=[&](const std::string& bytes){std::vector<uint8_t> disk;check(read_whole(instant_state_path(""),disk),"read migration fixture");check(rms_jar_identity(disk.data(),disk.size())==rms_jar_identity(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()),"original snapshot SHA256 unchanged");};
    auto overlap=[&](const std::vector<uint8_t>& actual,int w,int h,const std::vector<uint8_t>& old,int ow,int oh,uint8_t fill){
        check(actual.size()==size_t(w)*h*3,"resized surface byte count");
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<3;++c)
            check(actual[(size_t(y)*w+x)*3+c]==(x<ow&&y<oh?old[(size_t(y)*ow+x)*3+c]:fill),"surface retains row-wise pixel overlap");
    };
    const auto original_buffer=original.heap[buffer].pixels;
    auto migrate=[&](int w,int h,bool tagless){
        SavState state=original;if(tagless)state.heap[state.current].text.clear();
        const std::string bytes=write(state);
        select_screen_sidecar(jar,state.rms);dims(w,h,"trusted launch choice");
        check(restore_instant_state(jar,state.rms),"legacy-default snapshot migration succeeds");
        dims(w,h,"restore keeps trusted nondefault canvas choice");retained(bytes);
        overlap(VMS_RS(g_fb),w,h,original.fb,240,320,0);
        const auto& b=VMS_HEAP(g_heap)[buffer];check(b.width==w&&b.height==h,"host GameCanvas buffer resized");
        overlap(b.pixels,w,h,original_buffer,240,320,255);
        for(const char* field:{"width","height"}) {const Field* f=find_field(b.cls,field,"I");check(f&&b.fields[f->slot].i==(field[0]=='w'?w:h),"host buffer Image dimensions updated");}
        const auto& ordinary_image=VMS_HEAP(g_heap)[ordinary];
        check(ordinary_image.width==17&&ordinary_image.height==19&&ordinary_image.pixels==original.heap[ordinary].pixels,"game-owned Image untouched");
        const auto clip=VMS_RS(g_clip).at(graphics);check(clip.x==0&&clip.y==0&&clip.w==w&&clip.h==h,"host buffer full clip follows size");
        const auto subclip=VMS_RS(g_clip).at(subclip_graphics);check(subclip.x==5&&subclip.y==7&&subclip.w==53&&subclip.h==61,"game-defined narrowed clip preserved");
        check(b.alpha.size()==size_t(w)*h,"host buffer alpha surface resized");for(int y=0;y<h;++y)for(int x=0;x<w;++x)check(b.alpha[size_t(y)*w+x]==(x<240&&y<320?original.heap[buffer].alpha[size_t(y)*240+x]:255),"host buffer alpha retains row-wise overlap");
        check(screen_meta(VMS_HEAP(g_heap)[VMS_RS(g_current)].text).w==240,"sizeChanged deferred outside restore");
        SavState migrated;check(capture_state(migrated,jar),"capture immediately after migration");
        const auto captured_meta=screen_meta(migrated.heap[migrated.current].text);
        check(captured_meta.w==w&&captured_meta.h==h&&!captured_meta.notified,"immediate resave tags new size and retains pending sizeChanged");
        auto changes=[](){const int cid=ensure_class("ScreenChecks");const Field* f=find_field(cid,"changes","I");return VMS_CL(g_classes)[f->owner].statics[f->slot].i;};
        const int prior_changes=changes();host_events(false);
        check(changes()==prior_changes+1,"migrated canvas receives exactly one sizeChanged");host_events(false);
        check(changes()==prior_changes+1,"later visibility does not repeat sizeChanged");
        const auto meta=screen_meta(VMS_HEAP(g_heap)[VMS_RS(g_current)].text);check(meta.w==w&&meta.h==h&&meta.notified,"normal visibility safely delivers sizeChanged");
        Method* verify=find_method(ensure_class("ScreenChecks"),"verify","()I",false);
        check(verify&&run(verify,{}).i==w*1000+h,"restored Java Canvas reports migrated dimensions");
        retained(bytes);
        const auto migrated_bytes=write(migrated);
        check(restore_instant_state(jar,migrated.rms),"newly generated migrated J2S5 loads again");dims(w,h,"new migrated J2S5 retains new dimensions");retained(migrated_bytes);
        overlap(VMS_RS(g_fb),w,h,original.fb,240,320,0);host_events(false);
        const auto live=VMS_RS(g_current);const auto pixels=VMS_RS(g_fb);const auto off=VMS_HEAP(g_heap)[buffer].pixels;
        SavState bad=original;bad.midlet=int(bad.heap.size()+100);const auto bad_bytes=write(bad);
        check(!restore_instant_state(jar,bad.rms),"invalid snapshot rejected before migration");dims(w,h,"failed restore keeps live migrated dimensions");
        check(VMS_RS(g_current)==live&&VMS_RS(g_fb)==pixels&&VMS_HEAP(g_heap)[buffer].pixels==off,"failed restore keeps live objects and surfaces");retained(bad_bytes);
    };
    VMS_CL(g_props).clear();migrate(360,360,false); // verified SHA
    VMS_CL(g_props)={{"SEMC-Screen-Size","176,208"}};
    original.rms="vendor-owned-fixture";migrate(176,208,true); // tagless legacy + vendor
    VMS_CL(g_props).clear();{std::ofstream f(jar+".screen");f<<"360x640\n";}migrate(360,640,false);
    std::remove((jar+".screen").c_str());
    auto bytes=write(original);select_screen_sidecar(jar,original.rms);
    check(restore_instant_state(jar,original.rms),"unknown game still restores");dims(240,320,"unknown game keeps historical saved canvas");check(VMS_RS(g_fb)==original.fb,"unknown game keeps exact pixels");retained(bytes);
    VMS_CL(g_props)={{"SEMC-Screen-Size","176,208"}};
    {std::ofstream f(jar+".screen");f<<"invalid";}bytes=write(original);
    check(restore_instant_state(jar,original.rms),"invalid explicit setting still restores historical save");dims(240,320,"invalid explicit setting blocks automatic migration");retained(bytes);std::remove((jar+".screen").c_str());
    SavState nondefault;VMS_RS(g_screen_w)=360;VMS_RS(g_screen_h)=360;VMS_RS(g_fb).assign(360*360*3,53);
    check(capture_state(nondefault,jar),"capture existing nondefault save");bytes=write(nondefault);select_screen_sidecar(jar,nondefault.rms);
    check(restore_instant_state(jar,nondefault.rms),"existing nondefault save restores");dims(360,360,"automatic migration does not rewrite nondefault saved canvas");check(VMS_RS(g_fb)==nondefault.fb,"existing nondefault pixels exact");retained(bytes);
    std::cout<<"PASS legacy-default resume migration and transactional boundaries\n";
}
int main(int argc,char** argv) {
    check(argc==2,"metadata or restore mode");
    VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
    if(std::string(argv[1])=="metadata") {
        const std::string jar="build/autodetect-fixture.jar";
        std::remove((jar+".screen").c_str());
        VMS_CL(g_props)={{"SEMC-Screen-Size","176,208"}};
        select_screen_sidecar(jar);dims(176,208,"vendor logical size selected before launch");
        VMS_CL(g_props)={{"Nokia-MIDlet-Original-Display-Size","176,208"},{"Nokia-MIDlet-Target-Display-Size","240,320"}};
        select_screen_sidecar(jar);dims(176,208,"source size wins over vendor scaling destination");
        VMS_CL(g_props)={{"MIDlet-Data-Size","360640"},{"Nokia-MIDlet-Target-Display-Size","360,640"}};
        select_screen_sidecar(jar);dims(240,320,"storage and target size are not source dimensions");
        VMS_CL(g_props)={{"SEMC-Screen-Size","176,208"},{"Nokia-MIDlet-Original-Display-Size","240,320"}};
        select_screen_sidecar(jar);dims(240,320,"conflicting source hints retain conservative fallback");
        const std::string star="e808c5b9878639757edcf67d330536a7b95f553790c2cf23c7d267df06ecb6b8";
        VMS_CL(g_props).clear();
        select_screen_sidecar(jar,star);dims(360,360,"verified content identity selects fixed square canvas");
        select_screen_sidecar(jar,star+"0");dims(240,320,"nearby or unknown identity does not inherit a profile");
        select_screen_sidecar("build/pretend-360x640-S60V5.jar");dims(240,320,"filename is not a resolution hint");
        for(const auto& bad:{"0,320","360,9999999999999999999999","360,320junk","-1,320"}) {
            VMS_CL(g_props)={{"SEMC-Screen-Size",bad}};
            select_screen_sidecar(jar);dims(240,320,"invalid vendor dimensions cannot select a canvas");
        }
        VMS_CL(g_props)={{"SEMC-Screen-Size","176,208"}};
        {std::ofstream f(jar+".screen");f<<"320x240\n";}
        select_screen_sidecar(jar,star);dims(320,240,"explicit sidecar overrides automatic dimensions");
        {std::ofstream f(jar+".screen");f<<"broken";}
        select_screen_sidecar(jar);dims(240,320,"invalid explicit setting preserves default behavior");
        std::remove((jar+".screen").c_str());
        std::cout<<"PASS automatic vendor dimensions and override boundaries\n";
    } else if(std::string(argv[1])=="restore-fit") {
        restore_fit_checks();
    } else {
        VMS_RS(g_screen_w)=240;VMS_RS(g_screen_h)=320;VMS_RS(g_fb).assign(240*320*3,0);
        Method* setup=find_method(ensure_class("ScreenChecks"),"setup","(II)I",false);check(setup,"fixture setup method");
        check(run(setup,{Value::of_i(240),Value::of_i(320)}).i==1,"fixture setup");display_visibility();
        VMS_EXEC(g_midlet)=alloc_obj(ensure_class("java/lang/Object"));VMS_EXEC(g_midlet_state)=MidletState::Active;
        VMS_PLAT(g_rms_suite)="auto-screen-owned-fixture";
        SavState state;check(capture_state(state,"fixture.jar"),"capture original default state");
        std::string bytes;check(encode_state(state,bytes),"encode J2S5");
        {std::ofstream f(instant_state_path(""),std::ios::binary);f.write(bytes.data(),bytes.size());}
        VMS_RS(g_screen_w)=360;VMS_RS(g_screen_h)=360;VMS_RS(g_fb).assign(360*360*3,37);
        check(restore_instant_state("fixture.jar",state.rms),"old save restores despite a new automatic canvas selection");
        dims(240,320,"resume keeps saved dimensions");check(VMS_RS(g_fb)==state.fb,"resume keeps exact saved pixels");
        std::vector<uint8_t> retained;check(read_whole(instant_state_path(""),retained),"saved input retained");
        check(std::string(retained.begin(),retained.end())==bytes,"resume did not modify saved file");
        // A legacy J2S5 object without DSB1 metadata means 240x320. No new
        // fields or serializer revisions are required to continue that save.
        state.heap[state.current].text.clear();
        check(encode_state(state,bytes),"legacy tagless encode");
        auto write=[&](const std::string& data){std::ofstream f(instant_state_path(""),std::ios::binary);f.write(data.data(),data.size());check(bool(f),"owned input write");};
        write(bytes);VMS_RS(g_screen_w)=176;VMS_RS(g_screen_h)=208;
        check(restore_instant_state("fixture.jar",state.rms),"legacy tagless restore");dims(240,320,"legacy canvas preserved");
        const auto live=VMS_RS(g_current);const auto pixels=VMS_RS(g_fb);
        auto rejected=[&](const std::string& bad,const char* message){
            write(bad);VMS_RS(g_screen_w)=360;VMS_RS(g_screen_h)=360;
            check(!restore_instant_state("fixture.jar",state.rms),message);dims(360,360,"failed resume leaves dimensions untouched");
            check(VMS_RS(g_current)==live&&VMS_RS(g_fb)==pixels,"failed resume leaves live roots and pixels untouched");
            check(read_whole(instant_state_path(""),retained)&&std::string(retained.begin(),retained.end())==bad,"rejected save retained byte-for-byte");
        };
        for(char version='1';version<='4';++version){auto bad=bytes;bad[3]=version;rejected(bad,"unsupported version rejected");}
        SavState bad=state;bad.heap[bad.current].text=std::string("DSB1")+char(129)+char(2)+char(64)+char(1)+char(0)+char(1);
        std::string malformed;check(encode_state(bad,malformed),"malformed geometry encode");
        rejected(malformed,"invalid saved dimensions rejected before geometry changes");
        bad=state;bad.midlet=int(bad.heap.size()+100);check(encode_state(bad,malformed),"malformed roots encode");
        rejected(malformed,"invalid saved roots rejected before geometry changes");
        for(auto size:{std::pair<int,int>{176,208},{360,360},{360,640}}) {
            VMS_RS(g_screen_w)=size.first;VMS_RS(g_screen_h)=size.second;VMS_RS(g_fb).assign(size_t(size.first)*size.second*3,23);
            setup=find_method(ensure_class("ScreenChecks"),"setup","(II)I",false);check(setup,"restored fixture setup method");
            check(run(setup,{Value::of_i(size.first),Value::of_i(size.second)}).i==1,"nondefault fixture setup");display_visibility();
            check(capture_state(state,"fixture.jar")&&encode_state(state,bytes),"nondefault capture and encode");write(bytes);
            VMS_RS(g_screen_w)=240;VMS_RS(g_screen_h)=320;
            check(restore_instant_state("fixture.jar",state.rms),"nondefault saved canvas resumes from default launch choice");
            dims(size.first,size.second,"nondefault saved dimensions preserved");check(VMS_RS(g_fb)==state.fb,"nondefault saved pixels preserved");
        }
        std::cout<<"PASS original J2S5 retains its canvas dimensions and bytes\n";
    }
    p7::pool().shutdown();
}
