// Execute bounded, acyclic, read-only instance methods with the SAME fast
// interpreter. No Java allocation/call/write/monitor/clock read can occur.
// The scratch frame is not published. On any unresolved member, null/bounds
// check or insufficient budget, discard it and perform the ordinary invocation;
// the real Java frame then owns exceptions and their original stack trace.
static bool classify_read_leaf(const Method& m) {
#ifdef J2ME_P61_PROF
    return false; // retain per-op diagnostic hook accounting
#else
    if(m.native || (m.flags&(0x0008|0x0020|0x0100|0x0400)) || !m.handlers.empty() ||
       m.code.empty() || m.code.size()>96 || m.max_locals>8 || m.max_stack>8 ||
       m.name=="<init>" || m.name=="<clinit>" || city_method_traced_name(&m))return false;
    // Wide arguments need JVM two-local-slot layout, which this path excludes.
    const size_t close=m.desc.find(')');
    if(close==std::string::npos || close+1>=m.desc.size() || m.desc.find('J')!=std::string::npos || m.desc.find('D')!=std::string::npos)return false;
    const char ret=m.desc[close+1];if(ret!='I' && ret!='B' && ret!='C' && ret!='S' && ret!='Z' && ret!='L' && ret!='[')return false;
    bool start[96]={};
    for(size_t pc=0;pc<m.code.size();) {
        start[pc]=true;const int op=m.code[pc],len=jvm_op_length(m.code,int(pc));
        if(len<=0 || pc+size_t(len)>m.code.size())return false;
        const bool allowed=op<=8 || op==16 || op==17 || op==18 || op==19 || op==21 || op==25 ||
            (op>=26&&op<=29)||(op>=42&&op<=45)||op==46||(op>=50&&op<=54)||op==58||
            (op>=59&&op<=62)||(op>=75&&op<=78)||op==87||op==89||op==95||op==96||op==100||op==104||op==116||
            op==120||op==122||op==124||op==126||op==128||op==130||op==132||(op>=145&&op<=147)||
            (op>=153&&op<=167)||op==172||op==176||op==180||op==190||op==198||op==199;
        if(!allowed)return false;
        pc+=size_t(len);
    }
    for(size_t pc=0;pc<m.code.size();) {
        const int op=m.code[pc],len=jvm_op_length(m.code,int(pc));
        if((op>=153&&op<=167)||op==198||op==199) {
            const int target=int(pc)+rs16(m.code.data()+pc+1);
            if(target<=int(pc) || target>=int(m.code.size()) || !start[target])return false;
        }
        pc+=size_t(len);
    }
    return true;
#endif
}
static bool execute_read_leaf(Method* m,const Value* args,int count,unsigned budget,Value& result,unsigned& executed) {
    executed=0;
    if(!m || count<1 || count>8 || !budget)return false;
    if(!m->read_leaf_kind)m->read_leaf_kind=classify_read_leaf(*m)?1:2;
    if(m->read_leaf_kind!=1)return false;
    struct Scratch {std::vector<Value> locals,stack;};
    static thread_local Scratch scratch;
    Frame frame;frame.method=m;frame.locals.swap(scratch.locals);frame.stack.swap(scratch.stack);
    struct ReturnBuffers {
        Frame& f;Scratch& s;
        ~ReturnBuffers(){f.locals.clear();f.stack.clear();f.locals.swap(s.locals);f.stack.swap(s.stack);}
    } release{frame,scratch};
    frame.locals.assign(std::max<int>(m->max_locals,count),Value{});frame.stack.clear();frame.stack.reserve(m->max_stack);
    for(int i=0;i<count;++i)frame.locals[size_t(i)]=args[i];
    const int done=execute_fast_batch(frame,int(std::min<unsigned>(budget,96)));
    if(done>=int(budget) || frame.pc<0 || size_t(frame.pc)>=m->code.size() ||
       (m->code[frame.pc]!=172 && m->code[frame.pc]!=176) || frame.stack.size()!=1 || frame.stack.back().is_long) {
        ++m->read_leaf_fallbacks;return false;
    }
    result=frame.stack.back();executed=unsigned(done)+1;++m->read_leaf_calls;return true;
}
