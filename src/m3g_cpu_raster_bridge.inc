// Main-only VM snapshot/publication boundary. Workers see only m3g_raster data.
#include "m3g_cpu_worker.h"
namespace m3g_cpu_raster_bridge {
using m3g_gpu_bridge::Stamp;using m3g_gpu_bridge::stamp;using m3g_gpu_bridge::current;
using Boundary=m3g_raster_scope::Boundary;
struct Stats {uint64_t scopes=0,commands=0,batches=0,submitted_batches=0,worker_batches=0,inline_batches=0,triangles=0,pixels=0,captured_bytes=0,capture_us=0,commit_us=0,wait_us=0,worker_cpu_us=0,worker_wall_us=0,stale=0,failed=0,max_batch=0,peak_retained_bytes=0;std::array<uint64_t,8> flush_reasons{};std::array<uint64_t,5> batch_sizes{};};
static Stats stats;
struct TextureCopy {Stamp key;std::shared_ptr<const std::vector<uint32_t>> pixels;};
struct Context {uint64_t token=0;Stamp object,target,backing,color,depth;int w=0,h=0;std::shared_ptr<m3g_raster::Surface> surface;std::vector<m3g_raster::Command> commands;std::vector<TextureCopy> textures;size_t texture_bytes=0;};
static std::map<uint64_t,Context> contexts;
static size_t retained_bytes(){
    constexpr size_t max=m3g_raster::RasterQueue::MaxBytes;size_t total=0;
    auto add=[&](size_t n){if(n>max-total)return false;total+=n;return true;};
    for(const auto& entry:contexts){const auto& c=entry.second;if(!add(sizeof(Context)+64+sizeof(m3g_raster::Surface))||c.surface->colors.capacity()>max/4||!add(c.surface->colors.capacity()*4)||c.surface->depth.capacity()>max/2||!add(c.surface->depth.capacity()*2)||c.commands.capacity()>max/sizeof(m3g_raster::Command)||!add(c.commands.capacity()*sizeof(m3g_raster::Command))||c.textures.capacity()>max/sizeof(TextureCopy)||!add(c.textures.capacity()*sizeof(TextureCopy)))return max+1;
        for(const auto& t:c.textures)if(t.pixels->capacity()>max/4||!add(t.pixels->capacity()*4+sizeof(std::vector<uint32_t>)+64))return max+1;}
    return total;
}
static m3g_raster::RasterQueue queue;
static uint64_t now_us(){return m3g_gpu_bridge::time_ns()/1000;}
static uint64_t current_scope(){auto* stack=VMS_EXEC(g_active_stack);if(!m3g_raster_scope::enabled||r4::state.active||!stack||stack->empty())return 0;const auto& f=stack->back();return f.cpu_raster_epoch==resver::g_epoch?f.cpu_raster_scope:0;}
static void open_frame(Frame& f){if(!m3g_raster_scope::enabled||r4::state.active||!f.method||f.locals.empty()||!m3g_gpu_bridge::exact(f.locals[0].ref,"javax/microedition/m3g/Graphics3D")||f.method->native||f.method->name!="renderSubmesh"||f.method->desc!=m3g_gpu_bridge::render_descriptor||!m3g_gpu_bridge::trusted_class(f.method->class_id,"javax/microedition/m3g/Graphics3D")||VMS_CL(g_classes)[f.method->class_id].init_state!=2||!m3g_gpu_bridge::geometry_dependencies())return;f.cpu_raster_scope=m3g_raster_scope::next_token();f.cpu_raster_epoch=resver::g_epoch;++stats.scopes;}
static void close_frame(Frame& f){if(f.cpu_raster_scope&&!m3g_raster_scope::flush(f.cpu_raster_scope,Boundary::Return))die("CPU_RASTER return flush failed");f.cpu_raster_scope=0;f.cpu_raster_epoch=0;}
static bool target_current(const Context& c){
    if(!current(c.object)||!current(c.color)||(c.depth.slot&&!current(c.depth))||!current(c.target)||m3g_ref(c.object.slot,"target","Ljava/lang/Object;")!=c.target.slot||m3g_ref(c.object.slot,"rasterData","[I")!=c.color.slot||m3g_ref(c.object.slot,"depthBuffer","[S")!=c.depth.slot)return false;
    int w=0,h=0;if(!m3g_int(c.object.slot,"canvasWidth","I",w)||!m3g_int(c.object.slot,"canvasHeight","I",h)||w!=c.w||h!=c.h)return false;
    if(m3g_gpu_bridge::exact(c.target.slot,"javax/microedition/lcdui/Graphics")){auto m=VMS_RS(g_gfx_target).find(c.target.slot);if(m==VMS_RS(g_gfx_target).end()||m->second!=c.backing.slot)return false;if(c.backing.slot)return current(c.backing);return c.w==VMS_RS(g_screen_w)&&c.h==VMS_RS(g_screen_h);}
    return m3g_gpu_bridge::exact(c.target.slot,"javax/microedition/m3g/Image2D")&&m3g_ref(c.target.slot,"image","[I")==c.color.slot;
}
static bool flush_one(uint64_t token,Boundary reason){
    auto found=contexts.find(token);if(found==contexts.end())return true;Context c=std::move(found->second);contexts.erase(found);m3g_raster_scope::has_pending=!contexts.empty();
    if(c.commands.empty())return true;
    if(c.object.epoch!=resver::g_epoch){++stats.stale;return true;}
    if(!target_current(c)){++stats.stale;return false;}
    const uint64_t began=now_us();const size_t count=c.commands.size();m3g_raster::Result result;
    auto ticket=queue.submit(c.surface,std::move(c.commands));
    if(ticket){result=m3g_raster::RasterQueue::wait(ticket);++stats.submitted_batches;}
    else {result.code=m3g_raster::Code::Success;for(const auto& command:c.commands){auto n=m3g_raster::draw_owned(command,*c.surface);result.counts.triangles+=n.triangles;result.counts.pixels+=n.pixels;if(!n.ok){result.code=m3g_raster::Code::Failed;result.counts.ok=false;break;}}}
    stats.wait_us+=now_us()-began;stats.worker_cpu_us+=result.worker_cpu_us;stats.worker_wall_us+=result.worker_wall_us;
    if(result.code!=m3g_raster::Code::Success||!result.counts.ok){++stats.failed;return false;}
    if(c.object.epoch!=resver::g_epoch){++stats.stale;return true;}
    if(!target_current(c)){++stats.stale;return false;}
    if(ticket)++stats.worker_batches;else ++stats.inline_batches;
    const uint64_t commit=now_us();auto& colors=VMS_HEAP(g_heap)[c.color.slot].elems;
    uint8_t* color_view=colors.int_bytes_for_write();uint8_t* depth_view=c.depth.slot?VMS_HEAP(g_heap)[c.depth.slot].elems.short_bytes_for_write():nullptr;
    if(!color_view||(c.depth.slot&&!depth_view))return false;
    std::memcpy(color_view,c.surface->colors.data(),c.surface->colors.size()*sizeof(uint32_t));
    if(depth_view)std::memcpy(depth_view,c.surface->depth.data(),c.surface->depth.size()*sizeof(int16_t));
    stats.commit_us+=now_us()-commit;++stats.batches;stats.triangles+=result.counts.triangles;stats.pixels+=result.counts.pixels;stats.max_batch=std::max(stats.max_batch,uint64_t(count));++stats.flush_reasons[size_t(reason)];++stats.batch_sizes[count==1?0:count<8?1:count<32?2:count<64?3:4];
    VMS_RS(g_m3g_native_triangles)+=result.counts.triangles;VMS_RS(g_m3g_native_pixels)+=result.counts.pixels;return true;
}
static bool flush_all(Boundary reason){while(!contexts.empty())if(!flush_one(contexts.begin()->first,reason))return false;m3g_raster_scope::has_pending=false;return true;}
static bool fallback(){if(!m3g_cpu_raster_bridge::flush_all(Boundary::Fallback))die("CPU_RASTER fallback flush failed");return false;}
static bool command_supported(const m3g_raster::Command& q){
    if(q.w<=0||q.h<=0||q.w>800||q.h>800||uint64_t(q.w)*q.h>409600||q.count<0||q.count>4||q.mode<64||q.mode>68||q.alphaFactor<0||q.alphaFactor>256||!std::isfinite(q.threshold)||!std::isfinite(q.factor)||!std::isfinite(q.units))return false;
    for(int j=0;j<3;++j)if(!std::isfinite(q.x[j])||!std::isfinite(q.y[j])||!std::isfinite(q.z[j])||!std::isfinite(q.iw[j]))return false;
    if(!std::isfinite((q.x[1]-q.x[0])*(q.y[2]-q.y[0])-(q.y[1]-q.y[0])*(q.x[2]-q.x[0])))return false;
    for(int j=0;j<q.count;++j){const auto& t=q.textures[size_t(j)];if(t.width<=0||t.height<=0||uint64_t(t.width)*t.height>262144||t.mode<224||t.mode>228||t.format<96||t.format>100)return false;for(float uv:t.uv)if(!std::isfinite(uv))return false;}return true;
}
static bool capture(int graphics,const m3g_raster::Command& source,ArrayStorage* color,ArrayStorage* depth,const std::array<ArrayStorage*,4>& textures){
    const uint64_t began=now_us(),token=current_scope();if(!token||!queue.available()||!command_supported(source))return fallback();
    auto* frames=VMS_EXEC(g_active_stack);if(!frames||frames->empty()||frames->back().locals.empty()||frames->back().locals[0].ref!=graphics)return fallback();
    int color_ref=m3g_ref(graphics,"rasterData","[I"),depth_ref=m3g_ref(graphics,"depthBuffer","[S"),target=m3g_ref(graphics,"target","Ljava/lang/Object;");size_t area=size_t(source.w)*source.h;
    if(color_ref==m3g_ref(graphics,"renderableTriangles","[I"))return fallback(); // loop bound cannot alias deferred pixels
    if(m3g_array(color_ref,"[I",area)!=color||(depth&&m3g_array(depth_ref,"[S",area)!=depth)||(!depth&&(source.depthEnabled||depth_ref))||color==depth)return fallback();
    auto old=contexts.find(token);if(old!=contexts.end()&&(old->second.w!=source.w||old->second.h!=source.h||old->second.object.slot!=graphics||old->second.color.slot!=color_ref||old->second.depth.slot!=(depth?depth_ref:0)||old->second.commands.size()>=m3g_raster::RasterQueue::MaxCommands)){if(!flush_one(token,Boundary::Fallback))die("CPU_RASTER context switch flush failed");old=contexts.end();}
    if(old==contexts.end()){
        if(contexts.size()>=2&&!m3g_cpu_raster_bridge::flush_all(Boundary::Fallback))die("CPU_RASTER context budget flush failed");
        const size_t base=sizeof(Context)+64+sizeof(m3g_raster::Surface)+area*(depth?6:4)+64*sizeof(m3g_raster::Command);size_t retained=retained_bytes();
        if(retained>m3g_raster::RasterQueue::MaxBytes||base>m3g_raster::RasterQueue::MaxBytes-retained){if(!m3g_cpu_raster_bridge::flush_all(Boundary::Fallback))die("CPU_RASTER surface budget flush failed");if(base>m3g_raster::RasterQueue::MaxBytes)return false;}
        Context c;c.token=token;c.w=source.w;c.h=source.h;c.object=stamp(graphics);c.color=stamp(color_ref);if(depth)c.depth=stamp(depth_ref);c.target=stamp(target);
        if(m3g_gpu_bridge::exact(target,"javax/microedition/lcdui/Graphics")){auto m=VMS_RS(g_gfx_target).find(target);if(m==VMS_RS(g_gfx_target).end())return fallback();if(m->second){if(!m3g_gpu_bridge::exact(m->second,"javax/microedition/lcdui/Image"))return fallback();const auto& image=VMS_HEAP(g_heap)[m->second];if(image.width!=source.w||image.height!=source.h)return fallback();c.backing=stamp(m->second);}else if(source.w!=VMS_RS(g_screen_w)||source.h!=VMS_RS(g_screen_h))return fallback();}
        else return fallback(); // M1: direct Image2D keeps the original synchronous path
        c.surface=std::make_shared<m3g_raster::Surface>();c.surface->width=c.w;c.surface->height=c.h;c.surface->colors.resize(area);if(depth)c.surface->depth.resize(area);
        {const int32_t* packed=static_cast<const ArrayStorage&>(*color).int_data();const uint8_t* shorts=depth?static_cast<const ArrayStorage&>(*depth).short_bytes_for_read():nullptr;if(!packed||(depth&&!shorts))return fallback();std::memcpy(c.surface->colors.data(),packed,area*4);if(shorts)std::memcpy(c.surface->depth.data(),shorts,area*2);}c.commands.reserve(64);stats.captured_bytes+=area*(depth?6:4);old=contexts.emplace(token,std::move(c)).first;
    }
    auto& c=old->second;if(!target_current(c)){++stats.stale;return fallback();}
    constexpr size_t max=m3g_raster::RasterQueue::MaxBytes;size_t retained=retained_bytes();if(retained>max)return fallback();
    if(c.commands.size()==c.commands.capacity()){size_t next=std::min(m3g_raster::RasterQueue::MaxCommands,std::max(size_t(64),c.commands.capacity()*2));size_t delta=(next-c.commands.capacity())*sizeof(m3g_raster::Command);if(delta>max-retained)return fallback();c.commands.reserve(next);if(retained_bytes()>max)return fallback();}
    m3g_raster::Command command=source;
    auto* current_textures=source.count?m3g_array(m3g_ref(graphics,"textures","[Ljavax/microedition/m3g/Texture2D;"),"[Ljavax/microedition/m3g/Texture2D;",size_t(source.count)):nullptr;
    for(int unit=0;unit<source.count;++unit){auto* pixels=textures[size_t(unit)];if(!current_textures||!pixels||pixels==color||pixels==depth)return fallback();int texture=current_textures->get(size_t(unit)).ref,image=m3g_ref(texture,"texImage","Ljavax/microedition/m3g/Image2D;"),ref=m3g_ref(image,"image","[I");size_t n=size_t(command.textures[size_t(unit)].width)*command.textures[size_t(unit)].height;
        if(m3g_array(ref,"[I",n)!=pixels||pixels->size()!=n)return fallback();auto key=stamp(ref);std::shared_ptr<const std::vector<uint32_t>> owned;
        for(const auto& cache:c.textures)if(cache.key.epoch==key.epoch&&cache.key.slot==key.slot&&cache.key.generation==key.generation&&cache.key.layout==key.layout&&cache.key.content==key.content){owned=cache.pixels;break;}
        if(!owned){const size_t bytes=n*sizeof(uint32_t);retained=retained_bytes();if(retained>max)return fallback();size_t next=c.textures.size()==c.textures.capacity()?std::max(size_t(4),c.textures.capacity()*2):c.textures.capacity();size_t delta=(next-c.textures.capacity())*sizeof(TextureCopy);if(delta>max-retained)return fallback();retained+=delta;size_t extra=bytes+sizeof(std::vector<uint32_t>)+64;if(extra>max-retained)return fallback();c.textures.reserve(next);auto copy=std::make_shared<std::vector<uint32_t>>(n);{const int32_t* packed=static_cast<const ArrayStorage&>(*pixels).int_data();if(!packed)return fallback();std::memcpy(copy->data(),packed,n*4);}owned=copy;c.textures.push_back({key,owned});c.texture_bytes+=bytes;stats.captured_bytes+=bytes;if(retained_bytes()>max)return fallback();}
        command.textures[size_t(unit)].pixels=std::move(owned);
    }
    if(retained_bytes()>max)return fallback();c.commands.push_back(std::move(command));stats.peak_retained_bytes=std::max(stats.peak_retained_bytes,uint64_t(retained_bytes()));++stats.commands;stats.capture_us+=now_us()-began;m3g_raster_scope::has_pending=true;return true;
}
static bool configure(bool requested,bool cpu_profile,bool deterministic){
    if(!m3g_cpu_raster_bridge::flush_all(Boundary::Lifecycle))return false;
    m3g_raster_scope::enabled=requested&&!cpu_profile&&!deterministic;
    if(m3g_raster_scope::enabled&&!p7::pool().enabled())m3g_raster_scope::enabled=p7::pool().start();
    return m3g_raster_scope::enabled;
}
static std::string counters(){
    std::string line="M3GCPU_RASTER scope=service_cumulative counts=actual commands="+std::to_string(stats.commands)+" batches="+std::to_string(stats.batches)+" worker_batches="+std::to_string(stats.worker_batches)+" inline_batches="+std::to_string(stats.inline_batches)+" triangles="+std::to_string(stats.triangles)+" pixels="+std::to_string(stats.pixels)+" max_batch="+std::to_string(stats.max_batch)+" peak_bytes="+std::to_string(stats.peak_retained_bytes);
    for(size_t i=0;i<stats.flush_reasons.size();++i)line+=" flush_"+std::to_string(i)+"="+std::to_string(stats.flush_reasons[i]);return line;
}
static const bool installed=[](){m3g_raster_scope::query_callback=current_scope;m3g_raster_scope::flush_callback=flush_one;m3g_raster_scope::flush_all_callback=flush_all;m3g_raster::cpu_capture=capture;return true;}();
}
