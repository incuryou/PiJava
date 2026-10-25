#pragma once
#include "controller_skin_labels.h"
#include <cmath>
#define HOST_CONTROLLER_ALIGNMENT_REV 4
namespace host_controller {
struct Ink {int w=0,h=0;bool ok=false;};
using InkMeasure=Ink(*)(const std::string&,int);
struct Inset {int x,y,w,h;};
struct ButtonFace {int input,cx,cy;double rx,ry;Inset phone,tag;const char* physical;bool horizontal=false;};
struct InkRun {std::string text;int cx=0,cy=0,font=0,w=0,h=0;bool tag=false;};
struct FittedInk {std::array<InkRun,9> runs;int count=0;int lock_x=0,lock_y=0;bool lock=false,main_fallback=false,tag_fallback=false;};
inline Ink measure_ink(const std::string& s,int px,InkMeasure measure){
    if(measure){const auto m=measure(s,px);if(m.ok)return {std::max(1,m.w),std::max(1,m.h),true};}
    return {std::max(1,host_menu::text_advance(s,px)),std::max(1,px),true};
}
inline bool inside_face(const ButtonFace& f,const Inset& box,int cx,int cy,int w,int h){
    const int l=cx-(w-1)/2,r=l+w-1,t=cy-(h-1)/2,b=t+h-1;
    if(w+2>box.w)return false;
    if(l<box.x||r>box.x+box.w-1||t<box.y||b>box.y+box.h-1)return false;
    for(double x:{l,r})for(double y:{t,b}){
        const double dx=(x-f.cx)/(f.rx-.65),dy=(y-f.cy)/(f.ry-.65);
        if(dx*dx+dy*dy>1.0)return false;
    }
    return true;
}
inline bool inside_tag(const Inset& box,int cx,int cy,int w,int h){
    const int l=cx-(w-1)/2,t=cy-(h-1)/2;
    return w+2<=box.w&&l>=box.x&&l+w<=box.x+box.w&&t>=box.y&&t+h<=box.y+box.h;
}
// Layout complete mapped tokens using actual glyph ink. Size reduction and
// wrapping happen before drawing; maxWidth is never used to hide overflow.
inline bool fit_button(const Label& label,const ButtonFace& face,InkMeasure measure,FittedInk& result){
    result=FittedInk{};
    const bool has_tag=*face.physical;
    const int tag_x=face.tag.x+face.tag.w/2,tag_y=face.tag.y+face.tag.h/2;
    InkRun fixed_tag;bool tag_ready=false;
    if(has_tag)for(int size=8;size>=6;--size){const auto tag=measure_ink(face.physical,size,measure);if(inside_tag(face.tag,tag_x,tag_y,tag.w,tag.h)){fixed_tag={face.physical,tag_x,tag_y,size,tag.w,tag.h,true};tag_ready=true;break;}}
    result.tag_fallback=has_tag&&!tag_ready;
    const bool has_main=label.count>0||face.input==Menu;
    if(!has_main){if(tag_ready){result.runs[0]=fixed_tag;result.count=1;}return true;}
    std::array<std::string,8> lines;std::array<Ink,8> metrics;
    for(int px=14;px>=1;--px)for(int wrap=0;wrap<3;++wrap){
        const int wrap_width=std::max(1,face.phone.w*(100-wrap*15)/100);
        for(auto& line:lines)line.clear();int row=0;bool valid=true;
        for(int token=0;token<std::max(1,label.count);++token){
            const std::string value=label.count?phone_name(label.phones[size_t(token)]):label.text;
            std::string next=lines[size_t(row)];if(!next.empty())next+='/';next+=value;
            if(measure_ink(next,px,measure).w>wrap_width&&!lines[size_t(row)].empty()){
                if(++row==int(lines.size())){valid=false;break;}lines[size_t(row)]=value;
            }else lines[size_t(row)]=std::move(next);
            if(measure_ink(lines[size_t(row)],px,measure).w>wrap_width){valid=false;break;}
        }
        if(!valid)continue;
        const int rows=row+1,gap=px>=8?2:px>=4?1:0;int main_h=gap*(rows-1);
        for(int n=0;n<rows;++n){metrics[size_t(n)]=measure_ink(lines[size_t(n)],px,measure);main_h+=metrics[size_t(n)].h;}
        FittedInk fitted;fitted.count=rows+(tag_ready?1:0);fitted.lock=face.input==Menu;fitted.tag_fallback=has_tag&&!tag_ready;
        const int main_x=face.phone.x+face.phone.w/2,main_y=face.phone.y+face.phone.h/2;
        const double main_top=main_y-(main_h-1)/2.0;
        if(tag_ready)fitted.runs[size_t(rows)]=fixed_tag;
        if(fitted.lock){fitted.lock_x=face.cx;fitted.lock_y=face.cy-15;}
        double top=main_top;
        for(int n=0;n<rows;++n){const auto m=metrics[size_t(n)];fitted.runs[size_t(n)]={lines[size_t(n)],int(std::lround(main_x)),int(std::lround(top+(m.h-1)/2.0)),px,m.w,m.h,false};top+=m.h+gap;}
        for(int n=0;n<rows;++n){const auto& run=fitted.runs[size_t(n)];if(!inside_face(face,face.phone,run.cx,run.cy,run.w,run.h)){valid=false;break;}}
        if(valid){result=std::move(fitted);return true;}
    }
    if(tag_ready){result.runs[0]=fixed_tag;result.count=1;}result.main_fallback=true;
    return false;
}
}
