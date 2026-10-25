#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <cassert>
#include <iostream>
int main(){
 uint8_t yes[]={178,0,1,154,255,253};assert(pure_static_poll236(yes,6,0));assert(pure_static_poll236(yes,6,3));
 assert(!pure_static_poll236(yes,5,0));assert(!pure_static_poll236(yes,6,6));assert(!pure_static_poll236(yes,6,-1));
 uint8_t exitloop[]={178,0,1,153,0,6,167,255,250};assert(pure_static_poll236(exitloop,9,0));assert(pure_static_poll236(exitloop,9,3));assert(pure_static_poll236(exitloop,9,6));
 uint8_t no[]={179,0,1,154,255,253};assert(!pure_static_poll236(no,6,0));
 no[0]=178;no[5]=250;assert(!pure_static_poll236(no,6,0));
 VMS_CL(g_classpath)={"build/test-classes","romfs/classes"};VMS_DIAG(g_log_draw)=false;VMS_EXEC(g_slice_ms)=2;
 int c=ensure_class("FlagSpin236Checks");VMS_RS(g_paint_depth)=1;
 auto value=run(find_method(c,"runTest","()I",false),{});VMS_RS(g_paint_depth)=0;
 assert(!VMS_EXEC(g_thrown)&&value.i==1&&VMS_EXEC(g_flag_spin_pumps)>=1);stop_thread();
 std::cout<<"PASS read_only_static_flag_poll genuine_producer_write handoffs="<<VMS_EXEC(g_flag_spin_pumps)<<"\n";
}
