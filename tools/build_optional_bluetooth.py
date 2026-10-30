#!/usr/bin/env python3
"""Compile only owned Bluetooth sources to Java 8, then replace only their classes."""
import os,shutil,tempfile
from pathlib import Path
import build
out=build.ROOT/'build-optional-bluetooth';out.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='api-',dir=out) as temp:
 temp=Path(temp);empty=temp/'empty';empty.mkdir();classes=temp/'classes';classes.mkdir()
 sources=sorted((build.ROOT/'java/javax/bluetooth').glob('*.java'))
 build.run('Compile owned Bluetooth API',build.java_compiler(None)+['-source','8','-target','8','-encoding','UTF-8','-bootclasspath',build.ROOT/'romfs/classes','-classpath',empty,'-sourcepath',empty,'-d',classes]+sources)
 dest=build.ROOT/'romfs/classes/javax/bluetooth';dest.mkdir(parents=True,exist_ok=True)
 for source in sorted((classes/'javax/bluetooth').glob('*.class')):
  staged=dest/(source.name+'.tmp');shutil.copyfile(source,staged);os.replace(staged,dest/source.name)
