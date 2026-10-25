#!/usr/bin/env python3
"""Direct UI checks and the actual Switch Choice held-input branch."""
import argparse
from datetime import datetime,timezone
import json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def held_harness():
    source=(ROOT/'src/platform/switch_host.inc').read_text(encoding='utf-8')
    start=source.index('static bool update_keymap_editor(');start=source.index('{',start)+1
    branch=source[start:source.index('    if(previous_page!=',start)]
    start=source.index('if(!focused)');brace=source.index('{',start);depth=1;end=brace+1
    while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
    focus=source[start:end]
    return r'''
#include "host_keymap_ui.h"
#include <cstdio>
#include <cstdlib>
using u64=uint64_t;
enum:u64 {HidNpadButton_StickL=1,HidNpadButton_L=2,HidNpadButton_R=4,HidNpadButton_Plus=8,HidNpadButton_B=16,HidNpadButton_X=32,HidNpadButton_Y=64,HidNpadButton_A=128,HidNpadButton_AnyUp=256,HidNpadButton_AnyDown=512,HidNpadButton_AnyLeft=1024,HidNpadButton_AnyRight=2048};
host_keymap::Editor g_keymap_editor;int64_t tick=0;
int64_t monotonic_ms(){return tick;}
void svcSleepThread(int64_t){}
#define P7_SCOPE(x) (void)0
void require(bool ok,const char* m){if(!ok){std::fprintf(stderr,"ASSERT %s\n",m);std::exit(1);}}
void focus_step(bool focused){for(int once=0;once<1;++once){
''' + focus + r'''
}}
host_keymap::Result step(u64 down,u64 held){
''' + branch + r'''
return result;}
int main(){using namespace host_keymap;Store store;auto& e=g_keymap_editor;e.begin(store,std::string(64,'a'),"test",false,false);e.page=Page::Keymap;e.phase=Phase::Choice;e.choice=0;
 step(HidNpadButton_AnyDown,HidNpadButton_AnyDown);require(e.choice==1,"initial direction edge moves once");tick=319;step(0,HidNpadButton_AnyDown);require(e.choice==1,"held initial delay does not fire early");tick=320;step(0,HidNpadButton_AnyDown);require(e.choice==2,"actual held input repeats at 320 ms");tick=399;step(0,HidNpadButton_AnyDown);require(e.choice==2,"80 ms repeat does not fire early");tick=400;step(0,HidNpadButton_AnyDown);require(e.choice==3,"80 ms repeat fires at boundary");
 e.choice=InputCount;tick=480;step(0,HidNpadButton_AnyDown);require(e.choice==0,"held movement wraps last to first");tick=481;step(HidNpadButton_AnyUp,HidNpadButton_AnyUp);require(e.choice==InputCount,"direction reversal is immediate and wraps first to last");tick=482;step(0,0);tick=900;step(0,0);require(e.choice==InputCount,"released direction never repeats");
 tick=901;step(HidNpadButton_AnyDown,HidNpadButton_AnyDown);const int chosen=e.choice;tick=1221;step(HidNpadButton_A,HidNpadButton_AnyDown|HidNpadButton_A);require(e.phase==Phase::Slot&&e.choice==chosen,"simultaneous A takes priority over due held repeat");
 e.phase=Phase::Phone;const int phone=e.phone;tick=2000;step(0,HidNpadButton_AnyDown);require(e.phone==phone,"phone phase does not inherit held repeat");e.phase=Phase::Choice;tick=2001;step(0,HidNpadButton_AnyDown);const int resumed=e.choice;tick=2320;step(0,HidNpadButton_AnyDown);require(e.choice==resumed,"phase change resets the initial repeat delay");
 step(HidNpadButton_R,HidNpadButton_AnyDown);require(e.page==Page::SaveExit,"page action retains real R binding");const int save=e.save_selection;tick=3000;step(0,HidNpadButton_AnyDown);require(e.save_selection==save,"SaveExit never receives held-repeat moves");
 e.page=Page::Keymap;e.phase=Phase::Choice;tick=4000;step(HidNpadButton_AnyDown,HidNpadButton_AnyDown);focus_step(false);tick=5000;step(0,HidNpadButton_AnyDown);const int refocused=e.choice;tick=5319;step(0,HidNpadButton_AnyDown);require(e.choice==refocused,"actual focus loss resets the initial repeat timer");
 step(HidNpadButton_StickL,HidNpadButton_AnyDown);e.open=false;tick=6000;step(0,HidNpadButton_AnyDown);const int closed=e.choice;tick=7000;step(0,HidNpadButton_AnyDown);require(e.choice==closed,"closed editor never receives held moves");e.begin(store,std::string(64,'a'),"reopen",false,false);e.page=Page::Keymap;e.phase=Phase::Choice;tick=7001;step(0,HidNpadButton_AnyDown);const int reopened=e.choice;tick=7320;step(0,HidNpadButton_AnyDown);require(e.choice==reopened,"new open resets initial delay");
 std::puts("PASS KeymapChoiceHeld: real Switch held branch, 320/80 ms boundaries, wrap, release, phase/page reset and A priority");
}
'''
def main():
    p=argparse.ArgumentParser();p.add_argument('--evidence',type=Path,required=True);p.add_argument('--windows-adapters',action='store_true');p.add_argument('--sanitize',action='store_true');a=p.parse_args();out=a.evidence.resolve();out.mkdir(parents=True,exist_ok=True)
    flags=['-O2'];rows=[];ok=True
    if a.sanitize:flags=['-O1','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-fno-sanitize-recover=all']
    if a.windows_adapters:
        spec=out/'no-manifest.specs';spec.write_text('*endfile:\ncrtend.o%s\n');flags+=['-specs='+str(spec),'-Dlstat=stat']
    cpp=out/'choice-held.cpp';cpp.write_text(held_harness(),encoding='utf-8')
    for name,src,args in [('ui',ROOT/'tests/keymap_pro_ui_checks.cpp',[out/'menu.cfg']),('held',cpp,[])]:
        binary=out/(name+('.exe' if os.name=='nt' else ''))
        for phase,argv in [('compile',[shutil.which('g++'),'-std=c++17','-fno-exceptions','-I'+str(ROOT/'src'),*flags,src,'-o',binary]),('run',[binary,*args])]:
            start=datetime.now(timezone.utc).isoformat();r=subprocess.run([str(x) for x in argv],cwd=ROOT,capture_output=True,text=True,timeout=180)
            (out/(name+'-'+phase+'.stdout.txt')).write_text(r.stdout,encoding='utf-8');(out/(name+'-'+phase+'.stderr.txt')).write_text(r.stderr,encoding='utf-8');rows.append(dict(name=name+'-'+phase,argv=[str(x) for x in argv],raw_rc=r.returncode,utc_start=start,utc_end=datetime.now(timezone.utc).isoformat()));print(name,phase,r.returncode,r.stdout+r.stderr,flush=True)
            if r.returncode:ok=False;break
    (out/'results.json').write_text(json.dumps(dict(passed=ok,results=rows),indent=2)+'\n');return 0 if ok else 1
if __name__=='__main__':raise SystemExit(main())
