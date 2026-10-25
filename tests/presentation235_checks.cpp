// Deliberately deferred GPU mock: reads from staging happen later, at a reused
// slot's fence. A checksum proves no in-flight source was overwritten early.
#include "../src/switch_gpu_present.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
namespace dk::detail {
struct Device{};
struct MemBlock{std::vector<uint8_t> data;};
struct Queue{unsigned next=0;};
struct Swapchain{std::vector<const DkImage*> images;};
struct Op{const DkImage* source;DkImageRect src;const DkImage* dest;DkImageRect dst;uint64_t hash;};
struct CmdBuf{std::vector<Op> ops;DkFence* fence=nullptr;bool inflight=false;};
}
namespace mock {
struct Layout {uint32_t w,h,pitch,flags;};
struct Image {DkMemBlock mem;uint32_t offset;Layout layout;};
std::map<const DkImage*,Image> images;
std::vector<DkCmdBuf> pending;
std::map<DkCmdBuf,std::vector<uint32_t>> expected;
int blocks=0,commands=0,devices=0,queues=0,swapchains=0;
unsigned idle_waits=0,fence_waits=0,max_pending=0,completed=0,allocation=0,fail_allocation=0;
bool fail_flush=false,fail_fence=false,fail_acquire=false,bad_queue=false;
uint64_t hash_image(const Image& i){uint64_t h=1469598103934665603ULL;const size_t n=size_t(i.layout.pitch)*i.layout.h;assert(i.offset+n<=i.mem->data.size());for(size_t p=0;p<n;p++){h^=i.mem->data[i.offset+p];h*=1099511628211ULL;}return h;}
Layout layout(const DkImageLayout* p){Layout l;std::memcpy(&l,p,sizeof l);return l;}
void execute_one(){
 assert(!pending.empty());auto* cmd=pending.front();pending.erase(pending.begin());assert(cmd->inflight);
 for(const auto& op:cmd->ops){const auto src=images.at(op.source),dst=images.at(op.dest);assert(hash_image(src)==op.hash);
  for(unsigned y=0;y<op.dst.height;y++)for(unsigned x=0;x<op.dst.width;x++){
   const size_t sp=src.offset+size_t(op.src.y+y*op.src.height/op.dst.height)*src.layout.pitch+size_t(op.src.x+x*op.src.width/op.dst.width)*4;
   const size_t dp=dst.offset+size_t(op.dst.y+y)*dst.layout.pitch+size_t(op.dst.x+x)*4;
   assert(dp+4<=dst.mem->data.size()&&sp+4<=src.mem->data.size());std::memcpy(dst.mem->data.data()+dp,src.mem->data.data()+sp,4);
  }
 }
 if(expected.count(cmd)){const auto& want=expected.at(cmd);const auto img=images.at(cmd->ops.back().dest);assert(std::memcmp(want.data(),img.mem->data.data()+img.offset,want.size()*4)==0);expected.erase(cmd);}
 cmd->inflight=false;completed++;
}
void drained(){while(!pending.empty())execute_one();}
void clean(){assert(!blocks&&!commands&&!devices&&!queues&&!swapchains&&pending.empty()&&expected.empty());}
}
extern "C" {
DkDevice dkDeviceCreate(const DkDeviceMaker*){mock::devices++;return new dk::detail::Device;}
void dkDeviceDestroy(DkDevice p){mock::devices--;delete p;}
DkQueue dkQueueCreate(const DkQueueMaker*){mock::queues++;return new dk::detail::Queue;}
void dkQueueDestroy(DkQueue p){assert(mock::pending.empty());mock::queues--;delete p;}
bool dkQueueIsInErrorState(DkQueue){return mock::bad_queue;}
void dkQueueWaitIdle(DkQueue){mock::idle_waits++;mock::drained();}
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker* m){if(++mock::allocation==mock::fail_allocation)return nullptr;mock::blocks++;auto* b=new dk::detail::MemBlock;b->data.resize(m->size);return b;}
void dkMemBlockDestroy(DkMemBlock p){assert(mock::pending.empty());for(auto i=mock::images.begin();i!=mock::images.end();)if(i->second.mem==p)i=mock::images.erase(i);else ++i;mock::blocks--;delete p;}
void* dkMemBlockGetCpuAddr(DkMemBlock p){return p->data.data();}
DkResult dkMemBlockFlushCpuCache(DkMemBlock p,uint32_t off,uint32_t n){assert(size_t(off)+n<=p->data.size());return mock::fail_flush?DkResult_Fail:DkResult_Success;}
DkCmdBuf dkCmdBufCreate(const DkCmdBufMaker*){mock::commands++;return new dk::detail::CmdBuf;}
void dkCmdBufDestroy(DkCmdBuf c){assert(!c->inflight);mock::commands--;delete c;}
void dkCmdBufAddMemory(DkCmdBuf,DkMemBlock p,uint32_t off,uint32_t n){assert(size_t(off)+n<=p->data.size());}
void dkCmdBufClear(DkCmdBuf c){assert(!c->inflight);c->ops.clear();c->fence=nullptr;}
void dkCmdBufBarrier(DkCmdBuf,DkBarrier,uint32_t){}
void dkCmdBufBlitImage(DkCmdBuf c,const DkImageView* s,const DkImageRect* sr,const DkImageView* d,const DkImageRect* dr,uint32_t,uint32_t){assert(!c->inflight);c->ops.push_back({s->pImage,*sr,d->pImage,*dr,0});}
void dkCmdBufSignalFence(DkCmdBuf c,DkFence* f,bool flush){assert(flush);c->fence=f;}
DkCmdList dkCmdBufFinishList(DkCmdBuf c){return reinterpret_cast<uintptr_t>(c);}
void dkQueueSubmitCommands(DkQueue,DkCmdList list){auto* c=reinterpret_cast<DkCmdBuf>(list);assert(c->fence&&!c->inflight);for(auto& op:c->ops)op.hash=mock::hash_image(mock::images.at(op.source));c->inflight=true;mock::pending.push_back(c);mock::max_pending=std::max(mock::max_pending,unsigned(mock::pending.size()));}
DkResult dkFenceWait(DkFence* f,int64_t timeout){assert(timeout>0);mock::fence_waits++;if(mock::fail_fence)return DkResult_Timeout;bool found=false;for(auto* c:mock::pending)if(c->fence==f)found=true;if(found){while(!mock::pending.empty()){auto* c=mock::pending.front();mock::execute_one();if(c->fence==f)break;}}return DkResult_Success;}
int dkQueueAcquireImage(DkQueue q,DkSwapchain s){return mock::fail_acquire?-1:int((q->next++)%s->images.size());}
void dkQueuePresentImage(DkQueue,DkSwapchain,int){}
void dkImageLayoutInitialize(DkImageLayout* p,const DkImageLayoutMaker* m){mock::Layout l{m->dimensions[0],m->dimensions[1],m->pitchStride?m->pitchStride:m->dimensions[0]*4,m->flags};std::memcpy(p,&l,sizeof l);}
uint64_t dkImageLayoutGetSize(const DkImageLayout* p){auto l=mock::layout(p);return uint64_t(l.pitch)*l.h;}
uint32_t dkImageLayoutGetAlignment(const DkImageLayout*){return 4096;}
void dkImageInitialize(DkImage* p,const DkImageLayout* l,DkMemBlock m,uint32_t off){mock::images[p]={m,off,mock::layout(l)};}
DkSwapchain dkSwapchainCreate(const DkSwapchainMaker* m){mock::swapchains++;auto* s=new dk::detail::Swapchain;for(unsigned i=0;i<m->numImages;i++){auto* p=m->pImages[i];assert((mock::images.at(p).layout.flags&DkImageFlags_UsagePresent)&&!(mock::images.at(p).layout.flags&DkImageFlags_PitchLinear));s->images.push_back(p);}return s;}
void dkSwapchainDestroy(DkSwapchain p){mock::swapchains--;delete p;}
void dkSwapchainSetSwapInterval(DkSwapchain,uint32_t interval){assert(interval==1);}
}
int main(){
 using namespace j2me_present;std::vector<uint8_t> rgb(size_t(240)*320*3);std::vector<uint32_t> game,chrome(canvas_pixels),want;
 for(size_t i=0;i<rgb.size();i++)rgb[i]=uint8_t(i*37);
 assert(game_rgba(rgb,240,320,game));assert(game.size()==480*640);auto original=rgb;
 for(size_t i=0;i<chrome.size();i++)chrome[i]=0xff000000u|uint32_t(i*23);
 assert(!game_rgba(rgb,239,320,game));assert(compose_reference(chrome,&game,want));assert(rgb==original);
 const size_t top=size_t(game_y)*output_w+game_x;assert(want[top]==game[0]&&want[top+1]==game[1]&&want[top+output_w]==game[480]);assert(want[4]==chrome[4]);
 for(unsigned fail=1;fail<=9;fail++){mock::allocation=0;mock::fail_allocation=fail;SwitchGpuPresent p;assert(!p.init());mock::clean();}mock::fail_allocation=0;
 unsigned before=mock::idle_waits;uint64_t chrome_bytes=0,game_bytes=0;{
  SwitchGpuPresent p;assert(p.init()&&p.ready());uint64_t generation=0;
  for(unsigned n=0;n<60;n++){
   if(n%10==0){
    const int sizes[][2]={{240,320},{176,208},{360,360},{360,640},{640,360},{640,640}};
    const int w=sizes[n/10][0],h=sizes[n/10][1];rgb.resize(size_t(w)*h*3);
    for(size_t i=0;i<rgb.size();i++)rgb[i]=uint8_t(i*37+n);
    assert(game_rgba(rgb,w,h,game));
   }
   if(n%10==0){generation++;chrome[4]^=uint32_t(n+37);}game[0]^=uint32_t(n+1);
   const bool layered=n%7!=0;
   assert(p.present_layers(reinterpret_cast<uint8_t*>(chrome.data()),generation,layered?reinterpret_cast<uint8_t*>(game.data()):nullptr));
   if(layered){const auto& op=mock::pending.back()->ops.back();assert(op.src.width==480&&op.src.height==640&&op.dst.width==480&&op.dst.height==640&&op.dst.x==400&&op.dst.y==40);}
   assert(compose_reference(chrome,layered?&game:nullptr,want));mock::expected[mock::pending.back()]=want;
   chrome_bytes+=p.lastTimings().chrome_upload_bytes;game_bytes+=p.lastTimings().frame_upload_bytes;
   assert(mock::idle_waits==before);
  }
  assert(mock::max_pending==3&&mock::fence_waits>=57);p.close();mock::clean();
 }
 assert(chrome_bytes==uint64_t(18)*canvas_pixels*4);assert(game_bytes==uint64_t(51)*game_w*game_h*4);
 for(int failure=0;failure<4;failure++){
  SwitchGpuPresent p;assert(p.init());for(int i=0;i<3;i++)assert(p.present_layers(reinterpret_cast<uint8_t*>(chrome.data()),1,reinterpret_cast<uint8_t*>(game.data())));
  mock::fail_flush=failure==0;mock::fail_fence=failure==1;mock::fail_acquire=failure==2;mock::bad_queue=failure==3;
  assert(!p.present_layers(reinterpret_cast<uint8_t*>(chrome.data()),2,reinterpret_cast<uint8_t*>(game.data())));
  mock::fail_flush=mock::fail_fence=mock::fail_acquire=mock::bad_queue=false;p.close();mock::clean();
 }
 std::cout<<"PASS 235 GPU contract: 60 deferred frames pixel-exact, 3 simultaneous slots, no per-frame QueueWaitIdle, generations, 9 allocation failures, flush/fence/acquire/queue failures; NOT hardware validation\n";
}
