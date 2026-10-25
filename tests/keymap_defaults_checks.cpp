#include "../src/host_keymap.h"
#include "../src/controller_skin_labels.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char* m){if(!ok){std::fprintf(stderr,"ASSERT %s\n",m);std::exit(1);}}
int main(int argc,char** argv){
    using namespace host_keymap;require(argc==2,"isolated keymap path");
    const int sticks[]={SUp,SDown,SLeft,SRight},phones[]={Up,Down,Left,Right},numbers[]={N2,N8,N4,N6};
    const int dpad[]={DUp,DDown,DLeft,DRight},right[]={RUp,RDown,RLeft,RRight},arrows[]={-1,-2,-3,-4},digits[]={50,56,52,54};
    for(bool standard:{false,true}){
        const auto b=defaults(standard);Config automatic;
        for(int n=0;n<4;++n){
            require(std::count(b[phones[n]].begin(),b[phones[n]].end(),sticks[n])==1,"phone arrow has its default left-stick direction in both automatic modes");
            for(int slot:b[numbers[n]])require(slot<SUp||slot>SRight,"numeric 2468 has no left-stick binding");
            require(codes(automatic,standard,1u<<sticks[n])==std::set<int>{arrows[n]},"left stick emits phone arrow only, without the old numeric code");
            require(codes(automatic,standard,1u<<dpad[n])==std::set<int>{standard?arrows[n]:digits[n]},"DPad automatic standard/numeric behavior remains unchanged");
            require(codes(automatic,standard,1u<<right[n])==std::set<int>{digits[n]},"right-stick numeric 2468 remains unchanged");
        }
        host_controller::Legends labels;labels.update(automatic,standard,4);
        require(labels.label(SUp).text=="↑"&&labels.label(SDown).text=="↓"&&labels.label(SLeft).text=="←"&&labels.label(SRight).text=="→","dynamic controller labels use the new effective arrow bindings");
    }
    Store saved;Config custom;custom.automatic=false;custom.bindings=empty_bindings();
    for(int n=0;n<4;++n)custom.bindings[numbers[n]][1]=sticks[n];saved.global=custom;
    const auto serialized=serialize(saved);Store parsed;require(parse(serialized,parsed)&&serialize(parsed)==serialized,"custom legacy values parse without migration");
    require(save(argv[1],saved),"save explicit custom bindings");Store restored;require(load(argv[1],restored)&&serialize(restored)==serialized,"custom save/load preserves original slots exactly");
    for(bool standard:{false,true})for(int n=0;n<4;++n)require(codes(restored.global,standard,1u<<sticks[n])==std::set<int>{digits[n]},"explicit custom mappings retain their saved numeric behavior");
    Editor e;e.begin(restored,std::string(64,'a'),"test",false,false);e.page=Page::Keymap;act(e,Action::Reset);
    require(e.draft.automatic,"X reset selects the automatic defaults");
    for(int n=0;n<4;++n)require(codes(e.draft,false,1u<<sticks[n])==std::set<int>{arrows[n]},"X reset uses new left-stick arrow defaults");
    require(serialize(restored)==serialized,"unsaved reset never changes persisted custom settings");
    std::remove(argv[1]);std::puts("PASS KeymapDefaults: four left-stick arrows, two modes, unchanged DPad/right stick, custom roundtrip and X reset");
}
