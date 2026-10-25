"""Exercise the real Switch launcher repeat prelude and L/R movement branch."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'src/platform/switch_host.inc').read_text()
prelude=s[s.index('            const bool up_held='):s.index('            // Horizontal navigation only switches panes;')]
start=s.index('                if ((down & HidNpadButton_AnyDown)')
end=s.index('                } else if ((down & HidNpadButton_Minus)',start)
branch=s[start:end]+'                }\n'
out=root/'build/menu-page-repeat';out.mkdir(parents=True,exist_ok=True)
cpp=out/'checks.cpp'
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
std::vector<int> items(24);int sel=0;std::string status;
bool g_menu_note_error=false,g_menu_note_neutral=true;
int64_t tick=0;int64_t monotonic_ms(){return tick;}
bool step(u64 down,u64 held,int64_t now){tick=now;
'''+prelude+branch+r'''
return changed;
}
int main(){
 assert(step(4,4,0) && sel==9);
 assert(!step(0,4,299) && sel==9);
 assert(step(0,4,300) && sel==18 && g_menu_repeat.browsing);
 assert(step(0,4,380) && sel==0); // final page wraps to first
 assert(step(8,8,381) && sel==18); // reverse immediately
 step(0,0,382);assert(!g_menu_repeat.browsing);
 assert(!step(0,0,900) && sel==18);
 assert(!step(12,12,901) && sel==18); // L+R cancel
 step(0,0,902);sel=0;assert(step(8,8,903) && sel==18);
 assert(step(0,8,1203) && sel==9);assert(step(0,8,1283) && sel==0);
 assert(step(0,8,1363) && sel==18);
 g_menu_focus=host_menu::Focus::Bezel;step(0,8,1800);assert(sel==18 && !g_menu_repeat.browsing);
 g_menu_focus=host_menu::Focus::Games;step(0,0,1801);items.resize(4);sel=2;step(4,4,1802);assert(sel==2);
 step(0,0,1803);items.clear();sel=0;step(8,8,1804);assert(sel==0);
 // Shared repeat history cannot leak from row navigation into page navigation.
 items.resize(24);step(0,0,1900);step(1,1,1901);assert(sel==1);
 assert(step(4,4,1902) && sel==10);assert(!step(0,4,2100) && sel==10);
 assert(step(0,4,2202) && sel==19);assert(step(0,4,2282) && sel==1);
}
''')
exe=out/'checks';subprocess.run(['g++','-std=c++17','-O2','-I'+str(root/'src'),str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
print('PASS launcher L/R: 300/80ms repeat, circular pages, release, reversal, L+R cancellation, focus reset, short/empty lists and row-to-page transitions')
