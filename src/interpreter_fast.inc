// 0.2.34: bounded, non-safepoint bytecode batches. This is still an interpreter,
// not JIT or game-specific replacement code. It never calls Java/native methods,
// allocates Java objects, enters monitors, collects garbage or changes clocks.
// Unresolved members and operations that can throw return BEFORE consuming any
// operands, so the existing slow interpreter owns initialization and exceptions.
 VMS_EXEC_ACCESSOR(g_fast_ops, uint64_t) VMS_EXEC_ACCESSOR(g_fast_blocks, uint64_t) VMS_EXEC_ACCESSOR(g_slow_ops, uint64_t)
 VMS_EXEC_ACCESSOR(g_fast_float241_enabled, bool)
 VMS_EXEC_ACCESSOR(g_fast_float241_ops, uint64_t)
#if J2ME_OPT_FAST_DISPATCH
static int execute_fast_batch(Frame& fr, int budget) {
    auto& stack=fr.stack;
    auto& locals=fr.locals;
    const auto& code=fr.method->code;
    int done=0;
    while(done<budget) {
        const int pc=fr.pc;
        if(pc<0 || size_t(pc)>=code.size())break;
        const unsigned op=code[size_t(pc)];
        const size_t n=stack.size();
        int next=pc+1;
        auto have=[&](size_t extra){return size_t(pc)+extra<code.size();};
        auto push=[&](Value v){stack.push_back(v);};
        if(op>=26 && op<=45) {
            const unsigned slot=(op-26)%4;
            if(slot>=locals.size())break;
            push(locals[slot]);
        } else if(op>=59 && op<=78) {
            const unsigned slot=(op-59)%4;
            if(!n || slot>=locals.size())break;
            locals[slot]=stack.back();stack.pop_back();
        } else switch(op) {
        case 0:break;
        case 1:push(Value::of_ref(0));break;
        case 2:case 3:case 4:case 5:case 6:case 7:case 8:push(Value::of_i(int(op)-3));break;
        case 9:case 10:push(Value::of_l(int(op)-9));break;
        case 11:case 12:case 13:
            if(!VMS_EXEC(g_fast_float241_enabled))return done;
            push(Value::of_i(f_bits(float(op-11))));++VMS_EXEC(g_fast_float241_ops);break;
        case 16:
            if(!have(1))return done;
            push(Value::of_i(int8_t(code[pc+1])));next+=1;break;
        case 17:
            if(!have(2))return done;
            push(Value::of_i(rs16(code.data()+pc+1)));next+=2;break;
        case 18:case 19:case 20: {
            if(!have(op==18?1:2))return done;
            const unsigned idx=op==18?code[pc+1]:ru16(code.data()+pc+1);
            const auto& cp=VMS_CL(g_classes)[fr.method->class_id].cp;
            if(idx>=cp.size())return done;
            const auto& e=cp[idx];
            if(op==20) {
                if(e.tag!=5 && e.tag!=6)return done;
                push(Value::of_l(e.l));
            } else {
                if(e.tag!=3 && e.tag!=4)return done;
                push(Value::of_i(e.a));
            }
            next+=op==18?1:2;break;
        }
        case 21:case 22:case 23:case 24:case 25: {
            if(!have(1) || code[pc+1]>=locals.size())return done;
            push(locals[code[pc+1]]);next+=1;break;
        }
        case 54:case 55:case 56:case 57:case 58:
            if(!n || !have(1) || code[pc+1]>=locals.size())return done;
            locals[code[pc+1]]=stack.back();stack.pop_back();next+=1;break;
        case 46:case 47:case 48:case 49:case 50:case 51:case 52:case 53: {
            if(n<2)return done;
            const int ref=stack[n-2].ref,idx=stack[n-1].i;
            if(ref<=0 || size_t(ref)>=VMS_HEAP(g_heap).size() || idx<0 || size_t(idx)>=VMS_HEAP(g_heap)[ref].elems.size())return done;
            const Value v=VMS_HEAP(g_heap)[ref].elems.get(size_t(idx));
            stack[n-2]=v;stack.pop_back();break;
        }
        case 79:case 80:case 81:case 82:case 84:case 85:case 86: {
            if(n<3)return done;
            const int ref=stack[n-3].ref,idx=stack[n-2].i;
            if(ref<=0 || size_t(ref)>=VMS_HEAP(g_heap).size() || idx<0 || size_t(idx)>=VMS_HEAP(g_heap)[ref].elems.size())return done;
            // aastore intentionally remains slow: it can resolve component
            // classes and throw ArrayStoreException before storing a reference.
            Value v=stack[n-1];
            if(op==84)v.i=VMS_HEAP(g_heap)[ref].array_desc=="[Z"?(v.i&1):int8_t(v.i);
            else if(op==85)v.i=uint16_t(v.i);
            else if(op==86)v.i=int16_t(v.i);
            VMS_HEAP(g_heap)[ref].elems.set(size_t(idx),v);stack.resize(n-3);break;
        }
        case 87:
            if(!n)return done;stack.pop_back();break;
        case 88:
            if(!n || (!stack.back().is_long && n<2))return done;
            stack.resize(n-(stack.back().is_long?1:2));break;
        case 89: {
            if(!n)return done;const Value v=stack.back();push(v);break;
        }
        case 90: {
            if(n<2)return done;const Value v=stack[n-1],w=stack[n-2];
            stack[n-2]=v;stack[n-1]=w;push(v);break;
        }
        case 95:
            if(n<2)return done;std::swap(stack[n-1],stack[n-2]);break;
        case 96:case 100:case 104:case 108:case 112:
        case 120:case 122:case 124:case 126:case 128:case 130: {
            if(n<2)return done;
            const int32_t a=stack[n-2].i,b=stack[n-1].i;
            int32_t v=0;
            switch(op) {
            case 96:v=int_bits(uint32_t(a)+uint32_t(b));break;
            case 100:v=int_bits(uint32_t(a)-uint32_t(b));break;
            case 104:v=int_bits(uint32_t(a)*uint32_t(b));break;
            case 108:if(!b)return done;v=a==INT32_MIN&&b==-1?INT32_MIN:a/b;break;
            case 112:if(!b)return done;v=a==INT32_MIN&&b==-1?0:a%b;break;
            case 120:v=int_bits(uint32_t(a)<<(b&31));break;
            case 122:v=a>>(b&31);break;
            case 124:v=int_bits(uint32_t(a)>>(b&31));break;
            case 126:v=a&b;break;
            case 128:v=a|b;break;
            case 130:v=a^b;break;
            }
            stack[n-2]=Value::of_i(v);stack.pop_back();break;
        }
        // Stage241: float arithmetic stays interpreted, but no longer tears
        // down a fast block. Values are rounded to binary32 on every opcode;
        // no FMA fusion, fast-math, Java allocation/callback or new safepoint.
        case 98:case 102:case 106:case 110:case 114: {
            if(!VMS_EXEC(g_fast_float241_enabled)||n<2)return done;
            // Preserve the slow path's raw NaN payload selection as well as
            // numerical results. Non-finite operands are rare in normal 3D.
            if((uint32_t(stack[n-2].i)&0x7f800000u)==0x7f800000u ||
               (uint32_t(stack[n-1].i)&0x7f800000u)==0x7f800000u)return done;
            float a=bits_f(stack[n-2].i),b=bits_f(stack[n-1].i);volatile float v;
            switch(op){case 98:v=a+b;break;case 102:v=a-b;break;case 106:v=a*b;break;
                case 110:v=a/b;break;default:v=std::fmod(a,b);break;}
            stack[n-2]=Value::of_i(f_bits(v));stack.pop_back();++VMS_EXEC(g_fast_float241_ops);break;
        }
        case 118:
            if(!VMS_EXEC(g_fast_float241_enabled)||!n)return done;
            stack.back()=Value::of_i(f_bits(-bits_f(stack.back().i)));++VMS_EXEC(g_fast_float241_ops);break;
        case 134:
            if(!VMS_EXEC(g_fast_float241_enabled)||!n)return done;
            stack.back()=Value::of_i(f_bits(float(stack.back().i)));++VMS_EXEC(g_fast_float241_ops);break;
        case 139:
            if(!VMS_EXEC(g_fast_float241_enabled)||!n)return done;
            stack.back()=Value::of_i(java_fp_integer<int32_t>(bits_f(stack.back().i)));++VMS_EXEC(g_fast_float241_ops);break;
        case 149:case 150: {
            if(!VMS_EXEC(g_fast_float241_enabled)||n<2)return done;
            int v=fcmp(bits_f(stack[n-2].i),bits_f(stack[n-1].i),op==149?-1:1);
            stack[n-2]=Value::of_i(v);stack.pop_back();++VMS_EXEC(g_fast_float241_ops);break;
        }
        case 116:
            if(!n)return done;stack.back()=Value::of_i(int_bits(0u-uint32_t(stack.back().i)));break;
        case 132:
            if(!have(2) || code[pc+1]>=locals.size())return done;
            locals[code[pc+1]].i=int_bits(uint32_t(locals[code[pc+1]].i)+uint32_t(int32_t(int8_t(code[pc+2]))));next+=2;break;
        case 133:if(!n)return done;stack.back()=Value::of_l(stack.back().i);break;
        case 136:if(!n)return done;stack.back()=Value::of_i(int_bits(uint32_t(as_long(stack.back()))));break;
        case 145:if(!n)return done;stack.back()=Value::of_i(int8_t(stack.back().i));break;
        case 146:if(!n)return done;stack.back()=Value::of_i(uint16_t(stack.back().i));break;
        case 147:if(!n)return done;stack.back()=Value::of_i(int16_t(stack.back().i));break;
        case 153:case 154:case 155:case 156:case 157:case 158:case 198:case 199: {
            if(!n || !have(2))return done;
            const int32_t original=op>=198?stack.back().ref:stack.back().i;
            const int32_t v=(op==157||op==158)&&fr.method->has_rechecked_deadline?speed_rechecked_branch(fr,pc,original):original;
            const bool take=op==156&&!fr.method->cached_elapsed_gates.empty()?speed_cached_elapsed_branch(fr,pc,v):op==153||op==198?v==0:op==154||op==199?v!=0:op==155?v<0:op==156?v>=0:op==157?v>0:v<=0;
            next=take?pc+rs16(code.data()+pc+1):pc+3;stack.pop_back();break;
        }
        case 159:case 160:case 161:case 162:case 163:case 164:case 165:case 166: {
            if(n<2 || !have(2))return done;
            const int32_t a=op>=165?stack[n-2].ref:stack[n-2].i,b=op>=165?stack[n-1].ref:stack[n-1].i;
            const bool take=op==159||op==165?a==b:op==160||op==166?a!=b:op==161?a<b:op==162?a>=b:op==163?a>b:a<=b;
            next=take?pc+rs16(code.data()+pc+1):pc+3;stack.resize(n-2);break;
        }
        case 167:
            if(!have(2))return done;next=pc+rs16(code.data()+pc+1);break;
        case 178:case 179:case 180:case 181: {
            if(!have(2) || size_t(pc)>=fr.method->quick.size())return done;
            const auto& site=fr.method->quick[pc];
            if(site.kind!=1 || site.a<0 || size_t(site.a)>=VMS_CL(g_classes).size() || site.b<0)return done;
            const bool stat=op<=179,write=op==179||op==181;
            if(n<(stat?(write?1u:0u):(write?2u:1u)))return done;
            Value* field=nullptr;
            if(stat) {
                auto& cls=VMS_CL(g_classes)[site.a];
                if(cls.init_state!=2 || size_t(site.b)>=cls.statics.size())return done;
                field=&cls.statics[site.b];
            } else {
                const int ref=stack[n-(write?2:1)].ref;
                if(ref<=0 || size_t(ref)>=VMS_HEAP(g_heap).size() || size_t(site.b)>=VMS_HEAP(g_heap)[ref].fields.size())return done;
                field=&VMS_HEAP(g_heap)[ref].fields[site.b];
            }
            if(write){
                if(!stat && VMS_HEAP(g_heap)[stack[n-2].ref].indexed_key)++VMS_NATIVE(g_index_key_epoch);
                *field=stack.back();stack.resize(n-(stat?1:2));
            }
            else if(stat)push(*field);
            else stack.back()=*field;
            next+=2;break;
        }
        case 190: {
            if(!n)return done;const int ref=stack.back().ref;
            if(ref<=0 || size_t(ref)>=VMS_HEAP(g_heap).size())return done;
            stack.back()=Value::of_i(int32_t(VMS_HEAP(g_heap)[ref].elems.size()));break;
        }
        default:return done;
        }
        P61_OP(fr.method,pc,op);fr.op_pc=pc;fr.pc=next;++done;
    }
    return done;
}
#include "read_leaf.inc"
#endif
