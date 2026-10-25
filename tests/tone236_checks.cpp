#include "../src/nokia_ott236.h"
#include "../src/audio_engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>
static std::vector<uint8_t> read(const char* p){std::ifstream f(p,std::ios::binary);return {(std::istreambuf_iterator<char>(f)),{}};}
struct Writer {std::vector<uint8_t> bytes;size_t at=0;void add(unsigned v,int n){while(n--){if(at%8==0)bytes.push_back(0);bytes.back()|=((v>>n)&1)<<(7-at%8);at++;}}};
static std::vector<uint8_t> simple(unsigned loop=0){Writer w;w.add(2,8);w.add(0x4a,8);w.add(0x1d,7);w.add(2,3);w.add(1,8);w.add(0,3);w.add(0,2);w.add(loop,4);w.add(1,8);w.add(1,3);w.add(1,4);w.add(2,3);w.add(0,2);w.add(0,8);return w.bytes;}
int main(int argc,char**argv){
 std::string err;std::vector<uint8_t> midi;
 auto one=simple();assert(nokia_ott236::convert(one,midi,err));j2me_audio::Engine e;int h=e.load(one,"audio/x-nokia-ota",err);assert(h&&e.duration(h)==500000);assert(e.start(h));
 int16_t a[2048],b[2048];e.render(a,1024);auto state=e.capture();j2me_audio::Engine r;assert(r.restore(state)==1);e.render(a,1024);r.render(b,1024);assert(!std::memcmp(a,b,sizeof(a)));e.close(h);
 h=e.load(simple(2),"audio/x-nokia-ota",err);assert(h&&e.duration(h)==1500000);e.close(h);
 assert(!nokia_ott236::convert(simple(15),midi,err));
 for(size_t i=0;i<7;i++){auto cut=one;cut.resize(i);assert(!nokia_ott236::convert(cut,midi,err));}
 std::mt19937 gen(236);for(unsigned i=0;i<20000;i++){auto mutated=one;mutated[gen()%mutated.size()]^=uint8_t(gen());nokia_ott236::convert(mutated,midi,err);}
 for(int i=1;i<argc;i++){auto data=read(argv[i]);h=e.load(data,"audio/x-nokia-ota",err);if(!h){std::cerr<<"REJECT "<<argv[i]<<" "<<err<<"\n";return 1;}assert(e.start(h));e.render(a,1024);std::cout<<"PASS actual_tone "<<argv[i]<<" duration_us="<<e.duration(h)<<"\n";e.close(h);}
 std::cout<<"PASS tone236 known duration, finite repeat, infinite rejected, snapshot PCM, malformed 20000\n";
}
