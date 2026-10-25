#include "../src/native_jit236.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
using namespace jit236;
static uint64_t ref(const Program& p,const std::vector<int32_t>& args){
 std::array<uint32_t,32> a{};for(size_t i=0;i<args.size();++i)a[i]=uint32_t(args[i]);std::vector<uint32_t>s;size_t at=0;uint64_t ops=0;
 auto pop=[&](){auto x=s.back();s.pop_back();return x;};
 while(at<p.ins.size()){
 const auto&n=p.ins[at++];++ops;bool branch=false;
 if(n.op==21||(n.op>=26&&n.op<=29))s.push_back(a[n.index]);
 else if(n.op==54||(n.op>=59&&n.op<=62))a[n.index]=pop();
 else if((n.op>=2&&n.op<=8)||n.op==16||n.op==17||n.op==18||n.op==19)s.push_back(uint32_t(n.value));
 else if(n.op==132)a[n.index]+=uint32_t(n.value);
 else if(n.op==87)pop();else if(n.op==89)s.push_back(s.back());else if(n.op==95)std::swap(s[s.size()-1],s[s.size()-2]);
 else if(n.op==116)s.back()=0u-s.back();
 else if(n.op==145)s.back()=uint32_t(int32_t(int8_t(s.back())));
 else if(n.op==146)s.back()&=65535;
 else if(n.op==147)s.back()=uint32_t(int32_t(int16_t(s.back())));
 else if(n.op==172)return (ops<<32)|pop();
 else if(n.op>=153&&n.op<=164){int32_t b=0,a;if(n.op>=159){b=int32_t(pop());a=int32_t(pop());}else a=int32_t(pop());int k=n.op-(n.op<=158?153:159);branch=k==0?a==b:k==1?a!=b:k==2?a<b:k==3?a>=b:k==4?a>b:a<=b;}
 else if(n.op==167)branch=true;
 else if(n.op!=0){uint32_t b=pop(),a=pop(),r=0;switch(n.op){case 96:r=a+b;break;case 100:r=a-b;break;case 104:r=a*b;break;case 120:r=a<<(b&31);break;case 122:r=uint32_t(int32_t(a)>>(b&31));break;case 124:r=a>>(b&31);break;case 126:r=a&b;break;case 128:r=a|b;break;case 130:r=a^b;break;default:assert(false);}s.push_back(r);}
 if(branch){while(at<p.ins.size()&&p.ins[at].pc<n.target)++at;assert(at<p.ins.size()&&p.ins[at].pc==n.target);}
 }assert(false);return 0;
}
int main(int argc,char**argv){
 std::mt19937 rng(0x236);uint64_t checked=0,programs=0,rejected=0;
 for(int round=0;round<800;++round){
  std::vector<uint8_t> code={26,27};const int ops[]={96,100,104,120,122,124,126,128,130};code.push_back(uint8_t(ops[rng()%9]));
  for(int i=0;i<10;++i){code.push_back(16);code.push_back(uint8_t(rng()));code.push_back(uint8_t(ops[rng()%9]));if(i%3==0)code.push_back(uint8_t(145+rng()%3));}
  code.push_back(172);std::string d="(II)I";Spec spec{code.data(),code.size(),&d,8,2,4,false,false,nullptr};Program p;assert(verify(spec,p));Executable exe;assert(exe.install(emit_host(p)));++programs;
  for(int i=0;i<128;++i){std::vector<int32_t>a={int32_t(rng()),int32_t(rng())};assert(exe.fn(a.data())==ref(p,a));++checked;}
  if(round==0&&argc>1){auto arm=emit_a64(p);std::ofstream f(std::string(argv[1])+"-a64.bin",std::ios::binary);f.write((char*)arm.data(),arm.size());}
 }
 for(int op=153;op<=164;++op){
  // Conditional returns on both paths, including signed overflow-edge comparisons.
  std::vector<uint8_t> code={26};if(op>=159)code.push_back(27);size_t bc=code.size();code.insert(code.end(),{uint8_t(op),0,5,4,172,5,172});
  std::string d="(II)I";Spec sp{code.data(),code.size(),&d,8,2,2,false,false,nullptr};Program p;assert(verify(sp,p));Executable e;assert(e.install(emit_host(p)));++programs;
  for(int i=0;i<2000;++i){std::vector<int32_t>a={int32_t(rng()),int32_t(rng())};assert(e.fn(a.data())==ref(p,a));++checked;}
  code[bc+1]=255;code[bc+2]=255;assert(!verify(sp,p));++rejected;
 }
 {std::vector<uint8_t>b={26,59,132,0,255,26,89,95,96,172};std::string d="(I)I";Spec s{b.data(),b.size(),&d,8,1,2,false,false,nullptr};Program p;assert(verify(s,p));Executable e;assert(e.install(emit_host(p)));for(int i=0;i<2000;++i){std::vector<int32_t>a={int32_t(rng())};assert(e.fn(a.data())==ref(p,a));++checked;}}
 {std::string d="(I)I";std::vector<uint8_t>b={26,172};Spec s{b.data(),b.size(),&d,8,1,1,false,false,nullptr};Program p;
  assert(verify(s,p));s.handlers=true;assert(!verify(s,p));s.handlers=false;s.flags=0;assert(!verify(s,p));s.flags=8;b={27,172};s.code=b.data();assert(!verify(s,p));b={26,108,172};s.code=b.data();s.size=b.size();assert(!verify(s,p));b={26,167,255,255};s.code=b.data();s.size=b.size();assert(!verify(s,p));rejected+=4;}
 {Engine e;assert(e.configure(true));Site site;std::string d="(I)I";std::vector<uint8_t>b={26,4,96,172};Spec s{b.data(),b.size(),&d,8,1,2,false,false,nullptr};for(unsigned i=0;i<=Engine::HotThreshold;++i)e.prepare(site,s);int32_t a=41,result=0;unsigned ops=0;assert(!e.call(site,&a,1,3,result,ops));assert(e.call(site,&a,1,4,result,ops)&&result==42&&ops==4);e.clear();assert(!e.call(site,&a,1,4,result,ops));e.configure(false);assert(!e.prepare(site,s));}
 std::cout<<"PASS jit236 native_execution="<<architecture()<<" programs="<<programs<<" differential_cases="<<checked<<" explicit_rejects="<<rejected<<" budget_epoch_wx=pass\n";
}
