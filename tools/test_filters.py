#!/usr/bin/env python3
"""Standalone real host display-filter regressions; no Java or asset rebuild."""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def host_input_harness():
    """Compile the actual Switch event branches, with only platform IO stubbed."""
    source = (ROOT / 'src/platform/switch_host.inc').read_text(encoding='utf-8')
    def block(marker):
        start = source.index(marker)
        brace = source.index('{', start)
        depth = 1
        end = brace + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        return source[start:end]
    branches = [block('if (down & HidNpadButton_B)')]
    # Include the old special case when testing the filter1 baseline, so the
    # regression fails on its real behavior rather than a missing marker.
    old_filter = 'if (VMS_PLAT(g_menu_focus) == host_menu::Focus::Filter &&'
    if old_filter in source:
        branches.append(block(old_filter))
    branches.extend([block('if (down & HidNpadButton_AnyLeft)'),
                     block('if (down & HidNpadButton_AnyRight)'),
                     block('if (down & (HidNpadButton_AnyUp | HidNpadButton_AnyDown))')])
    confirm = block('if (down & HidNpadButton_A) {')
    # Keep the actual bezel/filter/FPS setting branches. The following speed
    # branch performs unrelated game IO and is outside this controller check.
    speed = '} else if (VMS_PLAT(g_menu_focus) == host_menu::Focus::Speed'
    confirm = confirm[:confirm.index(speed)] + '}\n}'
    branches.append(confirm)
    branches = '\nelse '.join(branches)
    saving = block('if(settings_changed)')
    return r'''
#include "host_menu_settings.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#define VMS_PLAT(x) x
enum {HidNpadButton_AnyLeft=1,HidNpadButton_AnyRight=2,HidNpadButton_A=4,HidNpadButton_AnyUp=8,HidNpadButton_AnyDown=16,HidNpadButton_B=32};
host_menu::Focus g_menu_focus=host_menu::Focus::Bezel,g_menu_option_focus=g_menu_focus;
host_menu::Settings g_menu_settings;bool g_menu_note_error=false,g_menu_note_neutral=true;
std::string status,path;int saves=0;
bool save_menu_settings(){++saves;return host_menu::save_settings(path,g_menu_settings);}
void require(bool ok,const char* m){if(!ok){std::fprintf(stderr,"ASSERT %s\n",m);std::exit(1);}}
void step(int down){bool changed=false,settings_changed=false;
''' + branches + '\n' + saving + r'''
}
int main(int argc,char** argv){
    require(argc==2,"isolated settings fixture argument");path=argv[1];
    using F=host_menu::Focus;using D=display_filter::Filter;
    step(HidNpadButton_AnyDown);require(g_menu_focus==F::Filter&&g_menu_option_focus==F::Filter&&saves==0,"real down enters filter without saving");
    step(HidNpadButton_AnyRight);require(g_menu_focus==F::Filter&&g_menu_settings.filter==D::Original&&saves==0,"right stays in options without changing filter or saving");
    step(HidNpadButton_AnyLeft|HidNpadButton_A);require(g_menu_focus==F::Games&&g_menu_settings.filter==D::Original&&saves==0,"left consumes simultaneous A and returns to games without changing filter");
    step(HidNpadButton_AnyRight|HidNpadButton_A);require(g_menu_focus==F::Filter&&g_menu_settings.filter==D::Original&&saves==0,"right consumes simultaneous A and restores remembered filter focus");
    host_menu::Settings loaded;
    const D choices[]={D::Bilinear,D::Scale2x,D::Lcd,D::Crt,D::ScanlinesDX,D::Original};int confirmations=0;
    for(D choice:choices){step(HidNpadButton_A);++confirmations;
        require(g_menu_focus==F::Filter&&g_menu_settings.filter==choice&&saves==confirmations,"A alone cycles exactly one filter and saves once");
        require(host_menu::load_settings(path,loaded)&&loaded.filter==choice,"A-selected filter survives restart");}
    step(HidNpadButton_AnyLeft);require(g_menu_focus==F::Games&&g_menu_settings.filter==D::Original&&saves==6,"left changes pane only");
    step(HidNpadButton_AnyRight);require(g_menu_focus==F::Filter&&saves==6,"right returns to remembered filter option only");
    step(HidNpadButton_B);require(g_menu_focus==F::Games&&saves==6,"B returns to games without saving");step(HidNpadButton_AnyRight);
    step(HidNpadButton_AnyDown);require(g_menu_focus==F::Speed&&saves==6,"real down reaches speed");
    step(HidNpadButton_AnyDown);step(HidNpadButton_AnyDown);require(g_menu_focus==F::Fps,"real last option clamps");
    step(HidNpadButton_AnyUp);step(HidNpadButton_AnyUp);require(g_menu_focus==F::Filter,"real up returns through filter");
    step(HidNpadButton_AnyUp);step(HidNpadButton_AnyUp);require(g_menu_focus==F::Bezel,"real first option clamps");
    step(HidNpadButton_A);require(g_menu_settings.bezel==5&&g_menu_settings.filter==D::Original&&saves==7,"bezel retains A-only confirmation behavior in the new choice order");
    step(HidNpadButton_AnyLeft|HidNpadButton_A);require(g_menu_focus==F::Games&&g_menu_settings.bezel==5&&saves==7,"bezel horizontal priority matches filter");
    require(std::remove(path.c_str())==0,"input cleanup");std::puts("PASS FiltersHostInput: real pane navigation, horizontal priority, A-only six-filter cycle and persistence");
}
'''

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--evidence', type=Path, default=ROOT / 'build' / 'filter-evidence')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--skip-build', action='store_true')
    args = parser.parse_args()
    output = args.evidence.resolve()
    output.mkdir(parents=True, exist_ok=True)
    compiler = shutil.which(os.environ.get('CXX', 'g++'))
    if not compiler:
        print('ASSERT C++ compiler unavailable', file=sys.stderr)
        return 1
    results = []
    def run(name, argv):
        before = datetime.now(timezone.utc).isoformat()
        proc = subprocess.run([str(x) for x in argv], cwd=ROOT, capture_output=True, text=True, timeout=180)
        (output / (name + '.stdout.txt')).write_text(proc.stdout, encoding='utf-8')
        (output / (name + '.stderr.txt')).write_text(proc.stderr, encoding='utf-8')
        results.append(dict(name=name, argv=[str(x) for x in argv], utc_start=before,
                            utc_end=datetime.now(timezone.utc).isoformat(), raw_rc=proc.returncode))
        print(proc.stdout, end='', flush=True)
        print(proc.stderr, end='', file=sys.stderr, flush=True)
        return proc.returncode == 0
    flags = (['-O1', '-g', '-fno-pie', '-no-pie', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
             if args.sanitize else ['-O2'])
    passed = True
    sources = {name: ROOT / 'tests' / (name + '.cpp') for name in ['filters_checks', 'filters_menu_checks']}
    if (ROOT / 'src/presentation/filters.h').exists():
        generated = output / 'filters_host_input.cpp'
        generated.write_text(host_input_harness(), encoding='utf-8')
        sources['filters_host_input'] = generated
    for name, source in sources.items():
        binary = output / (name + ('-san' if args.sanitize else ''))
        compiled = args.skip_build or run(name + '-compile', [compiler, '-std=c++17', '-fno-exceptions', '-pthread', '-I' + str(ROOT / 'src')] + flags + [source, '-o', binary])
        if compiled:
            arguments = [output / 'menu-input.cfg'] if name == 'filters_host_input' else []
            passed = run(name, [binary] + arguments) and passed
        else:
            passed = False
    (output / 'results.json').write_text(json.dumps(dict(sanitized=args.sanitize, results=results, passed=passed), indent=2) + '\n', encoding='utf-8')
    return 0 if passed else 1

if __name__ == '__main__':
    sys.exit(main())
