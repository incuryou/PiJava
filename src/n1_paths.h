// N1 (0.2.67-n1): SD layout for the Pi Java NSP/NRO. Paths only; no behavior change.
// Games: <root>/games/*.jar only (0.2.107 removed the legacy <root>/*.jar scan).
// Log: <root>/logs/j2me-vm.log; previous rotated to logs/j2me-vm.prev.log. Old sdmc:/switch/j2me-vm.log untouched.
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
namespace n1paths {
struct GameEntry { std::string label, path; bool duplicate=false; };
inline bool is_jar_file(const std::string& dir,const std::string& n,std::string& path){
    if(n.size()<=4)return false; std::string ext=n.substr(n.size()-4);
    for(char& c:ext)c=char(std::tolower(static_cast<unsigned char>(c)));
    if(ext!=".jar")return false; path=dir+"/"+n; struct stat st{};
    return stat(path.c_str(),&st)==0&&S_ISREG(st.st_mode);
}
inline void scan_dir(const std::string& dir,std::vector<GameEntry>& out){
    DIR* d=opendir(dir.c_str()); if(!d)return;
    while(dirent* e=readdir(d)){std::string n=e->d_name,p; if(is_jar_file(dir,n,p))out.push_back({n,p,false});}
    closedir(d);
}
// 0.2.107: only <root>/games/*.jar is scanned; JARs left in <root> are no longer listed.
inline std::vector<GameEntry> scan_games(const std::string& root){
    std::vector<GameEntry> items; scan_dir(root+"/games",items);
    std::sort(items.begin(),items.end(),[](const GameEntry& a,const GameEntry& b){return a.label<b.label;});
    return items;
}
// Creates <root>/logs, rotates the previous log. Returns the new log path.
inline std::string prepare_log(const std::string& root,bool* rotated=nullptr){
    mkdir(root.c_str(),0777); const std::string dir=root+"/logs"; mkdir(dir.c_str(),0777);
    const std::string cur=dir+"/j2me-vm.log",prev=dir+"/j2me-vm.prev.log"; struct stat st{};
    bool r=false;
    if(stat(cur.c_str(),&st)==0&&S_ISREG(st.st_mode)){ std::remove(prev.c_str()); r=std::rename(cur.c_str(),prev.c_str())==0; }
    if(rotated)*rotated=r; return cur;
}
inline std::string legacy_log_note(const std::string& legacy){
    struct stat st{}; const bool exists=stat(legacy.c_str(),&st)==0;
    return "LOGPATH new=logs/j2me-vm.log prev=logs/j2me-vm.prev.log legacy="+legacy+(exists?" legacy_state=kept_untouched":" legacy_state=absent")+" legacy_written=0";
}
}
