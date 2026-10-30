// VM integration for reviewed, bounded, non-suspending library operations.
// Host frontend, Java object layout and checkpoint format are not part of this API.
#include "library_index.h"
#include "reviewed_library_manifest.h"
 VMS_NATIVE_ACCESSOR(g_library_indexes, j2me_runtime::LibraryIndex)
 VMS_NATIVE_ACCESSOR(g_index_key_epoch, uint64_t) VMS_NATIVE_ACCESSOR(g_library_leaf_calls, uint64_t)
enum LibraryClass : uint8_t { LibUnknown,LibHashtable,LibCharacter,LibInteger,LibLong,LibByte,LibShort,LibBoolean,LibByteArrayStream,LibDataStream,LibString,LibVector };
static_assert(LibString==j2me_runtime::ScalarKey::string_kind,"reviewed String key kind");
enum LibraryIntrinsic : uint8_t { LibNone,LibFind,LibEquals,LibScalarValue,LibVectorSize,LibVectorGet,LibVectorSet };
static void bind_library_layout(int cid,const std::vector<uint8_t>& bytes){
#if J2ME_OPT_LIBRARY_INDEX
    auto& c=VMS_CL(g_classes)[cid];
    for(const auto& reviewed:kReviewedLibraryClasses){
        if(c.name!=reviewed.name)continue;
        if(rms_jar_identity(bytes.data(),bytes.size())!=reviewed.sha256)return;
        if(reviewed.id==LibByteArrayStream||reviewed.id==LibDataStream||reviewed.id==LibString){c.trusted_library=reviewed.id;return;}
        if(reviewed.id==LibHashtable||reviewed.id==LibVector){
            auto* keys=find_field(cid,reviewed.id==LibVector?"elementData":"keys","[Ljava/lang/Object;");
            auto* count=find_field(cid,reviewed.id==LibVector?"elementCount":"count","I");
            if(!keys||!count||keys->is_static||count->is_static)return;
            c.library_keys_slot=keys->slot;c.library_count_slot=count->slot;
        }else{
            const char* desc=reviewed.id==LibCharacter?"C":reviewed.id==LibLong?"J":reviewed.id==LibByte?"B":reviewed.id==LibShort?"S":reviewed.id==LibBoolean?"Z":"I";
            auto* value=find_field(cid,"value",desc);
            if(!(c.flags&0x0010)||!value||value->is_static||!(value->flags&0x0010))return;
            c.library_value_slot=value->slot;
        }
        c.trusted_library=reviewed.id;return;
    }
#else
    (void)cid;(void)bytes;
#endif
}
static int library_intrinsic_kind(const Method* method){
#if J2ME_OPT_LIBRARY_INDEX
    if(!method||method->native||(method->flags&0x0008))return LibNone;
    const int kind=VMS_CL(g_classes)[method->class_id].trusted_library;
    if(kind==LibVector){
        if(!(method->flags&0x0020))return LibNone;
        if(method->name=="size"&&method->desc=="()I")return LibVectorSize;
        if(method->name=="elementAt"&&method->desc=="(I)Ljava/lang/Object;")return LibVectorGet;
        if(method->name=="setElementAt"&&method->desc=="(Ljava/lang/Object;I)V")return LibVectorSet;
    }
    if(method->flags&0x0020)return LibNone;
    if(kind==LibHashtable && (method->flags&0x0002) && method->name=="find" && method->desc=="(Ljava/lang/Object;)I")return LibFind;
    if(kind>=LibCharacter && kind<=LibBoolean){
        if(method->name=="equals"&&method->desc=="(Ljava/lang/Object;)Z")return LibEquals;
        if((kind==LibCharacter&&method->name=="charValue"&&method->desc=="()C")||
           (kind==LibInteger&&method->name=="intValue"&&method->desc=="()I")||
           (kind==LibLong&&method->name=="longValue"&&method->desc=="()J")||
           (kind==LibByte&&method->name=="byteValue"&&method->desc=="()B")||
           (kind==LibShort&&method->name=="shortValue"&&method->desc=="()S")||
           (kind==LibBoolean&&method->name=="booleanValue"&&method->desc=="()Z"))return LibScalarValue;
    }
#endif
    return LibNone;
}
static unsigned library_intrinsic_ops(int kind){
    // Exact successful bytecode counts in the pinned Vector, including the
    // private checkIndex call. Callers retain the next original safepoint.
    return kind==LibVectorSize?3:kind==LibVectorGet?15:kind==LibVectorSet?16:0;
}
static bool library_scalar_key(int ref,j2me_runtime::ScalarKey& key){
    if(ref<=0||size_t(ref)>=VMS_HEAP(g_heap).size())return false;
    auto& o=VMS_HEAP(g_heap)[ref];if(o.cls<0)return false;const auto& c=VMS_CL(g_classes)[o.cls];
    if(c.trusted_library==LibString){
        // Only this exact reviewed class is bound. A subclass may override
        // equals, and a custom stored key may compare equal to a String.
        // All such tables retain the original ordered Java callback path.
        if(o.is_array||o.text.size()>j2me_runtime::LibraryIndex::max_string_bytes)return false;
        uint64_t hash=1469598103934665603ULL;for(unsigned char ch:o.text){hash^=ch;hash*=1099511628211ULL;}
        key.value=hash;key.kind=LibString;key.text=o.text;o.indexed_key=true;return true;
    }
    if(c.trusted_library<LibCharacter||c.trusted_library>LibBoolean||c.library_value_slot<0||size_t(c.library_value_slot)>=o.fields.size())return false;
    const auto& value=o.fields[c.library_value_slot];
    key.kind=c.trusted_library;key.value=c.trusted_library==LibLong?uint64_t(value.l):uint32_t(value.i);key.text={};
    o.indexed_key=true;return true;
}
static bool library_intrinsic_dispatch(int kind,const Method* method,Value* args,Value& result){
#if J2ME_OPT_LIBRARY_INDEX
    const int self=args[0].ref;
    if(self<=0||size_t(self)>=VMS_HEAP(g_heap).size()||VMS_HEAP(g_heap)[self].cls!=method->class_id)return false;
    const auto& cls=VMS_CL(g_classes)[method->class_id];
    if(kind==LibVectorSize||kind==LibVectorGet||kind==LibVectorSet){
        if(cls.trusted_library!=LibVector||cls.init_state!=2||VMS_HEAP(g_heap)[self].is_array||args[0].i||args[0].l||args[0].is_long)return false;
        // A contended call must create the ordinary Java callee frame. Its
        // existing initialization/monitor entry preserves blocking and retry
        // PCs, including exception paths, without a second native protocol.
        auto locked=VMS_EXEC(g_monitors).find(self);
        if(locked!=VMS_EXEC(g_monitors).end()&&locked->second.owner&&locked->second.owner!=&current_context())return false;
        const auto& fields=VMS_HEAP(g_heap)[self].fields;
        if(cls.library_keys_slot<0||cls.library_count_slot<0||
           size_t(cls.library_keys_slot)>=fields.size()||size_t(cls.library_count_slot)>=fields.size())return false;
        const auto& data_value=fields[cls.library_keys_slot];const auto& count_value=fields[cls.library_count_slot];
        if(data_value.i||data_value.l||data_value.is_long||count_value.ref||count_value.l||count_value.is_long)return false;
        const int data=data_value.ref,count=count_value.i;
        if(data<=0||size_t(data)>=VMS_HEAP(g_heap).size()||count<0)return false;
        auto& array=VMS_HEAP(g_heap)[data];
        if(!array.is_array||array.array_desc!="[Ljava/lang/Object;"||!array.elems.references()||size_t(count)>array.elems.size())return false;
        const int index=kind==LibVectorGet?args[1].i:kind==LibVectorSet?args[2].i:0;
        if(kind!=LibVectorSize&&(index<0||index>=count))return false;
        if(kind!=LibVectorSize){const auto& value=args[kind==LibVectorGet?1:2];if(value.ref||value.l||value.is_long)return false;}
        auto valid_ref=[](int ref){
            if(!ref)return true;
            if(ref<0||size_t(ref)>=VMS_HEAP(g_heap).size())return false;
            const auto& obj=VMS_HEAP(g_heap)[ref];
            return obj.is_array||(obj.cls>=0&&size_t(obj.cls)<VMS_CL(g_classes).size());
        };
        if(kind==LibVectorGet&&!valid_ref(array.elems.get(index).ref))return false;
        if(kind==LibVectorSet&&(args[1].i||args[1].l||args[1].is_long||!valid_ref(args[1].ref)))return false;
        // One host executor and no callback, Java allocation or safepoint between
        // this free/owned observation and exit. Reentrant depth is balanced by
        // the same monitor functions used by ordinary Java invocations.
        if(!monitor_enter(self))return false;
        if(kind==LibVectorSize)result=Value::of_i(count);
        else if(kind==LibVectorGet)result=array.elems.get(index);
        else array.elems.set(index,args[1]);
        monitor_exit(self);++VMS_NATIVE(g_library_leaf_calls);return true;
    }
    if(kind==LibFind){
        // Null/custom keys follow the original Java method and exception order.
        j2me_runtime::ScalarKey key;if(!library_scalar_key(args[1].ref,key))return false;
        const auto& fields=VMS_HEAP(g_heap)[self].fields;
        if(cls.trusted_library!=LibHashtable||cls.library_keys_slot<0||cls.library_count_slot<0||
           size_t(cls.library_keys_slot)>=fields.size()||size_t(cls.library_count_slot)>=fields.size())return false;
        const int keys=fields[cls.library_keys_slot].ref,count=fields[cls.library_count_slot].i;
        if(keys<=0||size_t(keys)>=VMS_HEAP(g_heap).size()||count<0)return false;
        const auto& array=VMS_HEAP(g_heap)[keys].elems;
        if(!VMS_HEAP(g_heap)[keys].is_array||!array.references()||size_t(count)>array.size())return false;
        const int index=VMS_NATIVE(g_library_indexes).find(keys,size_t(count),array.reference_version(),array.last_reference_write(),VMS_NATIVE(g_index_key_epoch),key,
            [&](size_t i,j2me_runtime::ScalarKey& k){return library_scalar_key(array.get(i).ref,k);});
        if(index==j2me_runtime::LibraryIndex::fallback)return false;
        result=Value::of_i(index);return true;
    }
    if(kind==LibEquals){
        j2me_runtime::ScalarKey a,b;if(!library_scalar_key(self,a))return false;
        // Reviewed wrappers are final. A foreign class is necessarily unequal;
        // no call to user-defined equals/hashCode is removed.
        result=Value::of_i(library_scalar_key(args[1].ref,b)&&a==b);++VMS_NATIVE(g_library_leaf_calls);return true;
    }
    if(kind==LibScalarValue){
        if(cls.library_value_slot<0||size_t(cls.library_value_slot)>=VMS_HEAP(g_heap)[self].fields.size())return false;
        const auto& v=VMS_HEAP(g_heap)[self].fields[cls.library_value_slot];
        result=cls.trusted_library==LibLong?Value::of_l(v.l):Value::of_i(v.i);++VMS_NATIVE(g_library_leaf_calls);return true;
    }
#else
    (void)kind;(void)method;(void)args;(void)result;
#endif
    return false;
}
