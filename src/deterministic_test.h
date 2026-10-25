#pragma once
// Opt-in R4 reference execution (desktop builds only since 0.2.107). This owner contains no Java handles or roots.
#include "suite_identity.h"
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <sys/stat.h>
namespace r4 {
struct Atom { std::string value; bool quoted=false; };
using Object=std::map<std::string,Atom>;
// Deliberately limited flat JSON schema, with duplicate keys and integer
// overflow rejected. Paths are relative basenames; no JSON coercion.
class Parser {
    const std::string& s; size_t p=0;
    void ws(){while(p<s.size()&&(s[p]==' '||s[p]=='\n'||s[p]=='\r'||s[p]=='\t'))++p;}
    bool character(char c){ws();if(p>=s.size()||s[p]!=c)return false;++p;return true;}
    bool string(std::string& out){
        if(!character('"'))return false;
        while(p<s.size()){
            unsigned char c=s[p++];if(c=='"')return true;
            if(c<32||c>=127)return false;
            if(c=='\\'){if(p==s.size())return false;c=s[p++];if(c!='"'&&c!='\\'&&c!='/')return false;}
            out+=char(c);
        }return false;
    }
public:
    explicit Parser(const std::string& text):s(text){}
    bool parse(Object& out){
        if(!character('{'))return false;ws();if(character('}'))return p==s.size();
        for(;;){
            std::string key;Atom a;if(!string(key)||out.count(key)||!character(':'))return false;ws();
            if(p<s.size()&&s[p]=='"'){a.quoted=true;if(!string(a.value))return false;}
            else {size_t start=p;if(p<s.size()&&s[p]=='-')++p;size_t first=p;while(p<s.size()&&s[p]>='0'&&s[p]<='9')++p;
                if(first==p|| (s[first]=='0'&&p-first>1))return false;a.value=s.substr(start,p-start);}
            out.emplace(key,a);ws();if(character('}'))break;if(!character(','))return false;
        }ws();return p==s.size();
    }
};
inline bool number(const Object& o,const char* name,int64_t& n){
    auto i=o.find(name);if(i==o.end()||i->second.quoted)return false;
    const auto& v=i->second.value;bool negative=!v.empty()&&v[0]=='-';uint64_t u=0;
    for(size_t j=negative?1:0;j<v.size();++j){unsigned d=unsigned(v[j]-'0');if(d>9||u>(uint64_t(INT64_MAX)-d)/10)return false;u=u*10+d;}
    n=negative?-int64_t(u):int64_t(u);return true;
}
inline bool named(const Object& o,const char* name,const char* value){auto i=o.find(name);return i!=o.end()&&i->second.quoted&&i->second.value==value;}
inline bool basename(const std::string& s){return !s.empty()&&s!="."&&s!=".."&&s.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos;}
inline bool read(const std::string& path,std::string& s,size_t max){
    std::ifstream f(path,std::ios::binary);if(!f)return false;char b[4096];
    while(f){f.read(b,sizeof(b));s.append(b,size_t(f.gcount()));if(s.size()>max)return false;}
    return f.eof();
}
struct Config {
    int64_t step_us=1000,steps=1000,epoch_ms=946684800000,fuel_per_tick=100000,capture_hz=60;
    std::string profile="reference-v1",seed="0000000000000000",tape,output,payload="hash-only";
};
struct Event {int64_t tick=0,seq=0;int key=0;bool release=false;};
inline bool key_allowed(int64_t k){return (k>=-8&&k<=-1)||(k>=8&&k<=12)||(k>=48&&k<=57)||k==35||k==42;}
inline bool config(const Object& o,Config& c){
    const std::set<std::string> allowed={"schema","mode","step_us","steps","epoch_ms","fuel_per_tick","capture_hz","fixture_seed_hex","execution_profile","rms_policy","payload_policy","snapshot_policy","input_tape","output_dir"};
    if(o.size()!=allowed.size())return false;for(const auto& e:o)if(!allowed.count(e.first))return false;
    if(!named(o,"schema","J2ME_TEST_RUN_1")||!named(o,"mode","deterministic")||!named(o,"rms_policy","fresh-isolated")||(!named(o,"payload_policy","hash-only")&&!named(o,"payload_policy","owned-fixtures-only"))||!named(o,"snapshot_policy","reject-before-io"))return false;
    c.payload=o.at("payload_policy").value;
    if(!number(o,"step_us",c.step_us)||!number(o,"steps",c.steps)||!number(o,"epoch_ms",c.epoch_ms)||!number(o,"fuel_per_tick",c.fuel_per_tick)||!number(o,"capture_hz",c.capture_hz))return false;
    for(auto key:{"execution_profile","fixture_seed_hex","input_tape","output_dir"})if(!o.at(key).quoted)return false;
    c.profile=o.at("execution_profile").value;c.seed=o.at("fixture_seed_hex").value;c.tape=o.at("input_tape").value;c.output=o.at("output_dir").value;
    return c.step_us>=1&&c.step_us<=20000&&c.steps>=1&&c.steps<=1000000&&c.epoch_ms>=0&&c.epoch_ms<=(INT64_MAX-c.steps*c.step_us)/1000&&c.fuel_per_tick>=1024&&c.fuel_per_tick<=10000000&&c.capture_hz>=1&&c.capture_hz<=240&&c.step_us*c.capture_hz<=1000000&&c.seed.size()==16&&c.seed.find_first_not_of("0123456789abcdef")==std::string::npos&&basename(c.tape)&&basename(c.output)&&(c.profile=="reference-v1"||c.profile=="optimized-v1");
}
inline bool events(const std::string& text,const Config& c,std::vector<Event>& out){
    std::set<int> held;size_t p=0;int64_t tick=-1,seq=-1;
    while(p<text.size()){
        size_t end=text.find('\n',p);if(end==std::string::npos)end=text.size();std::string line=text.substr(p,end-p);p=end+1;
        if(line.empty())return false;Object o;if(!Parser(line).parse(o)||o.size()!=4)return false;
        int64_t t,q,k;if(!number(o,"tick",t)||!number(o,"seq",q)||!number(o,"key",k)||!key_allowed(k)||t<0||t>=c.steps||q<0||t<tick||(t==tick&&q<=seq))return false;
        bool release=named(o,"action","up");if(!release&&!named(o,"action","down"))return false;
        if(release){if(!held.erase(int(k)))return false;}else if(!held.insert(int(k)).second)return false;
        out.push_back({t,q,int(k),release});tick=t;seq=q;
        if(out.size()>100000)return false;
    }return held.empty();
}
struct Runtime {
#ifdef __SWITCH__
    // 0.2.107: the R4 test mode is desktop-only. Switch never reads test-profile.json and
    // every r4::state.active branch folds to the normal production path at compile time.
    static constexpr bool active=false;
    bool paused=false,finished=false,worker_phase=false;
#else
    bool active=false,paused=false,finished=false,worker_phase=false;
#endif
    Config cfg;std::vector<Event> tape;size_t cursor=0;
    int64_t tick=0,remaining=0,worker_left=0;uint64_t opcodes=0,audio_frames=0,captures=0,paints=0;
    uint64_t fast_ops=0,jit_ops=0,uncertified_fallbacks=0,certified_shortcuts=0;
    std::string dir,profile_sha,tape_sha;FILE* journal=nullptr;FILE* result=nullptr;FILE* pcm=nullptr;
    rms_suite_identity_detail::Sha256 pcm_hash;
    int64_t us()const{return tick*cfg.step_us;}
    int64_t wall_ms()const{return cfg.epoch_ms+us()/1000;}
    void begin_tick(){remaining=cfg.fuel_per_tick;worker_left=cfg.fuel_per_tick/2;worker_phase=false;}
    int available(bool resumable,int quantum)const{return int(std::min<int64_t>(quantum,worker_phase&&resumable?std::min(remaining,worker_left):remaining));}
    void charge(unsigned n,bool resumable){remaining-=n;opcodes+=n;if(worker_phase&&resumable)worker_left-=n;}
    bool write(const std::string& line){return journal&&std::fwrite(line.data(),1,line.size(),journal)==line.size()&&std::fflush(journal)==0;}
    void close(){if(journal){std::fclose(journal);journal=nullptr;}if(result){std::fclose(result);result=nullptr;}if(pcm){std::fclose(pcm);pcm=nullptr;}}
};
inline Runtime state;
inline std::string hex(rms_suite_identity_detail::Sha256 hash){return hash.finish();}
inline bool mkdir_new(const std::string& path){
#ifdef _WIN32
    return ::mkdir(path.c_str())==0;
#else
    return ::mkdir(path.c_str(),0755)==0;
#endif
}
#ifndef __SWITCH__
inline bool load(const std::string& path,std::string& error){
    if(state.active){error="profile already frozen";return false;}
    std::string raw,ev;Object o;Config c;
    if(!read(path,raw,16384)||!Parser(raw).parse(o)||!config(o,c)){error="invalid R4 profile";return false;}
    size_t slash=path.find_last_of("/\\");std::string root=slash==std::string::npos?"":path.substr(0,slash+1);
    std::vector<Event> tape;if(!read(root+c.tape,ev,1024*1024)||!events(ev,c,tape)){error="invalid R4 input tape";return false;}
    if(!mkdir_new(root+c.output)){error="output directory must be new";return false;}
    state.cfg=c;state.tape=std::move(tape);state.dir=root+c.output;state.profile_sha=rms_jar_identity(reinterpret_cast<const uint8_t*>(raw.data()),raw.size());state.tape_sha=rms_jar_identity(reinterpret_cast<const uint8_t*>(ev.data()),ev.size());
    if(!mkdir_new(state.dir+"/rms")){error="RMS isolation directory";return false;}
    state.journal=std::fopen((state.dir+"/timeline.jsonl").c_str(),"wb");state.result=std::fopen((state.dir+"/result.json").c_str(),"wb");
    if(c.payload=="owned-fixtures-only")state.pcm=std::fopen((state.dir+"/owned-fixture.pcm").c_str(),"wb");
    if(!state.journal||!state.result||(c.payload=="owned-fixtures-only"&&!state.pcm)){state.close();error="cannot open outputs";return false;}
    state.active=true;state.remaining=1000000;return true;
}
#endif
inline void finish(const char* status,const char* reason){
    auto& s=state;if(s.finished)return;s.finished=true;
    bool ok=true;
    if(s.result){
        std::string line="{\"schema\":\"R4RESULT1\",\"status\":\""+std::string(status)+"\",\"reason\":\""+reason+"\",\"committed_ticks\":"+std::to_string(s.tick)+",\"virtual_us\":"+std::to_string(s.us())+",\"audio_frames\":"+std::to_string(s.audio_frames)+",\"captures\":"+std::to_string(s.captures)+",\"paints\":"+std::to_string(s.paints)+",\"opcodes\":"+std::to_string(s.opcodes)+",\"fast_ops\":"+std::to_string(s.fast_ops)+",\"jit_ops\":"+std::to_string(s.jit_ops)+",\"uncertified_fallbacks\":"+std::to_string(s.uncertified_fallbacks)+",\"certified_shortcuts\":"+std::to_string(s.certified_shortcuts)+",\"profile\":\""+s.cfg.profile+"\",\"profile_sha256\":\""+s.profile_sha+"\",\"tape_sha256\":\""+s.tape_sha+"\",\"pcm_sha256\":\""+hex(s.pcm_hash)+"\"}\n";
        ok=std::fwrite(line.data(),1,line.size(),s.result)==line.size()&&std::fflush(s.result)==0;
    }
    if(s.pcm){ok=std::fclose(s.pcm)==0&&ok;s.pcm=nullptr;}
    if(s.journal){ok=std::fclose(s.journal)==0&&ok;s.journal=nullptr;}
    if(s.result){ok=std::fclose(s.result)==0&&ok;s.result=nullptr;}
    if(!ok){std::fprintf(stderr,"R4 result output failure\n");std::exit(74);}
}
} // namespace r4
