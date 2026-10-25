import subprocess,sys,json,time,os
from pathlib import Path
r=Path(__file__).resolve().parents[1];e=r/'build'/'stage238-regression';e.mkdir(parents=True,exist_ok=True);san='--sanitize' in sys.argv;tag='san' if san else 'opt';rows=[]
def call(name,cmd,timeout=240):
 t=time.monotonic();p=subprocess.run(list(map(str,cmd)),cwd=r,capture_output=True,text=True,timeout=timeout,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
 row=dict(name=name,command=list(map(str,cmd)),returncode=p.returncode,seconds=round(time.monotonic()-t,3),stdout=p.stdout,stderr=p.stderr);rows.append(row);(e/('checks-'+tag+'.json')).write_text(json.dumps(rows,indent=2));print(name,p.returncode,p.stdout[-250:]+p.stderr[-250:],flush=True)
 return p.returncode==0
flags=['-O1','-g1','-fno-pie','-no-pie','-fsanitize=address,undefined,float-cast-overflow','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if san else ['-O2']
if san:
 if not call('build-texture-san',['g++']+flags+['-std=c++17','-fno-exceptions','tests/m3g_texture238_checks.cpp','build/desktop/puff.o','-ldl','-o','build/m3g-texture238-san']):sys.exit(1)
 call('texture-san',['build/m3g-texture238-san','12000'],60)
else:
 if not call('java-tests',['javac','-source','8','-target','8','-encoding','UTF-8','-bootclasspath','romfs/classes','-classpath','romfs/classes','-d','build/test-classes']+sorted((r/'tests/java').glob('*.java'))):sys.exit(1)
 if not call('build-runtime',['g++']+flags+['-std=c++17','-fno-exceptions','tests/runtime_checks.cpp','build/desktop/puff.o','-ldl','-o','build/runtime_checks']):sys.exit(1)
 tests=['M3GChecks','NativeChecks','VmChecks','TimeRandomChecks','IntegerChecks','LongChecks','IoChecks','AbiMediaChecks','SchedulerChecks','ArrayChecks','ArrayNativeChecks','ThreadChecks','ThreadCallbackChecks','ThreadGcChecks','ClassInitChecks','ThrowableChecks','ThrowableGcChecks','VolumeControlChecks','ZipNativeChecks','RmsReadChecks','PngHeaderChecks','YieldFrameChecks','PaintSchedulingChecks','MathEncodingChecks','ConnectionAbiChecks','RmsLifecycleChecks','NativeMediaChecks','UiChecks']
 for name in tests:call(name,['build/runtime_checks',name],35)
 if not call('build-texture',['g++']+flags+['-std=c++17','-fno-exceptions','tests/m3g_texture238_checks.cpp','build/desktop/puff.o','-ldl','-o','build/m3g-texture238-checks']):sys.exit(1)
 call('texture-differential',['build/m3g-texture238-checks','12000'],60)

raise SystemExit(0 if rows and all(x["returncode"]==0 for x in rows) else 1)
