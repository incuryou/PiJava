#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <utility>
// Native launcher view only. Filtering never mutates or sorts the full scan.
// UTF-8 Chinese is matched verbatim; ASCII letters ignore case without corrupting UTF-8 bytes.
namespace launcher_library {
inline unsigned char fold(unsigned char c){return c>='A'&&c<='Z'?c+('a'-'A'):c;}
inline bool contains(const std::string& name,const std::string& query){
    return std::search(name.begin(),name.end(),query.begin(),query.end(),[](unsigned char a,unsigned char b){return fold(a)==fold(b);})!=name.end()||query.empty();
}
struct View {
    bool favorites_only=false;
    std::string query;
    template<class Entries,class Favorite>
    void rebuild(const Entries& full,const Favorite& favorite,Entries& visible,int& selected)const{
        const std::string path=selected>=0&&size_t(selected)<visible.size()?visible[size_t(selected)].path:std::string();
        Entries next;next.reserve(full.size());
        for(const auto& item:full)if((!favorites_only||favorite(item.path))&&contains(item.label,query))next.push_back(item);
        visible=std::move(next);selected=std::max(0,std::min(selected,std::max(0,int(visible.size())-1)));
        if(!path.empty())for(size_t i=0;i<visible.size();++i)if(visible[i].path==path){selected=int(i);break;}
    }
};
}
