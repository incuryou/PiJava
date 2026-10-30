"""Compile the actual Switch menu branch with owned input/launch/presenter seams.

No game JAR, save or HID device is used. The source under test is extracted
unchanged, so the previous frozen tree must reproduce the hidden-game defect.
"""
from pathlib import Path
import argparse

CPP = r'''
#include "host_menu.h"
#include "host_speed.h"
#include <iostream>
#include <map>
#include <cstdlib>
using u64=uint64_t;
enum Buttons:u64 {HidNpadButton_A=1,HidNpadButton_B=2,HidNpadButton_X=4,
 HidNpadButton_Y=8,HidNpadButton_AnyLeft=16,HidNpadButton_AnyRight=32,
 HidNpadButton_AnyDown=64,HidNpadButton_AnyUp=128,HidNpadButton_R=256,
 HidNpadButton_ZR=512,HidNpadButton_L=1024,HidNpadButton_ZL=2048,HidNpadButton_Minus=4096};
enum class HostSurface {Game,Menu,Import};
struct Entry {std::string label,path;};
struct Profile {bool active=false;};
struct State {
 Profile g_p1_profile; HostSurface g_host_surface=HostSurface::Menu;
 host_menu::Focus g_menu_focus=host_menu::Focus::Games,g_menu_option_focus=host_menu::Focus::Bezel;
 host_menu::Settings g_menu_settings;
 std::string g_launched_path; int g_cpu_cleared=0,g_speed_cap=0,g_slices=0,g_chrome_bezel=0,g_chrome_fps=0;
 bool g_fps_reset=false,g_speed_frame_open=false,g_fb_dirty=false;
 int64_t g_speed_last_paint_us=0,g_speed_next_us=0,g_speed_slept_us=0;
 std::map<std::string,int> g_game_speeds;
 std::map<std::string,std::vector<uint8_t>> g_jar_files;std::map<std::string,std::string> g_props;
 std::vector<std::string> g_classpath;
} st;
#define VMS_PLAT(n) st.n
#define VMS_DIAG(n) st.n
#define VMS_EXEC(n) st.n
#define VMS_CL(n) st.n
#define VMS_RS(n) st.n
struct {bool scrolling=false;int64_t last_tick=0;} g_menu_name_ui;
std::vector<Entry> items; int sel=0,in_game=0,draws=0,launches=0,restores=0,writes=0,checks=0;
bool import_prompt=false,load_ok=true,restore_ok=true;
std::string status; std::map<std::string,bool> saves;
std::vector<uint32_t> pixels(1280*720);
std::vector<std::string> logs;
void check(bool ok,const char* what){++checks;if(!ok){std::cerr<<"FAIL "<<what<<"\n";std::exit(17);}}
void log_line(const std::string&s){logs.push_back(s);}
int64_t monotonic_ms(){return 1000;}
namespace speed_keys {struct State{}; int lookup(State&,std::map<std::string,int>&m,const std::string&p){return m[p];}
std::string key_for(State&,const std::string&p){return p;}void save_cache(State&){}
bool rewrite(State&,std::map<std::string,int>&){++writes;return true;}}
speed_keys::State g_speed_keys;
bool instant_state_exists(const std::string&p){return saves[p];}
void render_surface(){
 if(st.g_host_surface==HostSurface::Game)std::fill(pixels.begin(),pixels.end(),0xff2e170au);
 else host_menu::render_menu(pixels.data(),1280,{"Owned A","Owned B"},sel,st.g_menu_focus,st.g_menu_settings,status,
  [](int,int,const std::string&,uint32_t,int,int){});
}
void present(){render_surface();}
void draw_menu(const std::vector<Entry>&,int,const std::string&){++draws;st.g_host_surface=HostSurface::Menu;st.g_cpu_cleared=0;}
void draw_import_prompt(const Entry&){st.g_host_surface=HostSurface::Import;}
void launch_entry(const Entry&e){++launches;st.g_launched_path=e.path;st.g_host_surface=HostSurface::Game;st.g_cpu_cleared=42;}
bool read_whole(const std::string&,std::vector<uint8_t>&){return load_ok;}
bool unzip(const std::vector<uint8_t>&,std::map<std::string,std::vector<uint8_t>>&,std::string&){return load_ok;}
void unpack_nested_jars(){}void load_props(){}void load_sidecar_jad(const std::string&){}void select_screen_sidecar(const std::string&){}
std::string rms_jar_identity(const uint8_t*,size_t){return "owned-fixture";}
bool restore_instant_state(const std::string&,const std::string&){++restores;return restore_ok;}
bool save_menu_settings(){++writes;return true;}
int mock_mkdir(const char*,int){++writes;return 0;}
#define mkdir mock_mkdir
void step(u64 down){for(int once=0;once<1;++once){
/* ACTUAL_SWITCH_MENU_BRANCH */
}render_surface();}
void reset(){st=State{};g_menu_name_ui={};items={{"Owned A","fixture-a"},{"Owned B","fixture-b"}};
 sel=in_game=draws=launches=restores=writes=0;import_prompt=false;load_ok=restore_ok=true;status.clear();saves.clear();logs.clear();}
void game_visible(){check(in_game==1,"launch enters game mode");check(st.g_host_surface==HostSurface::Game,"no menu overwrite after launch");
check(pixels[0]==0xff2e170au,"presenter selects game pixels");check(launches==1,"exactly one launch");check(writes==0,"no implicit config/save writes");}
int main(){
 reset();step(HidNpadButton_X);check(in_game==0 && launches==0 && restores==0,"missing state does not launch or restore");
 check(status=="没有即时存档" && draws==1,"missing-state message");step(HidNpadButton_A);game_visible();check(draws==1,"missing-state retry does not redraw menu");
 reset();step(HidNpadButton_A);game_visible();
 reset();g_menu_name_ui.scrolling=true;step(HidNpadButton_A);game_visible();check(st.g_cpu_cleared==42 && g_menu_name_ui.last_tick==0,"menu scroll invalidation stops after launch");
 reset();step(HidNpadButton_X);step(HidNpadButton_X);step(HidNpadButton_A);game_visible();check(draws==2,"repeated missing-state retry");
 reset();step(HidNpadButton_X);step(HidNpadButton_AnyDown);check(sel==1 && status.empty() && !in_game,"navigation after refusal");step(HidNpadButton_A);game_visible();check(st.g_launched_path=="fixture-b","selected owned fixture launched");
 reset();saves["fixture-a"]=true;load_ok=false;step(HidNpadButton_X);check(!in_game && restores==0 && status=="即时存档无法读取","unreadable input returns to menu");step(HidNpadButton_A);game_visible();
 reset();saves["fixture-a"]=true;restore_ok=false;step(HidNpadButton_X);check(!in_game && restores==1 && st.g_host_surface==HostSurface::Menu && !status.empty(),"restore rejection returns to menu");step(HidNpadButton_A);game_visible();
 reset();saves["fixture-a"]=true;status="previous status";step(HidNpadButton_X);check(in_game==1 && restores==1 && launches==0 && draws==0 && status.empty(),"successful continue preserves game surface");check(st.g_host_surface==HostSurface::Game && pixels[0]==0xff2e170au,"successful continue game pixels");
 reset();st.g_p1_profile.active=true;step(HidNpadButton_X);check(!in_game && restores==0 && !status.empty(),"profile refusal stays in menu");step(HidNpadButton_A);game_visible();
 reset();items.clear();step(HidNpadButton_X);step(HidNpadButton_A);check(!in_game && launches==0 && restores==0,"empty library cannot launch");
 reset();step(HidNpadButton_X);step(HidNpadButton_AnyRight);step(HidNpadButton_AnyDown);check(st.g_menu_focus==host_menu::Focus::Speed,"options navigation still active");step(HidNpadButton_A);check(!in_game && launches==0 && writes>0,"explicit speed setting remains in menu");
 std::cout<<"PASS actual Switch menu branch checks="<<checks<<" hidden_game=0 owned_fixtures_only=1\n";
}
'''

def main():
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    text=(a.source/'src/platform/switch_host.inc').read_text(encoding='utf8')
    start=text.index('            // Four visible choices:')
    end=text.index('        } else {\n            if(down & HidNpadButton_StickL)',start)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(CPP.replace('/* ACTUAL_SWITCH_MENU_BRANCH */',text[start:end]),encoding='utf8')
    print('Emitted actual production menu branch; host I/O seams only, no game payload')

if __name__=='__main__':main()
