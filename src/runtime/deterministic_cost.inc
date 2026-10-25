// Bounded, read-only bytecode meter for pinned stream/wrapper/Hashtable
// shortcuts. Every counted operation is an original Java bytecode. Shadow
// fields/arrays are local C++ values: no Java allocation, callback, clock read,
// exception creation, mutation or retained root occurs during admission.
static bool gc_slot_live(int r);
struct R4CostMeter {
    unsigned cost=0,limit=0;
    std::map<std::pair<int,int>,Value> fields,arrays;
    bool live(int r)const{return r>0&&size_t(r)<VMS_HEAP(g_heap).size()&&gc_slot_live(r);}
    bool pinned(Method* m)const{return m&&!m->native&&VMS_CL(g_classes)[m->class_id].trusted_library&&VMS_CL(g_classes)[m->class_id].init_state==2&&(library_intrinsic_kind(m)||stream_intrinsic_kind(m));}
    Value field(int ref,int slot){auto k=std::make_pair(ref,slot);auto it=fields.find(k);return it==fields.end()?VMS_HEAP(g_heap)[ref].fields[slot]:it->second;}
    Value array(int ref,int slot){auto k=std::make_pair(ref,slot);auto it=arrays.find(k);return it==arrays.end()?VMS_HEAP(g_heap)[ref].elems.get(slot):it->second;}
    bool run(Method* m,const Value* args,int argc,Value& result,int depth=0){
        if(depth>12||!pinned(m))return false;
        const auto& code=m->code;const auto& owner=VMS_CL(g_classes)[m->class_id];
        std::vector<Value> local(std::max<int>(m->max_locals,argc*2+2)),stack;stack.reserve(m->max_stack+2);
        int slot=0;for(int i=0;i<argc;i++){local[slot]=args[i];slot+=args[i].is_long?2:1;}
        int pc=0;
        auto pop=[&](){Value v=stack.back();stack.pop_back();return v;};
        while(pc>=0&&size_t(pc)<code.size()){
            if(cost>=limit||stack.size()>256)return false;++cost;
            int start=pc,op=code[pc++];
            auto u8=[&](){return int(code[pc++]);};auto u16=[&](){int v=(int(code[pc])<<8)|code[pc+1];pc+=2;return v;};
            auto s16=[&](){return int(int16_t(u16()));};auto push=[&](Value v){stack.push_back(v);};
            // The pinned compiler emits only these bounded opcodes. An unknown
            // shape or an exception path falls back before any real write.
            if(op==0)continue;
            if(op==1){push(Value::of_ref(0));continue;}
            if(op>=2&&op<=8){push(Value::of_i(op-3));continue;}
            if(op==9||op==10){push(Value::of_l(op-9));continue;}
            if(op==16){push(Value::of_i(int8_t(u8())));continue;}
            if(op==17){push(Value::of_i(s16()));continue;}
            if(op==18||op==19){int cp=op==18?u8():u16();if(owner.cp[cp].tag!=3)return false;push(Value::of_i(owner.cp[cp].a));continue;}
            if(op==21||op==22||op==25){int k=u8();if(size_t(k)>=local.size())return false;push(local[k]);continue;}
            if((op>=26&&op<=33)||(op>=42&&op<=45)){int k=op>=42?op-42:op>=30?op-30:op-26;if(size_t(k)>=local.size())return false;push(local[k]);continue;}
            if(op==54||op==55||op==58){if(stack.empty())return false;int k=u8();if(size_t(k)>=local.size())return false;local[k]=pop();continue;}
            if((op>=59&&op<=66)||(op>=75&&op<=78)){if(stack.empty())return false;int k=op>=75?op-75:op>=63?op-63:op-59;if(size_t(k)>=local.size())return false;local[k]=pop();continue;}
            if(op==87){if(stack.empty())return false;pop();continue;}
            if(op==89){if(stack.empty())return false;push(stack.back());continue;}
            if(op==90){if(stack.size()<2)return false;Value a=pop(),b=pop();push(a);push(b);push(a);continue;}
            if(op==92){if(stack.size()<2)return false;Value a=pop(),b=pop();push(b);push(a);push(b);push(a);continue;}
            if(op==46||op==50||op==51){
                if(stack.size()<2)return false;int i=pop().i,r=pop().ref;if(!live(r)||!VMS_HEAP(g_heap)[r].is_array||i<0||size_t(i)>=VMS_HEAP(g_heap)[r].elems.size())return false;
                push(array(r,i));continue;
            }
            if(op==96||op==100||op==104||op==120||op==122||op==124||op==126||op==128||op==130){
                if(stack.size()<2)return false;uint32_t b=uint32_t(pop().i),a=uint32_t(pop().i),v=0;
                if(op==96)v=a+b;else if(op==100)v=a-b;else if(op==104)v=a*b;else if(op==120)v=a<<(b&31);else if(op==122)v=uint32_t((int_bits(a)>>(b&31)));else if(op==124)v=a>>(b&31);else if(op==126)v=a&b;else if(op==128)v=a|b;else v=a^b;push(Value::of_i(int_bits(v)));continue;
            }
            if(op==132){int k=u8(),delta=int8_t(u8());if(size_t(k)>=local.size())return false;local[k].i=int_bits(uint32_t(local[k].i)+uint32_t(delta));continue;}
            if(op==133){if(stack.empty())return false;push(Value::of_l(pop().i));continue;}
            if(op==136){if(stack.empty())return false;push(Value::of_i(int_bits(uint32_t(pop().l))));continue;}
            if(op==148){if(stack.size()<2)return false;int64_t b=pop().l,a=pop().l;push(Value::of_i(a<b?-1:a>b?1:0));continue;}
            if((op>=153&&op<=166)||op==198||op==199){
                int offset=s16();bool two=op>=159&&op<=166;if(stack.size()<size_t(two?2:1))return false;
                Value b=pop(),a=two?pop():b;int x=two?a.i:b.i,y=two?b.i:0;bool branch=false;
                switch(op){case 153:case 159:branch=x==y;break;case 154:case 160:branch=x!=y;break;case 155:case 161:branch=x<y;break;case 156:case 162:branch=x>=y;break;case 157:case 163:branch=x>y;break;case 158:case 164:branch=x<=y;break;case 165:branch=a.ref==b.ref;break;case 166:branch=a.ref!=b.ref;break;case 198:branch=b.ref==0;break;case 199:branch=b.ref!=0;break;}
                if(branch)pc=start+offset;continue;
            }
            if(op==167){pc=start+s16();continue;}
            if(op==172||op==173||op==176){if(stack.empty())return false;result=pop();return true;}
            if(op==177){result=Value{};return true;}
            if(op==180||op==181){
                int cp=u16();if(stack.size()<size_t(op==181?2:1))return false;Value value=op==181?pop():Value{};int ref=pop().ref;
                if(!live(ref))return false;std::string cn,fn,fd;resolve_member(owner,cp,cn,fn,fd);auto it=VMS_CL(g_class_index).find(cn);if(it==VMS_CL(g_class_index).end())return false;
                Field* f=find_field(it->second,fn,fd);if(!f||f->is_static||f->slot<0||size_t(f->slot)>=VMS_HEAP(g_heap)[ref].fields.size())return false;
                if(op==180)push(field(ref,f->slot));else fields[{ref,f->slot}]=value;continue;
            }
            if(op==190){if(stack.empty())return false;int r=pop().ref;if(!live(r)||!VMS_HEAP(g_heap)[r].is_array)return false;push(Value::of_i(int(VMS_HEAP(g_heap)[r].elems.size())));continue;}
            if(op==192||op==193){
                int cp=u16();if(stack.empty())return false;int ref=stack.back().ref;const auto name=cp_class(owner,cp);
                bool yes=false;if(ref){if(!live(ref))return false;int cls=VMS_HEAP(g_heap)[ref].cls;if(cls<0)return false;yes=VMS_CL(g_classes)[cls].name==name;}
                if(op==193){pop();push(Value::of_i(yes));}else if(ref&&!yes)return false;continue;
            }
            if(op==182||op==184){
                int cp=u16();std::string cn,mn,desc;resolve_member(owner,cp,cn,mn,desc);int n=count_args(desc)+(op==182?1:0);if(stack.size()<size_t(n))return false;
                std::vector<Value> a(n);for(int i=n-1;i>=0;--i)a[i]=pop();
                if(op==184){
                    if(cn!="java/lang/System"||mn!="arraycopy"||desc!="(Ljava/lang/Object;ILjava/lang/Object;II)V"||n!=5)return false;
                    // The original invokestatic must still initialize its
                    // owner and resolve the actual native before we skip it.
                    auto sys=VMS_CL(g_class_index).find(cn);
                    if(sys==VMS_CL(g_class_index).end()||VMS_CL(g_classes)[sys->second].init_state!=2)return false;
                    Method* target=find_method(sys->second,mn,desc,false);
                    if(!target||!target->native||!(target->flags&0x0008)||(target->flags&0x0020))return false;
                    int src=a[0].ref,from=a[1].i,dst=a[2].ref,to=a[3].i,len=a[4].i;
                    if(!live(src)||!live(dst)||VMS_HEAP(g_heap)[src].array_desc!="[B"||VMS_HEAP(g_heap)[dst].array_desc!="[B"||from<0||to<0||len<0||len>4096||size_t(from)>VMS_HEAP(g_heap)[src].elems.size()||size_t(len)>VMS_HEAP(g_heap)[src].elems.size()-size_t(from)||size_t(to)>VMS_HEAP(g_heap)[dst].elems.size()||size_t(len)>VMS_HEAP(g_heap)[dst].elems.size()-size_t(to))return false;
                    std::vector<Value> copy;for(int i=0;i<len;i++)copy.push_back(array(src,from+i));for(int i=0;i<len;i++)arrays[{dst,to+i}]=copy[i];continue;
                }
                if(!live(a[0].ref))return false;int cls=VMS_HEAP(g_heap)[a[0].ref].cls;if(cls<0)return false;
                Method* callee=find_method(cls,mn,desc,true);Value ret;if(!run(callee,a.data(),n,ret,depth+1))return false;if(!returns_void(desc))push(ret);continue;
            }
            return false;
        }return false;
    }
};
static bool r4_shortcut_cost(Method* m,const Value* args,int argc,unsigned limit,unsigned& cost,Value& result){
    R4CostMeter meter;meter.limit=std::min<unsigned>(limit,4096);
    if(!meter.run(m,args,argc,result))return false;cost=meter.cost;return true;
}
