#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "../src/host_keymap.h"
#include <cassert>
#include "../src/host_menu.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string g_test_state;
static std::string instant_state_path(const std::string&){return g_test_state;}
#include "../src/instant_state.inc"
int main(int argc,char** argv) {
    assert(argc==3);g_test_state=argv[2];VMS_CL(g_classpath)={argv[1],"romfs/classes"};
    VMS_DIAG(g_log_draw)=false;VMS_RS(g_screen_w)=24;VMS_RS(g_screen_h)=32;
    launch_class("GameCanvasBootChecks");assert(VMS_RS(g_current));
    const auto poll=[](){int ref=VMS_RS(g_current);return run(find_method(VMS_HEAP(g_heap)[ref].cls,"getKeyStates","()I",true),{Value::of_ref(ref)}).i;};
    host_keymap::Config config;config.automatic=false;config.bindings=host_keymap::defaults(false);
    host_keymap::assign(config,false,host_keymap::Fire,0,host_keymap::A);
    host_keymap::assign(config,false,host_keymap::Fire,1,host_keymap::R3);
    host_keymap::assign(config,false,host_keymap::Fire,2,host_keymap::Y);
    host_keymap::Tracker tracker;
    const auto step=[&](uint32_t held){
        auto next=host_keymap::codes(config,false,held);int states=0;
        for(int code:next){int action=game_key_action(code);if(action)states|=1<<action;}
        VMS_PLAT(g_key_states)=states;
        tracker.update(next,[](int key,bool release){dispatch_key(key,release);});
        if(Value* key_held=game_canvas_state(VMS_RS(g_current),"keyHeld"))key_held->i=states;
    };
    step(1u<<host_keymap::A);assert(poll()==(1<<8));assert(poll()==(1<<8));
    step((1u<<host_keymap::A)|(1u<<host_keymap::R3));assert(poll()==(1<<8));
    step(1u<<host_keymap::R3);assert(poll()==(1<<8));
    step(0);assert(poll()==0);
    step((1u<<host_keymap::A)|(1u<<host_keymap::R3)|(1u<<host_keymap::Y));assert(poll()==(1<<8));
    step((1u<<host_keymap::R3)|(1u<<host_keymap::Y));assert(poll()==(1<<8));
    step(1u<<host_keymap::Y);assert(poll()==(1<<8));step(0);assert(poll()==0);
    step(1u<<host_keymap::DUp);assert(poll()==(1<<1));
    step((1u<<host_keymap::DUp)|(1u<<host_keymap::SUp));assert(poll()==(1<<1));
    step(1u<<host_keymap::SUp);assert(poll()==(1<<1));
    host_freeze236(true);tracker.clear();assert(VMS_PLAT(g_host_paused)&&poll()==0);
    assert(sheet_save_instant_state("fixture.jar"));
    std::string saved;assert(instant_state_file::load(g_test_state,saved));SavState decoded;assert(decode_state(saved,decoded));
    assert(saved.substr(0,4)=="J2S5");
    host_freeze236(false);assert(!VMS_PLAT(g_host_paused)&&poll()==0);
    assert(restore_instant_state("fixture.jar",decoded.rms));assert(!VMS_PLAT(g_host_paused));
    step(0);step(1u<<host_keymap::R3);assert(poll()==(1<<8));step(0);assert(poll()==0);
    assert(VMS_EXEC(g_midlet_state)==MidletState::Active);
    for(int input=0;input<host_keymap::InputCount;++input) {
        tracker.clear();config=host_keymap::Config{};config.automatic=false;
        assert(host_keymap::assign(config,false,host_keymap::Fire,0,input));
        step(1u<<input);assert(poll()==(1<<8));step(0);assert(poll()==0);
    }
    config=host_keymap::Config{};
    const int right_inputs[]={host_keymap::RUp,host_keymap::RDown,host_keymap::RLeft,host_keymap::RRight};
    const int actions[]={1,6,2,5};
    for(int i=0;i<4;++i){step(1u<<right_inputs[i]);assert(poll()==(1<<actions[i]));step(0);assert(poll()==0);}
    step((1u<<host_keymap::DUp)|(1u<<host_keymap::SUp)|(1u<<host_keymap::RUp));assert(poll()==(1<<1));
    step((1u<<host_keymap::SUp)|(1u<<host_keymap::RUp));assert(poll()==(1<<1));
    step(1u<<host_keymap::RUp);assert(poll()==(1<<1));step(0);assert(poll()==0);
    step(1u<<host_keymap::Y);assert(poll()==(1<<8));step(0);assert(poll()==0);
    config.automatic=false;config.bindings=host_keymap::empty_bindings();
    assert(host_keymap::assign(config,false,host_keymap::Up,0,host_keymap::A));
    assert(host_keymap::assign(config,false,host_keymap::Fire,0,host_keymap::A));
    assert(host_keymap::assign(config,false,host_keymap::N5,0,host_keymap::R3));
    step(1u<<host_keymap::A);assert(poll()==((1<<1)|(1<<8)));assert(tracker.pressed.size()==2);
    step((1u<<host_keymap::A)|(1u<<host_keymap::R3));assert(poll()==((1<<1)|(1<<8)));
    step(1u<<host_keymap::R3);assert(poll()==(1<<8));assert(tracker.pressed==std::set<int>{53});
    step(0);assert(poll()==0 && tracker.pressed.empty());
    assert(delete_instant_state("fixture.jar"));std::string removed;assert(!instant_state_file::load(g_test_state,removed));
    lifecycle_destroy();
    std::puts("PASS real VM/GameCanvas: remapped confirmation FIRE, three physical confirmation holds, three direction holds, right stick numeric 2846, native freeze releases keys, freeze-save/restore J2S5, all 23 inputs, default Y confirmation, one physical key drives multiple phone keys, shared GameCanvas action release, real state deletion, resume has no sticky input");
}
