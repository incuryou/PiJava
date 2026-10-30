#!/usr/bin/env python3
"""Capture native UI behavior and compile extracted real Switch input branches."""
import argparse
from datetime import datetime,timezone
import json,os,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def block(source,start):
    brace=source.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[brace:end]

def input_harness():
    source=(ROOT/'src/platform/switch_host.inc').read_text(encoding='utf-8')
    start=source.index('static bool update_keymap_editor(');start=source.index('{',start)+1
    branch=source[start:source.index('    if(previous_page!=',start)]
    return r'''
#include "host_keymap_ui.h"
#include <cstdio>
#include <cstdlib>
using u64=uint64_t;
enum:u64 {HidNpadButton_StickL=1,HidNpadButton_L=2,HidNpadButton_R=4,HidNpadButton_Plus=8,HidNpadButton_B=16,HidNpadButton_X=32,HidNpadButton_Y=64,HidNpadButton_A=128,HidNpadButton_AnyUp=256,HidNpadButton_AnyDown=512,HidNpadButton_AnyLeft=1024,HidNpadButton_AnyRight=2048,HidNpadButton_Minus=4096};
host_keymap::Editor g_keymap_editor;int64_t tick=0;
int64_t monotonic_ms(){return tick;}
int failures=0;void check(bool ok,const char* m){if(!ok){++failures;std::fprintf(stderr,"FAIL %s\n",m);}}
host_keymap::Result step(u64 down,u64 held=0){
''' + branch + r'''
return result;}
int main(int argc,char** argv){using namespace host_keymap;if(argc!=2)return 2;
    const std::string id(64,'a');Store store;
    // Custom dense draft covers all 19 phone keys and all three slots.
    for(bool automatic:{false,true})for(Phase phase:{Phase::Phone,Phase::Slot,Phase::Choice}){
        auto& e=g_keymap_editor;e.begin(store,id,"fixture",true,true);e.page=Page::Keymap;e.phase=phase;e.phone=N7;e.slot=2;e.global=true;
        e.draft.automatic=automatic;for(auto& slots:e.draft.bindings)slots={{A,B,X}};
        const auto before=serialize(store);const int phone=e.phone,slot=e.slot;
        auto result=step(HidNpadButton_Minus);
        check(result==Result::None,"Minus only changes current draft");
        check(!e.draft.automatic,"Minus materializes an explicitly empty mapping instead of defaults");
        bool empty=true;for(const auto& slots:e.draft.bindings)for(int input:slots)empty&=input==none;
        check(empty,"Minus clears every one of the 19 by 3 draft bindings");
        check(serialize(store)==before,"Minus does not persist a Store change");
        check(e.standard&&e.was_paused&&e.global&&e.phone==phone&&e.slot==slot&&e.id==id&&e.page==Page::Keymap,"Minus preserves direction style, scope and editor context");
        result=step(HidNpadButton_Plus);check(result==Result::Save,"Plus retains normal draft save result after clear");
        check(commit(argv[1],store,id,e.draft,e.global),"cleared draft persists through ordinary commit");
        check(!lookup(store,id).automatic&&effective(lookup(store,id),true)==empty_bindings(),"saved mapping stays empty when reopened");
    }
    auto& e=g_keymap_editor;e.begin(store,id,"fixture",false,false);e.page=Page::SaveExit;const Config before=e.draft;
    check(step(HidNpadButton_Minus)==Result::None&&e.draft.automatic==before.automatic&&e.draft.bindings==before.bindings,"Minus does not clear outside mapping page");
    e.page=Page::Keymap;check(step(HidNpadButton_StickL)==Result::Close,"actual L3 branch remains reserved for menu close");
    for(int input:choice_inputs)check(input==none||(input>=0&&input<InputCount),"input choices contain no reserved L3 ID");
    std::remove(argv[1]);std::printf("real Switch clear mapping checks: %d failure(s)\n",failures);return failures?1:0;
}
'''

def launcher_harness():
    source=(ROOT/'src/platform/switch_host.inc').read_text(encoding='utf-8')
    start=source.index('} else if (down & HidNpadButton_A) {',source.index('host_menu::option_step'))
    branch=block(source,start)
    # Speed persistence is unrelated to rotation. Keep the real A dispatch and
    # replace only its speed filesystem body, which cannot run for this focus.
    speed=branch.index('host_menu::Focus::Speed')
    speed_body=block(branch,speed)
    branch=branch.replace(speed_body,'{}',1)
    start=source.index('            if(settings_changed)',start)
    saving='if(settings_changed) '+block(source,start)
    return r'''
#include "host_display_preferences.h"
#include <cstdio>
#define VMS_PLAT(x) x
host_menu::Settings g_menu_settings;
host_menu::Focus g_menu_focus=host_menu::Focus::Games;
struct Entry{std::string path,label="fixture";};std::vector<Entry> items(1);int sel=0;
std::string status;bool g_menu_note_error=false,g_menu_note_neutral=true;
int saves=0,presenters=0,invalidations=0;
void select_presenter(){++presenters;}bool save_menu_settings(){++saves;return true;}
void invalidate_display_preferences(){++invalidations;}void log_line(const std::string&){}
void confirm(){bool changed=false,settings_changed=false;
''' + branch + saving + r'''
}
int main(int argc,char** argv){if(argc!=2)return 2;int failures=0;auto check=[&](bool ok,const char* m){if(!ok){++failures;std::fprintf(stderr,"FAIL %s\n",m);}};
    const std::string dir=argv[1],first=dir+"/rotation-game-a.jar",second=dir+"/rotation-game-b.jar",global=dir+"/rotation-global.cfg";
    host_display::save_rotation(first,0);host_display::save_rotation(second,2);
    host_menu::Settings global_settings;global_settings.rotation=3;global_settings.show_fps=false;host_menu::save_settings(global,global_settings);
    items[0].path=first;
    g_menu_focus=host_menu::option_step(host_menu::Focus::Filter,1);
    const int bezel=g_menu_settings.bezel,filter=int(g_menu_settings.filter);const bool fps=g_menu_settings.show_fps;
    for(int expected:{1,2,3,0}){
        int before=invalidations;confirm();
        check(g_menu_settings.rotation==expected,"actual launcher A branch cycles the row below Filter through 90/180/270/0 degrees");
        check(host_display::load(first).rotation==expected,"actual launcher A branch persists rotation in selected game's sidecar");
        check(host_display::load(second).rotation==2,"rotation change leaves other game's sidecar untouched");
        check(invalidations==before+1,"successful per-game rotation invalidates cached display preferences");
    }
    check(saves==0,"per-game rotation never writes global menu settings");
    host_menu::Settings loaded;check(host_menu::load_settings(global,loaded)&&loaded.rotation==3&&!loaded.show_fps,"per-game rotation preserves global saved preferences");
    check(g_menu_settings.bezel==bezel&&int(g_menu_settings.filter)==filter&&g_menu_settings.show_fps==fps,"rotation confirmation preserves other options");
    items[0].path=dir+"/directory-that-does-not-exist/game.jar";const int before=g_menu_settings.rotation,invalid_before=invalidations;confirm();
    check(g_menu_settings.rotation==before&&invalidations==invalid_before&&g_menu_note_error,"failed sidecar save preserves current rotation and reports failure");
    check(host_display::load(first).rotation==0&&host_display::load(second).rotation==2,"failed rotation save leaves existing games unchanged");
    std::printf("real launcher rotation A checks: %d failure(s)\n",failures);return failures?1:0;
}
'''

def plus_harness():
    source=(ROOT/'src/platform/switch_host.inc').read_text(encoding='utf-8')
    anchor=source.index('else g_display_menu.begin();',source.index('// Consume horizontal presses'))
    start=source.rfind('else if',0,anchor)
    body=block(source,start)
    branch=source[start+len('else '):source.index('{',start)]+body
    return r'''
#include "host_display_menu.h"
#include <cstdio>
using u64=uint64_t;constexpr u64 HidNpadButton_Plus=1;
#define VMS_PLAT(x) x
host_menu::Focus g_menu_focus=host_menu::Focus::Games;
host_display_menu::Model g_display_menu;menu_actions::Repeat g_menu_repeat;
std::vector<int> items(1);int sel=0,draws=0,presents=0;std::string status;
void draw_menu(const std::vector<int>&,int,const std::string&){++draws;}void present(){++presents;}
void step(u64 down){for(int once=0;once<1;++once){
''' + branch + r'''
}}
int main(){int failures=0;auto check=[&](bool ok,const char* m){if(!ok){++failures;std::fprintf(stderr,"FAIL %s\n",m);}};
    // Numeric 5 also covers appended Rotation without requiring a new enum to compile on baseline.
    for(auto focus:{host_menu::Focus::Games,host_menu::Focus::Bezel,host_menu::Focus::Filter,host_menu::Focus::Speed,host_menu::Focus::Fps,static_cast<host_menu::Focus>(5)}){
        g_menu_focus=focus;g_display_menu.open=false;status.clear();int before=presents;
        step(HidNpadButton_Plus);
        check(g_display_menu.open&&presents==before+1,"actual launcher Plus branch opens More Settings from every focus");
        check(g_menu_focus==focus,"Plus opens overlay without changing remembered launcher focus");
        g_display_menu.open=false;step(0);check(!g_display_menu.open,"no Plus edge leaves More Settings closed");
    }
    items.clear();g_menu_focus=host_menu::Focus::Filter;g_display_menu.open=false;step(HidNpadButton_Plus);
    check(!g_display_menu.open&&status=="先选择一款游戏","Plus from options with no games reports normal selection prompt");
    std::printf("real launcher Plus checks: %d failure(s)\n",failures);return failures?1:0;
}
'''

def canvas_harness():
    source=(ROOT/'src/j2mevm.cpp').read_text(encoding='utf-8')
    body=block(source,source.index('static void select_screen_sidecar('))
    return r'''
#include "canvas_resolution.h"
#include "canvas_declared_size.h"
#include <cstdio>
std::unordered_map<std::string,std::string> g_props;
std::unordered_map<std::string,std::vector<uint8_t>> g_jar_files;
std::string g_canvas_notice;int g_screen_w=0,g_screen_h=0;
#define VMS_CL(x) x
#define VMS_RS(x) x
void log_line(const std::string&){}
void select_screen_sidecar(const std::string& jar,const std::string& identity="")
''' + body + r'''
int main(int argc,char** argv){if(argc!=2)return 2;int failures=0;
    auto check=[&](bool ok,const char* m){if(!ok){++failures;std::fprintf(stderr,"FAIL %s\n",m);}};
    const std::string path=std::string(argv[1])+"/unrecognized-game.jar";
    select_screen_sidecar(path);
    check(g_screen_w==240&&g_screen_h==320,"unknown game retains automatic default size");
    check(g_canvas_notice.empty(),"unknown size displays no unrecognized/manual canvas prompt");
    g_jar_files["broken.class"]={0xca,0xfe};select_screen_sidecar(path);
    check(g_canvas_notice.empty(),"ambiguous canvas displays no manual canvas selection prompt");
    std::printf("real canvas notice checks: %d failure(s)\n",failures);return failures?1:0;
}
'''

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--evidence',type=Path,required=True);parser.add_argument('--windows-adapters',action='store_true');args=parser.parse_args()
    out=args.evidence.resolve();out.mkdir(parents=True,exist_ok=True);flags=['-O2'];rows=[];ok=True
    if args.windows_adapters:
        spec=out/'no-manifest.specs';spec.write_text('*endfile:\ncrtend.o%s\n');flags+=['-specs='+str(spec),'-Dlstat=stat']
    compiler=shutil.which('g++');assert compiler,'g++ must be on PATH'
    inputs=out/'switch-clear.cpp';inputs.write_text(input_harness(),encoding='utf-8')
    canvas=out/'canvas-notice.cpp';canvas.write_text(canvas_harness(),encoding='utf-8')
    launcher=out/'launcher-rotation.cpp';launcher.write_text(launcher_harness(),encoding='utf-8')
    plus=out/'launcher-plus.cpp';plus.write_text(plus_harness(),encoding='utf-8')
    for name,src,params in [('render',ROOT/'tests/menu_refresh_checks.cpp',[out/'menu.cfg']),('input',inputs,[out/'keymap.cfg']),('canvas',canvas,[out]),('launcher',launcher,[out]),('plus',plus,[])]:
        binary=out/(name+('.exe' if os.name=='nt' else ''))
        for phase,argv in [('compile',[compiler,'-std=c++17','-fno-exceptions','-I'+str(ROOT/'src'),*flags,src,'-o',binary]),('run',[binary,*params])]:
            start=datetime.now(timezone.utc).isoformat();result=subprocess.run([str(x) for x in argv],cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=180)
            (out/(name+'-'+phase+'.stdout.txt')).write_text(result.stdout,encoding='utf-8');(out/(name+'-'+phase+'.stderr.txt')).write_text(result.stderr,encoding='utf-8')
            rows.append(dict(name=name+'-'+phase,argv=[str(x) for x in argv],raw_rc=result.returncode,utc_start=start,utc_end=datetime.now(timezone.utc).isoformat()))
            print(name,phase,result.returncode,result.stdout+result.stderr,flush=True)
            if result.returncode:ok=False;break
    (out/'results.json').write_text(json.dumps(dict(passed=ok,results=rows),indent=2)+'\n');return 0 if ok else 1
if __name__=='__main__':raise SystemExit(main())
