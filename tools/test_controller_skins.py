#!/usr/bin/env python3
"""Focused host controller-skin checks; no VM/game builds or cloud dispatch."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def cache_harness():
    source=(ROOT/'src/platform/switch_host.inc').read_text(encoding='utf-8')
    def block(marker):
        start=source.index(marker);brace=source.index('{',start);depth=1;end=brace+1
        while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
        return source[start:end]
    struct=block('struct GpuHostLayerCache')+';'
    sync=block('static void sync_controller_legends()') if 'static void sync_controller_legends()' in source else 'static void sync_controller_legends() {}'
    start=source.index('    // Phone skin and the FPS badge are host chrome.')
    chrome=source[start:source.index('    const uint8_t* src =',start)]
    start=source.index('        const bool rebuild=')
    end=source.index('        was_layered=layered;',start)+len('        was_layered=layered;')
    gpu=source[start:end]
    start=source.index('    std::string label = std::to_string(fps_shown);')
    prefix=source[start:source.index('#ifdef J2ME_ENABLE_DEKO3D',start)]
    return r'''
#include "controller_skin_labels.h"
#include "host_skin.h"
#include "host_speed.h"
#include "presentation/layers.h"
#include "presentation/gpu_filter.h"
#include <cstdio>
#include <cstdlib>
#define VMS_PLAT(x) x
#define VMS_EXEC(x) x
#define VMS_RS(x) x
using u32=uint32_t;
host_controller::Legends g_controller_legends;
host_keymap::Config g_keymap_active;
bool standard=false;bool standard_game_keys(){return standard;}
host_menu::Settings g_menu_settings;
host_skin::Cache g_host_skin;
// Production sync/chrome also keys adaptive geometry and oriented RGB dimensions.
int g_screen_w=240,g_screen_h=320,g_display_w=240,g_display_h=320;
std::vector<uint8_t> g_fb(size_t(g_screen_w)*g_screen_h*3);
const std::vector<uint8_t>* g_display_pixels=nullptr;
host_menu::UniversalLayout g_universal_layout;
auto g_universal_legend_layout=host_controller::universal_game_layout(g_universal_layout);
int g_cpu_cleared=0,g_chrome_bezel=-2,g_chrome_fps=-2,g_speed_cap=0,text_calls=0;
struct Font {struct M{bool ok;int advance;};M measure(const std::string& s,int n){return {true,host_menu::text_advance(s,n)};}} g_host_font;
void log_line(const std::string&){}
host_menu::Text host_text_callback(u32* frame,u32 pitch,bool centered=false){return [=](int x,int y,const std::string& s,uint32_t,int px,int maxw){++text_calls;if(centered){x-=maxw/2;y-=px/2;}uint32_t value=0xff000000;for(unsigned char c:s)value=value*33+c;host_menu::rect(frame,pitch,x,y,maxw,px,value|0xff000000);};}
void require(bool ok,const char* m){if(!ok){std::fprintf(stderr,"ASSERT %s\n",m);std::exit(1);}}
''' + struct+'\n'+sync+r'''
void compose_host_surface(u32* frame,u32 pitch,const std::string&,bool,bool force_chrome){
''' + chrome+r'''
}
GpuHostLayerCache cache;
void gpu_step(){int fps_shown=0;
''' + prefix+r'''
    auto& canvas=cache.canvas;auto& generation=cache.generation;auto& bezel=cache.bezel;auto& fps=cache.fps;
    auto& was_layered=cache.was_layered;auto& previous_label=cache.previous_label;const bool layered=true;
''' + gpu+r'''
}
int main(){
    g_menu_settings.bezel=4;g_menu_settings.show_fps=false;int reads=0;
    require(g_host_skin.select(4,{},[&](const char*,host_skin::Pixels& p,std::string&){++reads;p.assign(host_skin::pixel_count,host_menu::color(0x334455));return true;}),"preload controlled real cache");
    std::array<std::vector<uint32_t>,3> frames;for(auto& f:frames){f.resize(host_skin::pixel_count);compose_host_surface(f.data(),1280,"",true,false);}
    require(g_controller_legends.label(host_keymap::A).text=="5","actual CPU chrome synchronizes numeric effective mapping");
    const auto original=frames[0];require(g_cpu_cleared==3&&reads==1,"CPU ring warmup reuses decoded skin");
    g_keymap_active.automatic=false;g_keymap_active.bindings=host_keymap::empty_bindings();g_keymap_active.bindings[host_keymap::N1][0]=host_keymap::A;
    for(auto& f:frames)compose_host_surface(f.data(),1280,"",true,false);
    require(frames[0]!=original&&frames[0]==frames[1]&&frames[1]==frames[2]&&g_cpu_cleared==3,"mapping change repaints all three actual CPU frame slots");
    const int calls=text_calls;compose_host_surface(frames[0].data(),1280,"",true,false);require(text_calls==calls&&reads==1,"stable CPU frame has no redraw or decode");
    gpu_step();const auto generation=cache.generation;const auto canvas=cache.canvas;
    g_keymap_active.bindings[host_keymap::N1][0]=host_keymap::none;g_keymap_active.bindings[host_keymap::N9][0]=host_keymap::A;
    gpu_step();require(cache.generation>generation&&cache.canvas!=canvas,"actual GPU chrome rebuilds mapping with FPS disabled");
    const auto stable=cache.generation;gpu_step();require(cache.generation==stable,"unchanged GPU mapping reuses layer generation");
    g_keymap_active=host_keymap::Config{};standard=true;gpu_step();require(cache.generation>stable&&g_controller_legends.label(host_keymap::A).text=="确认","actual GPU observes automatic standard-mode change");
    std::puts("PASS ControllerChrome: real CPU triple slots, shared rendering, GPU cache generation and automatic mode invalidation with FPS off");
}
'''

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--evidence',type=Path,required=True)
    p.add_argument('--windows-adapters',action='store_true')
    p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();out=a.evidence.resolve();out.mkdir(parents=True,exist_ok=True)
    compiler=shutil.which(os.environ.get('CXX','g++'));rows=[]
    def run(name,argv):
        start=datetime.now(timezone.utc).isoformat();r=subprocess.run([str(x) for x in argv],cwd=ROOT,capture_output=True,text=True,timeout=180)
        (out/(name+'.stdout.txt')).write_text(r.stdout,encoding='utf-8');(out/(name+'.stderr.txt')).write_text(r.stderr,encoding='utf-8')
        rows.append(dict(name=name,argv=[str(x) for x in argv],raw_rc=r.returncode,utc_start=start,utc_end=datetime.now(timezone.utc).isoformat()))
        print(name,'raw_rc='+str(r.returncode),flush=True);print(r.stdout+r.stderr,flush=True);return r.returncode==0
    flags=['-O2']
    if a.sanitize:flags=['-O1','-g','-fno-pie','-no-pie','-fsanitize=address,undefined','-fno-sanitize-recover=all']
    if a.windows_adapters:
        specs=out/'no-manifest.specs';specs.write_text('*endfile:\ncrtend.o%s\n')
        flags+=['-specs='+str(specs),'-Dlstat=stat']
    binary=out/('controller-skins.exe' if os.name=='nt' else 'controller-skins')
    ok=run('model-compile',[compiler,'-std=c++17','-fno-exceptions',*flags,ROOT/'tests/controller_skins_checks.cpp','-o',binary])
    if ok:ok=run('model-run',[binary,out/'menu.cfg'])
    if (ROOT/'src/controller_skin_labels.h').exists():
        cpp=out/'controller-chrome.cpp';cpp.write_text(cache_harness(),encoding='utf-8');chrome=out/('controller-chrome.exe' if os.name=='nt' else 'controller-chrome')
        compiled=run('chrome-compile',[compiler,'-std=c++17','-fno-exceptions','-I'+str(ROOT/'src'),*flags,cpp,'-o',chrome])
        if compiled:ok=run('chrome-run',[chrome]) and ok
        else:ok=False
    (out/'results.json').write_text(json.dumps(dict(passed=ok,results=rows),indent=2)+'\n',encoding='utf-8')
    return 0 if ok else 1

if __name__=='__main__':raise SystemExit(main())
