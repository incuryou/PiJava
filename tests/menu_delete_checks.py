"""Exercise real launcher deletion and snapshot cleanup with disposable files."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/platform/switch_host.inc').read_text()
def function(text,start,end):
    a=text.index(start);return text[a:text.index(end,a)]
paths=function(source,'static std::string instant_state_path(', '#include "../instant_state.inc"')
controller=function(source,'static void update_menu_delete(', 'static std::string button_names(')
state=(root/'src/instant_state.inc').read_text()
delete=function(state,'static bool delete_instant_state(', 'static bool restore_instant_state(')
prefix=r'''
#include "host_menu.h"
#include "menu_catalog.h"
#include "instant_state_file.h"
#include "n1_paths.h"
#include <cassert>
#include <fstream>
#include <filesystem>
using u64=uint64_t;
constexpr u64 HidNpadButton_A=1,HidNpadButton_B=2,HidNpadButton_AnyUp=4,HidNpadButton_AnyDown=8;
struct Entry{std::string label,path;};
menu_actions::Deletion g_menu_delete;
bool g_menu_note_error=false,g_menu_note_neutral=true;
struct MenuUi{menu_catalog::State catalog;} g_menu_name_ui;
int redraws=0;void draw_menu(const std::vector<Entry>&,int,const std::string&){++redraws;}
void log_line(const std::string&){}
std::vector<Entry> scan_games(){std::vector<Entry> result;
for(const auto& e:n1paths::scan_games("sdmc:/switch/j2me"))result.push_back({e.label,e.path});return result;}
'''
tail=r'''
int main(){
 const std::string root="sdmc:/switch/j2me",a=root+"/games/a.jar",b=root+"/games/b.jar";
 std::filesystem::create_directories(root+"/games");std::filesystem::create_directories(root+"/state");
 auto put=[](const std::string& p){std::ofstream(p)<<"fixture";};
 put(a);put(b);put(root+"/root-only.jar");put(root+"/a.jad");put(root+"/keymap.cfg");put(root+"/speed.cfg");
 const auto sav=instant_state_path(a);for(auto suffix:{"",".tmp",".previous"})put(sav+suffix);
 auto items=scan_games();int sel=0;std::string status;
 g_menu_delete.begin(a,"a");update_menu_delete(HidNpadButton_A,items,sel,status);
 assert(g_menu_delete.confirming && std::filesystem::exists(a));
 update_menu_delete(0,items,sel,status);update_menu_delete(0,items,sel,status);
 assert(g_menu_delete.confirming && std::filesystem::exists(a)); // held A has no new down edge
 update_menu_delete(HidNpadButton_B,items,sel,status);assert(!g_menu_delete.confirming && g_menu_delete.open);
 update_menu_delete(HidNpadButton_B,items,sel,status);assert(!g_menu_delete.open && std::filesystem::exists(a));
 g_menu_delete.begin(a,"a");update_menu_delete(HidNpadButton_AnyDown,items,sel,status);
 update_menu_delete(HidNpadButton_A,items,sel,status);assert(instant_state_exists(a));
 update_menu_delete(HidNpadButton_A,items,sel,status);
 assert(status=="即时存档删除成功" && !g_menu_note_error && !g_menu_note_neutral);
 for(auto suffix:{"",".tmp",".previous"})assert(!std::filesystem::exists(sav+suffix));
 assert(std::filesystem::exists(a));
 g_menu_delete.begin(a,"a");g_menu_delete.selection=1;
 update_menu_delete(HidNpadButton_A,items,sel,status);update_menu_delete(HidNpadButton_A,items,sel,status);
 assert(status=="没有即时存档" && !g_menu_note_error && g_menu_note_neutral);
 // A recovery-only snapshot is also deletable.
 put(sav+".previous");g_menu_delete.begin(a,"a");g_menu_delete.selection=1;
 update_menu_delete(HidNpadButton_A,items,sel,status);update_menu_delete(HidNpadButton_A,items,sel,status);
 assert(!std::filesystem::exists(sav+".previous") && status=="即时存档删除成功");
 // A failed cleanup must be shown as failure and not remove the primary file.
 put(sav);std::filesystem::create_directory(sav+".previous");
 g_menu_delete.begin(a,"a");g_menu_delete.selection=1;update_menu_delete(1,items,sel,status);update_menu_delete(1,items,sel,status);
 assert(g_menu_note_error && std::filesystem::exists(sav));std::filesystem::remove(sav+".previous");
 // Delete only the selected JAR; a JAR left in the root is never listed (0.2.107 games/ only).
 assert(items.size()==2);
 g_menu_delete.begin(a,"a");update_menu_delete(1,items,sel,status);update_menu_delete(1,items,sel,status);
 assert(!std::filesystem::exists(a) && std::filesystem::exists(sav) && items.size()==1 && sel==0);
 assert(items[0].path==b && !g_menu_note_error && !g_menu_note_neutral && std::filesystem::exists(root+"/root-only.jar"));
 for(auto file:{"/a.jad","/keymap.cfg","/speed.cfg"})assert(std::filesystem::exists(root+file));
 // Missing target produces a failure, retains remaining list and selection.
 g_menu_delete.begin(a,"a");update_menu_delete(1,items,sel,status);update_menu_delete(1,items,sel,status);
 assert(g_menu_note_error && items.size()==1 && sel==0);
 for(const auto& p:{b}) {g_menu_delete.begin(p,"fixture");update_menu_delete(1,items,sel,status);update_menu_delete(1,items,sel,status);}
 assert(items.empty() && sel==0);
 // New overlay renders its actual choices and confirmation at bounded text sizes.
 std::vector<uint32_t> frame(1280*720);std::vector<std::string> text;
 host_menu::Text draw=[&](int x,int y,const std::string& s,uint32_t,int size,int width){
 assert(x>=0 && y>=0 && x+width<=1280 && y+size<=720);text.push_back(s);};
 g_menu_delete.begin("fixture.jar","游戏名");host_menu::render_delete(frame.data(),1280,g_menu_delete,draw,{});
 assert(std::find(text.begin(),text.end(),"删除游戏")!=text.end() && std::find(text.begin(),text.end(),"删除即时存档")!=text.end());
 g_menu_delete.act(menu_actions::Action::Confirm);text.clear();host_menu::render_delete(frame.data(),1280,g_menu_delete,draw,{});
 assert(std::find(text.begin(),text.end(),"删除此游戏 JAR？")!=text.end());
 std::puts("PASS MenuDeleteChecks: production controller + real temp filesystem; confirm/cancel, primary/recovery cleanup, failure notices, JAR-only removal, games/-only list, preserved configs/save/JAD, empty list, bounded overlay");
}
'''
build=root/'build/menu-delete';build.mkdir(parents=True,exist_ok=True)
cpp=build/'checks.cpp';cpp.write_text(prefix+paths+delete+controller+tail)
puff=build/'puff.o';binary=build/'checks'
subprocess.run(['gcc','-O2','-c',str(root/'src/puff.c'),'-o',str(puff)],check=True)
subprocess.run(['g++','-std=c++17','-O2','-I'+str(root/'src'),str(cpp),str(puff),'-o',str(binary)],check=True)
with tempfile.TemporaryDirectory() as d:subprocess.run([str(binary)],cwd=d,check=True)
