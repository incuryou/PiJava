#!/usr/bin/env python3
"""P1 F/G and timed D boundaries using actual production VM and host runner."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time
import build


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--out',type=Path)
    args=parser.parse_args()
    build.BUILD.mkdir(exist_ok=True)
    out=args.out or build.BUILD/('p1-closeout-san.json' if args.sanitize else 'p1-closeout-opt.json')
    out.parent.mkdir(parents=True,exist_ok=True)
    rows=[]
    function_map=[]
    for path,names in {
        'src/j2mevm.cpp':('launch_class','lifecycle_destroy','stop_thread','p1_host_work_turn','p1_host_post_present','advance_game_frame','thread_resume','game_wake_delay','p1_m3g_values','p1_begin_m3g_epoch','p1_report_m3g_epoch'),
        'src/p1_diagnostics.inc':('p1_new_launch','p1_thread_end','p1_caught','p1_report'),
        'src/m3g_mesh242.h':('identity',),
        'src/frame_schedule.h':('scheduled_frame',),
        'java/javax/microedition/m3g/Transform.java':('setIdentity',)
    }.items():
        lines=(build.ROOT/path).read_text().splitlines()
        for name in names:
            matches=[i+1 for i,line in enumerate(lines) if re.search(r'(?:static|inline|public).*\b'+name+r'\(',line) and ('{' in line or (i+1<len(lines) and lines[i+1].strip()=='{'))]
            assert matches,(path,name)
            function_map.append(dict(file=path,function=name,lines=matches))
    switch=(build.ROOT/'src/platform/switch_host.inc').read_text()
    assert 'pace_now - g_last_present_us >= 50000' in switch
    assert 'g_slice_ms=14;' in (build.ROOT/'src/j2mevm.cpp').read_text()
    scopes={
        'production_function_map':function_map,
        'production_functions':[
            'src/j2mevm.cpp: launch_class, lifecycle_destroy, stop_thread, p1_host_work_turn, p1_host_post_present, advance_game_frame, thread_resume, game_wake_delay, p1_m3g_values, p1_begin_m3g_epoch, p1_report_m3g_epoch',
            'src/frame_schedule.h: scheduled_frame (FrameWaitBudget + injected clock overload)',
            'src/p1_diagnostics.inc: p1_new_launch, p1_thread_end, p1_caught, p1_report',
            'java/javax/microedition/m3g/Transform.java: setIdentity -> M3GMesh242.identity',
            'src/m3g_mesh242.h: identity (actual native stats.identities increment)',
            'src/platform/switch_host.inc: 50ms heartbeat and 14ms slice constants used as explicit bound inputs; platform file itself is not executed by desktop test'],
        'covered':['F six endings in each B/H combination','G production same/different synthetic MIDlet transitions, canceled old pending work, scheduler/worker callbacks, actual M3G identity launch deltas','D actual short serial callback dispatched before heartbeat, controlled host availability plus measured VM/I/O bound, empty sleeps and heartbeat scheduled_frame'],
        'limitations':['Injected host acquire/wait/sleep and heartbeat clock; desktop monotonic elapsed measures short callback + diagnostics I/O, not console latency.',
            '50ms heartbeat + 14ms allowed host scheduling gap + 20ms fixed clock/desktop I/O tolerance; the controlled host gap is an assumption verified by fixture clock, not a production upper bound on arbitrary host I/O.',
            'No preemption guarantee for arbitrary long paint/callback; no physical HID, vsync, real audio sink, commercial game transitions or device passes.',
            'M3G proof exercises the production Transform.setIdentity native path; it does not claim full scene/render M3G coverage.'],
        'production_changes':False,'device_passes':False}
    def write(passed=False,checks=None):
        out.write_text(json.dumps(dict(version=build.VERSION,sanitize=args.sanitize,passed=passed,
            matrix=[dict(boundary=b,handoff=h) for b in (0,1) for h in (0,1)],
            bounds=dict(heartbeat_us=50000,scheduling_budget_us=14000,clock_host_io_tolerance_us=20000,limit_us=84000),
            **scopes,results=rows,asserted_checks=checks or []),indent=2)+'\n')
    def run(name,cmd,timeout=300):
        began=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=build.ROOT,text=True,capture_output=True,
            timeout=timeout,env=dict(os.environ,TERM='dumb',ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
        rows.append(dict(name=name,command=list(map(str,cmd)),returncode=p.returncode,elapsed_s=round(time.monotonic()-began,3),stdout=p.stdout,stderr=p.stderr))
        write();print(name,p.returncode,flush=True);print(p.stdout,end='',flush=True)
        if p.returncode:print(p.stderr,flush=True);raise SystemExit(1)
        return p
    cls=build.BUILD/'test-classes';cls.mkdir(exist_ok=True)
    run('java',build.java_compiler(None)+['-source','8','-target','8','-bootclasspath',build.ROOT/'romfs/classes',
        '-classpath',build.ROOT/'romfs/classes','-d',cls,build.ROOT/'tests/java/P1CloseoutChecks.java',build.ROOT/'tests/java/P1ThreadEndingChecks.java'])
    puff=build.BUILD/'p1-closeout-puff.o';run('puff',['gcc','-O2','-c',build.ROOT/'src/puff.c','-o',puff])
    binary=build.BUILD/('p1-closeout-san' if args.sanitize else 'p1-closeout-opt')
    flags=['-O1','-g1','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
    run('compile',['g++']+flags+['-std=c++17','-fno-exceptions',build.ROOT/'tests/p1_closeout_checks.cpp',puff,'-ldl','-o',binary],600)
    p=run('checks',[binary],60)
    checks=[]
    for b in (0,1):
        for h in (0,1):
            for ending in ('normal','caught_return','uncaught','no_target','reset','destroy'):
                assert re.search(rf'^PASS F B={b} H={h} ending={ending} ',p.stdout,re.M),(b,h,ending)
                checks.append(dict(scenario='F',boundary=b,handoff=h,ending=ending,passed=True))
            for transition in ('same','different'):
                assert re.search(rf'^PASS G B={b} H={h} transition={transition} ',p.stdout,re.M)
                checks.append(dict(scenario='G',boundary=b,handoff=h,transition=transition,passed=True))
            match=re.search(rf'^PASS D_bound B={b} H={h} (.+)$',p.stdout,re.M);assert match
            fields=dict(re.findall(r'(\w+)=(\S+)',match[1]))
            measured={k:int(fields[k]) for k in ('immediate_dispatch_us','measured_vm_io_us','controlled_plus_measured_dispatch_us','empty_sleep_us')}
            assert measured['controlled_plus_measured_dispatch_us']<=84000 and measured['empty_sleep_us']>0
            checks.append(dict(scenario='D_bound',boundary=b,handoff=h,passed=True,measurements=measured))
    for stage,b,h,session,launch in re.findall(r'^EPOCH stage=(\w+) B=(\d) H=(\d) session=(\d+) launch=(\d+)$',p.stdout,re.M):
        expected=3 if stage=='previous_nonzero' else 0 if stage.endswith('_zero') else 2
        # Assert emitted production report, in addition to native in-process checks.
        assert re.search(rf'P1M3G scope=launch_delta launch={launch} identity={expected} skin=0 quantize=0 light=0 gathered=0 emitted=0 texture=0 span=0 row=0(?:\n|$)',p.stderr),(stage,launch,expected)
        checks.append(dict(scenario='G_emitted_epoch',stage=stage,boundary=int(b),handoff=int(h),session=int(session),launch=int(launch),identity_delta=expected,passed=True))
    assert sum(c['scenario']=='G_emitted_epoch' for c in checks)==20
    write(True,checks);print('Results:',out)


if __name__=='__main__':main()
