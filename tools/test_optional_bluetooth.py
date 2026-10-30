#!/usr/bin/env python3
"""Scoped Bluetooth Java consumer, executed only by the actual PiJava VM."""
import argparse,json,subprocess,tempfile
from pathlib import Path
import build
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--library',type=Path,default=build.ROOT/'romfs/classes')
p.add_argument('--results',type=Path)
p.add_argument('--skip-native-build',action='store_true')
a=p.parse_args()
out=build.ROOT/'build-optional-bluetooth';out.mkdir(exist_ok=True)
classes=out/'fixture';classes.mkdir(exist_ok=True)
# Compile against an independent descriptor contract. Contract bodies never enter
# the consumer classpath at run time, making absent baseline classes observable.
contracts={
'DataElement':'''public class DataElement {
public static final int NULL=0,U_INT_1=8,U_INT_2=9,U_INT_4=10,U_INT_8=11,U_INT_16=12,INT_1=16,INT_2=17,INT_4=18,INT_8=19,INT_16=20,UUID=24,STRING=32,BOOL=40,DATSEQ=48,DATALT=56,URL=64;
public DataElement(int t){} public DataElement(boolean b){} public DataElement(int t,long v){} public DataElement(int t,Object v){}
public int getDataType(){return 0;} public long getLong(){return 0;} public boolean getBoolean(){return false;} public Object getValue(){return null;} public int getSize(){return 0;}
public void addElement(DataElement e){} public void insertElementAt(DataElement e,int i){} public boolean removeElement(DataElement e){return false;}
}''',
'LocalDevice':'''public class LocalDevice {
public static LocalDevice getLocalDevice() throws BluetoothStateException{return null;} public static boolean isPowerOn(){return false;} public static String getProperty(String s){return null;}
public boolean setDiscoverable(int m) throws BluetoothStateException{return false;} public int getDiscoverable(){return 0;}
public ServiceRecord getRecord(javax.microedition.io.Connection c){return null;} public void updateRecord(ServiceRecord r) throws ServiceRegistrationException{}
}''',
'ServiceRegistrationException':'public class ServiceRegistrationException extends java.io.IOException {public ServiceRegistrationException(){} public ServiceRegistrationException(String m){super(m);}}'
}
with tempfile.TemporaryDirectory(prefix='contract-',dir=out) as tmp:
 tmp=Path(tmp); src=tmp/'javax/bluetooth';src.mkdir(parents=True)
 paths=[]
 for name,body in contracts.items():
  path=src/(name+'.java');path.write_text('package javax.bluetooth;\n'+body);paths.append(path)
 build.run('Compile independent Bluetooth consumer descriptors',build.java_compiler(None)+['--release','8','-cp',a.library,'-d',tmp]+paths)
 build.run('Compile Bluetooth Java consumer',build.java_compiler(None)+['--release','8','-cp',str(tmp)+':'+str(a.library),'-d',classes,build.ROOT/'tests/java/OptionalBluetoothChecks.java'])
runner=out/'optional_bluetooth_checks';puff=out/'puff.o'
if not a.skip_native_build:
 build.run('Compile Bluetooth decompressor',['gcc','-O1','-c',build.ROOT/'src/puff.c','-o',puff])
 build.run('Compile scoped Bluetooth actual VM runner',['g++','-O0','-std=c++17','-fno-exceptions','-ffunction-sections','-fdata-sections',build.ROOT/'tests/optional_bluetooth_checks.cpp',puff,'-Wl,--gc-sections','-o',runner])
results=[]
for case in ['numeric','objects','sequence','declarations','unavailable','fallback']:
 cmd=[str(runner),str(classes),str(a.library.resolve()),case]
 proc=subprocess.run(cmd,cwd=build.ROOT,capture_output=True,text=True,timeout=30)
 result=dict(case=case,command=cmd,exit=proc.returncode,stdout=proc.stdout,stderr=proc.stderr);results.append(result)
 print(json.dumps(result),flush=True)
target=a.results or out/'results.json';target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(any(r['exit']!=0 for r in results))
