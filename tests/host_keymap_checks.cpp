#include <cstdio>
#include <cerrno>
#include <string>
static int fail_rename=0,calls=0;
static int checked_rename(const char* a,const char* b) {
    ++calls;
    if(fail_rename && std::string(a).find(".tmp")!=std::string::npos){errno=calls==1?EEXIST:EIO;return -1;}
    return std::rename(a,b);
}
#define HOST_MENU_SETTINGS_RENAME checked_rename
#include "../src/host_keymap_ui.h"
#include "../src/host_font.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <sys/stat.h>

using namespace host_keymap;
int main(int argc,char** argv) {
    assert(argc==5);
    Store legacy;assert(load(argv[4],legacy));
    std::ifstream old_config(argv[4],std::ios::binary);
    const std::string old_record((std::istreambuf_iterator<char>(old_config)),{});
    assert(old_record.rfind("J2ME_KEYMAP 1\n",0)==0);
    std::istringstream legacy_rows(old_record);std::string row;std::getline(legacy_rows,row);
    while(std::getline(legacy_rows,row)) {
        std::istringstream fields(row);std::string id,mode;fields>>id>>mode;
        const Config& migrated=id=="global"?legacy.global:legacy.games.at(id);
        assert(migrated.automatic==(mode=="auto"));
        if(mode=="custom")for(const auto& slots:migrated.bindings) {
            int first,second;assert(fields>>first>>second);
            assert(slots[0]==first && slots[1]==second && slots[2]==none);
        }
    }
    Store v2;const auto upgraded=serialize(legacy);assert(upgraded.rfind("J2ME_KEYMAP 2\n",0)==0);
    assert(parse(upgraded,v2) && serialize(v2)==upgraded);
    Store auto_v1;assert(parse("J2ME_KEYMAP 1\nglobal auto\n",auto_v1));
    assert(codes(auto_v1.global,false,1u<<RUp)==std::set<int>{50});


    Config c;
    assert(codes(c,false,1u<<A)==std::set<int>{53});
    assert(codes(c,false,1u<<Y)==std::set<int>{-5});
    assert(codes(c,true,1u<<Y)==std::set<int>{-5});
    assert(codes(c,true,1u<<A)==std::set<int>{-5});
    assert(codes(c,false,1u<<DUp)==std::set<int>{50});
    assert(codes(c,true,1u<<DUp)==std::set<int>{-1});
    for(bool standard:{false,true}){
        assert(codes(c,standard,1u<<SUp)==std::set<int>{-1});
        assert(codes(c,standard,1u<<SDown)==std::set<int>{-2});
        assert(codes(c,standard,1u<<SLeft)==std::set<int>{-3});
        assert(codes(c,standard,1u<<SRight)==std::set<int>{-4});
    }
    assert(codes(c,false,1u<<R3)==std::set<int>{48});
    for(bool standard:{false,true}) {
        assert(codes(c,standard,1u<<RUp)==std::set<int>{50});
        assert(codes(c,standard,1u<<RDown)==std::set<int>{56});
        assert(codes(c,standard,1u<<RLeft)==std::set<int>{52});
        assert(codes(c,standard,1u<<RRight)==std::set<int>{54});
    }
    assert(codes(c,false,~((1u<<InputCount)-1)).empty());
    assert(!assign(c,false,Fire,0,InputCount));
    assert(!assign(c,false,PhoneCount,0,A));
    assert(!assign(c,false,Fire,SlotCount,A));
    c.automatic=false;c.bindings=empty_bindings();
    assert(assign(c,false,Fire,0,R3));
    assert(assign(c,false,Fire,1,A));
    assert(effective(c,false)[Fire][0]==R3 && effective(c,false)[Fire][1]==A);
    assert(codes(c,false,(1u<<R3)|(1u<<A))==std::set<int>{-5});
    assert(assign(c,false,Fire,2,Y));assert(valid(c));
    Config triple_duplicate=c;triple_duplicate.bindings[Fire][2]=A;assert(!valid(triple_duplicate));
    Tracker tracker;std::vector<std::pair<int,bool>> events;
    const auto emit=[&](int k,bool r){events.emplace_back(k,r);};
    tracker.update(codes(c,false,1u<<A),emit);
    tracker.update(codes(c,false,(1u<<A)|(1u<<R3)),emit);
    tracker.update(codes(c,false,1u<<R3),emit);
    assert(events.size()==1 && events[0]==std::make_pair(-5,false));
    tracker.update({},emit);assert(events.size()==2 && events[1]==std::make_pair(-5,true));
    events.clear();
    tracker.update(codes(c,false,(1u<<A)|(1u<<R3)|(1u<<Y)),emit);
    tracker.update(codes(c,false,(1u<<A)|(1u<<Y)),emit);
    tracker.update(codes(c,false,1u<<Y),emit);assert(events.size()==1);
    tracker.update({},emit);assert(events.size()==2 && events[1]==std::make_pair(-5,true));
    Store store,loaded;const std::string id(64,'a'),other(64,'b'),path=std::string(argv[1])+"/keymap.cfg";
    assert(commit(path,store,id,c,false));assert(load(path,loaded));
    assert(!lookup(loaded,id).automatic && lookup(loaded,other).automatic);
    assert(lookup(loaded,id).bindings==c.bindings);
    assert(commit(path,store,other,Config{},false));
    assert(commit(path,store,id,c,true));assert(store.games.count(id)==0 && store.games.count(other)==1);
    assert(!lookup(store,id).automatic && lookup(store,other).automatic);
    Store unchanged=store;std::string good=serialize(store);
    const auto malformed_v2=std::string("J2ME_KEYMAP 2")+old_record.substr(old_record.find('\n'));
    assert(!parse(malformed_v2,loaded)); // v2 cannot silently accept only two slots
    const auto malformed_v1=std::string("J2ME_KEYMAP 1")+good.substr(good.find('\n'));
    assert(!parse(malformed_v1,loaded)); // v1 cannot consume a third slot as the next phone key

    for(const std::string& bad:{std::string("J2ME_KEYMAP 3\n"),good+"global auto\n",good+id+" custom 999\n",std::string(4*1024*1024+1,'x')}) {
        assert(!parse(bad,loaded));
    }
    Config shared=c;assert(assign(shared,false,N5,0,A));assert(assign(shared,false,Up,0,A));
    assert(valid(shared));assert((codes(shared,false,1u<<A)==std::set<int>{-5,-1,53}));
    assert(effective(shared,false)[Fire][1]==A && effective(shared,false)[N5][0]==A);
    assert(commit(path,store,id,shared,false));assert(load(path,loaded));
    assert((codes(lookup(loaded,id),false,1u<<A)==std::set<int>{-5,-1,53}));
    tracker.clear();events.clear();tracker.update(codes(shared,false,1u<<A),emit);
    assert(events.size()==3);tracker.update({},emit);assert(events.size()==6);
    for(int key:{-5,-1,53}){assert(std::count(events.begin(),events.end(),std::make_pair(key,false))==1);assert(std::count(events.begin(),events.end(),std::make_pair(key,true))==1);}
    assert(assign(shared,false,Fire,0,A));assert(shared.bindings[Fire][1]==none);
    assert(shared.bindings[N5][0]==A && shared.bindings[Up][0]==A);
    Config duplicate=c;duplicate.bindings[Fire][0]=A;assert(!valid(duplicate));
    // Restore the previous record for the failure/rollback contract below.
    assert(save(path,unchanged));store=unchanged;
    std::set<int> ordered;for(int i:choice_inputs)assert(ordered.insert(i).second);
    assert(ordered.size()==InputCount+1 && choice_inputs[0]==none);
    assert(choice_inputs[1]==A && choice_inputs[2]==B && choice_inputs[3]==X && choice_inputs[4]==Y);
    for(int input=none;input<InputCount;++input)assert(choice_inputs[choice_for_input(input)]==input);

    assert(!commit(path,store,"../escape",c,false));
    fail_rename=1;calls=0;assert(!commit(path,store,id,Config{},false));
    fail_rename=0;assert(serialize(store)==serialize(unchanged));assert(load(path,loaded));assert(serialize(loaded)==good);
    assert(std::rename(path.c_str(),(path+".previous").c_str())==0);assert(load(path,loaded));
    assert(save(path,loaded));
    static_assert(A==0 && R3==9 && DUp==10 && SRight==17,"append-only input IDs keep old configurations compatible");
    for(int input=0;input<InputCount;++input){Config all;all.automatic=false;assert(assign(all,false,Fire,0,input));assert(codes(all,false,1u<<input)==std::set<int>{-5});}
    Editor editor;editor.begin(store,id,"装甲特战队",false,false);editor.phone=Fire;
    assert(editor.page==Page::SaveExit);
    Editor slots_editor;slots_editor.begin(store,id,"三槽测试",false,false);act(slots_editor,Action::NextPage);
    act(slots_editor,Action::Confirm);assert(slots_editor.slot==0);
    act(slots_editor,Action::Down);assert(slots_editor.slot==1);
    act(slots_editor,Action::Down);assert(slots_editor.slot==2);
    act(slots_editor,Action::Confirm);slots_editor.choice=choice_for_input(RUp);
    act(slots_editor,Action::Confirm);assert(slots_editor.draft.bindings[N5][2]==RUp);
    act(slots_editor,Action::Down);assert(slots_editor.slot==0);
    act(slots_editor,Action::Up);assert(slots_editor.slot==2);
    assert(act(editor,Action::Confirm)==Result::SaveInstant);
    act(editor,Action::Global);act(editor,Action::Reset);assert(!editor.global);
    assert(act(editor,Action::Save)==Result::None);
    act(editor,Action::Down);assert(editor.save_selection==1);
    assert(act(editor,Action::Confirm)==Result::None && editor.pending==Pending::Delete);
    act(editor,Action::NextPage);assert(editor.page==Page::SaveExit);
    act(editor,Action::Back);assert(editor.pending==Pending::None);
    act(editor,Action::Confirm);assert(act(editor,Action::Confirm)==Result::DeleteInstant);
    act(editor,Action::Down);act(editor,Action::Confirm);assert(editor.pending==Pending::Exit);
    act(editor,Action::Back);act(editor,Action::Confirm);assert(act(editor,Action::Confirm)==Result::ExitGame);
    assert(act(editor,Action::Close)==Result::Close);
    act(editor,Action::NextPage);assert(editor.page==Page::Keymap);
    assert(editor.open && editor.phase==Phase::Phone);
    act(editor,Action::Confirm);assert(editor.phase==Phase::Slot);
    act(editor,Action::Confirm);assert(editor.phase==Phase::Choice);
    editor.choice=choice_for_input(R3);act(editor,Action::Confirm);assert(editor.phase==Phase::Slot);
    act(editor,Action::Global);assert(editor.global);
    assert(act(editor,Action::Save)==Result::Save);
    assert(act(editor,Action::Close)==Result::Close);
    act(editor,Action::Back);assert(editor.phase==Phase::Phone);
    assert(act(editor,Action::Back)==Result::Close);
    act(editor,Action::Reset);assert(editor.draft.automatic);
    editor.phone=Fire;move_phone(editor,0,-1);assert(editor.phone==Up);move_phone(editor,0,1);assert(editor.phone==Fire);
    std::ifstream fontfile(argv[2],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(fontfile)),{});
    host_font::Font font;assert(font.add_font(bytes.data(),bytes.size()));
    auto phone=load_phone(argv[3]);assert(phone.size()==227*518);
    constexpr size_t pitch=1297,guard=37;constexpr uint32_t sentinel=0x12345678;
    std::vector<uint32_t> buffer(guard+pitch*720+guard,sentinel);auto* frame=buffer.data()+guard;
    std::vector<std::pair<int,int>> centers;
    struct TextCall { int x,y;std::string label;uint32_t color;int size,maxw;bool centered; };
    std::vector<TextCall> text_calls;
    const auto captured=[&](const std::string& label)->const TextCall& {
        const auto call=std::find_if(text_calls.begin(),text_calls.end(),[&](const TextCall& c){return c.label==label;});
        assert(call!=text_calls.end());return *call;
    };
    auto text=[&](int x,int y,const std::string& t,uint32_t color,int size,int maxw){text_calls.push_back({x,y,t,color,size,maxw,false});assert(x>=0&&y>=0&&x+maxw<=1280&&y+size<=720);font.draw(frame,pitch,x,y,t,color,size,maxw);};
    auto center=[&](int x,int y,const std::string& t,uint32_t color,int size,int maxw){text_calls.push_back({x,y,t,color,size,maxw,true});if(t=="L3"||t=="R3")centers.emplace_back(x,y);font.draw_centered(frame,pitch,x,y,t,color,size,maxw);};
    auto right=[&](const std::string& label,int size){auto m=font.measure(label,size);return m.x+m.w;};
    editor.draft=Config{};editor.phase=Phase::Slot;editor.slot=1;editor.global=false;editor.note="";
    const auto dump=[&](const std::string& name) {
        std::ofstream out(std::string(argv[1])+"/"+name+".ppm",std::ios::binary);out<<"P6\n1280 720\n255\n";
        for(int y=0;y<720;++y)for(int x=0;x<1280;++x){uint32_t p=frame[y*pitch+x];char rgb[]={char(p&255),char((p>>8)&255),char((p>>16)&255)};out.write(rgb,3);}
    };
    const auto check_header=[&](Page page) {
        const auto& save_tab=captured("存档与退出");const auto& map_tab=captured("修改按键映射");
        const auto& l=captured("L");const auto& r=captured("R");
        assert(save_tab.centered && map_tab.centered);
        assert(save_tab.y==42 && map_tab.y==42 && l.y==42 && r.y==42);
        assert(save_tab.x+save_tab.maxw/2<map_tab.x-map_tab.maxw/2);
        assert(map_tab.x+map_tab.maxw/2<l.x-15 && l.x+15<r.x-15);
        for(const auto& call:text_calls)assert(call.label!=editor.name);
        const auto red=host_menu::color(0xc81e41),white=host_menu::color(0xffffff);
        assert(frame[42*pitch+34]==(page==Page::SaveExit?red:white));
        assert(frame[42*pitch+252]==(page==Page::Keymap?red:white));
    };
    const auto check_notice=[&](const std::string& label,uint32_t color) {
        const auto& call=captured(label);
        assert(!call.centered && call.x==354 && call.y==96 && call.size==17 && call.color==color);
    };
    editor.save_selection=0;editor.page=Page::SaveExit;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("save-exit");
    check_header(Page::SaveExit);
    check_notice("删除存档、退出游戏前会再次确认",host_menu::color(0x718094));
    editor.save_note="即时存档已保存";text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("instant-save-success");
    check_notice(editor.save_note,host_menu::color(0x208351));
    editor.save_note="即时存档保存失败";editor.save_note_error=true;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("instant-save-failure");
    check_notice(editor.save_note,host_menu::color(0xc81e41));
    editor.save_note="即时存档已删除";editor.save_note_error=false;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("instant-delete-success");
    check_notice(editor.save_note,host_menu::color(0x208351));
    editor.save_note="删除失败，请重试";editor.save_note_error=true;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("instant-delete-failure");
    check_notice(editor.save_note,host_menu::color(0xc81e41));
    editor.save_note="没有即时存档";editor.save_note_error=false;editor.save_note_neutral=true;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("instant-delete-absent");
    check_notice(editor.save_note,host_menu::color(0x718094));
    editor.save_note.clear();editor.save_note_neutral=false;
    assert(frame[150*pitch+80]==host_menu::color(0xf2f4f7)); // no phone on initial page
    editor.save_selection=1;editor.pending=Pending::Delete;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("delete-confirm");
    assert(captured("删除后将无法继续此即时存档。").size==17 && captured("请确认后执行。").size==17);
    editor.save_selection=2;editor.pending=Pending::Exit;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("exit-confirm");
    assert(captured("尚未保存的游戏进度将丢失。").size==17 && captured("请确认后执行。").size==17);
    editor.pending=Pending::None;editor.page=Page::Keymap;editor.phone=N2;editor.slot=2;
    text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("mapping-default");check_header(Page::Keymap);
    const auto game_name=editor.name;editor.name=std::string(160,'W');
    for(Page page:{Page::SaveExit,Page::Keymap}) {editor.page=page;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);check_header(page);}
    editor.name=game_name;editor.page=Page::Keymap;
    editor.phone=Fire;assign(editor.draft,false,Fire,1,A);assign(editor.draft,false,Fire,2,R3); // Three slots; A and R3 also retain their numeric bindings.
    render(frame,pitch,editor,phone,text,center,right);dump("mapping-editor");
    editor.phase=Phase::Choice;editor.choice=choice_for_input(X);render(frame,pitch,editor,phone,text,center,right);dump("mapping-selector");
    act(editor,Action::Reset);render(frame,pitch,editor,phone,text,center,right);dump("notice-reset");
    act(editor,Action::Global);render(frame,pitch,editor,phone,text,center,right);dump("notice-global");
    editor.note="映射保存成功";editor.note_error=false;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("notice-save");check_notice(editor.note,host_menu::color(0x208351));
    editor.note="保存失败，请重试";editor.note_error=true;text_calls.clear();render(frame,pitch,editor,phone,text,center,right);dump("notice-failure");check_notice(editor.note,host_menu::color(0xc81e41));
    for(size_t i=0;i<guard;++i)assert(buffer[i]==sentinel && buffer[guard+pitch*720+i]==sentinel);
    for(int y=0;y<720;++y)for(size_t x=1280;x<pitch;++x)assert(frame[y*pitch+x]==sentinel);
    assert(!centers.empty());
    // Verify the actual rasterized ink, not just anchor attributes.
    std::vector<std::string> symbols={"L3"};for(int input=0;input<InputCount;++input)symbols.push_back(input_symbol(input));
    for(const std::string& key:symbols)for(int size:{24,30,34}) {
        std::fill(buffer.begin(),buffer.end(),0xff2a2a2a);
        prompt(frame,pitch,100,100,key,size,center);
        int x0=1280,y0=720,x1=0,y1=0;
        for(int y=80;y<120;++y)for(int x=80;x<120;++x)if((frame[y*pitch+x]&255)>42){x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);}
        assert(std::abs((x0+x1)-200)<=2 && std::abs((y0+y1)-200)<=2);
    }
    std::puts("PASS keymap: AUTO numeric/standard, 23 assignable inputs, reserved L3, save/delete/exit confirmation, tab navigation, independent confirmation, triple-bind release, right stick numeric 2846 defaults, third-slot editor selection, shared physical key fans out and releases, ordered selector IDs, scoped persistence, global promotion, legacy v1 two-slot migration and v2 triple-slot roundtrip, parser rejection, FAT rollback/recovery, editor navigation, pixel guards, all 23 input icons and L3 ink centering, persistent two-tab headers and current red accent, game titles absent from both L3 pages, consistent 17px success/error/neutral feedback, confirmation typography, shared native renderer screenshots");
}
