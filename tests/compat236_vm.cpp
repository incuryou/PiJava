#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <iostream>
int main(int argc,char**argv){
 if(argc!=2)return 2;VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;
 configure_runtime_diagnostics();int c=ensure_class(argv[1]);Value v=run(find_method(c,"run","()I",false),{});
 if(VMS_EXEC(g_thrown)||v.i<=0){std::cerr<<"FAIL "<<argv[1]<<" value="<<v.i<<" thrown="<<VMS_EXEC(g_thrown)<<"\n";return 1;}
 std::cout<<"PASS "<<argv[1]<<" assertions="<<v.i<<"\n";stop_thread();return 0;
}
