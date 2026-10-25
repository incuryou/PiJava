// P2.5: ResourceKey / layout+content versions / dirty barriers. One direct case per write path:
// interpreter (iastore/bastore/aastore), fast path (hot iastore loop), System.arraycopy (prim, ref, empty),
// native stream copy (ByteArrayInputStream.read), native writable byte view (String.getBytes memcpy),
// image writes (Graphics.fillRect on a mutable Image), allocation/realloc (layout), slot reuse (generation),
// launch and instant-state restore (epoch). Read-only paths must leave versions unchanged.
// Also: barrier verifier (hash shadow) sees missed=0 across a GC scan after all writes, and detects a deliberate
// barrier bypass (negative control).
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include "../src/host_menu_settings.h"
#include "../src/host_speed.h"
static host_menu::Settings g_menu_settings;
static std::string instant_state_path(const std::string&) {return "build/resver.state";}
#include "../src/instant_state.inc"
static int fails=0;
#define CHECK(c,msg) do{ if(!(c)){ ++fails; std::cout<<"CHECK FAIL "<<msg<<"\n"; } else std::cout<<"CHECK ok "<<msg<<"\n"; }while(0)
static int fx;
static int sref(const char* n,const char* d){auto* f=find_field(fx,n,d);assert(f);return VMS_CL(g_classes)[f->owner].statics[f->slot].ref;}
static Value call(const char* n,const char* d="()V"){auto* m=find_method(fx,n,d,false);assert(m);Value r=run(m,{});assert(!VMS_EXEC(g_thrown));return r;}
static bool bumped(const ResourceStamp& before,int ref){return !resource_stamp_current(before) && resource_key_valid(before.key) && resource_stamp_of(ref).content>before.content;}
int main(){
    std::cout<<std::unitbuf;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_PLAT(g_boot_ms)=now_ms();
    const uint32_t e0=resver::g_epoch;
    launch_class("P1HostChecks");fx=ensure_class("ResVerChecks");call("readOnly"); // triggers <clinit>
    CHECK(resver::g_epoch>e0,"launch moves the epoch");
    const int ints=sref("ints","[I"),bytes=sref("bytes","[B"),refs=sref("refs","[Ljava/lang/Object;"),dst=sref("dst","[I");
    auto st=[&](int r){return resource_stamp_of(r);};
    ResourceStamp a=st(ints); call("iastore"); CHECK(bumped(a,ints),"interpreter iastore bumps content");
    CHECK(st(ints).layout==a.layout,"iastore keeps layout");
    a=st(ints); call("iastoreLoop"); CHECK(bumped(a,ints),"hot iastore loop (fast path) bumps content");
    a=st(bytes); call("bastore"); CHECK(bumped(a,bytes),"bastore bumps content");
    a=st(refs); call("aastore"); CHECK(bumped(a,refs),"aastore bumps content");
    a=st(dst); ResourceStamp src=st(ints); call("arraycopyPrim"); CHECK(bumped(a,dst),"arraycopy primitive bumps destination");
    CHECK(resource_stamp_current(src),"arraycopy leaves source version unchanged");
    a=st(refs); call("arraycopyRef"); CHECK(bumped(a,refs),"arraycopy reference bumps destination");
    a=st(dst); call("arraycopyEmpty"); CHECK(resource_stamp_current(a),"empty arraycopy writes nothing, version unchanged");
    a=st(bytes); call("streamRead"); CHECK(bumped(a,bytes),"native ByteArrayInputStream.read bumps content");
    ResourceStamp ri=st(ints),rb=st(bytes),rd=st(dst); call("readOnly"); call("stringFromBytes");
    CHECK(resource_stamp_current(ri)&&resource_stamp_current(rb)&&resource_stamp_current(rd),"read-only paths (iaload/baload/arraylength/new String(byte[])) keep versions");
    { const uint64_t v0=resver::g_stats.array_view; int gb=call("getBytes","()[B").ref;
      CHECK(gb>0&&resver::g_stats.array_view>v0&&VMS_HEAP(g_heap)[gb].elems.content_version()>VMS_HEAP(g_heap)[gb].elems.layout_version(),"native writable byte view (String.getBytes) bumps after allocation"); }
    call("makeImage"); int img=sref("img","Ljavax/microedition/lcdui/Image;");
    CHECK(img>0&&st(img).layout>0,"Image.createImage stamps image layout");
    a=st(img); call("drawImage"); CHECK(bumped(a,img),"Graphics.fillRect on mutable Image bumps image content");
    a=st(ints); call("reallocInts"); int ints2=sref("ints","[I");
    CHECK(ints2!=ints||st(ints2).layout!=a.layout,"new array has a new layout stamp");
    // slot reuse: collect the old 'ints' and allocate until its slot is reused -> generation differs, key invalid
    { ResourceKey k=a.key; gc_collect("p25-test");
      CHECK(!resource_key_valid(k),"freed array: its key is no longer valid");
      int reused=0; for(int i=0;i<100000&&!reused;++i){int r=alloc_array("[I",4); if(r==k.slot)reused=r;}
      CHECK(reused==k.slot,"slot reused by a new array");
      CHECK(!resource_key_valid(k)&&resource_key_of(reused).generation!=k.generation,"reused slot has a new generation; old key stays invalid"); }
    // barrier verifier: scan, write through every path again, scan: missed must be 0
    res_verify_scan("t0"); call("iastoreLoop"); call("bastore"); call("arraycopyPrim"); call("streamRead"); call("drawImage"); call("readOnly");
    res_verify_scan("t1");
    CHECK(g_res_verify.missed==0&&g_res_verify.bumped_changed>0&&g_res_verify.unchanged>0,"verifier: all writes went through barriers (missed=0 bumped_changed="+std::to_string(g_res_verify.bumped_changed)+" unchanged="+std::to_string(g_res_verify.unchanged)+")");
    // negative control: bypass the barrier deliberately (const_cast on the read view) -> verifier must detect it
    { const int b=sref("bytes","[B"); res_verify_scan("n0"); const uint64_t m0=g_res_verify.missed;
      const_cast<uint8_t*>(VMS_HEAP(g_heap)[b].elems.read_bytes())[0]^=0x5a; res_verify_scan("n1");
      CHECK(g_res_verify.missed==m0+1,"negative control: a write that bypasses the barrier is detected");
      const_cast<uint8_t*>(VMS_HEAP(g_heap)[b].elems.read_bytes())[0]^=0x5a; g_res_shadow.clear(); g_res_verify.missed=m0; g_res_verify.first_missed.clear(); }
    // instant-state restore moves the epoch: keys taken before restore are invalid afterwards
    { ResourceKey k=resource_key_of(sref("dst","[I")); SavState s; bool cap=capture_state(s,"resver-test");
      bool ok=cap&&apply_state(s); CHECK(ok,"capture+apply instant state");
      CHECK(ok&&!resource_key_valid(k),"restore moves epoch; pre-restore key invalid");
      fx=ensure_class("ResVerChecks"); // class ids are rebuilt by restore
      CHECK(ok&&resource_key_valid(resource_key_of(sref("dst","[I"))),"post-restore key valid"); }
    resver_report();
    std::cout<<"RESVER_RESULT "<<(fails?"FAIL":"PASS")<<" fails="<<fails<<" epoch="<<resver::g_epoch<<" clock="<<resver::g_clock<<"\n";
    return fails?1:0;
}
