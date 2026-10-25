#pragma once
// Host configuration only: never part of VM serialization or JAR identity.
#include "host_menu_settings.h"
#include <array>
#include <map>
#include <set>
#include <sstream>

namespace host_keymap {
enum Phone { SoftL, Up, SoftR, Left, Fire, Right, Down, N1, N2, N3, N4, N5, N6, N7, N8, N9, Star, N0, Hash, PhoneCount };
// L3 is the only reserved application input. IDs are append-only for config compatibility.
enum Input { A, B, X, L, R, ZL, ZR, Plus, Minus, R3, DUp, DDown, DLeft, DRight, SUp, SDown, SLeft, SRight, Y, RUp, RDown, RLeft, RRight, InputCount };
constexpr int none = -1;
inline bool is_direction(int i){return (i>=DUp&&i<=SRight)||(i>=RUp&&i<=RRight);}
constexpr int phone_codes[] = {-6,-1,-7,-3,-5,-4,-2,49,50,51,52,53,54,55,56,57,42,48,35};
constexpr const char* phone_names[] = {"左软键","↑","右软键","←","确认","→","↓","1","2","3","4","5","6","7","8","9","*","0","#"};
constexpr const char* input_names[] = {"A","B","X","L","R","ZL","ZR","+","−","R3","十字↑","十字↓","十字←","十字→","左摇↑","左摇↓","左摇←","左摇→","Y","右摇↑","右摇↓","右摇←","右摇→"};
// Display order is independent of the stable IDs stored in keymap.cfg.
constexpr std::array<int,InputCount+1> choice_inputs = {{none,A,B,X,Y,L,R,ZL,ZR,Plus,Minus,
    DUp,DDown,DLeft,DRight,SUp,SDown,SLeft,SRight,RUp,RDown,RLeft,RRight,R3}};
inline int choice_for_input(int input) {
    for(int choice=0;choice<=InputCount;++choice)if(choice_inputs[choice]==input)return choice;
    return 0;
}
constexpr int SlotCount=3;
using Bindings = std::array<std::array<int,SlotCount>, PhoneCount>;
inline Bindings empty_bindings() { Bindings b{}; for(auto& slots:b)slots.fill(none);return b; }
inline Bindings defaults(bool standard) {
    Bindings b=empty_bindings();
    b[SoftL][0]=X;b[SoftR][0]=B;b[standard?Fire:N5][0]=A;
    b[Fire][standard?1:0]=Y;
    b[N1][0]=L;b[N3][0]=R;b[N7][0]=ZL;b[N9][0]=ZR;
    b[Star][0]=Minus;b[Hash][0]=Plus;b[N0][0]=R3;
    b[standard?Up:N2][0]=DUp;b[standard?Down:N8][0]=DDown;
    b[standard?Left:N4][0]=DLeft;b[standard?Right:N6][0]=DRight;
    b[Up][standard?1:0]=SUp;b[Down][standard?1:0]=SDown;
    b[Left][standard?1:0]=SLeft;b[Right][standard?1:0]=SRight;
    b[N2][2]=RUp;b[N8][2]=RDown;b[N4][2]=RLeft;b[N6][2]=RRight;
    return b;
}
struct Config { bool automatic=true; Bindings bindings=empty_bindings(); };
inline Bindings effective(const Config& c,bool standard) { return c.automatic?defaults(standard):c.bindings; }
inline bool valid(const Config& c) {
    for(const auto& slots:c.bindings) {
        for(int i:slots)if(i<none || i>=InputCount)return false;
        for(int a=0;a<SlotCount;++a)for(int b=a+1;b<SlotCount;++b)
            if(slots[a]!=none && slots[a]==slots[b])return false;
    }
    return true;
}
inline bool assign(Config& c,bool standard,int phone,int slot,int input) {
    if(phone<0 || phone>=PhoneCount || slot<0 || slot>=SlotCount || input<none || input>=InputCount)return false;
    c.bindings=effective(c,standard);c.automatic=false;
    // Other phone keys may intentionally share this physical input.
    if(input!=none)for(int other=0;other<SlotCount;++other)if(other!=slot && c.bindings[phone][other]==input)c.bindings[phone][other]=none;
    c.bindings[phone][slot]=input;return true;
}
inline std::set<int> codes(const Config& c,bool standard,uint32_t held) {
    std::set<int> out;const auto b=effective(c,standard);
    for(int p=0;p<PhoneCount;++p)for(int i:b[p])if(i>=0 && i<InputCount && (held&(1u<<i)))out.insert(phone_codes[p]);
    return out;
}
struct Tracker {
    std::set<int> pressed;
    template<class Emit> void update(const std::set<int>& next,Emit emit) {
        for(int key:pressed)if(!next.count(key))emit(key,true);
        for(int key:next)if(!pressed.count(key))emit(key,false);
        pressed=next;
    }
    void clear(){pressed.clear();}
};
struct Store { Config global;std::map<std::string,Config> games; };
inline bool valid_id(const std::string& id) {
    if(id.size()!=64)return false;
    return id.find_first_not_of("0123456789abcdef")==std::string::npos;
}
inline Config lookup(const Store& s,const std::string& id) {
    auto it=s.games.find(id);return it==s.games.end()?s.global:it->second;
}
// A compact host file is read once, not one file per JAR or per button press.
inline bool parse(const std::string& record,Store& out) {
    if(record.size()>4*1024*1024)return false;
    std::istringstream stream(record);std::string line;
    if(!std::getline(stream,line))return false;
    const int stored_slots=line=="J2ME_KEYMAP 1"?2:line=="J2ME_KEYMAP 2"?SlotCount:0;
    if(!stored_slots)return false;
    Store loaded;bool saw_global=false;
    while(std::getline(stream,line)) {
        std::istringstream row(line);std::string id,mode,token;Config c;
        if(!(row>>id>>mode) || (id!="global"&&!valid_id(id)))return false;
        if(mode=="custom") {
            c.automatic=false;
            for(auto& slots:c.bindings)for(int slot=0;slot<stored_slots;++slot) {
                auto& value=slots[slot];
                if(!(row>>token))return false;
                if(token=="-1")value=none;
                else {
                    if(token.empty() || token.size()>2 || token.find_first_not_of("0123456789")!=std::string::npos)return false;
                    value=0;for(char digit:token)value=value*10+digit-'0';
                }
            }
        } else if(mode!="auto")return false;
        if(row>>token || !valid(c))return false;
        if(id=="global") {if(saw_global)return false;saw_global=true;loaded.global=c;}
        else if(!loaded.games.emplace(id,c).second || loaded.games.size()>8192)return false;
    }
    if(!saw_global)return false;
    out=std::move(loaded);return true;
}
inline std::string serialize(const Store& s) {
    std::ostringstream out;out<<"J2ME_KEYMAP 2\n";
    const auto row=[&](const std::string& id,const Config& c) {
        out<<id<<(c.automatic?" auto":" custom");
        if(!c.automatic)for(auto slots:c.bindings)for(int i:slots)out<<' '<<i;
        out<<'\n';
    };
    row("global",s.global);for(const auto& entry:s.games)row(entry.first,entry.second);return out.str();
}
inline bool load(const std::string& path,Store& out) {
    using namespace host_menu::settings_detail;
    const auto status=file_status(path);
    const std::string source=status==FileStatus::missing?path+".previous":path;
    if(status==FileStatus::error)return false;
    std::ifstream file(source,std::ios::binary);if(!file)return false;
    std::string record;char buffer[4096];
    while(file) {file.read(buffer,sizeof buffer);record.append(buffer,size_t(file.gcount()));if(record.size()>4*1024*1024)return false;}
    return !file.bad() && parse(record,out);
}
inline bool save(const std::string& path,const Store& s) {
    if(s.games.size()>8192 || !valid(s.global))return false;
    for(const auto& e:s.games)if(!valid_id(e.first)||!valid(e.second))return false;
    return host_menu::save_record(path,serialize(s));
}
inline bool commit(const std::string& path,Store& s,const std::string& id,const Config& c,bool global) {
    if(!valid_id(id)||!valid(c))return false;
    Store next=s;
    if(global) {next.global=c;next.games.erase(id);}else next.games[id]=c;
    if(!save(path,next))return false;
    s=std::move(next);return true;
}
enum class Phase { Phone, Slot, Choice };
enum class Page { SaveExit, Keymap };
enum class Pending { None, Delete, Exit };
struct ChoiceRepeat {
    int direction=0;int64_t next=0;
    void reset(){*this=ChoiceRepeat{};}
    int update(bool eligible,int held,bool pressed,int64_t now,bool suppress=false){
        if(!eligible||!held||suppress){reset();return 0;}
        if(pressed||held!=direction){direction=held;next=now+320;return held;}
        if(now<next)return 0;
        next=now+80;return held;
    }
};
struct Editor {
    bool open=false,global=false,standard=false,was_paused=false,note_error=false;
    int phone=N5,slot=0,choice=0;
    Phase phase=Phase::Phone;
    Page page=Page::SaveExit;
    Pending pending=Pending::None;
    int save_selection=0;
    bool save_note_error=false,save_note_neutral=false;
    std::string save_note;
    Config draft;
    ChoiceRepeat choice_repeat;
    std::string name,id,note;
    void begin(const Store& s,const std::string& game_id,const std::string& game_name,bool stdkeys,bool paused) {
        *this=Editor{};open=true;id=game_id;name=game_name;standard=stdkeys;was_paused=paused;draft=lookup(s,id);
    }
};
inline std::pair<int,int> position(int p) {
    if(p<6)return {p%3,p/3};
    if(p==Down)return {1,2};
    return {(p-N1)%3,3+(p-N1)/3};
}
inline void move_phone(Editor& e,int dx,int dy) {
    const auto pos=position(e.phone);
    for(int step=1;step<7;++step) {
        int x=pos.first+dx*step,y=pos.second+dy*step;
        if(x<0||x>2||y<0||y>6)return;
        for(int p=0;p<PhoneCount;++p)if(position(p)==std::make_pair(x,y)){e.phone=p;return;}
    }
}
enum class Action { Up, Down, Left, Right, Confirm, Back, Reset, Global, Save, Close, PreviousPage, NextPage };
enum class Result { None, Close, Save, SaveInstant, DeleteInstant, ExitGame };
inline Result act(Editor& e,Action a) {
    if(a!=Action::Up&&a!=Action::Down&&a!=Action::Left&&a!=Action::Right)e.choice_repeat.reset();
    if(a==Action::Close)return Result::Close;
    if(a==Action::PreviousPage || a==Action::NextPage) {
        if(e.pending==Pending::None)e.page=e.page==Page::SaveExit?Page::Keymap:Page::SaveExit;
        return Result::None;
    }
    if(e.page==Page::SaveExit) {
        if(e.pending!=Pending::None) {
            if(a==Action::Back)e.pending=Pending::None;
            else if(a==Action::Confirm){const auto result=e.pending==Pending::Delete?Result::DeleteInstant:Result::ExitGame;e.pending=Pending::None;return result;}
        } else if(a==Action::Back)return Result::Close;
        else if(a==Action::Up)e.save_selection=std::max(0,e.save_selection-1);
        else if(a==Action::Down)e.save_selection=std::min(2,e.save_selection+1);
        else if(a==Action::Confirm) {
            if(e.save_selection==0)return Result::SaveInstant;
            e.pending=e.save_selection==1?Pending::Delete:Pending::Exit;
        }
        return Result::None;
    }
    if(a==Action::Save)return Result::Save;
    if(a==Action::Reset){e.draft=Config{};e.phase=Phase::Phone;e.note="已恢复默认，请按 + 保存";e.note_error=false;return Result::None;}
    if(a==Action::Global){e.global=!e.global;e.note=e.global?"已切换为全局映射，请按 + 保存":"已切换为当前游戏映射，请按 + 保存";e.note_error=false;return Result::None;}
    if(a==Action::Back) {
        if(e.phase==Phase::Choice)e.phase=Phase::Slot;
        else if(e.phase==Phase::Slot)e.phase=Phase::Phone;
        else return Result::Close;
    } else if(a==Action::Confirm) {
        if(e.phase==Phase::Phone){e.phase=Phase::Slot;e.slot=0;}
        else if(e.phase==Phase::Slot){e.phase=Phase::Choice;e.choice=choice_for_input(effective(e.draft,e.standard)[e.phone][e.slot]);}
        else {assign(e.draft,e.standard,e.phone,e.slot,choice_inputs[e.choice]);e.phase=Phase::Slot;e.note="已修改，按 + 保存";e.note_error=false;}
    } else {
        int delta=a==Action::Down||a==Action::Right?1:-1;
        if(e.phase==Phase::Phone)move_phone(e,a==Action::Left?-1:a==Action::Right?1:0,a==Action::Up?-1:a==Action::Down?1:0);
        else if(e.phase==Phase::Slot)e.slot=(e.slot+delta+SlotCount)%SlotCount;
        else e.choice=(e.choice+delta+InputCount+1)%(InputCount+1);
    }
    return Result::None;
}
} // namespace host_keymap
