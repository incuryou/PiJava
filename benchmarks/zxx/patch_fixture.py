#!/usr/bin/env python3
"""Patch only primary iflt offset so wait edge visits footer goto, as real ZXX does.
No method grows and exception/StackMapTable offsets remain unchanged. This fixture
runs on the custom VM, not the Java verifier (modified target has matching locals).
"""
import json, struct, sys
from pathlib import Path

def patch(path):
    data=bytearray(Path(path).read_bytes()); pos=8
    def u1():
        nonlocal pos
        v=data[pos];pos+=1;return v
    def u2():
        nonlocal pos
        v=struct.unpack_from('>H',data,pos)[0];pos+=2;return v
    def u4():
        nonlocal pos
        v=struct.unpack_from('>I',data,pos)[0];pos+=4;return v
    cp=[None]*u2();i=1
    while i<len(cp):
        tag=u1()
        if tag==1:
            n=u2();cp[i]=(tag,bytes(data[pos:pos+n]).decode());pos+=n
        elif tag in (3,4):pos+=4
        elif tag in (5,6):
            cp[i]=(tag,struct.unpack_from('>q',data,pos)[0]);pos+=8;i+=1
        elif tag in (7,8,16,19,20):cp[i]=(tag,u2())
        elif tag in (9,10,11,12,18):cp[i]=(tag,u2(),u2())
        elif tag==15:pos+=3
        else:raise ValueError('unsupported cp '+str(tag))
        i+=1
    pos+=6;ninterfaces=u2();pos+=2*ninterfaces
    def members(find=False):
        nonlocal pos
        result=None
        for _ in range(u2()):
            flags,name,desc=u2(),u2(),u2()
            for _ in range(u2()):
                attr,size=u2(),u4();end=pos+size
                if find and cp[name][1]=='run' and cp[desc][1]=='()V' and cp[attr][1]=='Code':
                    pos+=4;n=u4();result=(pos,n)
                pos=end
        return result
    members();start,size=members(True)
    code=data[start:start+size]
    def utf(index):return cp[index][1]
    def member(index):
        _,cls,nt=cp[index];_,name,desc=cp[nt];return (utf(cp[cls][1]),utf(name),utf(desc))
    # Decode this deliberately small fixture; rejecting unsupported opcodes prevents
    # accidentally interpreting constant-pool bytes as branch opcodes.
    lengths={op:1 for op in range(256)}
    for op in (0x10,0x12,0x15,0x16,0x17,0x18,0x19,0x36,0x37,0x38,0x39,0x3a):lengths[op]=2
    for op in (0x11,0x13,0x14,0x84,*range(0x99,0xaa),*range(0xb2,0xb9),0xbb,0xbd,0xc0,0xc1,0xc6,0xc7):lengths[op]=3
    for op in (0xb9,0xba,0xc8,0xc9):lengths[op]=5
    instr=[];p=0
    while p<len(code):
        op=code[p]
        if op in (0xaa,0xab,0xc4):raise ValueError('unexpected variable instruction')
        instr.append((p,op));p+=lengths[op]
    yield_sites=[p for p,op in instr if op==0xb8 and member(struct.unpack_from('>H',code,p+1)[0])==('java/lang/Thread','yield','()V')]
    assert len(yield_sites)==1,yield_sites
    head=yield_sites[0]
    primary=[]
    for p,op in instr:
        if op==0x14 and cp[struct.unpack_from('>H',code,p+1)[0]]==(5,50):
            assert code[p+3]==0x94 and code[p+4]==0x9b
            primary.append(p+4)
    assert len(primary)==1,primary
    footers=[p for p,op in instr if op==0xa7 and p+struct.unpack_from('>h',code,p+1)[0]==head]
    assert footers, 'explicit backward goto missing'
    footer=footers[-1];branch=primary[0]
    old=branch+struct.unpack_from('>h',code,branch+1)[0]
    assert old in (head,footer),(head,footer,old)
    struct.pack_into('>h',data,start+branch+1,footer-branch)
    Path(path).write_bytes(data)
    return dict(loop_head_yield_pc=head,primary_iflt_pc=branch,original_wait_target_pc=old,patched_wait_target_pc=footer,footer_goto_target_pc=head,changes='two branch-offset bytes only' if old!=footer else 'already correct')
if __name__=='__main__':print(json.dumps(patch(sys.argv[1]),indent=2))
