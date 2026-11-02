#pragma once
// Reviewed compatibility for exact original JARs. Capture reads bounded editor
// state; writeback invokes only the same Java editing methods as its keypad UI.
namespace game_canvas_text {
struct Profile {
    const char *suite,*id,*slot,*overlay,*transition,*ready,*pending,*reset;
};
inline const Profile* profile(const std::string& suite) {
    static const Profile profiles[]={
        {"674638d85bd163cdbcd6f85f1a0e5f8f3099998844d68231916a47eb6dd16403","sims3_tc_mod","h","i","l","c","b","i"},
        {"3f83e98abb657769c588c98e16fa49cd23ff011ce5081a8f048b4338088cac08","sims3_ea_zh","j","k","m","d","c","k"}
    };
    for(const auto& p:profiles)if(suite==p.suite)return &p;
    return nullptr;
}
inline const Profile* profile(){return profile(VMS_PLAT(g_rms_suite));}
inline bool exact_class(int ref,const char* name) {
    return gc_slot_live(ref)&&!VMS_HEAP(g_heap)[ref].is_array&&
        VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].name==name&&
        VMS_CL(g_classes)[VMS_HEAP(g_heap)[ref].cls].init_state==2;
}
inline bool method_shape(int ref,const char* name,const char* desc,size_t bytes) {
    auto* m=find_method(VMS_HEAP(g_heap)[ref].cls,name,desc,false);
    return m&&!(m->flags&(0x0008|0x0100|0x0400))&&m->code.size()==bytes;
}
inline bool capture(game_text::Target& out) {
    using game_text::member;
    const auto* p=profile();
    if(!p||!game_text::quiet()||(VMS_PLAT(g_host_paused)&&!VMS_PLAT(g_host_frozen236)))return false;
    const int screen=VMS_RS(g_current);
    if(screen!=VMS_RS(g_shown)||!exact_class(screen,"r"))return false;
    const auto* menu=member(screen,"javax/microedition/lcdui/Displayable","commandMenu","Z");
    const auto* active=member(screen,"r","a","Lu;");
    const auto* overlay=member(screen,"r",p->overlay,"I");
    const auto* transition=member(screen,"r",p->transition,"Z");
    const auto* ready=member(screen,"r",p->ready,"Z");
    const auto* slot=member(screen,"r",p->slot,"I");
    const auto* names=member(screen,"r","g","[La;");
    if(!menu||menu->i||!active||!overlay||overlay->i!=-1||!transition||!ready||
       (transition->i&&!ready->i)||!slot||slot->i<0||slot->i>=3||!names||!gc_slot_live(names->ref))return false;
    const int controller=active->ref;
    if(!exact_class(controller,"x"))return false;
    const auto* back=member(controller,"u","a","Lr;");
    const auto* pending=member(controller,"x",p->pending,"I");
    const auto x=VMS_CL(g_class_index).find("x");
    const auto* mode=x==VMS_CL(g_class_index).end()?nullptr:find_field(x->second,"a","I");
    if(!back||back->ref!=screen||!pending||pending->i||!mode||!mode->is_static||mode->owner!=x->second||
       mode->slot<0||size_t(mode->slot)>=VMS_CL(g_classes)[mode->owner].statics.size()||
       VMS_CL(g_classes)[mode->owner].statics[mode->slot].i!=13)return false;
    const auto& list=VMS_HEAP(g_heap)[names->ref];
    if(!list.is_array||list.array_desc!="[La;"||list.elems.size()!=3)return false;
    const int buffer=list.elems.get(size_t(slot->i)).ref;
    if(!exact_class(buffer,"a"))return false;
    const auto* encoding=member(buffer,"a","a","B");
    const auto* offset=member(buffer,"a","a","I");
    const auto* length=member(buffer,"a","a","S");
    const auto* bytes=member(buffer,"a","a","[B");
    if(!encoding||uint8_t(encoding->i)!=0x80||!offset||offset->i||!length||length->i<0||length->i>8||
       !bytes||!gc_slot_live(bytes->ref))return false;
    const auto& data=VMS_HEAP(g_heap)[bytes->ref];
    if(!data.is_array||data.array_desc!="[B"||data.elems.size()!=16||
       !method_shape(screen,"q","()V",16)||!method_shape(screen,"b","(IC)V",63)||
       !method_shape(controller,p->reset,"()V",35)||!method_shape(screen,"a","(II)V",43))return false;
    std::string java;
    for(int i=0;i<length->i;++i) {
        const int cp=(uint8_t(data.elems.get(size_t(i)*2).i)<<8)|uint8_t(data.elems.get(size_t(i)*2+1).i);
        utf8_append(java,cp);
    }
    game_text::Target next;
    next.screen=resource_key_of(screen);next.field=resource_key_of(controller);
    next.canvas_buffer=resource_key_of(buffer);next.canvas_bytes=resource_key_of(bytes->ref);next.canvas_slot=slot->i;
    next.config.title="模拟市民姓名";next.config.max_size=8;next.config.single_bmp_name=true;
    if(!game_text::os_string(java,next.config.initial)||!host_text_input::supported(next.config))return false;
    out=std::move(next);return true;
}
inline game_text::Commit commit(const game_text::Target& target,const std::string& value) {
    using game_text::Commit;
    if(!game_text::quiet()||VMS_PLAT(g_host_paused)||!resource_key_valid(target.screen)||
       !resource_key_valid(target.field)||!resource_key_valid(target.canvas_buffer)||!resource_key_valid(target.canvas_bytes))return Commit::Stale;
    game_text::Target live;
    if(!capture(live)||!(target.screen==live.screen)||!(target.field==live.field)||
       !(target.canvas_buffer==live.canvas_buffer)||!(target.canvas_bytes==live.canvas_bytes)||
       target.canvas_slot!=live.canvas_slot||target.form||target.config.initial!=live.config.initial||
       target.config.max_size!=live.config.max_size||target.config.constraints!=live.config.constraints||
       target.config.multiline!=live.config.multiline||target.config.single_bmp_name!=live.config.single_bmp_name)return Commit::Stale;
    if(value==live.config.initial)return Commit::Unchanged;
    if(!host_text_input::valid_config(value,live.config))return Commit::Invalid;
    const auto* p=profile();if(!p)return Commit::Stale;
    int screen=target.screen.slot,controller=target.field.slot,buffer=target.canvas_buffer.slot,bytes=target.canvas_bytes.slot;
    RootScope roots(ReentrySite::DispatchKey,{&screen,&controller,&buffer,&bytes});
    // Look up each method at its invocation boundary: earlier callbacks may load
    // classes and move the class table. Never retain a borrowed Method pointer.
    auto invoke=[&](int receiver,const char* name,const char* desc,std::vector<Value> args={}) {
        auto* m=find_method(VMS_HEAP(g_heap)[receiver].cls,name,desc,false);
        if(!m)return false;
        args.insert(args.begin(),Value::of_ref(receiver));run_event_callback(m,args);
        return !VMS_EXEC(g_thrown);
    };
    if(!invoke(screen,"q","()V"))return Commit::Failed;
    int length=0;
    for(size_t i=0;i<value.size();) {
        const int cp=utf8_next(value,i);
        if(!invoke(screen,"b","(IC)V",{Value::of_i(length++),Value::of_i(cp)}))return Commit::Failed;
    }
    if(!invoke(controller,p->reset,"()V")||
       !invoke(screen,"a","(II)V",{Value::of_i(4),Value::of_i(length?2:0)}))return Commit::Failed;
    game_text::Target applied;
    if(!capture(applied)||!(target.screen==applied.screen)||!(target.field==applied.field)||
       !(target.canvas_buffer==applied.canvas_buffer)||!(target.canvas_bytes==applied.canvas_bytes)||
       target.canvas_slot!=applied.canvas_slot||applied.config.initial!=value)return Commit::Failed;
    VMS_RS(g_repaint)=true;VMS_RS(g_fb_dirty)=true;return Commit::Applied;
}
}
