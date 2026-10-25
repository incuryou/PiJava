#!/usr/bin/env python3
"""Build immutable isolated VM copies and run small sequential scheduling comparison."""
import argparse,difflib,hashlib,json,shutil,subprocess
from pathlib import Path
SOURCE=Path(__file__).resolve().parent
REPO=SOURCE.parent.parent
ROOT=REPO/'build/zxx-bench'
ROOT.mkdir(parents=True,exist_ok=True)
BASE=None
FINAL=REPO

def run(args,**kwargs):
    return subprocess.run([str(x) for x in args],cwd=ROOT,check=True,**kwargs)

def tool(name,module):return [shutil.which(name)] if shutil.which(name) else ['java','-m',module]

def build():
    (ROOT/'classes').mkdir(exist_ok=True)
    run(tool('javac','jdk.compiler/com.sun.tools.javac.Main')+['-source','8','-target','8','-bootclasspath',BASE/'romfs/classes','-classpath',BASE/'romfs/classes','-d',ROOT/'classes',SOURCE/'GatedCanvas.java'])
    before=(ROOT/'classes/GatedCanvas.class').read_bytes()
    (ROOT/'GatedCanvas.unpatched.class').write_bytes(before)
    p=run(['python3',SOURCE/'patch_fixture.py',ROOT/'classes/GatedCanvas.class'],capture_output=True,text=True)
    (ROOT/'fixture-patch.json').write_text(p.stdout)
    p=run(tool('javap','jdk.jdeps/com.sun.tools.javap.Main')+['-c','-p','-classpath',ROOT/'classes','GatedCanvas'],capture_output=True,text=True)
    (ROOT/'fixture.javap.txt').write_text(p.stdout)
    for variant,source in [('baseline31',BASE),('optimized33',FINAL)]:
        folder=ROOT/'variants'/variant
        if folder.exists():shutil.rmtree(folder)
        shutil.copytree(source/'src',folder/'src')
        vmfile=folder/'src/j2mevm.cpp';vm=vmfile.read_text()
        needle='static bool g_log_draw = true;'
        assert vm.count(needle)==1
        vm=vm.replace(needle,needle+'\nstatic int g_bench_paint_delay_ms=0,g_bench_present_delay_ms=0;')
        needle='    run(paint, {Value::of_ref(canvas), Value::of_ref(gid)});'
        assert vm.count(needle)==1
        vm=vm.replace(needle,needle+'\n    if(g_bench_paint_delay_ms>0)std::this_thread::sleep_for(std::chrono::milliseconds(g_bench_paint_delay_ms));')
        vmfile.write_text(vm)
        (folder/'instrumentation.diff').write_text(''.join(difflib.unified_diff((source/'src/j2mevm.cpp').read_text().splitlines(True),vm.splitlines(True),fromfile=str(source/'src/j2mevm.cpp'),tofile=str(vmfile))))
        run(['gcc','-O2','-c',folder/'src/puff.c','-o',folder/'puff.o'])
        command=['g++','-O2','-std=c++17','-fno-exceptions','-DBENCH_VM="'+str(vmfile)+'"','-DBENCH_ROM="'+str(BASE/'romfs/classes')+'"',SOURCE/'loop_probe.cpp',folder/'puff.o','-o',folder/'loop_probe']
        run(command)
        manifest={str(p.relative_to(folder/'src')):hashlib.sha256(p.read_bytes()).hexdigest() for p in (folder/'src').rglob('*') if p.is_file()}
        (folder/'build.json').write_text(json.dumps({'source':str(source),'isolated_source_hashes':manifest,'binary_sha256':hashlib.sha256((folder/'loop_probe').read_bytes()).hexdigest(),'command':[str(x) for x in command]},indent=2)+'\n')

def measure(repeats,observation,warmup):
    reports=[];cases=[(v,False,r) for r in range(1,repeats+1) for v in ('baseline31','optimized33')]+[('baseline31',True,1),('optimized33',True,1)]
    for variant,sync,repeat in cases:
        name=variant+('-sync' if sync else '-async')+'-'+str(repeat)
        with (ROOT/(name+'.stderr.log')).open('w') as stderr:
            p=run([ROOT/'variants'/variant/'loop_probe',int(sync),variant,observation,warmup],stdout=subprocess.PIPE,stderr=stderr,text=True,timeout=30)
        report=json.loads(p.stdout);report['repeat']=repeat
        assert report['recognized_primary_50ms']==1 and report['recognized_aux_100ms']==0,report
        assert report['certified_owners_start'] and report['verified_stable_owners_start'],report
        reports.append(report);(ROOT/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n')
        (ROOT/'results.json').write_text(json.dumps(reports,indent=2)+'\n')
        print(json.dumps(report),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--baseline',type=Path,required=True);p.add_argument('--skip-build',action='store_true');p.add_argument('--build-only',action='store_true');p.add_argument('--repeats',type=int,default=3);p.add_argument('--observation-ms',type=int,default=3000);p.add_argument('--warmup-ms',type=int,default=3000);a=p.parse_args();BASE=a.baseline.resolve()
    if not a.skip_build:build()
    if not a.build_only:measure(a.repeats,a.observation_ms,a.warmup_ms)
