#!/usr/bin/env python3
"""Reproduce launcher/L3 feature checks using disposable files, no real user data."""
import argparse,json,subprocess,tempfile,time
from pathlib import Path

root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sdk',type=Path,required=True)
p.add_argument('--font',type=Path,required=True)
p.add_argument('--output',type=Path,default=root/'build/menu-features')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
rows=[]
def run(name,argv,cwd=root):
    start=time.monotonic();r=subprocess.run([str(v) for v in argv],cwd=cwd,capture_output=True,text=True)
    (out/(name+'.stdout')).write_text(r.stdout);(out/(name+'.stderr')).write_text(r.stderr)
    rows.append(dict(name=name,argv=[str(v) for v in argv],cwd=str(cwd),rc=r.returncode,seconds=round(time.monotonic()-start,3)))
    (out/'results.json').write_text(json.dumps(dict(scope='Host functional checks; not Switch hardware acceptance',results=rows),ensure_ascii=False,indent=2)+'\n')
    print(name,'PASS' if r.returncode==0 else 'FAIL',flush=True)
    if r.returncode:raise SystemExit(r.returncode)
def compile(name,file,extra=()):
    binary=out/name
    run(name+'-compile',['g++','-O2','-std=c++17','-I'+str(root/'build'),root/'tests'/file,*extra,'-o',binary])
    return binary
with tempfile.TemporaryDirectory(prefix='j2me-menu-') as d:
    fixture=Path(d)
    run('prepare-5000',['python3',root/'tools/prepare_menu_perf_tests.py',fixture/'catalog'])
    puff=out/'puff.o';run('puff',['gcc','-O2','-c',root/'src/puff.c','-o',puff])
    catalog=compile('catalog','menu_catalog_checks.cpp',[puff,'-Wl,--wrap=fopen','-Wl,--wrap=stat'])
    run('catalog',[catalog],fixture/'catalog')
    names=compile('metadata','menu_name_checks.cpp',[puff]);run('metadata',[names])
    actions=compile('actions','menu_actions_checks.cpp');(fixture/'actions').mkdir();run('actions',[actions,fixture/'actions'])
    run('navigation',['python3',root/'tests/menu_navigation_checks.py'])
    run('delete-controller',['python3',root/'tests/menu_delete_checks.py'])
    run('l3-controller',['python3',root/'tests/switch_l3_menu_checks.py',a.sdk.resolve(),out/'l3-controller'])
    menu=compile('host-menu','host_menu_checks.cpp');run('host-menu',[menu])
    # A custom v1 record exercises the two-slot/v2-three-slot rejection checks.
    legacy=fixture/'v1.cfg';legacy.write_text('J2ME_KEYMAP 1\nglobal custom '+ ' '.join(['-1 -1']*19)+'\n')
    (fixture/'keymap').mkdir();(out/'l3-previews').mkdir(exist_ok=True)
    keymap=compile('keymap','host_keymap_checks.cpp')
    run('keymap',[keymap,fixture/'keymap',a.font.resolve(),root/'romfs/ui/nokia5300.rgba',legacy])
    preview=compile('preview','menu_preview.cpp')
    run('preview',[preview,out/'previews',a.font.resolve(),root/'romfs/ui/nokia5300.rgba'])
print('PASS menu feature suite:',out/'results.json')
