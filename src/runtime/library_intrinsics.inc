// VM integration for reviewed, bounded, non-suspending library operations.
// Host frontend, Java object layout and checkpoint format are not part of this API.
#include "library_index.h"
#include "reviewed_library_manifest.h"
 VMS_NATIVE_ACCESSOR(g_library_indexes, j2me_runtime::LibraryIndex)
 VMS_NATIVE_ACCESSOR(g_index_key_epoch, uint64_t) VMS_NATIVE_ACCESSOR(g_library_leaf_calls, uint64_t)
enum LibraryClass : uint8_t { LibUnknown,LibHashtable,LibCharacter,LibInteger,LibLong,LibByte,LibShort,LibBoolean,LibByteArrayStream,LibDataStream };
enum LibraryIntrinsic : uint8_t { LibNone,LibFind,LibEquals,LibScalarValue };
static void bind_library_layout(int cid,const std::vector<uint8_t>& bytes){
#if J2ME_OPT_LIBRARY_INDEX
    auto& c=VMS_CL(g_classes)[cid];
    for(const auto& reviewed:kReviewedLibraryClasses){
        if(c.name!=reviewed.name)continue;
        if(rms_jar_identity(bytes.data(),bytes.size())!=reviewed.sha256)return;
        if(reviewed.id==LibByteArrayStream||reviewed.id==LibDataStream){c.trusted_library=reviewed.id;return;}
        if(reviewed.id==LibHashtable){
            auto* keys=find_field(cid,"keys","[Ljava/lang/Object;");auto* count=find_field(cid,"count","I");
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
    if(!method||method->native||(method->flags&(0x0008|0x0020)))return LibNone;
    const int kind=VMS_CL(g_classes)[method->class_id].trusted_library;
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
static bool library_scalar_key(int ref,j2me_runtime::ScalarKey& key){
    if(ref<=0||size_t(ref)>=VMS_HEAP(g_heap).size())return false;
    auto& o=VMS_HEAP(g_heap)[ref];if(o.cls<0)return false;const auto& c=VMS_CL(g_classes)[o.cls];
    if(c.trusted_library<LibCharacter||c.trusted_library>LibBoolean||c.library_value_slot<0||size_t(c.library_value_slot)>=o.fields.size())return false;
    const auto& value=o.fields[c.library_value_slot];
    key.kind=c.trusted_library;key.value=c.trusted_library==LibLong?uint64_t(value.l):uint32_t(value.i);
    o.indexed_key=true;return true;
}
static bool library_intrinsic_dispatch(int kind,const Method* method,Value* args,Value& result){
#if J2ME_OPT_LIBRARY_INDEX
    const int self=args[0].ref;
    if(self<=0||size_t(self)>=VMS_HEAP(g_heap).size()||VMS_HEAP(g_heap)[self].cls!=method->class_id)return false;
    const auto& cls=VMS_CL(g_classes)[method->class_id];
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
