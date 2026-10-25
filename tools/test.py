#!/usr/bin/env python3
"""Execute regressions inside this interpreter, never on the host Java runtime."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import tempfile
import build

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sanitize', action='store_true', help='AddressSanitizer and undefined-behavior checks')
parser.add_argument('--skip-build', action='store_true', help='use the already rebuilt romfs/classes')
args = parser.parse_args()
build.BUILD.mkdir(exist_ok=True)
compiler = build.java_compiler(None)
if not args.skip_build:
    build.rebuild_java(compiler)
classes = build.BUILD / 'test-classes'
classes.mkdir(exist_ok=True)
sources = sorted((build.ROOT / 'tests' / 'java').glob('*.java'))
build.run('Compiling custom-VM tests', compiler + ['-source', '8', '-target', '8',
    '-bootclasspath', build.ROOT / 'romfs/classes', '-classpath', build.ROOT / 'romfs/classes',
    '-encoding', 'UTF-8', '-d', classes] + sources)
flags = ['-O1', '-g', '-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all'] if args.sanitize else ['-O2']
puff = build.BUILD / ('test-puff-san.o' if args.sanitize else 'test-puff.o')
runner = build.BUILD / ('runtime_checks-san' if args.sanitize else 'runtime_checks')
build.run('Compiling test decompressor', [build.executable('gcc')] + flags + ['-c', build.ROOT / 'src/puff.c', '-o', puff])
build.run('Compiling interpreter test harness', [build.executable('g++')] + flags + ['-std=c++17', '-fno-exceptions',
    '-I'+str(build.ROOT / 'src'), build.ROOT / 'tests/runtime_checks.cpp', puff, '-o', runner])
results = []
for name in ['M3GChecks', 'NativeChecks', 'VmChecks', 'TimeRandomChecks', 'IntegerChecks', 'LongChecks', 'ShortChecks', 'IoChecks', 'AbiMediaChecks', 'SchedulerChecks', 'ArrayChecks', 'ArrayNativeChecks', 'ThreadChecks', 'ThreadCallbackChecks', 'ThreadGcChecks', 'ClassInitChecks', 'ThrowableChecks', 'ThrowableGcChecks', 'VolumeControlChecks', 'ZipNativeChecks', 'RmsReadChecks', 'PngHeaderChecks', 'YieldFrameChecks', 'PaintSchedulingChecks', 'MathEncodingChecks', 'ConnectionAbiChecks', 'RmsLifecycleChecks', 'NativeMediaChecks', 'UiChecks', 'ClassReflectChecks']:
    before = time.monotonic()
    proc = subprocess.run([str(runner), name], cwd=build.ROOT, capture_output=True, text=True, timeout=30)
    item = dict(test=name, passed=proc.returncode == 0, seconds=round(time.monotonic()-before, 4), stdout=proc.stdout, stderr=proc.stderr)
    if name == 'ThrowableChecks':
        item['console_log_verified'] = all(marker in proc.stderr for marker in
            ['JAVA java.lang.Exception: 标准输出中文标记', 'JAVA java.lang.Exception: 默认错误中文标记', 'ThrowableChecks.systemStreams'])
        item['passed'] = item['passed'] and item['console_log_verified']
    results.append(item)
    print(proc.stdout.strip() or proc.stderr.strip(), flush=True)
for name, source, link_puff in [('HostFontChecks', 'host_font_checks.cpp', False),
                                ('FarmStartupChecks', 'farm_startup_checks.cpp', False),
                                ('HostMenuChecks', 'host_menu_checks.cpp', False),
                                ('HostMenuSettingsChecks', 'host_menu_settings_checks.cpp', False),
                                ('HostSkinChecks', 'host_skin_checks.cpp', False),
                                ('RasterChecks', 'raster_checks.cpp', True),
                                ('StarMapBoundsChecks', 'star_map_bounds_checks.cpp', True),
                                ('GraphicsProfileChecks', 'graphics_profile_checks.cpp', False),
                                ('StreamIntrinsicChecks', 'stream_intrinsic_checks.cpp', True),
                                ('PollGateChecks', 'poll_gate_checks.cpp', True),
                                ('FrameScheduleBudgetChecks', 'frame_schedule_budget_checks.cpp', False),
                                ('PaintStatsChecks', 'paint_stats_checks.cpp', False),
                                ('RuntimeProfileChecks', 'runtime_profile_checks.cpp', False),
                                ('PresentationChecks', 'presentation_checks.cpp', False),
                                ('SuiteIdentityChecks', 'suite_identity_checks.cpp', False),
                                ('RasterPrimitivesChecks', 'raster_primitives_checks.cpp', False),
                                ('AudioEngineChecks', 'audio_engine_checks.cpp', False),
                                ('SwitchAudioMock', 'switch_audio_mock.cpp', False),
                                ('SwitchAudioPauseMock', 'switch_audio_pause_mock.cpp', False),
                                ('PlatformHostChecks', 'platform_host_checks.cpp', True),
                                ('MediaEventChecks', 'media_event_checks.cpp', True),
                                ('GraphicsImageChecks', 'graphics_image_checks.cpp', True),
                                ('JpegImageChecks', 'jpeg_image_checks.cpp', True),
                                ('LifecycleChecks', 'lifecycle_checks.cpp', True),
                                ('FrameScheduleChecks', 'frame_schedule_checks.cpp', True),
                                ('PaceChecks', 'pace_checks.cpp', False),
                                ('SpeedPaceChecks', 'speed_pace_checks.cpp', False),
                                ('SpeedIntegrationChecks', 'speed_integration_checks.cpp', True),
                                ('GameCanvasGcChecks', 'gamecanvas_gc_checks.cpp', True),
                                ('InstantStateChecks', 'instant_state_checks.cpp', True),
                                ('Compat217Checks', 'compat217_checks.cpp', True),
                                ('JitRegionStaleGuard', 'jit_stale_guard_checks.cpp', True),
                                ('JitNegativeDepthGlue', 'jit_negative_depth_checks.cpp', True),
                                ('RuntimeStatsBaseline', 'runtime_stats_baseline_checks.cpp', True),
                                ('SpeedTimerPacing', 'speed_timer_pacing_checks.cpp', True),
                                ('JitLastExitReset', 'jit_last_exit_reset_checks.cpp', True),
                                ('JitBadExitSideEffects', 'jit_bad_exit_side_effect_checks.cpp', True),
                                ('JitStaleLogLimit', 'jit_stale_log_limit_checks.cpp', True),
                                ('PresentNewContentCount', 'present_new_content_checks.cpp', True),
                                ('JitLeafReinstallAccounting', 'jit_leaf_accounting_checks.cpp', True),
                                ('JitLeafSelfTestGuard', 'jit_leaf_selftest_guard_checks.cpp', True)]:
    binary = build.BUILD / (name + ('-san' if args.sanitize else ''))
    build.run('Compiling ' + name, [build.executable('g++')] + flags + ['-std=c++17', '-fno-exceptions',
        '-I'+str(build.ROOT / 'tests/mock_switch'), build.ROOT / 'tests' / source] + ([puff] if link_puff else []) + (['-Wl,--wrap=statvfs'] if name == 'Compat217Checks' else []) + ['-o', binary])
    before = time.monotonic()
    with tempfile.TemporaryDirectory(prefix='j2me-check-') as directory:
        arguments = [str(classes)] if name == 'InstantStateChecks' else ([str(classes), str(build.ROOT / 'romfs/classes'), directory] if name == 'Compat217Checks' else [])
        proc = subprocess.run([str(binary)] + arguments, cwd=build.ROOT, capture_output=True, text=True,
                              timeout=90 if name == 'PollGateChecks' else 30)
    results.append(dict(test=name, passed=proc.returncode == 0, seconds=round(time.monotonic()-before, 4),
                        stdout=proc.stdout, stderr=proc.stderr))
    print(proc.stdout.strip() or proc.stderr.strip(), flush=True)
output = build.BUILD / ('test-results-sanitized.json' if args.sanitize else 'test-results.json')
output.write_text(json.dumps(dict(version=build.VERSION, sanitized=args.sanitize, asan_options=os.environ.get("ASAN_OPTIONS", ""), results=results), indent=2)+'\n')
print('Results:', output)
sys.exit(0 if all(item['passed'] for item in results) else 1)
