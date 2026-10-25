// Synthetic VM objects exercise the real shared image raster entry. No user
// JAR, Java bytecode or live game state is changed by this fixture.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
static constexpr const char* star_sha="e808c5b9878639757edcf67d330536a7b95f553790c2cf23c7d267df06ecb6b8";
static std::vector<uint8_t> bytes(const char* hex){std::vector<uint8_t> b;for(;*hex;hex+=2)b.push_back(uint8_t(std::stoi(std::string(hex,2),nullptr,16)));return b;}
static uint32_t pixel(int x,int y){const size_t i=(size_t(y)*360+x)*3;return uint32_t(VMS_RS(g_fb)[i])<<16|uint32_t(VMS_RS(g_fb)[i+1])<<8|VMS_RS(g_fb)[i+2];}
int main(){
 VMS_CL(g_classpath)={"romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_RS(g_screen_w)=VMS_RS(g_screen_h)=360;
 const int image_class=ensure_class("javax/microedition/lcdui/Image"),gfx_class=ensure_class("javax/microedition/lcdui/Graphics");
 Method* create=find_method(image_class,"createImage","(II)Ljavax/microedition/lcdui/Image;",false);assert(create);
 const int ring=run(create,{Value::of_i(384),Value::of_i(384)}).ref;assert(ring>0);
 for(uint8_t c:VMS_HEAP(g_heap)[ring].pixels)assert(c==255); // MIDP mutable white stays unchanged.
 const int gfx=alloc_obj(gfx_class);gfx_bind(gfx,0,0);
 const int owner=int(VMS_CL(g_classes).size());VMS_CL(g_classes).emplace_back();auto& n=VMS_CL(g_classes).back();n.name="n";n.init_state=2;n.instance_fields=2;VMS_CL(g_class_index)["n"]=owner;
 n.fields={{"f","S",0,0,owner,false,0},{"g","S",0,1,owner,false,0},{"G","Ljavax/microedition/lcdui/Image;",8,0,owner,true,0},{"E","I",8,1,owner,true,0},{"F","I",8,2,owner,true,0}};
 n.statics={Value::of_ref(ring),Value::of_i(384),Value::of_i(384)};
 n.methods.resize(2);auto& inner=n.methods[0];inner.name="a";inner.desc="(Ljavax/microedition/lcdui/Graphics;IIIIII)V";inner.class_id=owner;inner.flags=10;
 inner.code=bytes("2a150515061d1504b6004f2ab2001d15051b6415061c641014b6004e2a0303110168110168b6004fb1");
 auto& outer=n.methods[1];outer.name="a";outer.desc="(Ljavax/microedition/lcdui/Graphics;II)V";outer.class_id=owner;outer.flags=17;
 const int map=alloc_obj(owner);std::vector<Frame> frames(2);frames[0].method=&outer;frames[0].locals={Value::of_ref(map),Value::of_ref(gfx),Value::of_i(-12),Value::of_i(120)};frames[1].method=&inner;frames[1].op_pc=25;frames[1].locals={Value::of_ref(gfx)};
 VMS_EXEC(g_active_stack)=&frames;VMS_PLAT(g_rms_suite)=star_sha;
 auto prepare=[&](int w,int h,int cx,int cy){
  VMS_HEAP(g_heap)[map].fields={Value::of_i(w),Value::of_i(h)};frames[0].locals[2]=Value::of_i(cx);frames[0].locals[3]=Value::of_i(cy);
  auto& image=VMS_HEAP(g_heap)[ring];std::fill(image.pixels.begin(),image.pixels.end(),255);
  for(int y=0;y<std::min(h,384);++y)for(int x=0;x<std::min(w,384);++x){size_t i=(size_t(y)*384+x)*3;image.pixels[i]=0x12;image.pixels[i+1]=0x34;image.pixels[i+2]=0x56;}
  VMS_RS(g_fb).assign(360*360*3,0);VMS_RS(g_clip)[gfx]={0,0,360,360,true};
 };
 auto draw=[&](int cx,int cy){draw_region(gfx,0,ring,0,0,384,384,0,-cx,-cy);};
 prepare(336,480,-12,0);const auto saved_pixels=VMS_HEAP(g_heap)[ring].pixels;draw(-12,0);
 assert(pixel(0,0)==0&&pixel(11,0)==0&&pixel(12,0)==0x123456&&pixel(347,0)==0x123456);
 assert(pixel(348,0)==0&&pixel(359,0)==0); // RED: old raster copies unused white ring pixels.
 assert(VMS_HEAP(g_heap)[ring].pixels==saved_pixels);assert(VMS_RS(g_clip)[gfx].x==0&&VMS_RS(g_clip)[gfx].w==360);
 prepare(240,320,-60,-20);draw(-60,-20);
 for(int y=0;y<360;++y)for(int x=0;x<360;++x)assert(pixel(x,y)==(x>=60&&x<300&&y>=20&&y<340?0x123456u:0u));
 prepare(480,480,0,0);draw(0,0);assert(pixel(0,0)==0x123456&&pixel(359,359)==0x123456);
 prepare(336,320,-12,-20);VMS_RS(g_clip)[gfx]={20,25,330,320,true};draw(-12,-20);assert(pixel(19,25)==0&&pixel(20,25)==0x123456&&pixel(347,339)==0x123456&&pixel(348,339)==0);assert(VMS_RS(g_clip)[gfx].x==20&&VMS_RS(g_clip)[gfx].w==330);
 assert(run(find_method(gfx_class,"getClipWidth","()I",false),{Value::of_ref(gfx)}).i==330);
 assert(run(find_method(gfx_class,"getClipX","()I",false),{Value::of_ref(gfx)}).i==20);
 Method native_draw;native_draw.name="drawImage";native_draw.desc="(Ljavax/microedition/lcdui/Image;III)V";native_draw.native=true;native_draw.class_id=gfx_class;
 prepare(336,480,-12,0);frames.emplace_back();frames.back().method=&native_draw;draw(-12,0);assert(pixel(347,0)==0x123456&&pixel(348,0)==0);frames.pop_back();
 prepare(336,320,-12,-20);VMS_RS(g_gfx_tx)[gfx]=7;VMS_RS(g_gfx_ty)[gfx]=5;
 draw_region(gfx,0,ring,0,0,384,384,0,19,25);
 for(int y=0;y<360;++y)for(int x=0;x<360;++x)assert(pixel(x,y)==(x>=19&&x<355&&y>=25&&y<345?0x123456u:0u));
 VMS_RS(g_gfx_tx)[gfx]=VMS_RS(g_gfx_ty)[gfx]=0;
 for(int camera:{-1000,1000}){
  prepare(336,320,camera,camera);draw_region(gfx,0,ring,0,0,384,384,0,0,0);
  for(uint8_t value:VMS_RS(g_fb))assert(value==0); // Empty world intersection.
  assert(VMS_RS(g_clip)[gfx].x==0&&VMS_RS(g_clip)[gfx].w==360);
 }
 const int offscreen=run(create,{Value::of_i(384),Value::of_i(384)}).ref;assert(offscreen>0);
 prepare(336,480,-12,0);std::fill(VMS_HEAP(g_heap)[offscreen].pixels.begin(),VMS_HEAP(g_heap)[offscreen].pixels.end(),0);
 draw_region(gfx,offscreen,ring,0,0,384,384,0,12,0);assert(VMS_HEAP(g_heap)[offscreen].pixels[size_t(359)*3]==255);
 prepare(336,480,-12,0);draw_region(gfx,0,ring,0,0,384,384,2,12,0);assert(pixel(359,0)==0x123456); // Mirrored draws are ordinary Graphics.
 // Every nonmatching guard preserves ordinary Graphics behavior.
 for(int guard=0;guard<8;++guard){
  prepare(336,480,-12,0);const auto code=inner.code;const auto desc=inner.desc;const auto original=frames[0].locals[0];
  if(guard==0)VMS_PLAT(g_rms_suite)="different-jar";
  if(guard==1)inner.code[0]^=1;
  if(guard==2)frames[1].op_pc=8;
  if(guard==3)inner.desc="(Ljavax/microedition/lcdui/Graphics;III)V";
  if(guard==4)frames[0].locals[0]=Value::of_ref(gfx);
  if(guard==5)n.statics[0]=Value::of_ref(0);
  if(guard==6)VMS_EXEC(g_active_stack)=nullptr; // HUD/other draw route.
  if(guard==7)n.statics[1]=Value::of_i(400);
  draw(-12,0);assert(pixel(359,0)==0xffffff);
  VMS_PLAT(g_rms_suite)=star_sha;inner.code=code;inner.desc=desc;frames[1].op_pc=25;frames[0].locals[0]=original;n.statics[0]=Value::of_ref(ring);n.statics[1]=Value::of_i(384);VMS_EXEC(g_active_stack)=&frames;
 }
 // Later sprites/UI may paint outside the map; no global clip or frame mask.
 prepare(336,480,-12,0);draw(-12,0);VMS_EXEC(g_active_stack)=nullptr;
 VMS_HEAP(g_heap)[ring].pixels[0]=0xab;VMS_HEAP(g_heap)[ring].pixels[1]=0xcd;VMS_HEAP(g_heap)[ring].pixels[2]=0xef;
 draw_region(gfx,0,ring,0,0,1,1,0,359,0);assert(pixel(359,0)==0xabcdef);
 VMS_RS(g_screen_w)=240;VMS_RS(g_screen_h)=320;VMS_RS(g_fb).assign(240*320*3,0);VMS_RS(g_clip)[gfx]={0,0,240,320,true};VMS_EXEC(g_active_stack)=&frames;
 draw_region(gfx,0,ring,0,0,384,384,0,0,0);assert(VMS_RS(g_fb)[0]==0xab); // Explicit default canvas unchanged.
 VMS_EXEC(g_active_stack)=nullptr;p7::pool().shutdown();std::cout<<"PASS StarMapBoundsChecks: local world clip, native image white, source/Java clip unchanged, narrow/large maps, negative camera, exact-JAR/origin guards, later UI and default canvas\n";
}
