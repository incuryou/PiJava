"""Exercise the production Switch menu controller with mocked platform services.
Physical HID constants are read from the installed libnx header, independently
of the production mapping table. No Switch hardware is claimed by this check.
"""
from pathlib import Path
import re, subprocess, sys

root=Path(__file__).resolve().parents[1]
sdk=Path(sys.argv[1]);destination=Path(sys.argv[2]);destination.mkdir(parents=True,exist_ok=True)
source=(root/'src/platform/switch_host.inc').read_text()
hid=(sdk/'libnx/include/switch/services/hid.h').read_text()
constants='\n'.join('constexpr u64 '+name+'=1ull<<'+bit+';' for name,bit in re.findall(r'(HidNpadButton_\w+)\s*=\s*BITL\((\d+)\)',hid))
for direction in ['Up','Down','Left','Right']:
    constants+='\nconstexpr u64 HidNpadButton_Any'+direction+'=HidNpadButton_'+direction+'|HidNpadButton_StickL'+direction+'|HidNpadButton_StickR'+direction+';'
start=source.index('static bool standard_game_keys()')
physical=source[start:source.index('static void select_game_keymap',start)]
start=source.index('static void close_keymap(')
controller=source[start:source.index('static int jar_class_files()',start)]
assert 'else if (in_game && (down & HidNpadButton_Y))' not in source
assert 'g_exit_sheet) = true' not in source
header=r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "host_keymap_ui.h"
using u64=uint64_t;
'''+constants+r'''
#define VMS_CL(n) n
#define VMS_PLAT(n) n
#define VMS_RS(n) n
std::map<std::string,int> g_jar_files;
host_keymap::Store g_keymaps;
host_keymap::Editor g_keymap_editor;
host_keymap::Config g_keymap_active;
host_keymap::Tracker g_keymap_tracker;
std::string g_keymap_id(64,'a'),g_keymap_name="装甲特战队",g_launched_path="fixture.jar";
std::vector<uint32_t> g_keymap_phone;
bool g_keymap_phone_attempted=false,g_host_paused=false,g_fb_dirty=false,g_fps_reset=false;
u64 g_keymap_blocked=0;
int g_cpu_cleared=3,saves=0,deletes=0,invalidations=0;
bool save_ok=true,delete_ok=true,exists=true;
const char* g_keymap_path;
std::vector<std::string> logs;
void log_line(const std::string& s){logs.push_back(s);}
void host_freeze236(bool paused){g_host_paused=paused;}
bool sheet_save_instant_state(const std::string&){++saves;return save_ok;}
bool instant_state_exists(const std::string&){return exists;}
bool delete_instant_state(const std::string&){++deletes;return delete_ok;}
struct Catalog{void invalidate_saved(){++invalidations;}};
struct MenuUi{Catalog catalog;}g_menu_name_ui;
'''
tail=r'''
int main(int argc,char** argv){
 assert(argc==2);g_keymap_path=argv[1];
 using namespace host_keymap;
 const u64 bits[]={HidNpadButton_A,HidNpadButton_B,HidNpadButton_X,HidNpadButton_L,HidNpadButton_R,HidNpadButton_ZL,HidNpadButton_ZR,
 HidNpadButton_Plus,HidNpadButton_Minus,HidNpadButton_StickR,HidNpadButton_Up,HidNpadButton_Down,HidNpadButton_Left,HidNpadButton_Right,
 HidNpadButton_StickLUp,HidNpadButton_StickLDown,HidNpadButton_StickLLeft,HidNpadButton_StickLRight,HidNpadButton_Y,
 HidNpadButton_StickRUp,HidNpadButton_StickRDown,HidNpadButton_StickRLeft,HidNpadButton_StickRRight};
 for(int i=0;i<InputCount;++i){g_keymap_active=Config{};g_keymap_active.automatic=false;assign(g_keymap_active,false,Fire,0,i);assert(mapped_buttons(bits[i])==(1u<<i));assert(mapped_codes(bits[i])==std::set<int>{-5});}
 g_keymap_active=Config{};assert(mapped_codes(HidNpadButton_Y)==std::set<int>{-5});
 assert(mapped_codes(HidNpadButton_StickRUp)==std::set<int>{50});
 assert(mapped_codes(HidNpadButton_StickRDown)==std::set<int>{56});
 assert(mapped_codes(HidNpadButton_StickRLeft)==std::set<int>{52});
 assert(mapped_codes(HidNpadButton_StickRRight)==std::set<int>{54});
 assign(g_keymap_active,false,Fire,1,A);assert((mapped_codes(HidNpadButton_A)==std::set<int>{-5,53}));
 assert(mapped_buttons(HidNpadButton_StickL)==0);
 assert(mapped_buttons(HidNpadButton_AnySL|HidNpadButton_AnySR)==0);
 open_keymap(HidNpadButton_StickL);
 assert(g_keymap_editor.open && g_keymap_editor.page==Page::SaveExit && g_host_paused && !g_keymap_phone_attempted);
 assert(!update_keymap_editor(HidNpadButton_A,HidNpadButton_A,false));assert(saves==1 && invalidations==1 && g_host_paused);
 assert(g_keymap_editor.save_note=="已保存即时存档" && !g_keymap_editor.save_note_error && !g_keymap_editor.save_note_neutral);
 save_ok=false;update_keymap_editor(HidNpadButton_A,0,false);
 assert(g_keymap_editor.save_note=="即时存档失败" && g_keymap_editor.save_note_error && !g_keymap_editor.save_note_neutral);
 save_ok=true;update_keymap_editor(HidNpadButton_A,0,false);
 assert(!g_keymap_editor.save_note_error && !g_keymap_editor.save_note_neutral);
 update_keymap_editor(HidNpadButton_Down,0,false);update_keymap_editor(HidNpadButton_A,0,false);
 assert(g_keymap_editor.pending==Pending::Delete && deletes==0);
 update_keymap_editor(HidNpadButton_R,0,false);assert(g_keymap_editor.page==Page::SaveExit);
 update_keymap_editor(HidNpadButton_B,0,false);assert(g_keymap_editor.pending==Pending::None);
 delete_ok=false;update_keymap_editor(HidNpadButton_A,0,false);update_keymap_editor(HidNpadButton_A,0,false);
 assert(deletes==1 && g_keymap_editor.save_note=="删除即时存档失败" && g_keymap_editor.save_note_error && !g_keymap_editor.save_note_neutral);
 delete_ok=true;update_keymap_editor(HidNpadButton_A,0,false);update_keymap_editor(HidNpadButton_A,0,false);
 assert(deletes==2 && g_keymap_editor.save_note=="已删除即时存档" && !g_keymap_editor.save_note_error && !g_keymap_editor.save_note_neutral);
 exists=false;update_keymap_editor(HidNpadButton_A,0,false);update_keymap_editor(HidNpadButton_A,0,false);assert(deletes==2 && g_keymap_editor.save_note=="没有即时存档" && !g_keymap_editor.save_note_error && g_keymap_editor.save_note_neutral);
 update_keymap_editor(HidNpadButton_Down,0,false);update_keymap_editor(HidNpadButton_A,0,false);
 assert(g_keymap_editor.pending==Pending::Exit);update_keymap_editor(HidNpadButton_B,0,false);
 update_keymap_editor(HidNpadButton_A,0,false);assert(update_keymap_editor(HidNpadButton_A,HidNpadButton_A,false));assert(!g_keymap_editor.open);
 g_host_paused=false;open_keymap(HidNpadButton_StickL);update_keymap_editor(HidNpadButton_R,0,false);
 assert(g_keymap_editor.page==Page::Keymap && g_keymap_phone_attempted);
 // Navigate the actual editor from numeric 5 to central confirmation, then select Y.
 for(int i=0;i<3;++i)update_keymap_editor(HidNpadButton_Up,0,false);
 assert(g_keymap_editor.phone==Fire);
 update_keymap_editor(HidNpadButton_A,0,false);update_keymap_editor(HidNpadButton_A,0,false);
 assert(g_keymap_editor.phase==Phase::Choice && choice_inputs[g_keymap_editor.choice]==Y);
 update_keymap_editor(HidNpadButton_Down,0,false);assert(choice_inputs[g_keymap_editor.choice]==L);
 update_keymap_editor(HidNpadButton_Up,0,false);update_keymap_editor(HidNpadButton_A,0,false);
 assert(g_keymap_editor.draft.bindings[Fire][0]==Y && g_keymap_editor.draft.bindings[N5][0]==A);
 update_keymap_editor(HidNpadButton_Down,0,false);update_keymap_editor(HidNpadButton_Down,0,false);
 assert(g_keymap_editor.slot==2);update_keymap_editor(HidNpadButton_A,0,false);
 for(int i=0;i<choice_for_input(R3);++i)update_keymap_editor(HidNpadButton_Down,0,false);
 update_keymap_editor(HidNpadButton_A,0,false);assert(g_keymap_editor.draft.bindings[Fire][2]==R3);
 update_keymap_editor(HidNpadButton_Y,0,false);assert(g_keymap_editor.global);
 update_keymap_editor(HidNpadButton_Plus,HidNpadButton_Plus,false);
 assert(g_keymap_editor.open && g_host_paused && g_keymap_editor.note=="映射保存成功" && !g_keymap_editor.note_error);
 Store persisted;assert(load(g_keymap_path,persisted));assert(persisted.global.bindings[Fire][2]==R3);
 update_keymap_editor(HidNpadButton_StickL,HidNpadButton_StickL|HidNpadButton_Plus,false);
 assert(!g_keymap_editor.open && !g_host_paused && (g_keymap_blocked&HidNpadButton_Plus));
 g_keymap_blocked=0;assert(mapped_codes(HidNpadButton_Y)==std::set<int>{-5});
 open_keymap(HidNpadButton_StickL);update_keymap_editor(HidNpadButton_R,0,false);
 const auto before=serialize(g_keymaps);g_keymap_path="/no-such-directory/l3/keymap.cfg";
 update_keymap_editor(HidNpadButton_Plus,HidNpadButton_Plus,false);assert(g_keymap_editor.open && g_host_paused && serialize(g_keymaps)==before);
 assert(g_keymap_editor.note=="保存失败，请重试" && g_keymap_editor.note_error);
 update_keymap_editor(HidNpadButton_X,0,false);
 assert(g_keymap_editor.draft.automatic && g_keymap_editor.note=="已恢复默认，请按 + 保存" && !g_keymap_editor.note_error);
 update_keymap_editor(HidNpadButton_Y,0,false);
 assert(g_keymap_editor.global && g_keymap_editor.note=="已切换为全局映射，请按 + 保存" && !g_keymap_editor.note_error);
 update_keymap_editor(HidNpadButton_Y,0,false);
 assert(!g_keymap_editor.global && g_keymap_editor.note=="已切换为当前游戏映射，请按 + 保存");
 update_keymap_editor(HidNpadButton_StickL,HidNpadButton_StickL|HidNpadButton_A,false);
 assert(!g_keymap_editor.open && !g_host_paused && (g_keymap_blocked&HidNpadButton_A));
 g_host_paused=true;open_keymap(HidNpadButton_StickL);update_keymap_editor(HidNpadButton_StickL,HidNpadButton_StickL,true);assert(g_host_paused);
 std::puts("PASS production Switch controller: 23 HID inputs, third-slot native input selection, right stick default 2846, L3/SL/SR excluded, default save page, lazy phone load, save/delete/exit/cancel, instant-save/delete success/failure and absent-save neutral severity, tab switching, global Y config, save succeeds without closing or resuming, reset/scope/save result notices, failed commit stays paused, closing input swallowed, existing pause preserved; old gameplay Y popup absent");
}
'''
# AnySL/SR are aliases in libnx, separate from its BITL declarations.
header=header.replace('#define VMS_CL(n) n','constexpr u64 HidNpadButton_AnySL=HidNpadButton_LeftSL|HidNpadButton_RightSL;\nconstexpr u64 HidNpadButton_AnySR=HidNpadButton_LeftSR|HidNpadButton_RightSR;\n#define VMS_CL(n) n')
cpp=destination/'switch_l3_menu_checks.cpp';cpp.write_text(header+physical+controller+tail)
binary=destination/'switch_l3_menu_checks'
subprocess.run(['g++','-std=c++17','-O2','-I'+str(root/'src'),str(cpp),'-o',str(binary)],check=True)
binary.chmod(0o755)
subprocess.run([str(binary),str(destination/'keymap.cfg')],check=True)
