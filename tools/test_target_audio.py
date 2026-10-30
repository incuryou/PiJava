#!/usr/bin/env python3
"""Decode target media by magic and assert nonzero PCM; never bundle game assets."""
import argparse, hashlib, json, subprocess, tempfile, zipfile
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--jar-dir',type=Path,required=True);p.add_argument('--output',type=Path,default=build.BUILD/'target-audio-results.json');a=p.parse_args()
runner=build.BUILD/'AudioEngineChecks'
if not runner.is_file():build.run('Compiling audio check',[build.executable('g++'),'-O2','-std=c++17',build.ROOT/'tests/audio_engine_checks.cpp','-o',runner])
records=[]
with tempfile.TemporaryDirectory(prefix='j2me-audio-') as temp:
 files=[]
 for jar in sorted(a.jar_dir.glob('*.jar')):
  row=dict(jar=jar.name,sha256=hashlib.sha256(jar.read_bytes()).hexdigest(),midi_suffix_count=0,media=[])
  with zipfile.ZipFile(jar) as z:
   for n in z.namelist():
    if n.lower().endswith('.mid'):row['midi_suffix_count']+=1
    if n.endswith('/'):continue
    b=z.read(n)
    fmt='MIDI' if b.startswith(b'MThd') else 'WAV' if b.startswith(b'RIFF') and b[8:12]==b'WAVE' else None
    if fmt:
     row['media'].append(dict(resource=n,format=fmt,bytes=len(b),sha256=hashlib.sha256(b).hexdigest()))
     f=Path(temp)/str(len(files));f.write_bytes(b);files.append(str(f))
  records.append(row)
 proc=subprocess.run([str(runner),*files],cwd=build.ROOT,capture_output=True,text=True,timeout=120)
 report=dict(version=build.VERSION,passed=proc.returncode==0,scope='Magic-identified target MIDI/WAV; first 3 seconds produce nonzero PCM; no hardware listening claim',resources=len(files),results=records,stdout=proc.stdout,stderr=proc.stderr)
 a.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
 print(proc.stdout or proc.stderr);raise SystemExit(proc.returncode)
