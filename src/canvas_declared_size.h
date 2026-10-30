#pragma once
#include "logical_screen.h"
#include <unordered_map>
#include <vector>
namespace canvas_declared_size {
struct Result{int w=0,h=0;bool found=false,conflict=false;std::string reason;};
namespace detail {
struct Reader{const std::vector<uint8_t>& b;size_t p=0;bool ok=true;uint32_t get(int n){if(!ok||size_t(n)>b.size()-p){ok=false;return 0;}uint32_t v=0;while(n--)v=(v<<8)|b[p++];return v;}void skip(size_t n){if(!ok||n>b.size()-p)ok=false;else p+=n;}};
struct Entry{int tag=0,index=0;int32_t value=0;std::string text;};
struct Getter{bool declared=false,constant=false;int value=0;};
struct Info{std::string name,parent;Getter w,h;bool abstract=false;};
inline bool parse(const std::vector<uint8_t>& bytes,Info& out){
 Reader r{bytes};if(r.get(4)!=0xcafebabe)return false;r.skip(4);const auto count=r.get(2);if(!count)return false;std::vector<Entry> cp(count);
 for(uint32_t i=1;i<count&&r.ok;++i){auto& e=cp[i];e.tag=r.get(1);switch(e.tag){
 case 1:{auto n=r.get(2);if(!r.ok||n>bytes.size()-r.p)return false;e.text.assign(reinterpret_cast<const char*>(bytes.data()+r.p),n);r.skip(n);break;}
 case 3:{auto v=r.get(4);e.value=v<=0x7fffffff?int32_t(v):int32_t(int64_t(v)-0x100000000LL);break;}
 case 4:r.skip(4);break;case 5:case 6:r.skip(8);if(++i>=count)return false;break;
 case 7:case 8:case 16:case 19:case 20:e.index=r.get(2);break;
 case 9:case 10:case 11:case 12:case 17:case 18:r.skip(4);break;case 15:r.skip(3);break;default:return false;}}
 auto text=[&](uint32_t n)->std::string{return n<cp.size()&&cp[n].tag==1?cp[n].text:std::string{};};
 auto cls=[&](uint32_t n)->std::string{return n<cp.size()&&cp[n].tag==7?text(cp[n].index):std::string{};};
 out.abstract=(r.get(2)&0x0400)!=0;out.name=cls(r.get(2));out.parent=cls(r.get(2));if(out.name.empty()||out.parent.empty())return false;
 const auto interfaces=r.get(2);r.skip(size_t(interfaces)*2);
 auto attributes=[&](){auto n=r.get(2);while(n--&&r.ok){r.skip(2);auto len=r.get(4);r.skip(len);}};
 const auto fields=r.get(2);for(uint32_t i=0;i<fields&&r.ok;++i){r.skip(6);attributes();}
 const auto methods=r.get(2);
 for(uint32_t i=0;i<methods&&r.ok;++i){auto flags=r.get(2);auto name=text(r.get(2)),desc=text(r.get(2));Getter* g=desc=="()I"&&!(flags&(0x0008|0x0002))?(name=="getWidth"?&out.w:name=="getHeight"?&out.h:nullptr):nullptr;if(g){if(g->declared)return false;g->declared=true;}
 auto attrs=r.get(2);while(attrs--&&r.ok){auto attr=text(r.get(2));auto length=r.get(4);if(!r.ok||length>bytes.size()-r.p)return false;const auto end=r.p+length;
 if(g&&!(flags&(0x0400|0x0100))&&attr=="Code"){
  if(length<12)return false;r.skip(4);auto len=r.get(4);if(!r.ok||len>end-r.p)return false;const auto start=r.p;int value=0;bool constant=false;
  if(len>=2){const auto op=r.get(1);if(op>=2&&op<=8){value=int(op)-3;constant=len==2;}
   else if(op==16){auto v=r.get(1);value=v<128?int(v):int(v)-256;constant=len==3;}
   else if(op==17){auto v=r.get(2);value=v<32768?int(v):int(v)-65536;constant=len==4;}
   else if(op==18||op==19){auto n=r.get(op==18?1:2);if(n<cp.size()&&cp[n].tag==3){value=cp[n].value;constant=len==uint32_t(op==18?3:4);}}
   if(constant)constant=r.get(1)==172;
  }
  r.p=start+len;auto handlers=r.get(2);r.skip(size_t(handlers)*8);attributes();if(!r.ok||r.p!=end)return false;
  if(constant&&handlers==0){g->constant=true;g->value=value;}
 }else r.skip(length);if(!r.ok||r.p!=end)return false;
 }}
 attributes();return r.ok&&r.p==bytes.size();
}
}
inline Result inspect(const std::unordered_map<std::string,std::vector<uint8_t>>& files){
 std::unordered_map<std::string,detail::Info> classes;size_t total=0,count=0;
 for(const auto& file:files){if(file.first.size()<6||file.first.substr(file.first.size()-6)!=".class")continue;
  total+=file.second.size();if(++count>4096||total>64*1024*1024)return {0,0,false,true,"inspection_limit"};
  detail::Info info;if(!detail::parse(file.second,info))return {0,0,false,true,"malformed_class"};
  if(!classes.emplace(info.name,info).second)return {0,0,false,true,"duplicate_class"};
 }
 Result out;
 for(const auto& item:classes){
  if(item.second.abstract)continue;
  bool has_child=false;for(const auto& candidate:classes)if(candidate.second.parent==item.first){has_child=true;break;}
  if(has_child)continue;
  const detail::Info* c=&item.second;detail::Getter w,h;bool canvas=false;
  for(int depth=0;depth<64&&c;++depth){if(!w.declared&&c->w.declared)w=c->w;if(!h.declared&&c->h.declared)h=c->h;
   if(c->parent=="javax/microedition/lcdui/Canvas"||c->parent=="javax/microedition/lcdui/game/GameCanvas"){canvas=true;break;}
   auto next=classes.find(c->parent);c=next==classes.end()?nullptr:&next->second;
  }
  if(!canvas)continue;
  if(!w.constant||!h.constant)return {0,0,false,false,"dynamic_canvas"};
  if(!logical_screen::valid(w.value,h.value))return {0,0,false,true,"invalid_fixed_extent"};
  if(out.found&&(out.w!=w.value||out.h!=h.value))return {0,0,false,true,"conflicting_fixed_extents"};
  out={w.value,h.value,true,false,"constant_canvas_getters"};
 }
 return out;
}
}
