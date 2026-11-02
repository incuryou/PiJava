#ifndef J2ME_GAME_DATA_EDITOR_H
#define J2ME_GAME_DATA_EDITOR_H
// Host-only weak search handles. Included after the runtime, before either host.
// No Value, Java root or borrowed heap address survives a public operation.
#include <limits>
namespace game_data {
inline bool parse_decimal(const std::string& text,int64_t& out) {
    if(text.empty())return false;
    size_t i=0;bool negative=false;
    if(text[0]=='-'||text[0]=='+'){negative=text[0]=='-';if(++i==text.size())return false;}
    uint64_t value=0,limit=negative?uint64_t(INT64_MAX)+1:uint64_t(INT64_MAX);
    for(;i<text.size();++i){if(text[i]<'0'||text[i]>'9')return false;unsigned d=unsigned(text[i]-'0');if(value>(limit-d)/10)return false;value=value*10+d;}
    out=negative?(value==uint64_t(INT64_MAX)+1?INT64_MIN:-int64_t(value)):int64_t(value);return true;
}
inline bool frozen() {
    return VMS_PLAT(g_host_frozen236)&&VMS_PLAT(g_host_paused)&&
        !VMS_EXEC(g_run_depth)&&!VMS_EXEC(g_pump_depth)&&!jit236::active_code_calls();
}
inline bool integer_kind(char k){return k=='B'||k=='S'||k=='C'||k=='I'||k=='J';}
inline bool in_range(char k,int64_t v){switch(k){case 'B':return v>=-128&&v<=127;case 'S':return v>=-32768&&v<=32767;case 'C':return v>=0&&v<=65535;case 'I':return v>=INT32_MIN&&v<=INT32_MAX;case 'J':return true;default:return false;}}
inline bool game_class(int cls) {
    if(cls<0||size_t(cls)>=VMS_CL(g_classes).size())return false;
    const auto& n=VMS_CL(g_classes)[cls].name;
    // JAR ownership plus reserved platform packages; obfuscated/default-package
    // games remain eligible. OEM APIs are excluded even if bundled by a game.
    for(const char* p:{"java/","javax/","j2me/","sun/","com/sun/","com/nokia/","com/siemens/","com/samsung/","com/sonyericsson/","com/motorola/","com/mascotcapsule/","com/vodafone/","com/j_phone/","org/w3c/","org/xml/"})if(n.compare(0,std::strlen(p),p)==0)return false;
    return VMS_CL(g_jar_files).count(n+".class")!=0;
}
class Session {
    struct Candidate {
        ResourceKey key;
        int object_class=-1,owner=-1,field=-1,slot=-1;
        size_t element=0;
        uint64_t layout=0;
        char kind=0;
        bool array=false,statik=false;
    };
    static constexpr size_t Limit=65536;
    std::vector<Candidate> results_,pending_;
    uint32_t epoch_=0;
    int64_t wanted_=0;
    bool busy_=false,complete_=false,refine_=false;
    char type_='I';
    size_t scan_class_=0,scan_field_=0,scan_object_=1,scan_element_=0,refine_index_=0;
    int owner_=-1;
    bool object_started_=false;
    static bool writable(const Field& f){return !(f.flags&0x10)&&f.desc.size()==1&&integer_kind(f.desc[0]);}
    static bool numeric(const Value& v,char kind){return !v.ref&&(kind=='J'?bool(v.is_long):(!v.is_long&&!v.l));}
    bool current()const{return epoch_&&epoch_==resver::g_epoch;}
    bool resolve(const Candidate& c,int64_t& out)const {
        if(!current()||c.key.epoch!=resver::g_epoch)return false;
        if(c.array){
            if(!resource_key_valid(c.key))return false;
            const auto& o=VMS_HEAP(g_heap)[c.key.slot];
            if(!o.is_array||o.array_desc!=std::string("[")+c.kind||o.elems.layout_version()!=c.layout||c.element>=o.elems.size())return false;
            Value v=o.elems.get(c.element);if(!numeric(v,c.kind))return false;out=c.kind=='J'?v.l:v.i;return true;
        }
        if(!game_class(c.owner))return false;
        const auto& cls=VMS_CL(g_classes)[c.owner];
        if(c.field<0||size_t(c.field)>=cls.fields.size())return false;
        const auto& f=cls.fields[c.field];
        if(!writable(f)||f.slot!=c.slot||f.desc[0]!=c.kind||f.is_static!=c.statik)return false;
        if(c.statik){if(cls.init_state!=2||c.slot<0||size_t(c.slot)>=cls.statics.size())return false;const auto& v=cls.statics[c.slot];if(!numeric(v,c.kind))return false;out=c.kind=='J'?v.l:v.i;return true;}
        if(!resource_key_valid(c.key))return false;
        const auto& o=VMS_HEAP(g_heap)[c.key.slot];
        if(o.is_array||o.cls!=c.object_class||c.slot<0||size_t(c.slot)>=o.fields.size())return false;
        const auto& v=o.fields[c.slot];if(!numeric(v,c.kind))return false;out=c.kind=='J'?v.l:v.i;return true;
    }
    void consider(const Candidate& c){
        if(type_!='*'&&c.kind!=type_)return;
        int64_t v;if(!resolve(c,v)||v!=wanted_)return;
        if(pending_.size()==Limit){results_.clear();pending_.clear();busy_=false;complete_=false;note="超过65536条，请换类型或更具体数值重新搜索";return;}
        pending_.push_back(c);
    }
    void finish(){results_.swap(pending_);pending_.clear();busy_=false;complete_=true;note="搜索完成："+std::to_string(results_.size())+"条";}
public:
    std::string note;
    void set_type(char kind){if(kind!='*'&&!integer_kind(kind))return;if(type_!=kind){reset();type_=kind;}}
    char type()const{return type_;}
    bool begin(int64_t value,bool refine) {
        if(!frozen()){note="请先暂停游戏再搜索";return false;}
        if(busy_){note="搜索正在进行中";return false;}
        if(epoch_&&!current())reset();
        if(refine&&!complete_){note="请重新搜索；结果过多时换类型或更具体数值";return false;}
        epoch_=resver::g_epoch;wanted_=value;refine_=refine;pending_.clear();busy_=true;
        scan_class_=scan_field_=scan_element_=refine_index_=0;scan_object_=1;owner_=-1;object_started_=false;
        note=refine?"正在缩小上轮结果…":"正在搜索游戏整数…";return true;
    }
    void step(size_t budget=16384) {
        if(!busy_)return;
        if(!valid())return;
        if(!frozen()){cancel();note="游戏已恢复，搜索已取消";return;}
        while(budget--&&busy_){
            if(refine_){if(refine_index_==results_.size()){finish();continue;}consider(results_[refine_index_++]);continue;}
            // Every field, array element, class/hierarchy transition and heap
            // slot consumes work, including slots which produce no matches.
            if(scan_class_<VMS_CL(g_classes).size()){
                const auto& cls=VMS_CL(g_classes)[scan_class_];
                if(!game_class(int(scan_class_))||cls.init_state!=2||scan_field_>=cls.fields.size()){++scan_class_;scan_field_=0;continue;}
                size_t fi=scan_field_++;const auto& f=cls.fields[fi];if(!f.is_static||!writable(f))continue;
                Candidate c;c.key.epoch=epoch_;c.owner=int(scan_class_);c.field=int(fi);c.slot=f.slot;c.kind=f.desc[0];c.statik=true;consider(c);continue;
            }
            if(scan_object_>=VMS_HEAP(g_heap).size()){finish();continue;}
            const auto& o=VMS_HEAP(g_heap)[scan_object_];
            if(!object_started_){object_started_=true;scan_element_=scan_field_=0;owner_=o.cls;
                if(!gc_slot_live(int(scan_object_))||(!o.is_array&&!game_class(o.cls))){++scan_object_;object_started_=false;}continue;}
            if(o.is_array){
                if(o.array_desc.size()!=2||!integer_kind(o.array_desc[1])||(type_!='*'&&o.array_desc[1]!=type_)||scan_element_>=o.elems.size()){++scan_object_;object_started_=false;continue;}
                Candidate c;c.key=resource_key_of(int(scan_object_));c.array=true;c.kind=o.array_desc[1];c.element=scan_element_++;c.layout=o.elems.layout_version();consider(c);continue;
            }
            if(owner_<0||size_t(owner_)>=VMS_CL(g_classes).size()){++scan_object_;object_started_=false;continue;}
            const auto& cls=VMS_CL(g_classes)[owner_];
            if(!game_class(owner_)||scan_field_>=cls.fields.size()){owner_=cls.super_id;scan_field_=0;continue;}
            size_t fi=scan_field_++;const auto& f=cls.fields[fi];if(f.is_static||!writable(f))continue;
            Candidate c;c.key=resource_key_of(int(scan_object_));c.object_class=o.cls;c.owner=owner_;c.field=int(fi);c.slot=f.slot;c.kind=f.desc[0];consider(c);
        }
    }
    bool busy()const{return busy_&&current();}
    size_t count()const{return current()?(busy_?pending_.size():results_.size()):0;}
    bool ready()const{return current()&&!busy_&&complete_;}
    bool valid(){if(!current()){if(epoch_) {reset();note="游戏已切换或读档，请重新搜索";}return false;}return true;}
    bool read(size_t index,int64_t& out)const{return frozen()&&!busy_&&complete_&&current()&&index<results_.size()&&resolve(results_[index],out);}
    bool write(size_t index,int64_t value){
        int64_t before;if(!read(index,before)){note="结果不可用，请暂停后重新搜索";return false;}
        const auto& c=results_[index];if(!in_range(c.kind,value)){note="数值超出当前整数类型范围";return false;}
        if(c.array)VMS_HEAP(g_heap)[c.key.slot].elems.set(c.element,c.kind=='J'?Value::of_l(value):Value::of_i(int32_t(value)));
        else {auto& v=c.statik?VMS_CL(g_classes)[c.owner].statics[c.slot]:VMS_HEAP(g_heap)[c.key.slot].fields[c.slot];if(c.kind=='J')v.l=value;else v.i=int32_t(value);}
        note="数值已修改";return true;
    }
    std::string label(size_t index)const{
        if(!frozen()||busy_||!complete_||!current()||index>=results_.size())return "";
        const auto& c=results_[index];int64_t v;if(!resolve(c,v))return "结果已失效";
        if(c.array)return "#"+std::to_string(c.key.slot)+" ["+c.kind+"["+std::to_string(c.element)+"]";
        return (c.statik?std::string("static "):"#"+std::to_string(c.key.slot)+" ")+VMS_CL(g_classes)[c.owner].name+"."+VMS_CL(g_classes)[c.owner].fields[c.field].name+" "+c.kind;
    }
    void reset(){results_.clear();pending_.clear();epoch_=0;busy_=complete_=false;note.clear();}
    void cancel(){if(!current()){reset();return;}pending_.clear();busy_=false;note=complete_?"已取消，保留上轮完整结果":"已取消搜索";}
};
}
#endif
