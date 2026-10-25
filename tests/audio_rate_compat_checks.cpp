#include "../src/audio_engine.h"
#include <cassert>
#include <iostream>
using Bytes=std::vector<uint8_t>;
static void le16(Bytes& b,unsigned n){b.push_back(uint8_t(n));b.push_back(uint8_t(n>>8));}
static void le32(Bytes& b,unsigned n){le16(b,n);le16(b,n>>16);}
static Bytes wave(){Bytes b={'R','I','F','F'};le32(b,36+96000);b.insert(b.end(),{'W','A','V','E','f','m','t',' ',16,0,0,0});le16(b,1);le16(b,1);le32(b,48000);le32(b,96000);le16(b,2);le16(b,16);b.insert(b.end(),{'d','a','t','a'});le32(b,96000);for(int i=0;i<48000;++i)le16(b,unsigned((i%100)*100));return b;}
static Bytes midi(){return {'M','T','h','d',0,0,0,6,0,0,0,1,0,96,'M','T','r','k',0,0,0,12,0,0x90,60,100,96,0x80,60,0,0,0xff,47,0};}
static int load(j2me_audio::Engine& e,const Bytes& b){std::string error;int h=e.load(b,"",error);assert(h>0&&error.empty());return h;}
int main(){
 j2me_audio::Engine e;int h=load(e,wave());assert(e.rate(h)==100000);assert(e.rate(h,0)==50000);assert(e.rate(h,999999)==200000);
 assert(e.start(h));assert(e.endHeadroom()==24000);std::vector<int16_t> samples(9600);e.render(samples.data(),4800);assert(e.time(h)==200000);
 for(int i=0;i<40;++i)assert(std::abs(int(samples[size_t(i)*2])-(i*2%100)*100)<=1);
 assert(e.rate(h,50000)==50000);e.seek(h,0);e.render(samples.data(),4800);assert(e.time(h)==50000);
 for(int i=0;i<40;++i)assert(std::abs(int(samples[size_t(i)*2])-i*50)<=1);
 e.pause(true);e.render(nullptr,4800);assert(e.time(h)==50000);e.pause(false);e.stop(h);e.render(nullptr,4800);assert(e.time(h)==50000);
 e.start(h);e.rate(h,100000);e.render(nullptr,4800);assert(e.time(h)==150000);
 e.rate(h,50000);e.seek(h,0);e.render(nullptr,1);assert(e.endHeadroom()==95999);e.render(nullptr,1);assert(e.time(h)==20);
 auto saved=e.capture();e.reset();assert(e.restore(saved)==1&&e.rate(h)==100000);assert(e.rate(h,50000)==50000);e.render(nullptr,4800);assert(e.time(h)==50020);e.close(h);assert(e.rate(h)==-1&&e.rate(h,100000)==-1);
 h=load(e,midi());e.rate(h,200000);assert(e.loop(h,2));e.start(h);assert(e.endHeadroom()==12000);e.render(nullptr,12000);int ended=0;assert(e.state(h)==400&&!e.pollEnd(ended));e.render(nullptr,12000);assert(e.state(h)==300&&e.time(h)==500000&&e.pollEnd(ended)&&ended==h&&!e.pollEnd(ended));
 e.seek(h,0);e.loop(h,-1);e.start(h);e.rate(h,50000);e.render(nullptr,48000);assert(e.state(h)==400&&e.time(h)==500000&&!e.pollEnd(ended));e.close(h);
 Bytes tiny=wave();tiny.resize(46);tiny[4]=38;tiny[5]=tiny[6]=tiny[7]=0;tiny[40]=2;tiny[41]=tiny[42]=tiny[43]=0;
 h=load(e,tiny);e.rate(h,200000);e.loop(h,2);e.start(h);e.render(nullptr,1);assert(e.state(h)==300&&e.pollEnd(ended)&&ended==h&&!e.pollEnd(ended));e.close(h);
 std::cout<<"PASS real PCM/MIDI rates, sample positions, live change, bounds, headroom, loops, pause and restore default\n";
}
