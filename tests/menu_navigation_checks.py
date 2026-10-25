"""Run the real production list-movement branch without a Switch host."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
source=(root/'src/platform/switch_host.inc').read_text()
begin=source.index('                if ((down & HidNpadButton_AnyDown)')
end=source.index('                else if ((down & (HidNpadButton_R | HidNpadButton_ZR))',begin)
branch=source[begin:end]
prelude=source[source.index('            const bool up_held='):source.index('            // Horizontal navigation only switches panes;')]
build=root/'build/menu-navigation';build.mkdir(parents=True,exist_ok=True)
cpp=build/'checks.cpp'
cpp.write_text(r'''
#include "host_menu.h"
#include <cassert>
#include <vector>
using u64=uint64_t;
constexpr u64 HidNpadButton_AnyDown=1,HidNpadButton_AnyUp=2,HidNpadButton_Down=1,HidNpadButton_Up=2;
constexpr u64 HidNpadButton_R=4,HidNpadButton_L=8,HidNpadButton_ZR=16,HidNpadButton_ZL=32;
#define VMS_PLAT(n) n
host_menu::Focus g_menu_focus=host_menu::Focus::Games;
menu_actions::Repeat g_menu_repeat;
std::vector<int> items(5);int sel=0;std::string status;
bool g_menu_note_error=false,g_menu_note_neutral=true;
int64_t tick=0;int64_t monotonic_ms(){return tick;}
bool step(u64 down,u64 held,int64_t now){tick=now;
'''+prelude+branch+r'''
return changed;
}
int main(){assert(step(2,2,0) && sel==4);step(0,0,1);step(1,1,2);assert(sel==0);
assert(!step(0,1,301) && sel==0 && !g_menu_repeat.browsing);
assert(step(0,1,302) && sel==1 && g_menu_repeat.browsing);
assert(!step(0,1,381) && sel==1);assert(step(0,1,382) && sel==2);
step(0,0,383);assert(!g_menu_repeat.browsing);step(2,2,384);assert(sel==1);
step(1,1,385);assert(sel==2); // immediate reversal
step(3,3,400);assert(sel==2); // opposing directions cancel
g_menu_focus=host_menu::Focus::Bezel;step(0,1,999);assert(!g_menu_repeat.browsing && sel==2);
g_menu_focus=host_menu::Focus::Games;step(0,0,1000);sel=0;items.resize(1);step(2,2,1001);assert(sel==0);
items.clear();step(1,1,1002);step(2,2,1003);assert(sel==0);}
''')
binary=build/'checks'
subprocess.run(['g++','-std=c++17','-O2','-I'+str(root/'src'),str(cpp),'-o',str(binary)],check=True)
subprocess.run([str(binary)],check=True)
print('PASS MenuNavigationChecks: production repeat preprocessing and list branch; wraparound, 300/80ms, release/reversal/opposing directions/focus, empty and single list')
