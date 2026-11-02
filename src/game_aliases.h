#pragma once
#include "game_names.h"
#include "host_menu_settings.h"

// Host display aliases only. Neither JAR paths nor Java/save identities change.
// One ASCII config path, UTF-8 values, hex-encoded original path keys.
namespace game_aliases {
constexpr size_t max_chars=64,max_file=8*1024*1024;
inline bool normalize(const std::string& input,std::string& out){
    std::vector<uint32_t> cps;
    if(!game_names::utf8_codepoints(input,&cps))return false;
    size_t units=0;for(auto c:cps)units+=c>0xffff?2:1;
    if(units>max_chars)return false; // Match the system keyboard's UTF-16 limit.
    for(auto c:cps)if(c<32||(c>=127&&c<=159)||c==0x2028||c==0x2029)return false;
    out=input;
    while(!out.empty()&&(out[0]==' '||out.compare(0,3,"\xe3\x80\x80")==0))out.erase(0,out[0]==' '?1:3);
    while(!out.empty()&&(out.back()==' '||(out.size()>=3&&out.compare(out.size()-3,3,"\xe3\x80\x80")==0)))out.resize(out.size()-(out.back()==' '?1:3));
    return true;
}
inline std::string hex(const std::string& path){
    std::string out;for(unsigned char c:path){out+="0123456789abcdef"[c>>4];out+="0123456789abcdef"[c&15];}return out;
}
inline bool unhex(const std::string& text,std::string& path){
    if(text.empty()||text.size()>8192||text.size()%2)return false;
    path.clear();int high=-1;
    for(char c:text){int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;if(n<0)return false;
        if(high<0)high=n;else{char b=char(high*16+n);if(!b)return false;path+=b;high=-1;}}
    return true;
}
struct Store {
    std::map<std::string,std::string> values;
    bool writable=false;
    bool load(const std::string& file){
        values.clear();writable=false;errno=0;FILE* f=std::fopen(file.c_str(),"rb");
        if(!f&&errno==ENOENT){errno=0;f=std::fopen((file+".previous").c_str(),"rb");if(!f&&errno==ENOENT){writable=true;return true;}}
        if(!f)return false;
        std::string record;char buf[4096];size_t n;
        while((n=std::fread(buf,1,sizeof(buf),f))!=0){record.append(buf,n);if(record.size()>max_file)break;}
        bool ok=!std::ferror(f)&&record.size()<=max_file;ok=std::fclose(f)==0&&ok;
        const std::string header="J2ME_NAMES 1\n";if(!ok||record.compare(0,header.size(),header)!=0)return false;
        std::map<std::string,std::string> parsed;
        for(size_t pos=header.size();pos<record.size();){
            size_t end=record.find('\n',pos),tab=record.find('\t',pos);std::string path,name;
            if(end==std::string::npos||tab==std::string::npos||tab>=end||!unhex(record.substr(pos,tab-pos),path)||
                !normalize(record.substr(tab+1,end-tab-1),name)||name.empty()||parsed.count(path))return false;
            parsed[path]=name;pos=end+1;
        }
        values.swap(parsed);writable=true;return true;
    }
    std::string label(const std::string& path,const std::string& fallback)const{
        auto i=values.find(path);return i==values.end()?fallback:i->second;
    }
    bool set(const std::string& file,const std::string& path,const std::string& input){
        std::string name;if(!writable||path.empty()||path.size()>4096||path.find('\0')!=std::string::npos||!normalize(input,name))return false;
        auto next=values;if(name.empty())next.erase(path);else next[path]=name;
        std::string record="J2ME_NAMES 1\n";for(const auto& entry:next){record+=hex(entry.first)+"\t"+entry.second+"\n";if(record.size()>max_file)return false;}
        if(!host_menu::save_record(file,record))return false;
        values.swap(next);return true;
    }
};
}
