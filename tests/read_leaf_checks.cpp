#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
#include <random>
int main(){
    Class cls;cls.name="ReadOnlyFixture";VMS_CL(g_classes).push_back(std::move(cls));
    const int src=alloc_array("[S",6),obj=int(VMS_HEAP(g_heap).size());
    VMS_HEAP(g_heap).push_back(Obj{});VMS_HEAP(g_heap)[obj].cls=0;
    VMS_HEAP(g_heap)[obj].fields={Value::of_ref(src),Value::of_i(0),Value::of_i(0)};
    Method m;m.class_id=0;m.name="bounds";m.desc="()I";m.max_locals=1;m.max_stack=4;
    // if(this.flag) return -this.values[this.index]; else return this.values[this.index];
    m.code={42,180,0,1,153,0,17,42,180,0,2,42,180,0,3,53,116,172,0,0,0,42,180,0,2,42,180,0,3,53,172};
    m.quick.resize(m.code.size());
    for(int pc:{1,8,12,22,26}){m.quick[pc].kind=1;m.quick[pc].a=0;m.quick[pc].b=pc==1?2:(pc==8||pc==22?0:1);}
    assert(classify_read_leaf(m));
    std::mt19937 rng(38581);unsigned cases=0,accepted=0;
    for(int trial=0;trial<2000;++trial){
        auto& fields=VMS_HEAP(g_heap)[obj].fields;
        fields[0]=Value::of_ref(trial%71==0?0:src);fields[1]=Value::of_i(int(rng()%10)-2);fields[2]=Value::of_i(trial%2);
        for(int i=0;i<6;++i)VMS_HEAP(g_heap)[src].elems.set(i,Value::of_i(int(rng())));
        Value receiver=Value::of_ref(trial%79==0?0:obj);
        for(unsigned budget=0;budget<=24;++budget){
            Frame reference;reference.method=&m;reference.locals={receiver};
            const int n=execute_fast_batch(reference,96);
            const bool valid=size_t(reference.pc)<m.code.size() && m.code[reference.pc]==172 && reference.stack.size()==1;
            const auto writes=resver::g_stats.array_content;const auto key_epoch=VMS_NATIVE(g_index_key_epoch);
            Value result=Value::of_i(0x1234);unsigned used=999;
            const bool ok=execute_read_leaf(&m,&receiver,1,budget,result,used);
            assert(ok==(valid && unsigned(n+1)<=budget));
            assert(resver::g_stats.array_content==writes && VMS_NATIVE(g_index_key_epoch)==key_epoch);
            if(ok){assert(used==unsigned(n+1) && result.i==reference.stack[0].i && result.ref==reference.stack[0].ref);++accepted;}
            else assert(!used && result.i==0x1234);
            ++cases;
        }
    }
    // Unsupported bodies must never execute through the speculative read path.
    for(auto body:std::vector<std::vector<uint8_t>>{{42,3,181,0,1,3,172},{42,3,3,79,3,172},{184,0,1,172},{167,0,0},{177}}){
        Method rejected=m;rejected.code=body;assert(!classify_read_leaf(rejected));
    }
    Method synchronized=m;synchronized.flags=0x20;assert(!classify_read_leaf(synchronized));
    Method wide=m;wide.desc="(J)I";assert(!classify_read_leaf(wide));
    Method handled=m;handled.handlers.push_back({0,17,18,0});assert(!classify_read_leaf(handled));
    Method unresolved=m;unresolved.read_leaf_kind=0;for(auto& q:unresolved.quick)q.kind=0;
    Value arg=Value::of_ref(obj),value;unsigned used;
    assert(!execute_read_leaf(&unresolved,&arg,1,96,value,used));
    assert(accepted>5000);
    std::cout<<"PASS read-only leaf cases="<<cases<<" accepted="<<accepted<<" exact result/fuel; no writes; null/bounds/unresolved/budget fallback; stores/calls/loops/sync/handlers excluded\n";
}
