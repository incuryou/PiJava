#!/usr/bin/env python3
"""Bounded city diagnostics: synthetic production-VM paths, never city JAR replay."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time
import build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    flavor = 'san' if args.sanitize else 'opt'
    out = (args.out or build.BUILD / ('p1-city-diagnostics-' + flavor + '.json')).resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    build.BUILD.mkdir(exist_ok=True)
    classes = build.BUILD / 'test-classes' / ('city-diagnostics-' + flavor)
    classes.mkdir(parents=True, exist_ok=True)
    binary = build.BUILD / ('p1-city-diagnostics-' + flavor)
    rows, checks = [], []
    scope = dict(synthetic_fixture=True, exact_target_identity_for_gating_only=True,
                 commercial_reproduction=False, real_user_RMS_access=False, device_passes=False,
                 budgets='launch cumulative; key64, rms128, rms phases64/op, wait64, host512 plus8/trace, method128, sync16, wait progress16, snapshot1',
                 timing='one real ~1.1s synchronized handler through production run_impl budget; limiter caps use controlled diagnostic time fields',
                 termination='SIGKILL for infinite synthetic key handler; linker --wrap=stat _exit(73) before real synthetic RMS stat only',
                 persistence='ofstream flush; no fsync/power-loss/SD-hard-block guarantee',
                 production_changes=False)

    def write(passed=False):
        out.write_text(json.dumps(dict(version=build.VERSION, sanitize=args.sanitize,
                                      passed=passed, scope=scope, results=rows, asserted_checks=checks), indent=2) + '\n')

    def run(name, cmd, timeout=300, expected=0, env=None):
        cmd = list(map(str, cmd))
        start = time.monotonic()
        p = subprocess.run(cmd, cwd=build.ROOT, capture_output=True, text=True, timeout=timeout,
                           env=env or dict(os.environ, TERM='dumb', ASAN_OPTIONS='detect_leaks=0',
                                          UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
        rows.append(dict(name=name, command=cmd, returncode=p.returncode, expected_returncode=expected,
                         elapsed_s=round(time.monotonic()-start, 3), stdout=p.stdout, stderr=p.stderr))
        write(); print(name, p.returncode, flush=True)
        if p.returncode != expected:
            print(p.stdout); print(p.stderr); raise SystemExit(1)
        return p

    run('java', build.java_compiler(None) + ['-source', '8', '-target', '8', '-bootclasspath',
        build.ROOT/'romfs/classes', '-classpath', build.ROOT/'romfs/classes', '-d', classes,
        build.ROOT/'tests/java/P1CityDiagnosticChecks.java'])
    puff = build.BUILD / ('p1-city-diagnostics-' + flavor + '-puff.o')
    run('puff', ['gcc', '-O2', '-c', build.ROOT/'src/puff.c', '-o', puff])
    flags = ['-O1', '-g1', '-fno-pie', '-no-pie', '-fsanitize=address,undefined,float-cast-overflow',
             '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
    run('compile', ['g++'] + flags + ['-std=c++17', '-fno-exceptions', build.ROOT/'tests/p1_city_diagnostic_checks.cpp',
        puff, '-ldl', '-Wl,--wrap=stat', '-o', binary], 600)
    env = dict(os.environ, TERM='dumb', ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    with tempfile.TemporaryDirectory(prefix='p1-city-diag-rms-') as rms:
        env['J2ME_RMS_DIR'] = rms
        def command(mode, log):
            return list(map(str, [binary, mode, log, classes, build.ROOT/'romfs/classes']))
        tail = out.with_suffix('.tail.log')
        run('original-buffered-tail', command('tail', tail), expected=71, env=env)
        text = tail.read_text()
        assert 'FIXTURE durable_baseline' in text and 'buffered_tail_marker' not in text
        checks.append('A original production queued P1EVENT lost at _exit while flushed baseline survives')
        crash = out.with_suffix('.keycrash.log')
        cmd = command('keycrash', crash)
        start = time.monotonic()
        # File polling observes the production flush before any host return.
        # stdout/stderr use temporary files so logging cannot fill a pipe.
        with tempfile.TemporaryFile(mode='w+') as stdout, tempfile.TemporaryFile(mode='w+') as stderr:
            p = subprocess.Popen(cmd, cwd=build.ROOT, stdout=stdout, stderr=stderr, env=env)
            seen = False
            try:
                until = time.monotonic()+15
                while time.monotonic()<until and p.poll() is None:
                    text = crash.read_text() if crash.exists() else ''
                    if re.search(r'KEYCALL .*phase=begin code=99 ', text) and 'CITYCAUGHT ' in text:
                        seen = True; break
                    time.sleep(.01)
                assert seen, 'KEYCALL and owned snapshot must be readable before killing running handler'
            finally:
                if p.poll() is None: p.kill()
                p.wait(timeout=5)
            stdout.seek(0); stderr.seek(0)
            rows.append(dict(name='saturated-key-abnormal-exit', command=cmd, returncode=p.returncode,
                expected_returncode=-signal.SIGKILL, elapsed_s=round(time.monotonic()-start,3),
                stdout=stdout.read(), stderr=stderr.read(), observed_before_kill=seen))
        assert p.returncode == -signal.SIGKILL
        text=crash.read_text()
        assert 'fixture saturation index=511' in text
        assert re.search(r'KEYCALL .*phase=begin code=99 ',text)
        assert not re.search(r'KEYCALL .*phase=end',text)
        assert 'original_caught_dumps_unchanged=0' in text
        checks.append('A actual dispatch_key begin and independent owned snapshot survive SIGKILL after generic event cap512')
        rmslog=out.with_suffix('.rmscrash.log')
        run('native-RMS-stage-interruption', command('rmscrash', rmslog), expected=73, env=env)
        lines=rmslog.read_text().splitlines()
        assert re.search(r'RMSIO .*phase=begin scope=stat ',lines[-1])
        assert 'operation_id=' in lines[-1] and 'actual_bytes=unknown' in lines[-1]
        assert not list(Path(rms).rglob('*.bin'))
        checks.append('C actual RecordStore native traced stat preflush survives test-only _exit with no synthetic store written')
        logfile=out.with_suffix('.checks.log')
        result=run('normal-caught-sync-caps-relaunch',command('checks',logfile),timeout=60,env=env)
        assert 'PASS actual_sync_handler' in result.stdout and 'PASS caps ' in result.stdout
        lines=logfile.read_text().splitlines()
        records=[]
        for line in lines:
            match=re.search(r'\] (KEYCALL|HOSTPHASE|WAIT|RMSIO|SYNCSTEP|CITYMETHOD) (.*)',line)
            if match: records.append((match[1],dict(re.findall(r'([^ ]+)=(\S+)',match[2]))))
        pending={}
        counts={}
        for tag, fields in records:
            counts[tag]=counts.get(tag,0)+1
            if tag not in ('KEYCALL','HOSTPHASE','WAIT','RMSIO'): continue
            phase=fields.get('phase')
            if phase not in ('begin','end'): continue
            key=(tag,fields.get('launch'),fields.get('trace_id'),fields.get('scope'),fields.get('operation_id'),fields.get('phase_id'))
            pending[key]=pending.get(key,0)+(1 if phase=='begin' else -1)
            assert pending[key]>=0,(tag,fields)
        assert all(n==0 for n in pending.values()),pending
        actual=[f for tag,f in records if tag=='SYNCSTEP' and f['launch']=='1']
        assert 1<=len(actual)<=2
        assert int(actual[0]['elapsed_us'])>=250000 and int(actual[0]['ops_since_key'])>1000
        assert actual[0]['origin']=='key_active' and int(actual[0]['run_depth'])>=1
        if len(actual)>1: assert int(actual[1]['mono_us'])-int(actual[0]['mono_us'])>=1000000
        assert not any(word in logfile.read_text() for word in ('TRACE_OPCODE','TRACE_OP ','OPCODE '))
        checks.extend(['B actual synchronized handler uses run_impl budget, at least1 SYNCSTEP, >=250ms, <=1/s, no opcode spam',
                       'B normal/caught/release endpoints paired, owned pre-key snapshot cap1 leaves original caught dump count unchanged',
                       'D key/host/wait/RMS phase/operation endpoints remain paired at caps; independent SYNCSTEP and WAIT caps16',
                       'D same-suite launch resets budgets and trace; non-target, marker-off and nonliteral marker disabled; unknown fields explicit'])
        scope['emitted_record_counts']=counts
        print(result.stdout, end='')
    write(True);print('Results:',out)


if __name__=='__main__': main()
