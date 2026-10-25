#include "../src/menu_catalog.h"
#include "../src/n1_paths.h"
#include "../src/speed_keys.h"
#include <cassert>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include "menu_stat.inc" // Generated verbatim from the Switch host's production helpers.
static int jar_opens=0,state_stats=0;
extern "C" FILE* __real_fopen(const char*,const char*);
extern "C" int __real_stat(const char*,struct stat*);
extern "C" FILE* __wrap_fopen(const char* p,const char* mode) {
    std::string s=p;if(s.size()>4&&s.substr(s.size()-4)==".jar")++jar_opens;
    return __real_fopen(p,mode);
}
extern "C" int __wrap_stat(const char* p,struct stat* st) {
    if(std::string(p).find("/state/")!=std::string::npos)++state_stats;
    return __real_stat(p,st);
}
int main() {
    const std::string root="sdmc:/switch/j2me";
    auto items=n1paths::scan_games(root);
    assert(items.size()==5000); // root-only.jar in <root> is not listed
    assert(items[0].path==root+"/games/g00000.jar"&&!items[0].duplicate&&items[0].label=="g00000.jar");
    for(const auto& item:items)assert(item.path.rfind(root+"/games/",0)==0);
    for(size_t i=1;i<items.size();++i)assert(items[i-1].label<items[i].label);
    menu_catalog::State model;std::vector<std::string> labels,paths;std::vector<uint8_t> saved;
    jar_opens=state_stats=0;model.rebuild(items,labels,paths,saved);
    assert(jar_opens==0&&state_stats==0);
    auto* storage=labels.data();auto* path_storage=paths.data();
    model.prepare_page(paths,saved,0,9,instant_state_exists);
    assert(state_stats==18); // Two actual production stat calls per absent save.
    model.select(paths,0,0);
    assert(model.poll_name(paths,0)); // A stationary selection has no artificial delay.
    assert(jar_opens==1);
    model.rebuild(items,labels,paths,saved);
    jar_opens=state_stats=0;model.prepare_page(paths,saved,0,9,instant_state_exists);
    storage=labels.data();path_storage=paths.data();
    for(int i=0;i<100;++i){model.prepare_page(paths,saved,i%9,9,instant_state_exists);model.select(paths,i%9,i*20);assert(!model.poll_name(paths,i*20,true));}
    assert(state_stats==18&&jar_opens==0&&labels.data()==storage&&paths.data()==path_storage);
    assert(!model.poll_name(paths,1980,true));assert(model.poll_name(paths,1980));
    struct stat jar_info{};assert(stat(paths[0].c_str(),&jar_info)==0);
    const auto size_text=std::to_string((uint64_t(jar_info.st_size)+1023)/1024)+" KB";
    assert(jar_opens==1&&model.details[0]=="根目录中文名 · 版本未知 · "+size_text);
    assert(!model.poll_name(paths,1980)&&jar_opens==1);
    model.select(paths,1,2200);assert(model.poll_name(paths,2200));assert(jar_opens==2);
    model.select(paths,0,2400);assert(!model.name_pending&&!model.poll_name(paths,2400,true)&&jar_opens==2&&model.details[0]=="根目录中文名 · 版本未知 · "+size_text);
    model.prepare_page(paths,saved,9,9,instant_state_exists);assert(state_stats==36);
    model.prepare_page(paths,saved,4999,9,instant_state_exists);assert(state_stats==46); // Final page: five entries.
    {std::ofstream f(instant_state_path(paths[0]));f<<"test only";}
    model.invalidate_saved();state_stats=0;model.prepare_page(paths,saved,0,9,instant_state_exists);
    assert(saved[0]&&state_stats==17);
    std::remove(instant_state_path(paths[0]).c_str());
    {std::ofstream f(instant_state_path(paths[0])+".previous");f<<"test recovery";}
    model.invalidate_saved();model.prepare_page(paths,saved,0,9,instant_state_exists);assert(saved[0]);
    std::remove((instant_state_path(paths[0])+".previous").c_str());
    assert(mkdir(instant_state_path(paths[0]).c_str(),0700)==0);
    model.invalidate_saved();model.prepare_page(paths,saved,0,9,instant_state_exists);assert(!saved[0]);
    assert(rmdir(instant_state_path(paths[0]).c_str())==0);
    model.invalidate_saved();model.prepare_page(paths,saved,0,9,instant_state_exists);assert(!saved[0]);
    speed_keys::State speed;std::map<std::string,int> overrides;
    jar_opens=0;assert(speed_keys::lookup(speed,overrides,paths[0])==0&&jar_opens==0&&speed.hashed==0&&speed.memo.empty());
    const auto key=speed_keys::key_for(speed,paths[0]);assert(!key.empty()&&speed.hashed==1&&jar_opens==1);
    overrides[key]=60;assert(speed_keys::lookup(speed,overrides,paths[0])==60&&speed.hashed==1);
    const std::string moved=root+"/games/moved.jar";
    assert(std::rename(paths[0].c_str(),moved.c_str())==0);
    assert(speed_keys::lookup(speed,overrides,moved)==60&&speed.hashed==2);
    assert(std::rename(moved.c_str(),paths[0].c_str())==0);
    const std::string original_path=paths[0];
    std::ifstream original_file(original_path,std::ios::binary);
    const std::string original((std::istreambuf_iterator<char>(original_file)),{});original_file.close();
    {std::ofstream bad(original_path,std::ios::binary);bad<<"invalid ZIP";}
    model.rebuild(items,labels,paths,saved);assert(model.selected==-1&&model.names.names.empty()&&model.names.reads==0);
    model.select(paths,0,3000);assert(model.poll_name(paths,3000)&&model.details[0]=="g00000 · 版本未知 · 1 KB"&&model.names.misses==1);
    {std::ofstream restore(original_path,std::ios::binary);restore<<original;}
    model.rebuild(items,labels,paths,saved);model.select(paths,0,3200);
    assert(model.poll_name(paths,3200)&&model.details[0]=="根目录中文名 · 版本未知 · "+size_text);
    std::vector<n1paths::GameEntry> empty;model.rebuild(empty,labels,paths,saved);
    model.prepare_page(paths,saved,0,9,instant_state_exists);model.select(paths,0,3000);assert(!model.poll_name(paths,4000)&&labels.empty());
    std::cout<<"PASS MenuCatalogChecks: 5000 games, games/ only (root JAR ignored), stable list storage, page-only stat, immediate details/browsing deferral/cache, save invalidation/recovery/non-file rejection, empty catalog, native speed no hash, SHA override and moved JAR\n";
}
