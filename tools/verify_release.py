#!/usr/bin/env python3
"""Verify NRO asset equality, AArch64 ELF, NACP version and staged source assets."""
import argparse,hashlib,json,struct
from pathlib import Path
import build

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,default=build.BUILD/'release-verification.json');a=p.parse_args()
    r=build.ROOT;objects=build.BUILD/'switch';nro=build.BUILD/('J2me-vm-'+build.VERSION+'.nro')
    build.verify_nro(nro,True,r/'assets/icon.jpg');data=nro.read_bytes();start=struct.unpack_from('<I',data,0x18)[0]
    no,ns,ro,rs=struct.unpack_from('<QQQQ',data,start+0x18)
    nacp=data[start+no:start+no+ns];romfs=data[start+ro:start+ro+rs]
    assert nacp==(objects/'j2mevm.nacp').read_bytes(),'NACP mismatch'
    version=nacp[0x3060:0x3070].split(b'\0')[0].decode();assert version==build.NACP_VERSION,'NACP version mismatch'
    assert romfs==(objects/'romfs.bin').read_bytes(),'ROMFS mismatch'
    elf=(objects/'j2mevm.elf').read_bytes();assert elf[:4]==b'\x7fELF' and struct.unpack_from('<H',elf,18)[0]==183,'not AArch64 ELF'
    staged=objects/'romfs-release';files=[]
    for f in sorted(staged.rglob('*')):
        if f.is_file():
            rel=f.relative_to(staged);original=r/'romfs'/rel
            assert f.read_bytes()==original.read_bytes(),'staged asset mismatch: '+str(rel)
            assert rel.parts[0] in ('classes','font','skins') or rel.as_posix() in ('ui/pro-controller.rgba','shaders/filter.vert.dksh','shaders/filter.frag.dksh'),'unexpected production asset'
            assert f.suffix!='.jar','game/example JAR accidentally embedded'
            files.append({'path':str(rel),'sha256':sha(f),'size':f.stat().st_size})
    sources={str(f.relative_to(r)):sha(f) for f in sorted((r/'src').rglob('*')) if f.is_file()}
    result={'version':build.VERSION,'passed':True,'nro_sha256':sha(nro),'nro_size':len(data),'elf_sha256':sha(objects/'j2mevm.elf'),'elf_machine':'AArch64','nacp_version':version,'romfs_embedded_equal':True,'nacp_embedded_equal':True,'romfs_files':files,'source_sha256':sources,'scope':'static build/artifact integrity only; no physical Switch execution'}
    a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(result,indent=2)+'\n');print('Verified NRO',nro.name,'ROMFS files',len(files),'bytes',len(data))
if __name__=='__main__':main()
