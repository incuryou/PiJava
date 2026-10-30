#pragma once
// Launcher-only input and deletion state; never part of a Java save.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace menu_actions {
inline int move(int selected,int direction,int count) {
    if(count<=0)return 0;
    return (std::max(0,std::min(selected,count-1))+direction+count)%count;
}
// Keep the selected row when changing pages; a short final page clamps it.
inline int page(int selected,int direction,int count,int rows) {
    if(count<=0||rows<=0)return 0;
    selected=std::max(0,std::min(selected,count-1));
    const int pages=(count-1)/rows+1;
    const int next=move(selected/rows,direction,pages);
    return std::min(next*rows+selected%rows,count-1);
}
struct Repeat {
    int direction=0;int64_t next=0;bool browsing=false;
    void reset(){*this=Repeat{};}
    int update(int held,int64_t now) {
        if(!held){reset();return 0;}
        if(held!=direction){direction=held;next=now+300;browsing=false;return held;}
        if(now<next)return 0;
        next=now+80;browsing=true;return held;
    }
};
enum class Action { Up,Down,Confirm,Back };
enum class Result { None,DeleteGame,DeleteInstant,InheritSave };
struct Deletion {
    bool open=false,confirming=false;int selection=0;
    std::string path,label;
    bool begin(const std::string& p,const std::string& name) {
        *this=Deletion{};if(p.empty())return false;
        path=p;label=name;open=true;return true;
    }
    Result act(Action action) {
        if(!open)return Result::None;
        if(action==Action::Back){if(confirming)confirming=false;else open=false;}
        else if(action==Action::Confirm){
            if(selection==2){open=false;return Result::InheritSave;}
            else if(!confirming)confirming=true;
            else{open=false;return selection==0?Result::DeleteGame:Result::DeleteInstant;}
        }else if(!confirming)selection=move(selection,action==Action::Up?-1:1,3);
        return Result::None;
    }
};
inline bool remove_game(const std::string& path,const std::string& root) {
    const std::string prefix=root+"/";
    if(path.compare(0,prefix.size(),prefix)!=0)return false;
    std::string name=path.substr(prefix.size());
    if(name.compare(0,6,"games/")==0)name.erase(0,6);
    if(name.find('/')!=std::string::npos || name.find('\\')!=std::string::npos || name.size()<=4)return false;
    std::string ext=name.substr(name.size()-4);
    for(char& c:ext)if(c>='A'&&c<='Z')c=char(c+32);
    if(ext!=".jar")return false;
    struct stat st{};
    return lstat(path.c_str(),&st)==0 && S_ISREG(st.st_mode) && std::remove(path.c_str())==0;
}
}
