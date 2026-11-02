#!/usr/bin/env python3
"""GPU3D evidence runner. Host success is never Switch hardware success.

Windows host runs use a disposable adapted source copy and frozen ROMFS classes.
Device schema=1: target=switch, suite, baseline_sha, candidate_sha, device={id,
clocks}, cases=[{name,status=passed,raw,raw_sha256}]. Raw files are relative to
results JSON and must stay within its directory. A case must name the suite.
Raw JSON repeats device_id, suite, case, baseline_sha, candidate_sha and status,
and contains samples with positive frames and nonnegative main_cpu_us.
Verification is an integrity/scope check,
not automatic image analysis or a performance acceptance decision.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import time
import wave
import zipfile


def sha256(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''): h.update(block)
    return h.hexdigest()


def source_fingerprint(root):
    h = hashlib.sha256()
    for folder in ('src', 'tests', 'java', 'third_party', 'romfs/classes'):
        for path in sorted((root/folder).rglob('*')):
            if path.is_file() and '__pycache__' not in path.parts:
                h.update(path.relative_to(root).as_posix().encode()+b'\0'+sha256(path).encode()+b'\n')
    return h.hexdigest()


def validate_device_results(data, folder, suite):
    def need(ok, reason):
        if not ok: raise ValueError(reason)
    need(isinstance(data, dict), 'device results must be an object')
    need(suite == 'baseline', 'device suite not implemented: ' + suite)
    need(data.get('schema') == 1 and data.get('target') == 'switch', 'device schema/target required')
    need(data.get('suite') == suite, 'suite does not match requested scope')
    for key in ('baseline_sha', 'candidate_sha'):
        need(isinstance(data.get(key), str) and re.fullmatch('[0-9a-f]{64}', data[key]), key + ' required')
    device = data.get('device', {})
    need(isinstance(device, dict) and all(isinstance(device.get(k), str) and device[k].strip() for k in ('id', 'clocks')), 'device identity and clocks required')
    cases = data.get('cases')
    need(isinstance(cases, list) and len(cases) > 0, 'raw cases required')
    names = []
    for case in cases:
        need(isinstance(case, dict), 'case must be an object')
        name = case.get('name')
        need(isinstance(name, str) and name.strip() and name not in names, 'unique case name required')
        need(case.get('status') == 'passed', 'case failed or not run: ' + name)
        raw_name = case.get('raw')
        need(isinstance(raw_name, str) and raw_name.strip(), 'raw file required')
        raw = (folder / raw_name).resolve()
        need(raw.is_relative_to(folder.resolve()), 'raw file escapes results directory')
        need(raw.is_file() and raw.stat().st_size > 0, 'raw file missing or empty')
        need(case.get('raw_sha256') == sha256(raw), 'raw file SHA mismatch')
        record = json.loads(raw.read_text(encoding='utf-8'))
        need(isinstance(record, dict), 'raw record must be an object')
        need(record.get('device_id') == device['id'] and record.get('suite') == suite and record.get('case') == name, 'raw scene/device identity mismatch')
        need(all(record.get(k) == data[k] for k in ('baseline_sha', 'candidate_sha')), 'raw build identity mismatch')
        need(record.get('status') == 'passed' and isinstance(record.get('samples'), list) and len(record['samples']) > 0, 'raw samples/status required')
        for sample in record['samples']:
            need(isinstance(sample, dict) and type(sample.get('frames')) is int and sample['frames'] > 0 and type(sample.get('main_cpu_us')) in (int, float) and sample['main_cpu_us'] >= 0, 'raw frame/CPU counters required')
        names.append(name)
    need(suite in names, 'required suite scene missing: ' + suite)
    return names


LIGHT = ['p7_worker_checks', 'p7_worker_exit_check', 'frame_schedule_checks',
         'frame_schedule_budget_checks', 'game_favorites_checks', 'favorites_ui_checks',
         'menu_page_memory_checks', 'host_display_menu_checks', 'host_keymap_checks']
HEAVY = ['resource_version_checks', 'array_storage', 'm3g_stage242_checks',
         'm3g_cpu_checks', 'micro3d_state_checks', 'instant_state_checks', 'p7_audio_pcm_check']


def host_suite(name):
    if name == 'baseline': return LIGHT + HEAVY + ['hostvm', 'favorites_controller_checks']
    raise ValueError('host suite not implemented: ' + name)


def pcm_mode_evidence(text, worker):
    line = next((s for s in text.splitlines() if s.startswith('PCMCHECK ')), '')
    fields = dict(re.findall(r'(\w+)=(\d+)', line))
    return (fields.get('audio') == str(int(worker)) and int(fields.get('frames_requested', '0')) > 0
            and fields.get('frames_requested') == fields.get('frames_dumped')
            and (not worker or int(fields.get('frames_worker', '0')) > 0))


def adapt_test_text(name, text):
    if name == 'game_favorites_checks.cpp':
        return text.replace('std::filesystem::rename(file,file+".previous");',
                            'f.close();std::filesystem::remove(file+".previous");std::filesystem::rename(file,file+".previous");').replace('std::ofstream(file)<<before', 'std::ofstream(file,std::ios::binary)<<before')
    if name == 'favorites_controller_checks.py':
        return text.replace('.read_text()', '.read_text(encoding="utf-8")').replace('cpp.write_text(prefix+helper+s[a:b]+tail)', 'cpp.write_text(prefix+helper+s[a:b]+tail,encoding="utf-8")').replace("'g++','-std=c++17'", "'g++','-no-canonical-prefixes','-include',os.environ['GPU3D_WINDOWS_INCLUDE'],'-std=c++17'").replace('from pathlib import Path', 'from pathlib import Path\nimport os')
    return text


class Run:
    def __init__(self, args):
        self.args = args
        self.out = args.out.absolute()
        self.out.mkdir(parents=True, exist_ok=True)
        previous = self.out / 'summary.json'
        self.previous = json.loads(previous.read_text(encoding='utf-8')) if args.resume and previous.exists() else None
        if previous.exists() and not self.previous:
            raise ValueError('output already has summary.json; use a fresh evidence directory')
        self.env = dict(os.environ)
        self.report = dict(schema=1, target=args.target, suite=args.suite, status='failed',
                           scope='host reference/contracts only' if args.target == 'host' else args.target,
                           commands=[], cases=[], python=sys.version, started_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()))
        self.report['runner_sha256'] = sha256(Path(__file__))
        self.report['attempt'] = self.previous.get('attempt', 1)+1 if self.previous else 1
        self.prefix = ('attempt'+str(self.report['attempt'])+'-') if self.previous else ''
        if self.previous:
            if args.target != 'host' or self.previous['target'] != 'host': raise ValueError('resume supports host only')
            self.report['previous_attempt'] = self.previous
        self.save()

    def save(self):
        (self.out / 'summary.json').write_text(json.dumps(self.report, indent=2), encoding='utf-8')

    def previous_case(self, name):
        report = self.previous
        while report:
            for case in report.get('cases', []):
                if case['name'] == name and case.get('binary_sha256'): return case
            report = report.get('previous_attempt')
        return {}

    def command(self, label, cmd, cwd, env=None, timeout=600):
        cmd = list(map(str, cmd)); start = time.monotonic()
        active_env = env or self.env
        record = dict(label=label, command=cmd, cwd=str(cwd), environment={k: active_env.get(k) for k in ('J2ME_RMS_DIR', 'J2ME_P7_WORKER', 'J2ME_P7_AUDIO', 'J2ME_P7_PCM_DUMP')})
        try:
            p = subprocess.run(cmd, cwd=cwd, env=env or self.env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout)
            record['returncode'] = p.returncode; output = p.stdout
        except subprocess.TimeoutExpired as e:
            record['returncode'] = 'timeout'; output = e.stdout or b''
        except OSError as e:
            record['returncode'] = 'launch-failed'; output = str(e).encode()
        log = self.out / (self.prefix + label + '.log'); log.write_bytes(output)
        record.update(elapsed_s=round(time.monotonic()-start, 3), log=log.name, log_sha256=sha256(log))
        self.report['commands'].append(record); self.save()
        print(label, record['returncode'], output.decode(errors='replace')[-300:], flush=True)
        return record['returncode'] == 0

    def host(self):
        names = host_suite(self.args.suite)
        self.report['planned_cases'] = names
        self.report['cases'] = [dict(name=n, status='not-run') for n in names]
        self.save()
        if not self.args.workspace or not self.args.baseline: raise ValueError('host requires --workspace and --baseline')
        work = self.args.workspace.absolute()
        baseline = self.args.baseline.absolute()
        source = (self.args.source or baseline).absolute()
        if self.out.is_relative_to(baseline): raise ValueError('output must be outside immutable baseline')
        cc = work / 'toolchains/mingw64/bin/g++.exe'  # Do not resolve this junction.
        gcc = cc.with_name('gcc.exe'); javac = work / 'toolchains/jdk8u504-b01/bin/javac.exe'
        font = (self.args.font or work/'font500-inspection/font/NotoSansSC-Medium-500.ttf').absolute()
        adapter = work / 'build-env'; host = self.out / 'host-source'
        for dep in (cc, gcc, javac, font, adapter/'prepare_host.py', adapter/'windows_test_names.h', baseline/'SOURCE-MANIFEST.json', source/'src/j2mevm.cpp'):
            if not dep.is_file(): raise ValueError('missing dependency: ' + str(dep))
        self.report['host_menu_test_font'] = dict(path=str(font), sha256=sha256(font), scope='external rasterizer test fixture; not a shipping baseline font')
        self.report['tools_sha256'] = {str(p): sha256(p) for p in (cc, gcc, javac, adapter/'prepare_host.py', adapter/'windows_test_names.h', adapter/'windows_compat.h')}
        # Check every shipped source-manifest entry, both before and after testing.
        manifest = json.loads((baseline/'SOURCE-MANIFEST.json').read_text(encoding='utf-8'))['files']
        def verify_baseline():
            for name, entry in manifest.items():
                path = baseline / name
                if not path.is_file() or sha256(path) != entry['sha256']: raise ValueError('baseline manifest mismatch: ' + name)
        verify_baseline()
        self.report['source_inputs_sha256'] = source_fingerprint(source)
        if self.previous and (source != baseline or self.previous.get('source') != str(source) or self.previous.get('baseline_manifest_sha256') != sha256(baseline/'SOURCE-MANIFEST.json')):
            raise ValueError('resume requires the same immutable baseline')
        self.report.update(source=str(source), baseline=str(baseline), baseline_manifest_sha256=sha256(baseline/'SOURCE-MANIFEST.json'), baseline_files_verified=len(manifest),
                           source_role='baseline' if source == baseline else 'candidate', planned_cases=names)
        self.env['PATH'] = str(cc.parent) + os.pathsep + self.env['PATH']
        self.env['GPU3D_WINDOWS_INCLUDE'] = str(adapter/'windows_test_names.h')
        self.env['J2ME_RMS_DIR'] = str(self.out/'rms'); (self.out/'rms').mkdir(exist_ok=True)
        for name in ('J2ME_P7_WORKER', 'J2ME_P7_AUDIO', 'J2ME_P7_PCM_DUMP'): self.env.pop(name, None)
        for label, cmd in [('compiler-version', [cc, '--version']), ('java-version', [javac, '-version']),
                           ('prepare', [sys.executable, adapter/'prepare_host.py', '--source', source, '--output', host])]:
            if not self.command(label, cmd, source): raise ValueError(label + ' failed')
        adaptations = []
        for name in ('game_favorites_checks.cpp', 'favorites_controller_checks.py'):
            path = host/'tests'/name; before = path.read_text(encoding='utf-8')
            path.write_text(adapt_test_text(name, before), encoding='utf-8')
            adaptations.append(dict(path='tests/'+name, input_sha256=hashlib.sha256(before.encode()).hexdigest(), adapted_sha256=sha256(path)))
        self.report['test_adaptations'] = adaptations
        classes = host/'build/test-classes'; classes.mkdir(parents=True, exist_ok=True)
        api = host/'romfs/classes'
        api_hash = {p.relative_to(api).as_posix(): sha256(p) for p in api.rglob('*.class')}
        fixture_names = ['P1HostChecks', 'FrameScheduleChecks', 'ResVerChecks', 'M3GStage242Checks', 'M3GTexture238Checks', 'M3GSpan239Checks', 'M3GSpan239FrameChecks', 'M3GRow240Checks', 'Micro3DStateChecks', 'Micro3DInternalMathChecks', 'InstantStateChecks']
        argfile = self.out/(self.prefix+'fixtures.args')
        argv = ['-source','8','-target','8','-encoding','UTF-8','-bootclasspath',api,'-classpath',api,'-d',classes] + [source/('tests/java/'+n+'.java') for n in fixture_names]
        argfile.write_text('\n'.join('"'+str(x).replace('\\','/')+'"' for x in argv), encoding='utf-8')
        if not self.command('compile-fixtures', [javac, '-J-Duser.language=en', '@'+str(argfile)], host): raise ValueError('fixture compilation failed')
        puff = self.out/'puff.o'
        if not self.command('compile-puff', [gcc, '-no-canonical-prefixes','-O2','-c',host/'src/puff.c','-o',puff], host): raise ValueError('puff compilation failed')
        flags = ['-no-canonical-prefixes','-O2','-g0','-std=c++17','-fno-exceptions','-include',adapter/'windows_test_names.h','-I'+str(adapter)]
        # Existing CPU fixture expects a renamed row240 main in its private copy.
        row = (host/'tests/m3g_row240_checks.cpp').read_text(encoding='utf-8')
        (host/'tests/runtime_fit31/row240_fixture.inc').write_text(row.replace('"../src/', '"../../src/').replace('int main(', 'int m3g_row_original_main('), encoding='utf-8')
        fixture_wav = self.out/'tone.wav'
        with wave.open(str(fixture_wav), 'wb') as f:
            f.setnchannels(1); f.setsampwidth(2); f.setframerate(22050)
            f.writeframes(b''.join(struct.pack('<h', ((i*97)%8192)-4096) for i in range(22050)))
        jar = self.out/(self.prefix+'audio-fixture.jar')
        with zipfile.ZipFile(jar, 'w') as z: z.write(fixture_wav, 'test-tone.wav')
        self.report['audio_fixture_sha256'] = sha256(jar)
        paths = dict(array_storage='tests/runtime_fit20/array_payload_full_checks.cpp', m3g_cpu_checks='tests/runtime_fit31/m3g_cpu_checks.cpp', micro3d_state_checks='tests/runtime_fit32/micro3d_state_checks.cpp', hostvm='src/j2mevm.cpp')
        for name in names:
            case = next(c for c in self.report['cases'] if c['name'] == name)
            if name == 'favorites_controller_checks':
                # This existing script compiles the real handler; run only the private copy.
                case['status'] = 'passed' if self.command(name,[sys.executable,host/'tests/favorites_controller_checks.py'],host) else 'failed'
                self.save(); continue
            cpp = host/paths.get(name, 'tests/'+name+'.cpp'); exe = self.out/(name+'.exe')
            compile_flags = [('-std=c++20' if f == '-std=c++17' else f) for f in flags] if name == 'p7_audio_pcm_check' else flags
            old = self.previous_case(name)
            reusable = exe.is_file() and old.get('binary_sha256') == sha256(exe) and name != 'game_favorites_checks'
            if reusable:
                self.report['commands'].append(dict(label='reuse-'+name, binary=str(exe), binary_sha256=sha256(exe), reason='same immutable baseline, prior successful compile; tests rerun'))
            elif not self.command('compile-'+name, [cc]+compile_flags+[cpp,puff,'-pthread','-o',exe], host):
                case['status'] = 'failed'; self.save(); continue
            case.update(binary=str(exe), binary_sha256=sha256(exe))
            if name == 'hostvm':
                case['status'] = 'passed'; case['scope'] = 'built reusable VM only; no game executed'
            elif name == 'p7_audio_pcm_check':
                ok = True
                for mode in ('inline','worker'):
                    env = dict(self.env, J2ME_P7_PCM_DUMP=str(self.out/(self.prefix+mode+'.pcm')), J2ME_P7_WORKER='0', J2ME_P7_AUDIO='0')
                    if mode == 'worker': env.update(J2ME_P7_WORKER='1',J2ME_P7_AUDIO='1')
                    ok = self.command(name+'-'+mode,[exe,jar,self.out/(self.prefix+mode+'-audio.log'),'300'],host,env) and ok
                    raw_log = self.out/self.report['commands'][-1]['log']
                    ok = pcm_mode_evidence(raw_log.read_text(encoding='utf-8', errors='replace'), mode == 'worker') and ok
                a,b = self.out/(self.prefix+'inline.pcm'), self.out/(self.prefix+'worker.pcm')
                case['status'] = 'passed' if ok and a.is_file() and b.is_file() and a.stat().st_size > 0 and sha256(a) == sha256(b) else 'failed'
                case['scope'] = 'deterministic generated WAV; actual engine PCM equality; no game audio coverage'
                if a.is_file() and b.is_file(): case.update(inline_sha256=sha256(a),worker_sha256=sha256(b),bytes=a.stat().st_size)
            else:
                args = [exe]
                if name == 'game_favorites_checks':
                    folder = self.out/'favorites-data'; folder.mkdir(exist_ok=True); args += [folder]
                    for p in folder.glob('*'): p.unlink()
                if name == 'host_keymap_checks':
                    folder = self.out/'keymap-data'; folder.mkdir(exist_ok=True)
                    for p in folder.glob('*'): p.unlink()
                    legacy = self.out/'keymap-v1.cfg'; legacy.write_text('J2ME_KEYMAP 1\nglobal custom '+' '.join(['-1 -1']*19)+'\n', encoding='utf-8', newline='\n')
                    args += [folder, font, legacy]
                if name == 'm3g_stage242_checks': args += ['64','current','0']
                if name == 'm3g_cpu_checks': args += ['verify']
                if name == 'instant_state_checks': args += [classes]
                case['status'] = 'passed' if self.command(name,args,host) else 'failed'
            self.save()
        verify_baseline()
        if self.report['source_inputs_sha256'] != source_fingerprint(source): raise ValueError('tested source changed during run')
        if api_hash != {p.relative_to(api).as_posix(): sha256(p) for p in api.rglob('*.class')}: raise ValueError('frozen API bytecode changed')
        self.report.update(api_class_count=len(api_hash), frozen_api_exact=True, baseline_unchanged=True)
        if any(c['status'] != 'passed' for c in self.report['cases']): raise ValueError('one or more host cases failed; see individual logs')

    def execute(self):
        try:
            if self.args.target == 'host': self.host()
            elif self.args.target == 'switch-build': raise ValueError('switch-build not implemented: no candidate compiler/artifact identity pipeline yet; no hardware tests executed')
            else:
                if not self.args.results: raise ValueError('switch-verify requires --results with device identity and raw records')
                result = self.args.results.absolute()
                data = json.loads(result.read_text(encoding='utf-8'))
                names = validate_device_results(data, result.parent, self.args.suite)
                self.report.update(results_sha256=sha256(result), device=data['device'], cases=[dict(name=n,status='passed') for n in names], scope='verified supplied device records; runner did not collect hardware data')
            self.report['status'] = 'passed'
        except (ValueError, OSError, KeyError, TypeError) as e:
            self.report['error'] = str(e); print('FAIL:', e, flush=True)
        self.save()
        return 0 if self.report['status'] == 'passed' else 1


def main():
    if hasattr(sys.stdout, 'reconfigure'): sys.stdout.reconfigure(errors='backslashreplace')
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--target', choices=['host','switch-build','switch-verify'], required=True)
    ap.add_argument('--suite', required=True)
    for name in ('workspace','baseline','source','results','font'): ap.add_argument('--'+name, type=Path)
    ap.add_argument('--resume', action='store_true', help='reuse verified binaries from this output for the same immutable baseline; rerun tests')
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    try: return Run(args).execute()
    except ValueError as e: print('FAIL:',e); return 1

if __name__ == '__main__': sys.exit(main())
