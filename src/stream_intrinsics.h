#ifndef J2ME_VM_STREAM_INTRINSICS_H
#define J2ME_VM_STREAM_INTRINSICS_H

// This header follows the VM's object, method, field and exception helpers.
// Classify the resolved method, never the constant-pool owner. In particular,
// inherited methods and invokespecial still require an exact runtime class.
enum StreamIntrinsicKind {
    StreamNone, StreamRead, StreamReadBytes, StreamAvailable, StreamSkip,
    StreamMark, StreamReset, StreamUnsignedByte, StreamUnsignedShort, StreamInt
};
static int stream_intrinsic_kind(const Method* method) {
    if (!method || method->native || (method->flags & (0x0008 | 0x0020))) return StreamNone;
    const std::string& owner = VMS_CL(g_classes)[method->class_id].name;
    const std::string& name = method->name;
    const std::string& desc = method->desc;
    if (owner == "java/io/ByteArrayInputStream") {
        if (name == "read" && desc == "()I") return StreamRead;
        if (name == "read" && desc == "([BII)I") return StreamReadBytes;
        if (name == "available" && desc == "()I") return StreamAvailable;
        if (name == "skip" && desc == "(J)J") return StreamSkip;
        if (name == "mark" && desc == "(I)V") return StreamMark;
        if (name == "reset" && desc == "()V") return StreamReset;
    } else if (owner == "java/io/DataInputStream" && desc == "()I") {
        if (name == "readUnsignedByte") return StreamUnsignedByte;
        if (name == "readUnsignedShort") return StreamUnsignedShort;
        if (name == "readInt") return StreamInt;
    }
    return StreamNone;
}
static int stream_field_slot(int cls,unsigned key) {
    static const char* names[]={"in","pos","count","mark","buf"};
    static const char* descs[]={"Ljava/io/InputStream;","I","I","I","[B"};
    if(cls<0 || size_t(cls)>=VMS_CL(g_classes).size() || key>=5)return -1;
    auto& owner=VMS_CL(g_classes)[cls];int& slot=owner.stream_slots[key];
    if(slot==-2) {
        const Field* field=find_field(cls,names[key],descs[key]);
        slot=field&&!field->is_static&&field->slot>=0&&field->slot<owner.instance_fields?field->slot:-1;
    }
    return slot;
}
static bool stream_intrinsic_dispatch(int kind, const Method* method, Value* args, Value& result) {
    const int receiver = args[0].ref;
    if (receiver <= 0 || receiver >= (int)VMS_HEAP(g_heap).size() ||
        VMS_HEAP(g_heap)[receiver].cls != method->class_id) return false;
    int stream = receiver;
    const bool data_input = kind >= StreamUnsignedByte;
    if (data_input) {
        const int input = stream_field_slot(method->class_id,0);
        if (input<0 || size_t(input)>=VMS_HEAP(g_heap)[receiver].fields.size()) return false;
        stream = VMS_HEAP(g_heap)[receiver].fields[input].ref;
        // Let the Java method deliver null/foreign-input behavior. Calling a
        // subclass's inherited read may itself contain overridden operations.
        if (stream <= 0 || stream >= (int)VMS_HEAP(g_heap).size()) return false;
        int cls = VMS_HEAP(g_heap)[stream].cls;
        if (cls < 0 || VMS_CL(g_classes)[cls].name != "java/io/ByteArrayInputStream") return false;
    }
    const int cls = VMS_HEAP(g_heap)[stream].cls;
    const int pos_slot=stream_field_slot(cls,1),count_slot=stream_field_slot(cls,2),
        mark_slot=stream_field_slot(cls,3),buf_slot=stream_field_slot(cls,4);
    if(pos_slot<0||count_slot<0||mark_slot<0||buf_slot<0)return false;
    Obj& source = VMS_HEAP(g_heap)[stream];
    if(size_t(std::max({pos_slot,count_slot,mark_slot,buf_slot}))>=source.fields.size())return false;
    int32_t pos = source.fields[pos_slot].i;
    const int32_t count = source.fields[count_slot].i;
    if (kind == StreamAvailable) {
        result = Value::of_i(int_bits(uint32_t(count) - uint32_t(pos))); return true;
    }
    if (kind == StreamMark) {
        source.fields[mark_slot] = Value::of_i(pos); return true;
    }
    if (kind == StreamReset) {
        source.fields[pos_slot] = source.fields[mark_slot]; return true;
    }
    if (kind == StreamSkip) {
        int64_t skipped = args[1].l;
        if (skipped <= 0) skipped = 0;
        else skipped = std::min<int64_t>(skipped, int_bits(uint32_t(count) - uint32_t(pos)));
        source.fields[pos_slot] = Value::of_i(int_bits(uint32_t(pos) + uint32_t(skipped)));
        result = Value::of_l(skipped); return true;
    }
    if (kind == StreamReadBytes) {
        const int dst = args[1].ref, off = args[2].i, len = args[3].i;
        // Java checks the destination and range before zero length and EOF.
        if (!dst) return false;
        if (dst < 0 || dst >= (int)VMS_HEAP(g_heap).size() || VMS_HEAP(g_heap)[dst].array_desc != "[B") return false;
        if (off < 0 || len < 0 || off > (int)VMS_HEAP(g_heap)[dst].elems.size() - len) {
            return false;
        }
        if (!len) { result = Value::of_i(0); return true; }
        if (pos >= count) { result = Value::of_i(-1); return true; }
    } else if (pos >= count) {
        if (data_input) return false;
        result = Value::of_i(-1);
        return true;
    }
    const int buf = source.fields[buf_slot].ref;
    // Corrupted/protected state must retain Java's exception and mutation order.
    if (buf <= 0 || buf >= (int)VMS_HEAP(g_heap).size() || VMS_HEAP(g_heap)[buf].array_desc != "[B" ||
        pos < 0 || count < pos || size_t(count) > VMS_HEAP(g_heap)[buf].elems.size()) return false;
    if (kind == StreamReadBytes) {
        const int amount = std::min(args[3].i, count - pos);
        if (!VMS_HEAP(g_heap)[args[1].ref].elems.copy_from(VMS_HEAP(g_heap)[buf].elems, pos, args[2].i, amount)) return false;
        source.fields[pos_slot] = Value::of_i(pos + amount);
        result = Value::of_i(amount); return true;
    }
    const int needed = kind == StreamUnsignedShort ? 2 : kind == StreamInt ? 4 : 1;
    const int amount = std::min(needed, count - pos);
    // Java constructs the exception itself, preserving its null message,
    // initialization and stack trace. Retry before consuming partial input;
    // the sequential Java reads consume the remainder in their normal order.
    if (amount != needed) return false;
    const uint8_t* bytes = VMS_HEAP(g_heap)[buf].elems.read_bytes();
    uint32_t value = 0;
    for (int i = 0; i < amount; ++i) value = (value << 8) | bytes[pos + i];
    source.fields[pos_slot] = Value::of_i(pos + amount);
    result = Value::of_i(int_bits(value));
    return true;
}

#endif
