#pragma once
#include "game_aliases.h"
#include <set>

// Launcher metadata only: newest favorite first, original scan order otherwise.
namespace game_favorites {
struct Store {
    bool writable=false;
private:
    std::vector<std::string> ordered;
    std::map<std::string,size_t> ranks;
    void assign(std::vector<std::string> next){ordered=std::move(next);ranks.clear();for(size_t i=0;i<ordered.size();++i)ranks.emplace(ordered[i],i);}
public:
    bool contains(const std::string& path)const{return ranks.count(path)!=0;}
    bool load(const std::string& file){
        assign({});writable=false;errno=0;FILE* f=std::fopen(file.c_str(),"rb");
        if(!f&&errno==ENOENT){errno=0;f=std::fopen((file+".previous").c_str(),"rb");if(!f&&errno==ENOENT){writable=true;return true;}}
        if(!f)return false;
        std::string record;char buffer[4096];size_t n;
        while((n=std::fread(buffer,1,sizeof(buffer),f))){record.append(buffer,n);if(record.size()>game_aliases::max_file)break;}
        bool ok=!std::ferror(f)&&record.size()<=game_aliases::max_file;ok=std::fclose(f)==0&&ok;
        const std::string header="J2ME_FAVORITES 1\n";if(!ok||record.compare(0,header.size(),header)!=0)return false;
        std::vector<std::string> next;std::set<std::string> seen;
        for(size_t pos=header.size();pos<record.size();){size_t end=record.find('\n',pos);std::string path;
            if(end==std::string::npos||!game_aliases::unhex(record.substr(pos,end-pos),path)||!seen.insert(path).second)return false;
            next.push_back(path);pos=end+1;
        }
        assign(std::move(next));writable=true;return true;
    }
    bool set(const std::string& file,const std::string& path,bool favorite){
        if(!writable||path.empty()||path.size()>4096||path.find('\0')!=std::string::npos)return false;
        auto next=ordered;next.erase(std::remove(next.begin(),next.end(),path),next.end());if(favorite)next.insert(next.begin(),path);
        std::string record="J2ME_FAVORITES 1\n";for(const auto& item:next){record+=game_aliases::hex(item)+"\n";if(record.size()>game_aliases::max_file)return false;}
        if(!host_menu::save_record(file,record))return false;assign(std::move(next));return true;
    }
    template<class Entries>void order(Entries& items)const{
        const auto rank=[&](const std::string& path){auto i=ranks.find(path);return i==ranks.end()?ordered.size():i->second;};
        std::stable_sort(items.begin(),items.end(),[&](const auto&a,const auto&b){return rank(a.path)<rank(b.path);});
    }
};
}
