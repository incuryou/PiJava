#!/usr/bin/env python3
"""Native C++ renderer assertions for the centered, phone-free mapping page."""
from pathlib import Path
import subprocess,argparse
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=ROOT/'build/keymap-centered');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
call='host_keymap::render(frame,pitch,e,text,center,right,&preview,measure)'
cpp=out/'checks.cpp';cpp.write_text('#define DRAW_EDITOR() '+call+'\n#include "'+str(ROOT/'tests/keymap_centered_checks.cpp')+'"\n')
exe=out/'checks';subprocess.run(['g++','-std=c++17','-O2','-I'+str(ROOT/'src'),cpp,'-o',exe],check=True)
r=subprocess.run([exe],cwd=ROOT,capture_output=True,text=True);(out/'result.log').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);raise SystemExit(r.returncode)
