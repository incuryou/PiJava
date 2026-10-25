// Independent image-face fixture: traced on the 460x318 RGBA, not read from
// runtime anchors. Every glyph is rasterized without the production clipping
// width so a clipped/truncated label cannot make the test pass.
#include "../src/host_pro_controller.h"
#include "../src/host_font.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
static host_font::Font* font;
#if __has_include("../src/controller_text_fit.h")
static host_controller::Ink ink(const std::string& s,int px){const auto m=font->measure(s,px);return {m.w,m.h,m.ok};}
#endif
struct Face {int input,cx,cy;double rx,ry;};
static const Face faces[]={
 {host_keymap::ZL,866,198,39,9},{host_keymap::L,855,226,43,9.5},{host_keymap::Minus,920,244,10,11},
 {host_controller::Menu,851,293,23,24},{host_keymap::DUp,900,340,10.5,10.5},{host_keymap::DLeft,874,366,10.5,11},
 {host_keymap::DRight,926,366,10.5,11},{host_keymap::DDown,900,392,10.5,10.5},
 {host_keymap::ZR,1088,198,39,9},{host_keymap::R,1100,226,43,9.5},{host_keymap::Plus,1034,244,10,11},
 {host_keymap::X,1103,264,16,17},{host_keymap::Y,1067,296,16,17},{host_keymap::A,1138,296,16,17},
 {host_keymap::B,1103,328,16,17},{host_keymap::R3,1054,372,24,25}};
struct Bounds {int l=2000,t=2000,r=-1,b=-1;void add(int x,int y){l=std::min(l,x);t=std::min(t,y);r=std::max(r,x);b=std::max(b,y);}bool valid()const{return r>=l;}};
static bool physical(const std::string& s){for(const char* v:{"A","B","X","Y","L","R","ZL","ZR","+","-","L3","R3"})if(s==v)return true;return false;}
static int errors=0;
static void fail(const char* kind,int input){if(errors<24)std::fprintf(stderr,"ASSERT %s input=%d\n",kind,input);++errors;}
int main(int argc,char** argv){
 if(argc!=2)return 2;std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)),{});host_font::Font raster;if(!raster.add_font(data.data(),data.size()))return 3;font=&raster;
 std::vector<uint32_t> frame(1280*720),glyph(frame.size()),clipped(frame.size());host_pro::Preview preview;if(!preview.load("romfs/ui/pro-controller.rgba"))return 4;
 const int phone_centers[][2]={{866,198},{855,226},{920,244},{851,291},{900,340},{874,366},{926,366},{900,392},{1088,198},{1100,226},{1034,244},{1103,259},{1067,291},{1138,290},{1103,323},{1054,372}};
 const int tag_boxes[][4]={{885,191,17,11},{878,217,17,11},{912,258,17,13},{844,300,15,9},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{1053,191,17,11},{1061,217,17,11},{1026,258,17,13},{1099,268,9,9},{1063,300,9,9},{1134,299,9,9},{1099,332,9,9},{1046,380,17,13}};
 std::array<uint64_t,16> tag_pixels{};
 for(int variant=0;variant<3;++variant){
  host_keymap::Config c;if(variant==1){host_keymap::assign(c,false,host_keymap::SoftL,1,host_keymap::A);host_keymap::assign(c,false,host_keymap::N1,1,host_keymap::A);host_keymap::assign(c,false,host_keymap::N9,1,host_keymap::A);}if(variant==2){c.automatic=false;c.bindings=host_keymap::empty_bindings();}
  std::array<Bounds,16> group,main_group;std::array<std::string,16> content;
  const host_menu::CenteredText center=[&](int cx,int cy,const std::string& s,uint32_t,int px,int maxw){
   size_t face=0;double best=1e30;for(size_t n=0;n<16;++n){const auto& f=faces[n];const double d=double(cx-f.cx)*(cx-f.cx)/(f.rx*f.rx)+double(cy-f.cy)*(cy-f.cy)/(f.ry*f.ry);if(d<best){best=d;face=n;}}
   const auto& f=faces[face];const bool tag=physical(s);if(px>(tag?8:14))fail(tag?"physical tag exceeds 8 px budget":"mini phone legend exceeds 14 px budget",f.input);
   std::fill(glyph.begin(),glyph.end(),0);raster.draw_centered(glyph.data(),1280,cx,cy,s,0xffffffffu,px,512);Bounds own;
   std::fill(clipped.begin(),clipped.end(),0);raster.draw_centered(clipped.data(),1280,cx,cy,s,0xffffffffu,px,maxw);if(clipped!=glyph)fail("actual production maxWidth clips glyph pixels",f.input);
   for(int y=std::max(0,cy-50);y<std::min(720,cy+51);++y)for(int x=std::max(0,cx-160);x<std::min(1280,cx+161);++x)if(glyph[size_t(y)*1280+x]&0xffffffu){own.add(x,y);group[face].add(x,y);const double d=double(x-f.cx)*(x-f.cx)/(f.rx*f.rx)+double(y-f.cy)*(y-f.cy)/(f.ry*f.ry);const auto* box=tag_boxes[face];if(tag?(x<box[0]||x>=box[0]+box[2]||y<box[1]||y>=box[1]+box[3]):d>1.001){fail(tag?"physical tag leaves its independent side/below rectangle":"unclipped phone ink leaves independently traced button face",f.input);break;}}
   if(!own.valid())fail("mapped legend must contain visible ink",f.input);else if(own.r-own.l+1>maxw)fail("production maxWidth would clip the complete glyph",f.input);
   if(tag&&(f.input==host_keymap::L||f.input==host_keymap::R||f.input==host_keymap::ZL||f.input==host_keymap::ZR)){const double plane=f.input==host_keymap::L||f.input==host_keymap::R?(215+229)/2.0:(186+206)/2.0;if(std::abs((own.t+own.b)/2.0-plane)>.75)fail("shoulder physical ink is not centered on its actual side-plane vertical span",f.input);}
   if(!tag){if(!content[face].empty())content[face]+='/';content[face]+=s;if(own.valid()){main_group[face].add(own.l,own.t);main_group[face].add(own.r,own.b);}}
   else {uint64_t hash=1469598103934665603ull;for(size_t i=0;i<glyph.size();++i)if(glyph[i]){hash^=i;hash*=1099511628211ull;hash^=glyph[i];hash*=1099511628211ull;}if(variant==0)tag_pixels[face]=hash;else if(tag_pixels[face]!=hash)fail("physical tag pixels moved between default/multi/unbound states",f.input);}
  };
  const host_menu::Text text=[](int,int,const std::string&,uint32_t,int,int){};
#if __has_include("../src/controller_text_fit.h")
  preview.render(frame.data(),1280,c,false,host_keymap::none,text,center,ink);
#else
  preview.render(frame.data(),1280,c,false,host_keymap::none,text,center);
#endif
  for(size_t n=0;n<16;++n){const auto& f=faces[n];const auto& label=preview.labels().label(f.input);const bool bound=label.count>0||f.input==host_controller::Menu;const auto& b=main_group[n];if(bound&&(!b.valid()||std::abs((b.l+b.r)/2.0-phone_centers[n][0])>1.5||std::abs((b.t+b.b)/2.0-phone_centers[n][1])>1.5))fail("main glyph group is off its fixed inset center",f.input);if(content[n]!=(bound?label.text:""))fail("complete multi-binding omitted, or unbound main is not blank",f.input);}
 }
 // The six stick centers were independently measured on the decoded J2SJ.
 const int centers[][4]={{186,281,1024,519},{193,290,1031,519},{182,283,1034,522}};
 for(int bezel=4;bezel<=6;++bezel){host_controller::Legends labels;labels.update(host_keymap::Config{},false,bezel);const auto& want=centers[bezel-4];int sticks=0;
  host_menu::CenteredText center=[&](int cx,int cy,const std::string& s,uint32_t,int px,int){const bool small=bezel==6&&(s=="*"||s=="#");if(s!="0"&&s!="菜单"&&!small)return;if(!small)++sticks;std::fill(glyph.begin(),glyph.end(),0);raster.draw_centered(glyph.data(),1280,cx,cy,s,0xffffffffu,px,512);Bounds b;for(int y=0;y<720;++y)for(int x=0;x<1280;++x)if(glyph[size_t(y)*1280+x]&0xffffffu)b.add(x,y);const int offset=s=="0"?2:0;const int ex=small?(s=="*"?313:965):want[offset],ey=small?161:want[offset+1];if(std::abs((b.l+b.r)/2.0-ex)>1.5||std::abs((b.t+b.b)/2.0-ey)>1.5)fail("game ink is off the independently measured image center",bezel);};
  labels.render(frame.data(),1280,bezel,[](int,int,const std::string&,uint32_t,int,int){},host_controller::Context::Game,center);
  if(sticks!=2)fail("both game stick glyphs were actually rendered",bezel);
 }
 if(errors){std::fprintf(stderr,"TOTAL FACE/INK ASSERTIONS FAILED %d\n",errors);return 1;}
 std::puts("PASS ControllerTextFit: true glyphs inside image faces, fixed physical pixels across three states, blank unbound legends, complete multi-maps and six stick centers");
}
