#pragma once
#include "host_keymap_ui.h"
#include <vector>

// Host-only view state. VM locations and candidate ownership stay in game_data::Session.
namespace host_data {
constexpr size_t visible_rows=8;
enum class Result { None, Back, Resume };
enum class InputResult { Accepted, Cancelled, Unavailable, Invalid };
inline const char* type_name(char type) {
    switch(type){case 'S':return "有符号16位";case 'B':return "有符号8位";case 'J':return "有符号64位";
        case 'C':return "无符号16位（字符）";case '*':return "全部整数";default:return "有符号32位";}
}
inline char next_type(char type) {
    switch(type){case 'I':return 'S';case 'S':return 'B';case 'B':return 'J';case 'J':return 'C';case 'C':return '*';default:return 'I';}
}
struct Editor {
    bool open=false,note_error=false;
    size_t selection=0;
    std::string note;
    void begin(){open=true;note.clear();note_error=false;}
    void clamp(size_t count){selection=count?std::min(selection,count-1):0;}
    void move(int delta,size_t count) {
        clamp(count);
        if(delta<0)selection-=std::min(selection,size_t(-delta));
        else if(count)selection+=std::min(count-1-selection,size_t(delta));
    }
    size_t first()const{return selection/visible_rows*visible_rows;}
};
struct Row {std::string location,value;};
struct View {
    size_t count=0;
    bool busy=false,ready=false;
    char type='I';
    std::string note;
    std::vector<Row> rows;
};
inline void render(uint32_t* f,size_t pitch,const Editor& e,const View& v,
                   const host_menu::Text& text,const host_menu::CenteredText& center,
                   const host_keymap::TextRight& right={}) {
    if(!f||pitch<1280)return;
    using host_menu::rect;using host_menu::color;
    const auto ink=color(0x26313e),muted=color(0x718094),red=color(0xc81e41),white=color(0xffffff);
    rect(f,pitch,0,0,1280,720,color(0xf2f4f7));
    host_keymap::render_tabs(f,pitch,host_keymap::Page::Data,center);
    if(text) {
        text(800,100,std::string("类型：")+type_name(v.type),muted,20,432);
        text(48,99,"候选数量："+std::to_string(v.count)+(v.busy?" · 搜索中…":""),ink,21,1184);
        text(48,132,"位置",muted,17,820);text(920,132,"当前值",muted,17,300);
    }
    const size_t first=e.first();
    for(size_t row=0;row<visible_rows;++row) {
        int y=160+int(row)*44;
        const bool present=row<v.rows.size(),selected=present&&first+row==e.selection;
        host_keymap::rounded(f,pitch,48,y,1184,40,7,selected?color(0xffe7ed):white);
        if(selected)rect(f,pitch,48,y+6,4,28,red);
        if(present&&text) {
            text(64,y+9,std::to_string(first+row+1)+". "+v.rows[row].location,selected?red:ink,18,832);
            text(920,y+9,v.rows[row].value,selected?red:ink,18,296);
        }
    }
    if(text) {
        if(v.rows.empty())text(64,177,v.busy?"正在扫描，请稍候；按 B 取消":v.ready?"没有匹配结果；按 + 重新搜索":"按 + 输入游戏中显示的整数，开始首次搜索",muted,20,1150);
        if(v.count)text(48,530,"当前项："+std::to_string(e.selection+1)+" / "+std::to_string(v.count)+" · ↑ / ↓ 逐项 · ← / → 翻页",muted,17,1184);
        else text(48,530,"返回游戏改变数值后，可再进入此页按 X 筛选",muted,17,1184);
        const std::string note=e.note.empty()?v.note:e.note;
        if(!note.empty())text(48,565,note,e.note_error?red:muted,17,1184);
        text(48,597,"返回游戏改变数值 → 再进此页按 X 筛选；Y 只清候选，不撤销修改",muted,15,1184);
    }
    host_keymap::footer(f,pitch,{{"A","修改"},{"B","返回"},{"X","继续筛选"},{"Y","重置搜索"},{"−","切换类型"},{"+","首次搜索"}},text,center,right);
}
} // namespace host_data
