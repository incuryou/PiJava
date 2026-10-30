#!/usr/bin/env python3
"""Regression for a parked key continuation owning the repaint entry monitor."""
import argparse,json,os,subprocess,time
from pathlib import Path
import build

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sanitize',action='store_true')
    ap.add_argument('--out',type=Path,required=True)
    ap.add_argument('--host-regression',action='store_true')
    ap.add_argument('--reuse-binary',action='store_true',help='rerun an already built native harness; Java fixture is always recompiled')
    ap.add_argument('--vm-source',type=Path,help='include a separate unmodified production VM for baseline comparison')
    ap.add_argument('--expect-block',action='store_true',help='baseline only: accept a process timeout inside the first key callback')
    args=ap.parse_args();root=build.ROOT;build.BUILD.mkdir(exist_ok=True)
    out=args.out.resolve();out.parent.mkdir(parents=True,exist_ok=True);rows=[]
    def run(name,cmd,timeout=600):
        began=time.monotonic();command=list(map(str,cmd))
        try:
            p=subprocess.run(command,cwd=root,text=True,capture_output=True,timeout=timeout,
                             env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
            row=dict(name=name,command=command,returncode=p.returncode,stdout=p.stdout,stderr=p.stderr)
        except subprocess.TimeoutExpired as e:
            def text(v):return v.decode(errors='replace') if isinstance(v,bytes) else v or ''
            row=dict(name=name,command=command,returncode=None,timed_out=True,stdout=text(e.stdout),stderr=text(e.stderr))
        row['elapsed_s']=time.monotonic()-began;rows.append(row)
        row['accepted']=row['returncode']==0 or (name=='checks' and args.expect_block and row.get('timed_out',False))
        if name=='checks' and args.expect_block:row['accepted']=row.get('timed_out',False)
        out.write_text(json.dumps(dict(version=build.VERSION,sanitize=args.sanitize,
            passed=name==('host-regression' if args.host_regression else 'checks') and all(r['accepted'] for r in rows),
            reused_native_binary=args.reuse_binary,vm_source=str(args.vm_source.resolve()) if args.vm_source else str(root/'src/j2mevm.cpp'),expected_block=args.expect_block,
            scope='actual interpreter, Java sleep, monitor and synchronous serviceRepaints; desktop clocks; no forced Java returns',
            matrix=[dict(boundary=b,handoff=h) for b in (0,1) for h in (0,1)],
            paint_modes=['synchronized_method','entry_synchronized_this','no_canvas_lock'],rounds=2,results=rows),indent=2)+'\n')
        print(name,row['returncode'],flush=True);print(row['stdout'],flush=True)
        if not row['accepted']:print(row['stderr'],flush=True);raise SystemExit(1)
    classes=build.BUILD/'test-classes';classes.mkdir(exist_ok=True)
    run('java',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',root/'romfs/classes',
        '-classpath',root/'romfs/classes','-d',classes,root/'tests/java/CityMonitorContinuationChecks.java'])
    binary=build.BUILD/('city-monitor-'+('baseline-' if args.vm_source else '')+('san' if args.sanitize else 'opt'))
    flags=['-O1','-g1','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow',
        '-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
    if args.reuse_binary:
        if not binary.is_file():raise SystemExit('requested native binary does not exist: '+str(binary))
    else:
        puff=build.BUILD/'city-monitor-puff.o';run('puff',['gcc','-O2','-c',root/'src/puff.c','-o',puff])
        if args.vm_source:flags+=['-DCITY_MONITOR_VM_SOURCE="'+str(args.vm_source.resolve())+'"']
        run('compile',['g++']+flags+['-std=c++17','-fno-exceptions',root/'tests/city_monitor_continuation_checks.cpp',puff,'-ldl','-o',binary])
    run('checks',[binary],5 if args.expect_block else 45)
    if args.host_regression:
        command=['python',root/'tools/test_p1_host.py','--out',out.with_name(out.stem+'-p1-host.json')]
        if args.sanitize:command.append('--sanitize')
        run('host-regression',command,900)
if __name__=='__main__':main()
