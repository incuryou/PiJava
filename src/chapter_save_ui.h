#pragma once
#include "chapter_save.h"
#include "host_menu.h"

namespace chapter_ui {
enum class Action { Up,Down,PreviousPage,NextPage,Confirm,Back };
enum class Event { None,Prepare,Import };
struct Entry { std::string label,path; };
struct State {
    bool open=false,confirming=false,busy=false;int selected=0;
    std::string target_path,target_label,source_id,target_id,note;
    std::vector<Entry> sources;size_t stores=0;
    static constexpr int rows=6;
    template<class Entries> bool begin(const Entries& items,int target) {
        *this=State{};
        if(target<0 || target>=int(items.size()))return false;
        target_path=items[target].path;target_label=items[target].label;
        for(const auto& item:items)if(item.path!=target_path)sources.push_back({item.label,item.path});
        open=!sources.empty();return open;
    }
    Event act(Action action) {
        if(!open||busy)return Event::None;
        if(action==Action::Back){if(confirming){confirming=false;source_id.clear();target_id.clear();}else open=false;}
        else if(action==Action::Confirm)return confirming?Event::Import:Event::Prepare;
        else if(!confirming){
            if(action==Action::Up||action==Action::Down)selected=menu_actions::move(selected,action==Action::Down?1:-1,int(sources.size()));
            else selected=menu_actions::page(selected,action==Action::NextPage?1:-1,int(sources.size()),rows);
            note.clear();
        }
        return Event::None;
    }
    bool prepare(const std::string& root,std::string& error) {
        if(!open||selected<0||selected>=int(sources.size()))return false;
        source_id=chapter_save::jar_identity(sources[selected].path);target_id=chapter_save::jar_identity(target_path);
        if(source_id.empty()||target_id.empty()){error="无法读取来源或目标 JAR";return false;}
        if(source_id==target_id){error="来源与目标是同一个游戏文件，请选择其他章节";return false;}
        std::vector<chapter_save::Store> found;
        if(!chapter_save::inspect(root,source_id,found,error))return false;
        stores=found.size();confirming=true;return true;
    }
    chapter_save::Result execute(const std::string& root) {
        chapter_save::Result result;
        if(!open||!confirming){result.error="请先选择来源并确认";return result;}
        // Bypass the speed-key cache: replacing either JAR invalidates confirmation.
        if(chapter_save::jar_identity(sources[selected].path)!=source_id || chapter_save::jar_identity(target_path)!=target_id)
            result.error="游戏文件已变化，请重新选择来源";
        else result=chapter_save::import_stores(root,source_id,target_id);
        confirming=false;busy=false;open=!result.ok;note=result.error;return result;
    }
};
inline void render(uint32_t* frame,size_t pitch,const State& state,const host_menu::Text& text) {
    if(!frame||pitch<1280||!state.open)return;
    using host_menu::color;
    host_menu::rect(frame,pitch,172,64,936,592,color(0xe4e9ee));
    host_menu::rect(frame,pitch,176,68,928,584,color(0xffffff));
    const auto ink=color(0x26313e),muted=color(0x718094),accent=color(0x208351),red=color(0xc81e41);
    if(!text)return;
    text(204,88,"继承游戏内存档",ink,28,872);
    text(204,132,"目标："+state.target_label,ink,21,872);
    if(state.confirming){
        text(204,194,"来源："+state.sources[state.selected].label,ink,23,872);
        text(204,244,"将导入 "+std::to_string(state.stores)+" 个存档库，替换目标游戏内存档。",ink,21,872);
        text(204,296,"目标原有进度会整体备份，来源存档保持不变。",muted,20,872);
        text(204,342,"仅适用于存档格式兼容的章节；不会转换即时存档。",muted,20,872);
        text(204,388,"完成后按 A 正常开始游戏，勿用 X 继续旧即时存档。",muted,20,872);
        text(204,570,state.busy?"正在复制并校验存档，请稍候…":"A 确认导入    B 返回选择",accent,23,872);
    }else{
        text(204,176,"选择上一章（来源游戏）",muted,19,872);
        const int first=state.selected/State::rows*State::rows;
        for(int i=first;i<std::min(first+State::rows,int(state.sources.size()));++i){
            const int y=212+(i-first)*45;
            host_menu::rect(frame,pitch,200,y-4,880,40,color(i==state.selected?0xe4f2e9:0xf2f4f7));
            text(216,y+3,state.sources[i].label,i==state.selected?accent:ink,21,844);
        }
        text(204,572,"↑ ↓ 选择  L / R 翻页  A 下一步  B 取消",muted,21,872);
    }
    if(!state.note.empty())text(204,524,state.note,red,18,872);
}
} // namespace chapter_ui
