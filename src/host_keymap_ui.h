#pragma once
#include "host_keymap.h"
#include "host_pro_controller.h"
#include <cmath>

namespace host_keymap {
inline void blend(uint32_t& dst,uint32_t src,unsigned alpha) {
    uint32_t result=0xff000000u;
    for(int shift=0;shift<24;shift+=8)result|=((((src>>shift)&255)*alpha+((dst>>shift)&255)*(255-alpha)+127)/255)<<shift;
    dst=result;
}
inline void rounded(uint32_t* f,size_t pitch,int x,int y,int w,int h,int r,uint32_t color) {
    for(int yy=std::max(0,y);yy<std::min(720,y+h);++yy)for(int xx=std::max(0,x);xx<std::min(1280,x+w);++xx) {
        float dx=std::max(float(x+r)-xx-.5f,std::max(0.f,xx+.5f-float(x+w-r)));
        float dy=std::max(float(y+r)-yy-.5f,std::max(0.f,yy+.5f-float(y+h-r)));
        float coverage=std::max(0.f,std::min(1.f,r+.5f-std::sqrt(dx*dx+dy*dy)));
        blend(f[size_t(yy)*pitch+xx],color,unsigned(coverage*255+.5f));
    }
}
inline void prompt(uint32_t* f,size_t pitch,int cx,int cy,const std::string& key,int size,const host_menu::CenteredText& center) {
    rounded(f,pitch,cx-size/2,cy-size/2,size,size,size/2,host_menu::color(0x2a2a2a));
    if(key=="+") {
        int r=std::max(3,size/5),stroke=std::max(1,size/12);
        host_menu::rect(f,pitch,cx-r,cy-stroke/2,2*r+1,stroke,host_menu::color(0xffffff));
        host_menu::rect(f,pitch,cx-stroke/2,cy-r,stroke,2*r+1,host_menu::color(0xffffff));
    } else if(key=="−") {
        int r=std::max(3,size/5),stroke=std::max(1,size/12);
        host_menu::rect(f,pitch,cx-r,cy-stroke/2,2*r+1,stroke,host_menu::color(0xffffff));
    } else if(center) {
        const bool single=key.size()==1 || key=="↑" || key=="↓" || key=="←" || key=="→";
        center(cx,cy,key,host_menu::color(0xffffff),std::max(8,size*(single?3:2)/5),size-4);
    }
}
inline const char* input_symbol(int input) {
    constexpr const char* directions[]={"↑","↓","←","→","L↑","L↓","L←","L→"};
    constexpr const char* right[]={"R↑","R↓","R←","R→"};
    if(input>=DUp && input<=SRight)return directions[input-DUp];
    if(input>=RUp && input<=RRight)return right[input-RUp];
    return input_names[input];
}
inline void input_prompt(uint32_t* f,size_t pitch,int cx,int cy,int input,int size,const host_menu::CenteredText& center) {
    prompt(f,pitch,cx,cy,input_symbol(input),size,center);
}
constexpr int phone_key_height=56,phone_row_step=63;

inline std::vector<uint32_t> load_phone(const char* path) {
    constexpr size_t count=227*518;
    std::FILE* file=std::fopen(path,"rb");if(!file)return {};
    std::vector<uint32_t> out(count);
    bool ok=std::fread(out.data(),4,count,file)==count && std::fgetc(file)==EOF && !std::ferror(file);
    if(std::fclose(file))ok=false;
    if(!ok)out.clear();return out;
}
using TextRight = std::function<int(const std::string&,int)>;
inline void footer(uint32_t* f,size_t pitch,
                   std::initializer_list<std::pair<const char*,std::string>> actions,
                   const host_menu::Text& text,const host_menu::CenteredText& center,const TextRight& right) {
    using host_menu::color;
    host_menu::rect(f,pitch,0,627,1280,93,color(0xffffff));
    int left=34;
    for(const auto& action:actions) {
        prompt(f,pitch,left+15,667,action.first,30,center);
        const int x=left+39;
        const int width=right?right(action.second,18):host_menu::text_advance(action.second,18);
        if(text)text(x,656,action.second,color(0x26313e),18,std::max(1,width));
        left=x+width+32; // Equal blank space from text ink to the next icon.
    }
}
inline void render(uint32_t* f,size_t pitch,const Editor& e,const std::vector<uint32_t>& phone,
                   const host_menu::Text& text,const host_menu::CenteredText& center,const TextRight& right={},host_pro::Preview* preview=nullptr,host_controller::InkMeasure ink_measure=nullptr) {
    const bool saves=e.page==Page::SaveExit;
    if(!f||pitch<1280)return;
    using host_menu::rect;using host_menu::color;
    const auto ink=color(0x26313e),muted=color(0x718094),red=color(0xc81e41),line=color(0xcbd4df),white=color(0xffffff);
    rect(f,pitch,0,0,1280,720,color(0xf2f4f7));rect(f,pitch,0,0,1280,82,white);
    rect(f,pitch,saves?32:250,27,5,30,red);
    // Header ink and the accent bar share the exact same vertical center.
    if(center) {
        const std::string save_tab="存档与退出",map_tab="修改按键映射";
        const int save_width=host_menu::text_advance(save_tab,29),map_width=host_menu::text_advance(map_tab,29);
        center(49+save_width/2,42,save_tab,saves?ink:muted,29,save_width);
        center(267+map_width/2,42,map_tab,saves?muted:ink,29,map_width);
    }
    prompt(f,pitch,1080,42,"L",30,center);prompt(f,pitch,1122,42,"R",30,center);
    if(center)center(1191,42,"切换页面",muted,18,92);
    if(!saves && phone.size()==227*518)for(int y=17;y<518;++y)for(int x=0;x<227;++x) {
        uint32_t sample=phone[size_t(y)*227+x];blend(f[size_t(y+103)*pitch+x+49],sample,sample>>24);
    }
    if(!saves)rect(f,pitch,309,111,1,494,color(0xdfe4eb));
    rect(f,pitch,32,625,1216,1,color(0xdfe4eb));
    if(saves) {
        if(text) {
            const bool neutral=e.save_note.empty() || e.save_note_neutral;
            text(260,108,e.save_note.empty()?"删除存档、退出游戏前会再次确认":e.save_note,
                 neutral?muted:e.save_note_error?red:color(0x208351),17,845);
            text(260,151,"当前游戏的即时存档",muted,18,845);
        }
        const char* titles[]={"保存即时存档","删除即时存档","退出游戏"};
        const char* subtitles[]={"保存当前游戏进度","删除当前游戏的即时存档","退出到游戏列表"};
        for(int i=0;i<3;++i) {
            int y=191+i*106;bool selected=i==e.save_selection;
            rounded(f,pitch,260,y+4,760,84,12,color(0xe4e9ee));
            rounded(f,pitch,260,y,760,84,12,selected?red:color(0xcbd4df));
            rounded(f,pitch,262,y+2,756,80,10,selected?color(0xfff6f8):white);
            if(selected)rounded(f,pitch,275,y+21,4,40,2,red);
            if(text){text(297,y+13,titles[i],selected?red:ink,26,686);text(297,y+49,subtitles[i],muted,17,686);}
        }
        if(e.pending!=Pending::None) {
            for(int y=82;y<625;++y)for(int x=0;x<1280;++x)blend(f[size_t(y)*pitch+x],color(0x15202b),104);
            rounded(f,pitch,380,274,520,156,16,white);
            if(text){text(416,303,e.pending==Pending::Delete?"删除当前游戏的即时存档？":"退出当前游戏？",ink,26,448);
                text(416,359,e.pending==Pending::Delete?"删除后将无法继续此即时存档。":"尚未保存的游戏进度将丢失。",muted,17,448);}
        }
        footer(f,pitch,{{"A","确认"},{"B",e.pending==Pending::None?"返回":"取消"}},text,center,right);
        return;
    }
    if(text && !e.note.empty())text(332,88,e.note,e.note_error?red:color(0x208351),15,900);
    if(text){text(332,117,"选择手机按键",ink,22,330);text(716,117,"选择 Switch 按键",ink,22,330);
        text(1100,121,std::string("手机 ")+phone_names[e.phone],red,18,145);}
    rect(f,pitch,688,111,1,494,color(0xdfe4eb));
    auto b=effective(e.draft,e.standard);
    // The highlight follows the same phone identity as the center grid.
    struct KeyRect{int x,y,w,h;};KeyRect phone_key{};
    if(e.phone>=N1){const int p=e.phone-N1;phone_key={34+(p%3)*52,365+(p/3)*26,49,23};}
    else {static constexpr KeyRect nav[]={{47,266,28,16},{103,267,21,12},{151,266,28,16},{91,282,13,23},{104,282,20,21},{124,282,13,23},{103,304,21,12}};phone_key=nav[e.phone];}
    const int hx=49+phone_key.x,hy=103+phone_key.y;
    rect(f,pitch,hx,hy,phone_key.w,2,red);rect(f,pitch,hx,hy+phone_key.h-2,phone_key.w,2,red);
    rect(f,pitch,hx,hy,2,phone_key.h,red);rect(f,pitch,hx+phone_key.w-2,hy,2,phone_key.h,red);
    for(int p=0;p<PhoneCount;++p) {
        auto pos=position(p);int x=332+pos.first*111;
        int y=155+pos.second*62;
        int h=55;bool selected=p==e.phone;
        rounded(f,pitch,x,y+3,102,h,10,color(0xe4e9ee));
        rounded(f,pitch,x,y,102,h,10,selected?red:line);
        rounded(f,pitch,x+(selected?2:1),y+(selected?2:1),102-(selected?4:2),h-(selected?4:2),9,selected?color(0xfff6f8):color(0xfcfdff));
        if(center)center(x+51,y+17,phone_names[p],selected?red:ink,21,96);
        int count=0;for(int i:b[p])if(i>=0)++count;
        if(!count) {if(center)center(x+51,y+40,"—",muted,12,96);}
        else {
            int index=0;
            for(int i:b[p])if(i>=0)input_prompt(f,pitch,x+51+(2*index++-(count-1))*15,y+40,i,22,center);
        }
    }
    const int selected=e.phase==Phase::Choice?choice_inputs[e.choice]:b[e.phone][std::max(0,std::min(SlotCount-1,e.slot))];
    if(preview)preview->render(f,pitch,e.draft,e.standard,selected,text,center,ink_measure);
    for(int slot=0;slot<SlotCount;++slot) {
        const int x=716+slot*178,y=490;bool focused=e.phase!=Phase::Phone && slot==e.slot;
        if(text)text(x+12,y-24,"映射 "+std::to_string(slot+1),focused?red:muted,16,140);
        rounded(f,pitch,x,y+4,164,66,10,color(0xe4e9ee));
        rounded(f,pitch,x,y,164,66,10,focused?red:color(0xbcc9d6));
        rounded(f,pitch,x+2,y+2,160,62,8,focused?color(0xfff6f8):white);
        int i=b[e.phone][slot];
        if(i==none){if(center)center(x+82,y+32,"未设置",muted,18,150);}
        else {
            input_prompt(f,pitch,x+82,y+32,i,34,center);
        }
    }
    if(text&&e.phase!=Phase::Choice){text(716,573,selected==none?"未设置":std::string("Switch ")+input_names[selected]+" → 手机 "+phone_names[e.phone],ink,18,520);
        text(716,600,"方向键选择 · A 确认 · B 返回 · L3 固定菜单",muted,15,520);}
    if(e.phase==Phase::Choice) {
        // A compact scrolling selector, not a wall of all controller keys.
        const int box=716+std::max(0,std::min(SlotCount-1,e.slot))*178;
        rounded(f,pitch,box,302,164,306,12,color(0xbcc9d6));
        rounded(f,pitch,box+2,304,160,302,10,white);
        int first=std::max(0,std::min(e.choice-2,InputCount-5));
        for(int row=0;row<6;++row) {
            int option=first+row,y=310+row*48;
            if(option==e.choice)rounded(f,pitch,box+6,y,152,44,7,color(0xffe7ed));
            if(option==0){if(text)text(box+16,y+14,"未设置",muted,17,135);}
            else {
                int input=choice_inputs[option];input_prompt(f,pitch,box+30,y+22,input,28,center);
                if(text)text(box+53,y+14,input_names[input],ink,16,105);
            }
        }
    }
    footer(f,pitch,{{"A","确认"},{"B","返回"},{"X","恢复默认"},
                    {"Y",e.global?"改为游戏映射":"改为全局映射"},{"+","保存当前映射"}},text,center,right);
}
} // namespace host_keymap
