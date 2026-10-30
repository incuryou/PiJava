#ifndef J2ME_AUDIO_ENGINE_H
#define J2ME_AUDIO_ENGINE_H
#include "p7_worker.h"
// Original implementation. File semantics follow MIDI Association SMF 1.0 and
// Microsoft RIFF PCM. No third-party synthesizer, soundfont or source is embedded.
// 48 kHz stereo, main-thread only. GM programs are approximate harmonic timbres.
#include "nokia_ott236.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <deque>
#include <limits>
#include <string>
#include <vector>

namespace j2me_audio {
class Engine {
public:
    static constexpr int SampleRate = 48000;
    static constexpr int Closed = 0, Prefetched = 300, Started = 400;
    using Decoder=bool(*)(const std::vector<uint8_t>&,const std::string&,std::vector<int16_t>&,uint32_t&,int&,std::string&);
    static constexpr size_t MaxHandles = 64, MaxVoices = 64;
private:
    Decoder decoder_=nullptr;
    static constexpr size_t MaxBytes = 8 * 1024 * 1024, MaxTotalBytes = 32 * 1024 * 1024;
    static constexpr size_t MaxEvents = 100000;
    static constexpr uint64_t MaxFrames = uint64_t(SampleRate) * 7200;
    struct Event { uint64_t at = 0; uint32_t order = 0; uint32_t tempo = 0; uint8_t status = 0, a = 0, b = 0; };
    struct Channel { int program = 0, volume = 100, expression = 127, pan = 64, bend = 8192; bool sustain = false; double bendScale = 1.0, panLeft = 0.70431739445081454, panRight = 0.70988520753289097; };
    struct Voice { bool active = false, held = false, released = false; int channel = 0, note = 0, velocity = 0, program = 0; uint64_t begin = 0, release = 0; double phase = 0, envelope = 0; uint32_t noise = 1; };
    struct Player {
        int handle = 0, state = Prefetched, volume = 100, loops = 1, remaining = 1;
        int rate = 100000, rateFraction = 0;
        uint64_t frame = 0, frames = 0; size_t cursor = 0, bytes = 0;
        bool midi = false; uint32_t pcmRate = 0; int pcmChannels = 0;
        std::vector<int16_t> pcm; std::vector<Event> events;
        std::array<Channel,16> channels{}; std::array<Voice,32> voices{};
    };
    std::array<Player,MaxHandles> players_{};
    std::array<double,2048> sine_{}; std::array<double,128> frequencies_{};
    std::deque<int> ends_; int nextHandle_ = 1; size_t totalBytes_ = 0; bool paused_ = false;
    static uint16_t be16(const uint8_t* p) { return uint16_t(uint16_t(p[0]) << 8 | p[1]); }
    static uint32_t be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
    static uint16_t le16(const uint8_t* p) { return uint16_t(p[0] | uint16_t(p[1]) << 8); }
    static uint32_t le32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
    static bool vlq(const std::vector<uint8_t>& b, size_t& p, size_t end, uint32_t& v) {
        v=0; for(int i=0;i<4;++i) { if(p>=end)return false; uint8_t x=b[p++];v=(v<<7)|(x&127);if(!(x&128))return true; } return false;
    }
    static bool fail(std::string& e,const char* s) { e=s; return false; }
    static bool parseMidi(const std::vector<uint8_t>& b, Player& p, std::string& e) {
        if(b.size()<14)return fail(e,"truncated MIDI header");
        uint32_t h=be32(&b[4]); if(h<6||h>b.size()-8)return fail(e,"invalid MIDI header length");
        unsigned format=be16(&b[8]),tracks=be16(&b[10]),division=be16(&b[12]);
        if(format>1||!tracks||tracks>64||(format==0&&tracks!=1))return fail(e,"unsupported MIDI format or track count");
        if(division&0x8000)return fail(e,"SMPTE MIDI division is unsupported");
        if(!division)return fail(e,"zero MIDI division");
        size_t pos=8+h;uint32_t order=0;uint64_t finalTick=0;
        for(unsigned t=0;t<tracks;++t) {
            if(pos>b.size()||b.size()-pos<8||std::memcmp(&b[pos],"MTrk",4))return fail(e,"missing MIDI track");
            uint32_t len=be32(&b[pos+4]);pos+=8;if(len>b.size()-pos)return fail(e,"truncated MIDI track");
            size_t end=pos+len;uint64_t tick=0;uint8_t running=0;bool ended=false;
            while(pos<end) {
                if(p.events.size()>=MaxEvents||order>=MaxEvents*4)return fail(e,"MIDI event limit exceeded");
                uint32_t delta=0;if(!vlq(b,pos,end,delta))return fail(e,"invalid MIDI delta");
                tick+=delta;if(tick>uint64_t(division)*7200*1000)return fail(e,"MIDI tick limit exceeded");
                if(pos>=end)return fail(e,"truncated MIDI event");
                uint8_t status=b[pos];if(status&128) {++pos;if(status<0xf0)running=status;else running=0;} else {if(!running)return fail(e,"invalid MIDI running status");status=running;}
                Event event;event.at=tick;event.order=order++;
                if(status==0xff) {
                    if(pos>=end)return fail(e,"truncated MIDI meta event");
                    uint8_t kind=b[pos++];uint32_t n;
                    if(!vlq(b,pos,end,n)||n>end-pos)return fail(e,"invalid MIDI meta length");
                    if(kind==0x51) {if(n!=3)return fail(e,"invalid MIDI tempo length");event.tempo=uint32_t(b[pos])<<16|uint32_t(b[pos+1])<<8|b[pos+2];if(!event.tempo)return fail(e,"zero MIDI tempo");p.events.push_back(event);}
                    if(kind==0x2f) {if(n)return fail(e,"invalid MIDI end-of-track");ended=true;pos+=n;if(pos!=end)return fail(e,"bytes after MIDI end-of-track");break;}
                    pos+=n;
                } else if(status==0xf0||status==0xf7) {
                    uint32_t n;if(!vlq(b,pos,end,n)||n>end-pos)return fail(e,"invalid MIDI sysex length");pos+=n;
                } else if(status>=0x80&&status<0xf0) {
                    unsigned kind=status&0xf0;size_t n=(kind==0xc0||kind==0xd0)?1:2;
                    if(n>end-pos)return fail(e,"truncated MIDI channel event");
                    for(size_t i=0;i<n;++i)if(b[pos+i]&128)return fail(e,"invalid MIDI data byte");
                    event.status=status;event.a=b[pos++];if(n==2)event.b=b[pos++];p.events.push_back(event);
                } else return fail(e,"unsupported MIDI system event");
            }
            if(!ended)return fail(e,"MIDI track has no end-of-track");
            finalTick=std::max(finalTick,tick);pos=end;
        }
        if(pos!=b.size())return fail(e,"trailing MIDI chunks are unsupported");
        Event ending;ending.at=finalTick;ending.order=order++;p.events.push_back(ending);
        std::stable_sort(p.events.begin(),p.events.end(),[](const Event& a,const Event& b){return a.at<b.at||(a.at==b.at&&a.order<b.order);});
        uint64_t tick=0;uint32_t tempo=500000;long double elapsed=0;
        for(auto& ev:p.events) {elapsed+=static_cast<long double>(ev.at-tick)*tempo/division;tick=ev.at;long double frames=elapsed*SampleRate/1000000;
            if(frames>MaxFrames)return fail(e,"MIDI duration limit exceeded");
            ev.at=uint64_t(frames+0.5L);if(ev.tempo)tempo=ev.tempo;}
        p.frames=p.events.empty()?0:p.events.back().at;
        if(!p.frames)return fail(e,"empty MIDI duration");
        p.midi=true;return true;
    }
    static bool parseWav(const std::vector<uint8_t>& b, Player& p, std::string& e) {
        if(b.size()<12||std::memcmp(&b[8],"WAVE",4))return fail(e,"invalid WAV header");
        uint32_t riff=le32(&b[4]);if(riff<4||uint64_t(riff)+8>b.size())return fail(e,"truncated WAV RIFF");
        size_t end=size_t(riff)+8,pos=12,data=0,length=0;uint16_t channels=0,bits=0,align=0;uint32_t rate=0;bool fmt=false,hasData=false;
        while(pos<end) {
            if(end-pos<8)return fail(e,"truncated WAV chunk");
            uint32_t n=le32(&b[pos+4]);size_t payload=pos+8;if(n>end-payload)return fail(e,"truncated WAV chunk payload");
            if(!std::memcmp(&b[pos],"fmt ",4)) {if(fmt||n<16)return fail(e,"invalid WAV format chunk");if(le16(&b[payload])!=1)return fail(e,"only PCM WAV is supported");
                channels=le16(&b[payload+2]);rate=le32(&b[payload+4]);align=le16(&b[payload+12]);bits=le16(&b[payload+14]);
                if((channels!=1&&channels!=2)||(bits!=8&&bits!=16)||rate<1000||rate>192000||align!=channels*(bits/8)||le32(&b[payload+8])!=rate*align)return fail(e,"invalid WAV PCM parameters");
                fmt=true;}
            if(!std::memcmp(&b[pos],"data",4)) {if(hasData)return fail(e,"multiple WAV data chunks are unsupported");data=payload;length=n;hasData=true;}
            size_t padded=size_t(n)+(n&1);
            // Some shipped PCM8 assets omit only the final data-chunk pad byte.
            // Accept exactly that bounded RIFF-end case, never a truncated payload
            // or an unpadded intermediate/metadata chunk.
            if(padded>end-payload){
                if((n&1)&&size_t(n)==end-payload&&!std::memcmp(&b[pos],"data",4))padded=n;
                else return fail(e,"missing WAV chunk padding");
            }
            pos=payload+padded;
        }
        if(!fmt||!hasData||!length||length%align)return fail(e,"missing or incomplete WAV PCM data");
        p.pcm.resize(length/(bits/8));
        for(size_t i=0;i<p.pcm.size();++i) {if(bits==8)p.pcm[i]=int16_t((int(b[data+i])-128)*256);else {uint16_t v=le16(&b[data+i*2]);p.pcm[i]=int16_t(v<=32767?int(v):int(v)-65536);}}
        p.pcmRate=rate;p.pcmChannels=channels;p.frames=(uint64_t(length/align)*SampleRate+rate-1)/rate;
        if(p.frames>MaxFrames)return fail(e,"WAV duration limit exceeded");
        return true;
    }
    Player* player(int h) {for(auto& p:players_)if(p.handle==h&&h>0)return &p;return nullptr;}
    const Player* player(int h) const {for(const auto& p:players_)if(p.handle==h&&h>0)return &p;return nullptr;}
    void clearEnd(int h) {ends_.erase(std::remove(ends_.begin(),ends_.end(),h),ends_.end());}
    static void rewind(Player& p) {p.frame=0;p.rateFraction=0;p.cursor=0;p.channels={};p.voices={};}
    static void releaseVoice(Voice& v,uint64_t at) {if(!v.released){v.released=true;v.release=at;}v.held=false;}
    size_t activeVoices() const {size_t n=0;for(const auto& p:players_)if(p.state==Started)for(const auto& v:p.voices)n+=v.active;return n;}
    void admitVoices(Player& p) {
        const size_t room=MaxVoices-std::min(MaxVoices,activeVoices());
        size_t count=0;for(const auto& v:p.voices)count+=v.active;
        // Keep stopped note state and position; on restart/restore admit only
        // notes that fit, without stealing from another running player.
        while(count>room){
            Voice* oldest=nullptr;
            for(auto& v:p.voices)if(v.active&&(!oldest||v.begin<oldest->begin))oldest=&v;
            if(!oldest)break;
            oldest->active=false;--count;
        }
    }
    void dispatch(Player& p,const Event& ev) {
        if(!ev.status)return;
        int c=ev.status&15,kind=ev.status&0xf0;auto& ch=p.channels[size_t(c)];
        if(kind==0xc0)ch.program=ev.a;
        else if(kind==0xe0){ch.bend=ev.a|(int(ev.b)<<7);ch.bendScale=std::pow(2.0,(ch.bend-8192)/8192.0/6.0);}
        else if(kind==0xb0) {
            if(ev.a==7)ch.volume=ev.b;else if(ev.a==11)ch.expression=ev.b;else if(ev.a==10){ch.pan=ev.b;ch.panLeft=std::sqrt(1-ev.b/127.0);ch.panRight=std::sqrt(ev.b/127.0);}
            else if(ev.a==64){bool held=ch.sustain;ch.sustain=ev.b>=64;if(held&&!ch.sustain)for(auto& v:p.voices)if(v.active&&v.channel==c&&v.held)releaseVoice(v,ev.at);}
            else if(ev.a==120){for(auto& v:p.voices)if(v.channel==c)v.active=false;}
            else if(ev.a==123){for(auto& v:p.voices)if(v.active&&v.channel==c){if(ch.sustain)v.held=true;else releaseVoice(v,ev.at);}}
            else if(ev.a==121){ch.volume=100;ch.expression=127;ch.pan=64;ch.bend=8192;ch.bendScale=1;ch.panLeft=0.70431739445081454;ch.panRight=0.70988520753289097;ch.sustain=false;for(auto& v:p.voices)if(v.active&&v.channel==c&&v.held)releaseVoice(v,ev.at);}
        } else if(kind==0x80||(kind==0x90&&!ev.b)) {
            for(auto& v:p.voices)if(v.active&&v.channel==c&&v.note==ev.a&&!v.released&&!v.held){if(ch.sustain)v.held=true;else releaseVoice(v,ev.at);break;}
        } else if(kind==0x90) {
            for(auto& v:p.voices)if(v.active&&((v.released&&ev.at>=v.release&&ev.at-v.release>=3840)||(v.channel==9&&ev.at>=v.begin&&ev.at-v.begin>SampleRate)))v.active=false;
            Voice* chosen=nullptr;for(auto& v:p.voices)if(!v.active){chosen=&v;break;}
            if(!chosen||(p.state==Started&&activeVoices()>=MaxVoices)) {
                chosen=nullptr;for(auto& v:p.voices)if(v.active&&(!chosen||v.begin<chosen->begin))chosen=&v;
                if(!chosen)return; // Global polyphony is full; never exceed its bound.
            }
            *chosen=Voice{};chosen->active=true;chosen->channel=c;chosen->note=ev.a;chosen->velocity=ev.b;chosen->program=ch.program;chosen->begin=ev.at;chosen->noise=uint32_t(ev.at)^uint32_t(ev.a*31337+1);
        }
    }
    double sine(double phase) const {return sine_[size_t(phase*2048)&2047];}
    double voiceSample(Player& p,Voice& v) {
        const auto& ch=p.channels[size_t(v.channel)];double age=double(p.frame-v.begin)/SampleRate;
        bool drum=v.channel==9;double attack=std::min(1.0,age*200.0);double sustain=0.65;double decay=std::exp(-age*(drum?15.0:(v.program<16?2.0:0.25)));
        double env=attack*(sustain+(1-sustain)*decay);if(v.program<16)env*=std::exp(-age*0.8);
        if(drum)env=attack*decay;
        if(v.released) {double r=double(p.frame-v.release)/SampleRate;if(r>=0.08){v.active=false;return 0;}env*=1-r/0.08;}
        if(drum&&age>1){v.active=false;return 0;}
        double freq=frequencies_[size_t(v.note)]*ch.bendScale;
        double wave=0;
        if(drum) {v.noise^=v.noise<<13;v.noise^=v.noise>>17;v.noise^=v.noise<<5;double noise=double(int32_t(v.noise))/2147483648.0;
            if(v.note==35||v.note==36){freq=55+80*std::exp(-age*30);wave=sine(v.phase);}
            else if(v.note>=41&&v.note<=50&&v.note!=42&&v.note!=44&&v.note!=46){freq=80+(v.note-41)*12;wave=0.8*sine(v.phase)+0.2*noise;}
            else {wave=(v.note==38||v.note==40)?0.75*noise+0.25*sine(v.phase):noise;}
        } else {
            int family=v.program/8;double fundamental=sine(v.phase),h2=sine(v.phase*2),h3=sine(v.phase*3),h4=sine(v.phase*4);
            switch(family) {case 0:wave=.65*fundamental+.23*h2+.12*h3;break;case 1:wave=.8*fundamental+.2*h4;break;case 2:wave=.55*fundamental+.25*h2+.2*h4;break;
                case 3:case 4:wave=.65*fundamental+.2*h2+.15*h3;break;case 5:case 6:case 7:wave=.6*fundamental+.25*h2+.15*h3;break;
                case 8:wave=.7*fundamental+.3*h3;break;case 9:wave=fundamental;break;case 10:wave=.55*fundamental+.3*h2+.15*h4;break;default:wave=.8*fundamental+.2*h2;break;}
        }
        v.phase+=freq/SampleRate;v.phase-=std::floor(v.phase);v.envelope=env;
        return wave*env*(v.velocity/127.0)*(ch.volume/127.0)*(ch.expression/127.0)*0.18;
    }
    void seekInternal(Player& p,uint64_t at) {
        rewind(p);p.frame=std::min(at,p.frames);
        if(p.midi) {while(p.cursor<p.events.size()&&p.events[p.cursor].at<=p.frame)dispatch(p,p.events[p.cursor++]);
            for(auto& v:p.voices)if(v.active){double age=double(p.frame-v.begin)/SampleRate;v.phase=std::fmod(age*frequencies_[size_t(v.note)],1.0);if((v.released&&p.frame-v.release>=3840)||(v.channel==9&&age>1))v.active=false;}}
    }
    static int16_t quantize(double x) {x=std::max(-1.0,std::min(1.0,x));return int16_t(std::lrint(x*32767));}
public:
    Engine() {constexpr double pi=3.14159265358979323846;for(size_t i=0;i<sine_.size();++i)sine_[i]=std::sin(2*pi*i/sine_.size());for(size_t i=0;i<frequencies_.size();++i)frequencies_[i]=440*std::pow(2.0,(double(i)-69)/12);}
    void setDecoder(Decoder decoder){decoder_=decoder;}
    int load(const std::vector<uint8_t>& bytes,const std::string& type,std::string& error) {
        error.clear();if(bytes.size()>MaxBytes){error="audio resource exceeds 8 MiB";return 0;}Player* slot=nullptr;for(auto& p:players_)if(!p.handle){slot=&p;break;}
        if(!slot){error="audio player limit exceeded";return 0;}Player parsed;
        bool ok=false;if(type=="audio/x-nokia-ota"){
            std::vector<uint8_t> smf;ok=nokia_ott236::convert(bytes,smf,error)&&parseMidi(smf,parsed,error);
        }else if(bytes.size()>=4&&!std::memcmp(bytes.data(),"MThd",4))ok=parseMidi(bytes,parsed,error);
        else if(bytes.size()>=12&&!std::memcmp(bytes.data(),"RIFF",4)&&!std::memcmp(&bytes[8],"WAVE",4))ok=parseWav(bytes,parsed,error);
        else if(decoder_){ok=decoder_(bytes,type,parsed.pcm,parsed.pcmRate,parsed.pcmChannels,error);
            if(ok&&(parsed.pcmChannels==1||parsed.pcmChannels==2)&&parsed.pcmRate>=1000&&parsed.pcmRate<=192000&&parsed.pcm.size()%size_t(parsed.pcmChannels)==0)
                parsed.frames=(uint64_t(parsed.pcm.size()/size_t(parsed.pcmChannels))*SampleRate+parsed.pcmRate-1)/parsed.pcmRate;
            else ok=false;
            if(!parsed.frames||parsed.frames>MaxFrames)ok=false;
        } else {error="unsupported audio magic ("+type+")";return 0;}
        if(!ok&&decoder_&&error=="only PCM WAV is supported"){
            parsed=Player{};ok=decoder_(bytes,type,parsed.pcm,parsed.pcmRate,parsed.pcmChannels,error);
            if(ok&&(parsed.pcmChannels==1||parsed.pcmChannels==2)&&parsed.pcmRate>=1000&&parsed.pcmRate<=192000&&parsed.pcm.size()%size_t(parsed.pcmChannels)==0)
                parsed.frames=(uint64_t(parsed.pcm.size()/size_t(parsed.pcmChannels))*SampleRate+parsed.pcmRate-1)/parsed.pcmRate;
            else ok=false;
            if(!parsed.frames||parsed.frames>MaxFrames)ok=false;
        }
        if(!ok)return 0;
        parsed.bytes=parsed.pcm.capacity()*sizeof(int16_t)+parsed.events.capacity()*sizeof(Event);
        if(parsed.bytes>MaxTotalBytes-totalBytes_){error="audio memory budget exceeded";return 0;}
        if(nextHandle_==std::numeric_limits<int>::max()){error="audio handle space exhausted";return 0;}
        parsed.handle=nextHandle_++;totalBytes_+=parsed.bytes;*slot=std::move(parsed);return slot->handle;
    }
    bool start(int h) {auto* p=player(h);if(!p)return false;if(p->state==Started)return true;if(p->frame>=p->frames)rewind(*p);admitVoices(*p);p->remaining=p->loops;p->state=Started;clearEnd(h);return true;}
    bool stop(int h) {auto* p=player(h);if(!p)return false;p->state=Prefetched;return true;}
    bool close(int h) {auto* p=player(h);if(!p)return false;totalBytes_-=p->bytes;*p=Player{};clearEnd(h);return true;}
    int64_t seek(int h,int64_t us) {auto* p=player(h);if(!p)return -1;uint64_t f=us<=0?0:uint64_t(us/1000000)*SampleRate+uint64_t(us%1000000)*SampleRate/1000000;seekInternal(*p,std::min(f,p->frames));clearEnd(h);return time(h);}
    int64_t time(int h) const {auto* p=player(h);return p?int64_t(p->frame*1000000/SampleRate):-1;}
    int64_t duration(int h) const {auto* p=player(h);return p?int64_t(p->frames*1000000/SampleRate):-1;}
    int volume(int h,int level) {auto* p=player(h);if(!p)return -1;p->volume=std::max(0,std::min(100,level));return p->volume;}
    int volume(int h) const {auto* p=player(h);return p?p->volume:-1;}
    int rate(int h,int value) {auto* p=player(h);if(!p)return -1;p->rate=std::max(50000,std::min(200000,value));return p->rate;}
    int rate(int h) const {auto* p=player(h);return p?p->rate:-1;}
    bool loop(int h,int count) {auto* p=player(h);if(!p||count==0||count< -1)return false;p->loops=count;p->remaining=count;return true;}
    int state(int h) const {auto* p=player(h);return p?p->state:Closed;}
    void pause(bool value) {paused_=value;}
    size_t loadedPlayers() const {size_t n=0;for(const auto& p:players_)if(p.handle)++n;return n;}
    size_t activePlayers() const {size_t n=0;for(const auto& p:players_)if(p.handle && p.state==Started)++n;return n;}
    bool paused() const {return paused_;}
    bool pendingEnds() const {return !ends_.empty();}
    // P7.3 (read-only helper): render(n) can append to ends_ only if n >= this value. Conservative: every started
    // non-infinite player counts from its current frame to its end, ignoring extra loops.
    uint64_t endHeadroom() const {
        if(paused_)return std::numeric_limits<uint64_t>::max();
        uint64_t h=std::numeric_limits<uint64_t>::max();
        for(const auto& p:players_)if(p.handle&&p.state==Started&&p.remaining!=-1) {
            uint64_t left=p.frame<p.frames?(p.frames-p.frame)*100000-uint64_t(p.rateFraction):0;
            h=std::min<uint64_t>(h,(left+uint64_t(p.rate)-1)/uint64_t(p.rate));
        }
        return h;
    }
    // P7.3-r2 (read-only): render() would only zero-fill and change no state.
    bool idle() const {if(paused_)return true;for(const auto& p:players_)if(p.handle&&p.state==Started)return false;return true;}
    bool pollEnd(int& h) {if(ends_.empty())return false;h=ends_.front();ends_.pop_front();return true;}
    void reset() {for(auto& p:players_)p=Player{};totalBytes_=0;ends_.clear();paused_=false;}
    int playTone(int note,int durationMs,int level) {
        if(note<0||note>127||durationMs<1||durationMs>60000)return 0;
        Player* slot=nullptr;for(auto& p:players_)if(!p.handle){slot=&p;break;}if(!slot||nextHandle_==std::numeric_limits<int>::max())return 0;
        Player p;p.handle=nextHandle_++;p.midi=true;p.frames=uint64_t(durationMs)*SampleRate/1000;p.volume=std::max(0,std::min(100,level));
        Event on;on.status=0x90;on.a=uint8_t(note);on.b=100;p.events.push_back(on);Event off;off.at=p.frames;off.status=0x80;off.a=uint8_t(note);p.events.push_back(off);p.bytes=p.events.size()*sizeof(Event);if(p.bytes>MaxTotalBytes-totalBytes_)return 0;totalBytes_+=p.bytes;*slot=std::move(p);start(slot->handle);return slot->handle;
    }
    // Instant save keeps the decoded player, not the Java byte[] (that is cleared
    // after realize). Handles stay the same so NativePlayer.handle still matches.
    struct SavedEvent { uint64_t at = 0; uint32_t order = 0, tempo = 0; uint8_t status = 0, a = 0, b = 0; };
    struct SavedPlayer {
        int handle = 0, state = Prefetched, volume = 100, loops = 1, remaining = 1;
        uint64_t frame = 0; bool midi = false; uint32_t pcmRate = 0; int pcmChannels = 0;
        std::vector<int16_t> pcm; std::vector<SavedEvent> events;
    };
    std::vector<SavedPlayer> capture() const {
        std::vector<SavedPlayer> out;
        for (const auto& p : players_) {
            if (!p.handle) continue;
            SavedPlayer s;
            s.handle = p.handle; s.state = p.state; s.volume = p.volume; s.loops = p.loops; s.remaining = p.remaining;
            s.frame = p.frame; s.midi = p.midi; s.pcmRate = p.pcmRate; s.pcmChannels = p.pcmChannels; s.pcm = p.pcm;
            s.events.reserve(p.events.size());
            for (const auto& e : p.events) s.events.push_back({e.at, e.order, e.tempo, e.status, e.a, e.b});
            out.push_back(std::move(s));
        }
        return out;
    }
    int restore(const std::vector<SavedPlayer>& saved) {
        int restored = 0;
        for (const auto& s : saved) {
            if (s.handle <= 0 || s.events.size() > MaxEvents) continue;
            Player* slot = nullptr;
            for (auto& p : players_) if (!p.handle) { slot = &p; break; }
            if (!slot) break;
            Player p;
            p.handle = s.handle; p.volume = std::max(0, std::min(100, s.volume));
            p.loops = s.loops == 0 ? 1 : s.loops; p.remaining = s.remaining;
            p.midi = s.midi; p.pcm = s.pcm; p.pcmRate = s.pcmRate; p.pcmChannels = s.pcmChannels;
            p.events.reserve(s.events.size());
            for (const auto& e : s.events) { Event ev; ev.at = e.at; ev.order = e.order; ev.tempo = e.tempo; ev.status = e.status; ev.a = e.a; ev.b = e.b; p.events.push_back(ev); }
            if (p.midi) p.frames = p.events.empty() ? 0 : p.events.back().at;
            else if ((p.pcmChannels == 1 || p.pcmChannels == 2) && p.pcmRate >= 1000 && !p.pcm.empty() && p.pcm.size() % size_t(p.pcmChannels) == 0) {
                const size_t count = p.pcm.size() / size_t(p.pcmChannels);
                p.frames = (uint64_t(count) * SampleRate + p.pcmRate - 1) / p.pcmRate;
            }
            if (!p.frames || p.frames > MaxFrames) continue;
            p.bytes = p.pcm.size() * sizeof(int16_t) + p.events.size() * sizeof(Event);
            if (p.bytes > MaxTotalBytes - totalBytes_) continue;
            seekInternal(p, std::min(s.frame, p.frames));
            if(s.state==Started)admitVoices(p);
            p.state = s.state == Started ? Started : Prefetched;
            p.volume = std::max(0, std::min(100, s.volume));
            p.loops = s.loops == 0 ? 1 : s.loops; p.remaining = s.remaining;
            totalBytes_ += p.bytes;
            if (p.handle >= nextHandle_) nextHandle_ = p.handle + 1;
            *slot = std::move(p);
            ++restored;
        }
        paused_ = false;
        return restored;
    }
    // Destination always has 2*frames samples. Null destination advances silently.
    void render(int16_t* stereo,size_t frames) {
        P7_SCOPE(AudioMix);
        if(frames>std::numeric_limits<size_t>::max()/2)return;
        if(stereo)std::fill(stereo,stereo+frames*2,int16_t(0));
        if(paused_)return;
        std::array<Player*,MaxHandles> active{};size_t activeCount=0;
        for(auto& p:players_)if(p.handle&&p.state==Started)active[activeCount++]=&p;
        if(!activeCount)return;
        for(size_t f=0;f<frames;++f) {
            double left=0,right=0;
            for(size_t a=0;a<activeCount;++a) {
                Player& p=*active[a];if(p.state!=Started)continue;
                while(p.frame>=p.frames) {if(p.remaining==-1||p.remaining>1){if(p.remaining>1)--p.remaining;uint64_t extra=p.frame-p.frames;int fraction=p.rateFraction;rewind(p);p.frame=extra;p.rateFraction=fraction;}else {p.frame=p.frames;p.rateFraction=0;p.state=Prefetched;p.voices={};if(ends_.size()<64)ends_.push_back(p.handle);break;}}
                if(p.state!=Started)continue;
                double l=0,r=0;
                if(p.midi) {while(p.cursor<p.events.size()&&p.events[p.cursor].at<=p.frame)dispatch(p,p.events[p.cursor++]);
                    for(auto& v:p.voices)if(v.active) {double sample=voiceSample(p,v);const auto& channel=p.channels[size_t(v.channel)];l+=sample*channel.panLeft;r+=sample*channel.panRight;}}
                else {uint64_t pos=p.frame*p.pcmRate;size_t index=size_t(pos/SampleRate);double fraction=double(pos%SampleRate)/SampleRate;
                    if(p.rateFraction){fraction+=double(p.rateFraction)*p.pcmRate/(100000.0*SampleRate);index+=size_t(fraction);fraction-=size_t(fraction);}
                    size_t count=p.pcm.size()/size_t(p.pcmChannels);size_t next=std::min(index+1,count-1);index=std::min(index,count-1);
                    l=(p.pcm[index*size_t(p.pcmChannels)]*(1-fraction)+p.pcm[next*size_t(p.pcmChannels)]*fraction)/32768.0;
                    if(p.pcmChannels==2)r=(p.pcm[index*2+1]*(1-fraction)+p.pcm[next*2+1]*fraction)/32768.0;else r=l;}
                left+=l*(p.volume/100.0);right+=r*(p.volume/100.0);
                p.rateFraction+=p.rate;p.frame+=uint64_t(p.rateFraction/100000);p.rateFraction%=100000;
                // A >1x step may consume the last frame of more than one short loop.
                while(p.frame>p.frames && (p.remaining==-1 || p.remaining>1)) {
                    if(p.remaining>1)--p.remaining;
                    uint64_t extra=p.frame-p.frames;int fraction=p.rateFraction;
                    rewind(p);p.frame=extra;p.rateFraction=fraction;
                }
                if(p.frame>=p.frames&&p.remaining==1){p.frame=p.frames;p.rateFraction=0;p.state=Prefetched;p.voices={};if(ends_.size()<64)ends_.push_back(p.handle);}
            }
            if(stereo){stereo[f*2]=quantize(left);stereo[f*2+1]=quantize(right);}
        }
    }
};
} // namespace j2me_audio
#endif
