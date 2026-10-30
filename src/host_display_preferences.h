#pragma once
#include "host_menu_settings.h"
#include "logical_screen.h"

// Explicit, path-local presentation preferences. Reading never writes/migrates
// files, and no game identity, filename profile or JAR contents affect choices.
namespace host_display {
struct Preferences { int rotation=0,screen_state=0,w=240,h=320; };
inline bool valid_path(const std::string& jar) {
    return !jar.empty() && jar.find('\0')==std::string::npos;
}
inline Preferences load(const std::string& jar) {
    Preferences out;
    if(!valid_path(jar))return out;
    out.screen_state=logical_screen::sidecar(jar,out.w,out.h);
    const std::string path=jar+".rotation";
    errno=0;std::FILE* f=std::fopen(path.c_str(),"rb");
    // The atomic writer's complete journal is a read-only fallback.
    if(!f && errno==ENOENT)f=std::fopen((path+".previous").c_str(),"rb");
    if(!f)return out;
    char buffer[4];const size_t n=std::fread(buffer,1,sizeof(buffer),f);
    const bool read_ok=!std::ferror(f);const bool close_ok=std::fclose(f)==0;
    if(!read_ok||!close_ok||n>3)return out;
    std::string record(buffer,n);
    if(!record.empty()&&record.back()=='\n')record.pop_back();
    if(!record.empty()&&record.back()=='\r')record.pop_back();
    if(record.size()==1&&record[0]>='0'&&record[0]<='3')out.rotation=record[0]-'0';
    return out;
}
inline void load_into(const std::string& jar,host_menu::Settings& settings) {
    const auto choice=load(jar);
    settings.rotation=choice.rotation;
    settings.canvas=choice.screen_state==1?-1:0;
    if(choice.screen_state==1)for(int i=1;i<host_menu::canvas_count;++i)
        if(host_menu::canvas_size(i)==std::make_pair(choice.w,choice.h)){settings.canvas=i;break;}
}
inline bool save_rotation(const std::string& jar,int rotation) {
    return valid_path(jar)&&rotation>=0&&rotation<4&&
        host_menu::save_record(jar+".rotation",std::to_string(rotation)+"\n");
}
inline bool save_screen(const std::string& jar,int w,int h) {
    if(!valid_path(jar))return false;
    const std::string path=jar+".screen";
    if(w==0&&h==0) {
        // Auto is an explicit user action. Clear a complete old journal first
        // so an interrupted previous write cannot later resurrect a fixed size.
        for(const auto& file:{path+".previous",path}) {
            const auto state=host_menu::settings_detail::file_status(file);
            if(state==host_menu::settings_detail::FileStatus::error)return false;
            if(state==host_menu::settings_detail::FileStatus::readable&&std::remove(file.c_str())!=0)return false;
        }
        return true;
    }
    return logical_screen::valid(w,h)&&host_menu::save_record(path,std::to_string(w)+"x"+std::to_string(h)+"\n");
}
}
