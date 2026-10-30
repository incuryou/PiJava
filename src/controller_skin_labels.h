#pragma once
#include "controller_skin_layout.h"
#include <algorithm>
#include <string>

namespace host_controller {
constexpr int Menu=host_keymap::InputCount,Zero=Menu+1,LabelCount=Zero+1;
inline bool mapped_skin(int bezel){return bezel>=4&&bezel<=6;}
inline const char* phone_name(int p){
    static constexpr const char* names[]={"左软","↑","右软","←","确认","→","↓","1","2","3","4","5","6","7","8","9","*","0","#"};
    return names[p];
}
struct Label {
    std::string text;
    std::array<int,host_keymap::PhoneCount> phones{};
    int count=0,rows=0,font=32;
    std::array<std::string,8> lines;
    std::array<int,8> widths{};
};
using Measure=int(*)(const std::string&,int);
enum class Context {Game,Editor};
inline const std::array<Anchor,16>& game_layout(int bezel){
    static const auto centered=[] {
        auto result=layouts;
        for(auto& skin:result){
            for(auto pair:{std::pair<int,int>{0,8},{1,9}}){
                auto& l=skin[size_t(pair.first)];auto& r=skin[size_t(pair.second)];
                const int ll=std::min(l.x,l.tag_x-16),lr=std::max(l.x+l.w,l.tag_x+16);
                const int rl=std::min(r.x,r.tag_x-16),rr=std::max(r.x+r.w,r.tag_x+16);
                const int center=(ll+lr+2560-rl-rr+2)/4;
                const int width=(std::max(lr-ll,rr-rl)+1)/2*2;
                l.x=center-width/2;r.x=1280-center-width/2;l.w=r.w=width;
                l.y=r.y=(l.y+r.y)/2;l.h=r.h=std::max(l.h,r.h);
            }
            for(auto& a:skin)if(a.input==host_keymap::A||a.input==host_keymap::B||a.input==host_keymap::X||a.input==host_keymap::Y){a.y-=10;a.h+=20;}
        }
        // Independent centers traced on the decoded, unchanged J2SJ artwork.
        constexpr int sticks[][4]={{186,281,1024,519},{193,290,1031,519},{182,283,1034,522}};
        for(size_t n=0;n<result.size();++n)for(int key=0;key<2;++key){auto& a=result[n][key?15:3];a.x=sticks[n][key*2]-a.w/2;a.y=sticks[n][key*2+1]-a.h/2;}
        // Black controller's two small round faces are lower than the old
        // source legend coordinates; use the decoded picture's centers.
        result[2][2].x=313-result[2][2].w/2;result[2][2].y=161-result[2][2].h/2;
        result[2][10].x=965-result[2][10].w/2;result[2][10].y=161-result[2][10].h/2;
        return result;
    }();
    return centered[size_t(bezel-4)];
}
// Use exactly the artwork projection for phone-key legends, including the
// visibility threshold. The caller invalidates Legends when this layout moves.
inline std::array<Anchor,16> universal_game_layout(const host_menu::UniversalLayout& layout){
    auto result=game_layout(6);
    for(size_t n=0;n<result.size();++n){
        auto& anchor=result[n];const auto& pane=layout.panes[n/8];
        if(!pane.w){anchor.w=anchor.h=0;continue;}
        const int right=pane.project_x(anchor.x+anchor.w),bottom=pane.project_y(anchor.y+anchor.h);
        anchor.x=pane.project_x(anchor.x);anchor.y=pane.project_y(anchor.y);
        anchor.w=right-anchor.x;anchor.h=bottom-anchor.y;
        anchor.tag_x=pane.project_x(anchor.tag_x);anchor.tag_y=pane.project_y(anchor.tag_y);
    }
    return result;
}
class Legends {
    host_keymap::Bindings previous_=host_keymap::empty_bindings();
    int bezel_=-1;bool dirty_=true;uint64_t generation_=0;
    const std::array<Anchor,16>* layout_=nullptr;
    std::array<Label,LabelCount> labels_;
    static int advance(const std::string& text,int px,const Measure& measure){
        return measure?std::max(0,measure(text,px)):host_menu::text_advance(text,px);
    }
    static void fit(Label& label,const Anchor& anchor,const Measure& measure,int minimum=6,int maximum=32){
        for(int px=maximum;px>=minimum;--px){
            for(auto& line:label.lines)line.clear();
            int row=0;bool fits=true;
            const int tokens=std::max(1,label.count);
            for(int token=0;token<tokens;++token){
                const std::string value=label.count?phone_name(label.phones[token]):label.text;
                std::string candidate=label.lines[row];if(!candidate.empty())candidate+='/';candidate+=value;
                if(advance(candidate,px,measure)>anchor.w&&!label.lines[row].empty()){
                    if(++row==int(label.lines.size())){fits=false;break;}
                    label.lines[row]=value;
                }else label.lines[row]=std::move(candidate);
                if(advance(label.lines[row],px,measure)>anchor.w){fits=false;break;}
            }
            if(fits&&(row+1)*(px+2)-2<=anchor.h){
                label.rows=row+1;label.font=px;
                for(int line=0;line<label.rows;++line)label.widths[line]=advance(label.lines[line],px,measure);
                return;
            }
        }
        // The bounded catalog has at most 19 short phone tokens; the smallest
        // approved rectangle fits all of them at 6 px. No label is truncated.
        label.rows=0;
    }
public:
    void invalidate(){dirty_=true;}
    uint64_t generation()const{return generation_;}
    const Label& label(int input)const{return labels_[size_t(input)];}
    bool update(const host_keymap::Config& config,bool standard,int bezel,const Measure& measure={},const std::array<Anchor,16>* custom_layout=nullptr){
        const auto bindings=host_keymap::effective(config,standard);
        const auto* selected=custom_layout?custom_layout:mapped_skin(bezel)?&game_layout(bezel==j2me_present::adaptive_bezel?6:bezel):nullptr;
        if(!dirty_&&bezel==bezel_&&bindings==previous_&&selected==layout_)return false;
        layout_=selected;
        dirty_=false;bezel_=bezel;previous_=bindings;++generation_;
        for(auto& label:labels_){label.text.clear();label.count=label.rows=0;}
        for(int phone=0;phone<host_keymap::PhoneCount;++phone){
            std::array<bool,host_keymap::InputCount> seen{};
            for(int input:bindings[phone])if(input>=0&&input<host_keymap::InputCount&&!seen[size_t(input)]){
                seen[size_t(input)]=true;auto& label=labels_[size_t(input)];
                label.phones[size_t(label.count++)]=phone;
                if(!label.text.empty())label.text+='/';label.text+=phone_name(phone);
            }
        }
        for(auto& label:labels_)if(label.text.empty())label.text="—";
        labels_[Menu].text="菜单";labels_[Menu].count=0;
        labels_[Zero]=labels_[host_keymap::R3];
        if(layout_)for(const auto& anchor:*layout_){auto& label=labels_[size_t(anchor.input)];bool chinese=false;for(int n=0;n<label.count;++n)if(label.phones[n]==host_keymap::SoftL||label.phones[n]==host_keymap::SoftR||label.phones[n]==host_keymap::Fire)chinese=true;fit(label,anchor,measure,custom_layout?3:6,!custom_layout&&chinese?28:32);}
        return true;
    }
    void render(uint32_t* frame,size_t pitch,int bezel,const host_menu::Text& text,Context context=Context::Game,const host_menu::CenteredText& center={})const{
        if(!frame||pitch<size_t(host_menu::width)||!mapped_skin(bezel)||bezel!=bezel_||!text)return;
        const uint32_t base_ink=host_menu::color(bezel>=6?0xffffff:0x20252b);
        if(!layout_)return;
        for(const auto& anchor:*layout_){
            if(anchor.w<=0||anchor.h<=0)continue;
            const uint32_t ink=bezel==4&&(anchor.input==Menu||anchor.input==Zero)?host_menu::color(0xffffff):base_ink;
            const auto& label=labels_[size_t(anchor.input)];
            if(context==Context::Game&&label.count==0&&anchor.input!=Menu)continue;
            const int height=label.rows*(label.font+2)-2;
            for(int row=0;row<label.rows;++row){
                if(center){center(anchor.x+anchor.w/2,anchor.y+anchor.h/2+(2*row-label.rows+1)*(label.font+2)/2,label.lines[row],ink,label.font,anchor.w);continue;}
                const int x=anchor.x+(anchor.w-label.widths[row])/2;
                const int y=anchor.y+(anchor.h-height)/2+row*(label.font+2);
                text(x,y,label.lines[row],ink,label.font,anchor.x+anchor.w-x);
            }
            if(context==Context::Editor&&*anchor.tag){if(center)center(anchor.tag_x,anchor.tag_y,anchor.tag,ink,11,32);else text(anchor.tag_x-16,anchor.tag_y-6,anchor.tag,ink,11,32);}
        }
    }
};
}
