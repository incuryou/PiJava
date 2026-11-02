#pragma once
#include "host_text_input.h"

// Standard MIDP input and verified Canvas adapters. No game JAR/schema changes.
namespace game_text {
enum class Commit { Applied, Unchanged, Invalid, Stale, Failed };
struct Target {
    ResourceKey screen,field;
    host_text_input::Config config;
    bool form=false;
    ResourceKey canvas_buffer,canvas_bytes;
    int canvas_slot=-1;
};
inline bool quiet() {
    return !VMS_EXEC(g_run_depth)&&!VMS_EXEC(g_pump_depth)&&!jit236::active_code_calls()&&
        !VMS_EXEC(g_thrown)&&!VMS_EXEC(g_notify_destroyed)&&VMS_EXEC(g_midlet_state)==MidletState::Active;
}
inline bool is_instance(int ref,const char* owner) {
    if(!gc_slot_live(ref)||VMS_HEAP(g_heap)[ref].is_array)return false;
    const auto it=VMS_CL(g_class_index).find(owner);
    return it!=VMS_CL(g_class_index).end()&&class_assignable(VMS_HEAP(g_heap)[ref].cls,it->second);
}
inline const Value* member(int ref,const char* owner,const char* name,const char* desc) {
    if(!gc_slot_live(ref))return nullptr;
    const auto it=VMS_CL(g_class_index).find(owner);
    if(it==VMS_CL(g_class_index).end())return nullptr;
    const auto* f=find_field(it->second,name,desc);
    const auto& o=VMS_HEAP(g_heap)[ref];
    return f&&!f->is_static&&f->slot>=0&&size_t(f->slot)<o.fields.size()?&o.fields[f->slot]:nullptr;
}
inline bool os_string(const std::string& java,std::string& out) {
    // The VM stores UTF-16 code units as CESU-8. Do not replace an unpaired
    // surrogate with '?', since editing would then silently destroy original data.
    std::string canonical;bool high=false;
    for(size_t i=0;i<java.size();) {
        const int cp=utf8_next(java,i);utf8_append(canonical,cp);
        if(high){if(cp<0xdc00||cp>0xdfff)return false;high=false;}
        else if(cp>=0xd800&&cp<=0xdbff)high=true;
        else if(cp>=0xdc00&&cp<=0xdfff)return false;
    }
    if(high||canonical!=java)return false;
    const auto bytes=encode_string(java,3);out.assign(bytes.begin(),bytes.end());return true;
}
inline std::string java_string(const std::string& text) {
    ArrayStorage bytes;bytes.reset("[B",text.size());
    for(size_t i=0;i<text.size();++i)bytes.set(i,Value::of_i(uint8_t(text[i])));
    std::string out;append_encoded(out,bytes,0,int(text.size()),3);return out;
}
inline bool capture_standard(Target& out) {
    if(!quiet()||(VMS_PLAT(g_host_paused)&&!VMS_PLAT(g_host_frozen236)))return false;
    const int screen=VMS_RS(g_current);
    if(!gc_slot_live(screen)||screen!=VMS_RS(g_shown))return false;
    const auto* menu=member(screen,"javax/microedition/lcdui/Displayable","commandMenu","Z");
    if(!menu||menu->i)return false;
    int field=0;bool form=false;
    if(is_instance(screen,"javax/microedition/lcdui/TextBox")) {
        const auto* v=member(screen,"javax/microedition/lcdui/TextBox","field","Ljavax/microedition/lcdui/TextField;");
        if(v)field=v->ref;
    } else if(is_instance(screen,"javax/microedition/lcdui/Form")) {
        form=true;
        const auto* focus=member(screen,"javax/microedition/lcdui/Form","focus","I");
        const auto* count=member(screen,"javax/microedition/lcdui/Form","count","I");
        const auto* items=member(screen,"javax/microedition/lcdui/Form","items","[Ljavax/microedition/lcdui/Item;");
        if(!focus||!count||!items||focus->i<0||focus->i>=count->i||!gc_slot_live(items->ref))return false;
        const auto& list=VMS_HEAP(g_heap)[items->ref];
        if(!list.is_array||size_t(focus->i)>=list.elems.size())return false;
        field=list.elems.get(size_t(focus->i)).ref;
    }
    if(!is_instance(field,"javax/microedition/lcdui/TextField"))return false;
    const auto* owner=member(field,"javax/microedition/lcdui/Item","owner","Ljavax/microedition/lcdui/Displayable;");
    const auto* text=member(field,"javax/microedition/lcdui/TextField","text","Ljava/lang/String;");
    const auto* max=member(field,"javax/microedition/lcdui/TextField","maxSize","I");
    const auto* constraints=member(field,"javax/microedition/lcdui/TextField","constraints","I");
    if(!owner||owner->ref!=screen||!text||!gc_slot_live(text->ref)||!max||!constraints)return false;
    Target next;next.screen=resource_key_of(screen);next.field=resource_key_of(field);next.form=form;
    if(!os_string(read_string(text->ref),next.config.initial))return false;
    next.config.max_size=max->i;next.config.constraints=constraints->i;next.config.multiline=!form;
    const auto* title=member(screen,"javax/microedition/lcdui/Displayable","title","Ljava/lang/String;");
    const auto* label=member(field,"javax/microedition/lcdui/Item","label","Ljava/lang/String;");
    const std::string java_title=label&&label->ref?read_string(label->ref):title&&title->ref?read_string(title->ref):"输入文字";
    if(!os_string(java_title,next.config.title))next.config.title="输入文字";
    if(!host_text_input::supported(next.config))return false;
    out=std::move(next);return true;
}
inline Commit commit_standard(const Target& target,const std::string& value) {
    if(!quiet()||VMS_PLAT(g_host_paused)||!resource_key_valid(target.screen)||!resource_key_valid(target.field))return Commit::Stale;
    Target live;
    if(!capture_standard(live)||!(target.screen==live.screen)||!(target.field==live.field)||target.form!=live.form||
       target.config.initial!=live.config.initial||target.config.max_size!=live.config.max_size||
       target.config.constraints!=live.config.constraints)return Commit::Stale;
    if(!host_text_input::valid_config(value,target.config))return Commit::Invalid;
    if(value==target.config.initial)return Commit::Unchanged;
    int field=target.field.slot,screen=target.screen.slot,string=0;
    RootScope roots(ReentrySite::DispatchKey,{&field,&screen,&string});
    string=make_string(java_string(value));
    auto* setter=find_method(VMS_HEAP(g_heap)[field].cls,"setString","(Ljava/lang/String;)V",true);
    if(!setter)return Commit::Failed;
    run_event_callback(setter,{Value::of_ref(field),Value::of_ref(string)});
    if(VMS_EXEC(g_thrown))return Commit::Failed;
    // A user setter may replace the screen or detach its item. Never deliver
    // an item-state event to a different view after that callback.
    if(!resource_key_valid(target.screen)||!resource_key_valid(target.field))return Commit::Stale;
    const auto* owner=member(field,"javax/microedition/lcdui/Item","owner","Ljavax/microedition/lcdui/Displayable;");
    if(target.form&&owner&&owner->ref==screen&&VMS_RS(g_current)==screen) {
        auto* changed=find_method(VMS_HEAP(g_heap)[screen].cls,"itemChanged","(Ljavax/microedition/lcdui/Item;)V",true);
        if(changed)run_event_callback(changed,{Value::of_ref(screen),Value::of_ref(field)});
    }
    VMS_RS(g_repaint)=true;VMS_RS(g_fb_dirty)=true;
    return VMS_EXEC(g_thrown)?Commit::Failed:Commit::Applied;
}
} // namespace game_text
#include "game_canvas_text_input.h"
namespace game_text {
inline bool capture(Target& target){return capture_standard(target)||game_canvas_text::capture(target);}
inline Commit commit(const Target& target,const std::string& value){
    return target.canvas_slot>=0?game_canvas_text::commit(target,value):commit_standard(target,value);
}
}
