#include <ctime>
#include <iomanip>
#include <sys/resource.h>
#define main emulator_main
#include BENCH_VM
#undef main
int main(int argc,char**argv) {
    if(argc!=5)return 2;int mode=std::atoi(argv[1]),iterations=std::atoi(argv[2]);
    g_classpath={argv[3],BENCH_ROM};g_log_draw=false;g_fb.assign(240*320*3,0);
    int cls=ensure_class("OptimizationBench");Method* method=find_method(cls,"run","(II)I",false);
    run(method,{Value::of_i(mode),Value::of_i(std::min(500,iterations))});gc_collect("warmup");
    int64_t start=paint_clock_us();clock_t cpu=std::clock();
    int checksum=run(method,{Value::of_i(mode),Value::of_i(iterations)}).i;
    double cpu_ms=1000.0*(std::clock()-cpu)/CLOCKS_PER_SEC;
    int64_t duration=paint_clock_us()-start;struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
    std::cout<<std::setprecision(10)<<"{\"variant\":\""<<argv[4]<<"\",\"mode\":"<<mode<<",\"iterations\":"<<iterations
        <<",\"checksum\":"<<checksum<<",\"wall_ms\":"<<duration/1000.0<<",\"process_cpu_ms\":"<<cpu_ms
        <<",\"max_rss_kib\":"<<usage.ru_maxrss<<",\"unhandled_exception\":"<<(g_thrown?"true":"false")
        <<",\"scope\":\"desktop synthetic VM kernels, production profiler disabled; not game FPS\"}\n";
    return g_thrown?1:0;
}
