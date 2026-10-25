#!/usr/bin/env python3
"""Exercise the real Switch menu branch with host stubs; no copied controller logic."""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    source = (ROOT / 'src/j2mevm.cpp').read_text()
    begin = source.index('            // Four visible choices only:')
    end = source.index('        } else {\n            if(down & HidNpadButton_StickL)', begin)
    branch = source[begin:end]
    prefix = r'''
#include "host_menu.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
enum : unsigned long long {
 HidNpadButton_A=1, HidNpadButton_B=2, HidNpadButton_X=4,
 HidNpadButton_AnyLeft=8, HidNpadButton_AnyRight=16,
 HidNpadButton_AnyUp=32, HidNpadButton_AnyDown=64,
 HidNpadButton_R=128, HidNpadButton_ZR=256,
 HidNpadButton_L=512, HidNpadButton_ZL=1024, HidNpadButton_Minus=2048
};
using F=host_menu::Focus;
F g_menu_focus=F::Games, g_menu_option_focus=F::Bezel;
host_menu::Settings g_menu_settings;
struct Item { std::string label="test.jar", path="/switch/j2me/test.jar"; };
std::vector<Item> items(23);
int sel=0,in_game=0,g_slices=0,saves=0,launches=0,presents=0;
bool import_prompt=false,save_ok=true;
std::string status;
bool save_menu_settings(){++saves;return save_ok;}
void draw_menu(const std::vector<Item>&,int,const std::string&){}
void draw_import_prompt(const Item&){}
void log_line(const std::string&){}
void launch_entry(const Item&){++launches;}
void present(){++presents;}
void step(unsigned long long down){for(int once=0;once<1;++once){
'''
    suffix = r'''
}}
int main(){
 step(HidNpadButton_AnyRight);assert(g_menu_focus==F::Bezel&&saves==0);
 step(HidNpadButton_AnyRight|HidNpadButton_A);assert(g_menu_settings.bezel==0&&saves==0);
 step(HidNpadButton_AnyLeft|HidNpadButton_A);assert(g_menu_focus==F::Games&&launches==0&&saves==0);
 step(HidNpadButton_AnyLeft|HidNpadButton_A);assert(launches==0);
 step(HidNpadButton_AnyRight);step(HidNpadButton_A);assert(g_menu_settings.bezel==1&&saves==1);
 for(int i=1;i<host_menu::bezel_count;++i)step(HidNpadButton_A);
 assert(g_menu_settings.bezel==0&&saves==host_menu::bezel_count);
 int before=saves;
 step(HidNpadButton_AnyDown);assert(g_menu_focus==F::Speed);
 step(HidNpadButton_A);assert(saves==before&&!status.empty());
 step(HidNpadButton_AnyDown);assert(g_menu_focus==F::Fps);
 step(HidNpadButton_AnyDown);assert(g_menu_focus==F::Fps);
 step(HidNpadButton_AnyRight);assert(g_menu_settings.show_fps&&saves==before);
 step(HidNpadButton_A);assert(!g_menu_settings.show_fps&&saves==before+1);
 step(HidNpadButton_AnyLeft);assert(g_menu_focus==F::Games);
 step(HidNpadButton_AnyRight);assert(g_menu_focus==F::Fps&&!g_menu_settings.show_fps);
 step(HidNpadButton_B);step(HidNpadButton_X);assert(g_menu_focus==F::Fps);
 step(HidNpadButton_A);assert(g_menu_settings.show_fps&&saves==before+2);
 step(HidNpadButton_AnyUp);step(HidNpadButton_AnyUp);step(HidNpadButton_AnyUp);assert(g_menu_focus==F::Bezel);
 save_ok=false;step(HidNpadButton_A);assert(g_menu_settings.bezel==1&&status.find("保存失败")!=std::string::npos);save_ok=true;
 step(HidNpadButton_X);assert(g_menu_focus==F::Games);
 step(HidNpadButton_AnyDown);assert(sel==1);step(HidNpadButton_AnyUp);assert(sel==0);
 step(HidNpadButton_R);assert(sel==9);step(HidNpadButton_ZR);assert(sel==18);
 step(HidNpadButton_R);assert(sel==22);step(HidNpadButton_L);assert(sel==13);
 step(HidNpadButton_A);assert(launches==1&&in_game==1);in_game=0;
 step(HidNpadButton_Minus);assert(import_prompt&&presents==1);
 items.clear();sel=0;step(HidNpadButton_A);assert(launches==1);
 step(HidNpadButton_AnyRight);step(HidNpadButton_A);assert(g_menu_settings.bezel==2);
 std::puts("PASS MenuInputChecks: real native branch, pane navigation, A confirmation, wraparound, focus memory, speed placeholder, FPS, save failure, paging, launch and empty list");
}
'''
    build = ROOT / 'build/menu-input-checks'
    build.mkdir(parents=True, exist_ok=True)
    cpp = build / 'menu_input.cpp'
    cpp.write_text(prefix + branch + suffix)
    binary = build / ('sanitized' if args.sanitize else 'normal')
    flags = ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-no-pie'] if args.sanitize else ['-O2']
    subprocess.run(['g++', '-std=c++17', *flags, '-I', str(ROOT / 'src'), str(cpp), '-o', str(binary)], check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    subprocess.run([str(binary)], check=True, env=env)

if __name__ == '__main__':
    main()
