#include "../src/audio_engine.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using Bytes=std::vector<uint8_t>;
static void be32(Bytes& b,uint32_t n){b.push_back(n>>24);b.push_back(n>>16);b.push_back(n>>8);b.push_back(n);}
static void le16(Bytes& b,uint16_t n){b.push_back(n);b.push_back(n>>8);}
static void le32(Bytes& b,uint32_t n){le16(b,uint16_t(n));le16(b,uint16_t(n>>16));}
static Bytes smf(const std::vector<Bytes>& tracks){Bytes b={'M','T','h','d',0,0,0,6,0,uint8_t(tracks.size()>1),0,uint8_t(tracks.size()),0,96};for(const auto& t:tracks){b.insert(b.end(),{'M','T','r','k'});be32(b,uint32_t(t.size()));b.insert(b.end(),t.begin(),t.end());}return b;}
static Bytes wave(int bits,int channels,int rate,const Bytes& pcm){Bytes b={'R','I','F','F'};le32(b,36+uint32_t(pcm.size())+uint32_t(pcm.size()%2));b.insert(b.end(),{'W','A','V','E','f','m','t',' ',16,0,0,0});le16(b,1);le16(b,uint16_t(channels));le32(b,uint32_t(rate));le32(b,uint32_t(rate*channels*bits/8));le16(b,uint16_t(channels*bits/8));le16(b,uint16_t(bits));b.insert(b.end(),{'d','a','t','a'});le32(b,uint32_t(pcm.size()));b.insert(b.end(),pcm.begin(),pcm.end());if(pcm.size()%2)b.push_back(0);return b;}
static long long energy(const std::vector<int16_t>& b,int c=-1){long long n=0;for(size_t i=0;i<b.size();++i)if(c<0||int(i%2)==c)n+=std::abs(int(b[i]));return n;}
int main(int argc,char** argv){
 j2me_audio::Engine engine;std::string error;std::vector<int16_t> pcm(4800*2);int ended=0;
 Bytes music=smf({{0,0xc0,40,0,0xb0,10,0,0,0x90,69,100,96,69,0,0,0xff,0x2f,0}}); // running-status note-off
 int h=engine.load(music,"audio/midi",error);assert(h>0&&error.empty());assert(engine.duration(h)==500000);assert(engine.state(h)==300);assert(engine.start(h));engine.render(pcm.data(),4800);assert(energy(pcm)>100000&&energy(pcm,1)==0);assert(engine.time(h)==100000);
 engine.pause(true);engine.render(pcm.data(),4800);assert(energy(pcm)==0&&engine.time(h)==100000);engine.pause(false);engine.render(pcm.data(),4800);assert(energy(pcm)>0&&engine.time(h)==200000);
 assert(engine.stop(h));engine.render(pcm.data(),4800);assert(energy(pcm)==0&&engine.time(h)==200000);assert(engine.start(h));assert(engine.seek(h,0)==0);engine.volume(h,0);engine.render(pcm.data(),4800);assert(energy(pcm)==0);engine.volume(h,100);assert(engine.seek(h,0)==0);engine.render(pcm.data(),4800);long long full=energy(pcm);engine.volume(h,50);engine.seek(h,0);engine.render(pcm.data(),4800);assert(energy(pcm)>full*49/100&&energy(pcm)<full*51/100);
 assert(engine.loop(h,2));engine.seek(h,0);engine.render(nullptr,24000);assert(engine.state(h)==400&&!engine.pollEnd(ended));engine.render(nullptr,24000);assert(engine.state(h)==300&&engine.pollEnd(ended)&&ended==h&&!engine.pollEnd(ended));
 assert(engine.loop(h,-1));engine.start(h);engine.render(nullptr,72000);assert(engine.state(h)==400&&!engine.pollEnd(ended));assert(engine.close(h)&&engine.state(h)==0);assert(!engine.start(h));
 Bytes tempo=smf({{0,0xff,0x51,3,7,0xa1,0x20,96,0xff,0x51,3,15,0x42,0x40,96,0xff,0x2f,0},{0,0x90,60,100,0x81,0x40,0x80,60,0,0,0xff,0x2f,0}});
 h=engine.load(tempo,"",error);assert(h>0&&engine.duration(h)==1500000);engine.close(h);
 Bytes sustain=smf({{0,0xb0,64,127,0,0x90,60,100,24,0x80,60,0,72,0xb0,64,0,96,0xff,0x2f,0}});h=engine.load(sustain,"",error);assert(h>0);engine.start(h);engine.seek(h,300000);engine.render(pcm.data(),4800);assert(energy(pcm)>0);engine.seek(h,700000);engine.render(pcm.data(),4800);assert(energy(pcm)==0);engine.close(h);
 for(size_t i=0;i<music.size();++i){Bytes truncated(music.begin(),music.begin()+i);assert(engine.load(truncated,"audio/midi",error)==0);}
 Bytes bad=music;bad[12]=0x80;assert(engine.load(bad,"",error)==0&&error.find("SMPTE")!=std::string::npos);bad=music;bad[13]=0;assert(engine.load(bad,"",error)==0);bad=smf({{0,0x90,60,200,96,0xff,0x2f,0}});assert(engine.load(bad,"",error)==0);bad=smf({{0,0x90,60,100,0x81,0x81,0x81,0x81,0x01,0xff,0x2f,0}});assert(engine.load(bad,"",error)==0);assert(engine.load(Bytes{'P','N','G'},"audio/midi",error)==0);
 Bytes wav8=wave(8,1,24000,Bytes(2400,192));h=engine.load(wav8,"audio/x-wav",error);assert(h>0&&engine.duration(h)==100000);engine.start(h);engine.render(pcm.data(),4800);assert(energy(pcm,0)==energy(pcm,1)&&pcm[0]>0&&engine.pollEnd(ended)&&ended==h);engine.close(h);
 Bytes raw16;for(int i=0;i<2400;++i){le16(raw16,16384);le16(raw16,uint16_t(-8192));}h=engine.load(wave(16,2,24000,raw16),"",error);assert(h>0);engine.start(h);engine.render(pcm.data(),4800);assert(pcm[0]>0&&pcm[1]<0&&energy(pcm,0)>energy(pcm,1));engine.close(h);
 for(size_t i=0;i<wav8.size();i+=137){Bytes truncated(wav8.begin(),wav8.begin()+i);assert(engine.load(truncated,"",error)==0);}
 h=engine.playTone(69,100,80);assert(h>0);engine.render(pcm.data(),4800);assert(energy(pcm)>0&&engine.pollEnd(ended)&&ended==h);engine.close(h);assert(!engine.playTone(128,100,100));assert(!engine.playTone(69,-1,100));
 // Controller attenuation, right pan, percussion and immediate All Sound Off.
 Bytes right=smf({{0,0xb0,10,127,0,0x90,72,110,96,0xff,0x2f,0}});h=engine.load(right,"",error);assert(h>0);engine.start(h);engine.render(pcm.data(),4800);assert(energy(pcm,0)==0&&energy(pcm,1)>0);engine.close(h);
 Bytes mute=smf({{0,0xb0,11,0,0,0x90,72,110,96,0xff,0x2f,0}});h=engine.load(mute,"",error);assert(h>0);engine.start(h);engine.render(pcm.data(),4800);assert(!energy(pcm));engine.close(h);
 Bytes drum=smf({{0,0x99,38,100,96,0xff,0x2f,0}});h=engine.load(drum,"",error);assert(h>0);engine.start(h);engine.render(pcm.data(),4800);assert(energy(pcm)>0);engine.close(h);
 Bytes kill=smf({{0,0x90,60,100,24,0xb0,120,0,72,0xff,0x2f,0}});h=engine.load(kill,"",error);assert(h>0);engine.start(h);engine.seek(h,200000);engine.render(pcm.data(),4800);assert(!energy(pcm));engine.close(h);
 // A deterministic bounded malformed-input mutation sweep exercises every parser.
 uint32_t rng=0x175;for(int caseNo=0;caseNo<2000;++caseNo){Bytes fuzz=(caseNo&1)?music:wav8;for(int j=0;j<1+caseNo%5;++j){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;size_t offset=rng%fuzz.size();fuzz[offset]^=uint8_t(rng>>24);}h=engine.load(fuzz,"",error);if(h>0){engine.start(h);engine.render(pcm.data(),4800);engine.close(h);}}
 std::vector<int> handles;for(size_t i=0;i<j2me_audio::Engine::MaxHandles;++i){h=engine.load(music,"",error);assert(h>0);handles.push_back(h);}assert(!engine.load(music,"",error));engine.reset();for(int handle:handles)assert(engine.state(handle)==0);h=engine.load(music,"",error);assert(h>handles.back());engine.close(h);
 int fixtures=0;for(int i=1;i<argc;++i){std::ifstream f(argv[i],std::ios::binary);Bytes b((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());h=engine.load(b,"",error);if(h<=0){std::cerr<<argv[i]<<": "<<error<<"\n";return 2;}engine.start(h);long long sum=0;for(int block=0;block<30;++block){engine.render(pcm.data(),4800);sum+=energy(pcm);}if(!sum){std::cerr<<argv[i]<<": silent first 3 seconds\n";return 3;}engine.close(h);++fixtures;}
 std::cout<<"audio engine checks passed; real magic MIDI fixtures="<<fixtures<<"\n";
}
