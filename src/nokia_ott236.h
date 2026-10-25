#pragma once
// Original bounded OTT -> SMF adapter. Smart Messaging basic/temporary songs.
// Format cross-reference: FreeJ2ME-Plus NokiaOTTDecoder (GPL-3.0), not a source
// transplant. Reserved instructions and unbounded embedded loops are rejected.
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace nokia_ott236 {
struct Bits {
    const std::vector<uint8_t>& data; size_t at=0; bool valid=true;
    uint32_t take(unsigned n) {
        if(n>24 || at>data.size()*8 || n>data.size()*8-at){valid=false;return 0;}
        uint32_t v=0;while(n--){v=(v<<1)|((data[at/8]>>(7-at%8))&1);++at;}return v;
    }
};
struct Instruction {uint8_t op=0,a=0,b=0,c=0;};
inline void put32(std::vector<uint8_t>& b,uint32_t v){for(int shift=24;shift>=0;shift-=8)b.push_back(uint8_t(v>>shift));}
inline void vlq(std::vector<uint8_t>& b,uint32_t v){uint8_t tmp[5];unsigned n=0;tmp[n++]=v&127;while((v>>=7))tmp[n++]=uint8_t((v&127)|128);while(n)b.push_back(tmp[--n]);}
inline bool convert(const std::vector<uint8_t>& input,std::vector<uint8_t>& midi,std::string& error) {
    midi.clear();error.clear();auto fail=[&](const char* e){error=e;return false;};
    if(input.size()<7||input.size()>65536)return fail("Nokia OTT size limit or truncated header");
    Bits bits{input};
    if(bits.take(8)!=2||bits.take(8)!=0x4a||bits.take(7)!=0x1d)return fail("unsupported Nokia OTT command header");
    const unsigned song=bits.take(3);
    if(song==1){unsigned title=bits.take(4);for(unsigned i=0;i<title;++i)bits.take(8);}
    else if(song!=2)return fail("unsupported Nokia OTT song type");
    const unsigned patterns=bits.take(8);if(!patterns)return fail("empty Nokia OTT sequence");
    std::array<std::vector<Instruction>,4> definitions;
    std::vector<uint8_t> track{0,0xc0,80,0,0xb0,7,92};
    uint32_t tick=0,last=0;size_t events=0;int volume=92;
    auto event=[&](uint32_t at,uint8_t status,uint8_t a,uint8_t b){vlq(track,at-last);last=at;track.push_back(status);track.push_back(a);track.push_back(b);++events;};
    static const unsigned tempos[]={25,28,31,35,40,45,50,56,63,70,80,90,100,112,125,140,160,180,200,225,250,285,320,355,400,450,500,565,635,715,800,900};
    static const unsigned volumes[]={0,48,56,64,72,80,88,92,100,104,108,112,116,120,124,127};
    for(unsigned p=0;p<patterns;++p){
        if(bits.take(3)!=0)return fail("invalid Nokia OTT pattern header");
        unsigned id=bits.take(2),repeats=bits.take(4),count=bits.take(8);
        if(!bits.valid)return fail("truncated Nokia OTT pattern");
        if(repeats==15)return fail("Nokia OTT embedded infinite loop is not implemented");
        if(count){definitions[id].clear();for(unsigned i=0;i<count;++i){
            Instruction op;op.op=uint8_t(bits.take(3));
            if(op.op==1){op.a=uint8_t(bits.take(4));op.b=uint8_t(bits.take(3));op.c=uint8_t(bits.take(2));if(op.a>12||op.b>5)return fail("reserved Nokia OTT note or duration");}
            else if(op.op==2||op.op==3){op.a=uint8_t(bits.take(2));if(op.op==3&&op.a==3)return fail("reserved Nokia OTT articulation");}
            else if(op.op==4)op.a=uint8_t(bits.take(5));
            else if(op.op==5)op.a=uint8_t(bits.take(4));
            else return fail("unsupported Nokia OTT instruction");
            if(!bits.valid)return fail("truncated Nokia OTT instruction");definitions[id].push_back(op);
        }}else if(definitions[id].empty())return fail("undefined Nokia OTT pattern reference");
        for(unsigned repeat=0;repeat<=repeats;++repeat){int scale=1,style=0;
            for(const auto& op:definitions[id]){
                if(events>60000||tick>13824000u)return fail("Nokia OTT event or duration limit");
                if(op.op==1){uint32_t duration=3840u>>op.b;
                    if(op.c==1)duration=duration*3/2;else if(op.c==2)duration=duration*7/4;else if(op.c==3)duration=duration*2/3;
                    if(op.a){uint8_t note=uint8_t(60+scale*12+op.a-1);uint32_t sounding=style==1?duration:style==2?duration/2:duration*20/21;
                        event(tick,0x90,note,93);event(tick+sounding,0x80,note,0);}
                    tick+=duration;
                }else if(op.op==2)scale=op.a;
                else if(op.op==3)style=op.a;
                else if(op.op==4){uint32_t us=60000000/tempos[op.a];vlq(track,tick-last);last=tick;track.insert(track.end(),{0xff,0x51,3,uint8_t(us>>16),uint8_t(us>>8),uint8_t(us)});++events;}
                else if(op.op==5){volume=int(volumes[op.a]);event(tick,0xb0,7,uint8_t(volume));}
            }
        }
    }
    // A programming terminator and final byte padding contain only zero bits.
    while(bits.at<input.size()*8){if(bits.take(1)!=0)return fail("trailing Nokia OTT command is unsupported");}
    if(!bits.valid||tick==0)return fail("empty or truncated Nokia OTT song");
    vlq(track,tick-last);track.insert(track.end(),{0xff,0x2f,0});
    midi={'M','T','h','d',0,0,0,6,0,0,0,1,3,0xc0,'M','T','r','k'};
    put32(midi,uint32_t(track.size()));midi.insert(midi.end(),track.begin(),track.end());return true;
}
}
