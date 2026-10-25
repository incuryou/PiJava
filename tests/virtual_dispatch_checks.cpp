#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
int main(int argc,char** argv){
    assert(argc==3);VMS_CL(g_classpath)={argv[1],argv[2]};configure_runtime_diagnostics();
    for(int pass=0;pass<2;++pass){
        if(pass){stop_thread();VMS_CL(g_classes).clear();VMS_CL(g_class_index).clear();}
        int cls=ensure_class("VirtualDispatchChecks");
        const Value result=run(find_method(cls,"run","()I",false),{});
        assert(!VMS_EXEC(g_thrown) && result.i==37002);
        uint64_t calls=0;
        for(const auto& c:VMS_CL(g_classes))for(const auto& m:c.methods)calls+=m.read_leaf_calls;
        assert(calls>10000);
        std::cout<<"PASS virtual dispatch pass="<<pass<<" assertions="<<result.i<<" read_leaf_calls="<<calls<<"\n";
    }
    stop_thread();p7::pool().shutdown();
}
