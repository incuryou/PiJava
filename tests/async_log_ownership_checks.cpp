// Exercise the actual VM diagnostics and logger through a controlled ostream.
// No production hook or replacement implementation is used.
#define main emulator_main
#include "../src/j2mevm.cpp"
#undef main
#include <iostream>

class BlockedLogBuffer final : public std::streambuf {
    std::mutex mu;
    std::condition_variable cv;
    bool entered=false,released=false,writing=false,write_done=false,writer_flushed=false;
    bool block_write,block_flush,flush_entered=false,flushing=false;
    std::string token;
    std::thread::id writer;
    unsigned foreign_sync_overlap=0;
    std::string bytes;
protected:
    std::streamsize xsputn(const char* text,std::streamsize n) override {
        std::unique_lock<std::mutex> lock(mu);
        if(writer==std::thread::id{})writer=std::this_thread::get_id();
        if(block_write&&!entered&&(token.empty()||std::string(text,size_t(n)).find(token)!=std::string::npos)) {
            entered=true;writing=true;write_done=writer_flushed=false;cv.notify_all();
            cv.wait(lock,[&]{return released;});
        }
        bytes.append(text,size_t(n));writing=false;write_done=true;cv.notify_all();
        return n;
    }
    int overflow(int ch) override {
        if(ch==traits_type::eof())return traits_type::not_eof(ch);
        std::lock_guard<std::mutex> lock(mu);bytes.push_back(char(ch));return ch;
    }
    int sync() override {
        std::unique_lock<std::mutex> lock(mu);
        if((writing||flushing)&&std::this_thread::get_id()!=writer)++foreign_sync_overlap;
        if(block_flush&&!flush_entered&&std::this_thread::get_id()==writer) {
            flush_entered=flushing=true;writer_flushed=false;cv.notify_all();
            cv.wait(lock,[&]{return released;});flushing=false;
        }
        if(write_done&&!writing&&std::this_thread::get_id()==writer)writer_flushed=true;
        cv.notify_all();return 0;
    }
public:
    explicit BlockedLogBuffer(bool writes=true,bool flushes=false,std::string match={}):block_write(writes),block_flush(flushes),token(std::move(match)){}
    bool wait_for_write() {
        std::unique_lock<std::mutex> lock(mu);
        return cv.wait_for(lock,std::chrono::seconds(3),[&]{return entered&&writing;});
    }
    bool wait_for_flush() {
        std::unique_lock<std::mutex> lock(mu);
        return cv.wait_for(lock,std::chrono::seconds(3),[&]{return flush_entered&&flushing;});
    }
    void release() {std::lock_guard<std::mutex> lock(mu);released=true;cv.notify_all();}
    bool blocked() {std::lock_guard<std::mutex> lock(mu);return (writing||flushing)&&!released;}
    bool flushed() {std::lock_guard<std::mutex> lock(mu);return writer_flushed;}
    unsigned overlaps() {std::lock_guard<std::mutex> lock(mu);return foreign_sync_overlap;}
    std::string contents() {std::lock_guard<std::mutex> lock(mu);return bytes;}
};

static std::streambuf* attach_probe(const std::string& path,BlockedLogBuffer& probe) {
    j2melog::shutdown();auto& file=VMS_PLAT(g_log);file.open(path,std::ios::out|std::ios::trunc);
    auto* original=static_cast<std::ostream&>(file).rdbuf(&probe);
    j2melog::g_path=path;j2melog::g_sync=false;
    VMS_DIAG(g_p1).enabled=true;VMS_DIAG(g_p1).queued=0;VMS_DIAG(g_p1).caught_zero_pending=false;
    j2melog::start_writer();return original;
}
static void detach_probe(std::streambuf* original) {
    j2melog::shutdown();auto& file=VMS_PLAT(g_log);
    static_cast<std::ostream&>(file).rdbuf(original);file.close();j2melog::g_path.clear();
}
struct Result {bool fixture=false,queue_empty=false,early=false,flushed_at_return=false;unsigned overlaps=0;};
static Result check_boundary(const std::string& path,const std::string& mode) {
    BlockedLogBuffer probe;
    auto* original=attach_probe(path,probe);
    log_line("LOG ownership probe "+mode);
    Result result;result.fixture=probe.wait_for_write();
    if(result.fixture) {
        {std::lock_guard<std::mutex> lock(j2melog::g_mu);result.queue_empty=j2melog::g_q.empty();}
        // An old direct flush completes immediately while xsputn remains
        // blocked. A corrected durable drain may legitimately wait, so the
        // controller eventually releases the write to let that path finish.
        std::mutex controller_mu;std::condition_variable controller_cv;bool returned=false;
        std::thread controller([&] {
            std::unique_lock<std::mutex> lock(controller_mu);
            controller_cv.wait_for(lock,std::chrono::milliseconds(400),[&]{return returned;});
            probe.release();
        });
        if(mode=="drain")log_drain();else p1_flush(mode=="p1_reset"?"reset":"host_turn");
        result.early=probe.blocked();result.flushed_at_return=probe.flushed();result.overlaps=probe.overlaps();
        {std::lock_guard<std::mutex> lock(controller_mu);returned=true;}
        controller_cv.notify_one();controller.join();
    } else probe.release();
    detach_probe(original);
    return result;
}
static bool slow_flush(const std::string& path) {
    BlockedLogBuffer probe(false,true);auto* original=attach_probe(path,probe);
    log_line("LOG slow flush first");const bool fixture=probe.wait_for_flush();
    std::thread controller([&]{std::this_thread::sleep_for(std::chrono::milliseconds(400));probe.release();});
    const auto begin=std::chrono::steady_clock::now();log_line("LOG slow flush queued");
    const auto enqueue_us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count();
    const bool ack=j2melog::drain(),flushed=probe.flushed();controller.join();detach_probe(original);
    const bool ok=fixture&&ack&&flushed&&enqueue_us<100000&&probe.overlaps()==0&&probe.contents().find("LOG slow flush queued\n")!=std::string::npos;
    std::cout<<"case=slow_flush enqueue_us="<<enqueue_us<<" acknowledged="<<ack<<" foreign_sync="<<probe.overlaps()<<" "<<(ok?"PASS":"FAIL")<<"\n";return ok;
}
static bool multi_batch(const std::string& path) {
    BlockedLogBuffer probe(true,false,"LOG batch=179");auto* original=attach_probe(path,probe);
    for(int n=0;n<180;++n)j2melog::enqueue("LOG batch="+std::to_string(n),true);
    const bool fixture=probe.wait_for_write();
    std::thread controller([&]{std::this_thread::sleep_for(std::chrono::milliseconds(400));probe.release();});
    const bool ack=j2melog::drain(),early=probe.blocked(),flushed=probe.flushed();
    controller.join();detach_probe(original);const auto text=probe.contents();bool ordered=true;size_t previous=0;
    for(int n=0;n<180;++n) {
        const auto at=text.find("LOG batch="+std::to_string(n)+"\n",previous);
        if(at==std::string::npos){ordered=false;break;}previous=at+1;
    }
    const bool ok=fixture&&ack&&!early&&flushed&&ordered&&probe.overlaps()==0;
    std::cout<<"case=multi_batch records=180 ordered="<<ordered<<" acknowledged="<<ack<<" returned_early="<<early<<" "<<(ok?"PASS":"FAIL")<<"\n";return ok;
}
static bool queue_drop(const std::string& path) {
    BlockedLogBuffer probe;auto* original=attach_probe(path,probe);
    j2melog::enqueue("LOG full queue blocked",true);const bool fixture=probe.wait_for_write();
    for(size_t n=0;n<j2melog::kQueueCap;++n)j2melog::enqueue("LOG kept="+std::to_string(n),true);
    const auto before=j2melog::g_drops.load();j2melog::enqueue("discarded-hf",false);
    const bool dropped=j2melog::g_drops.load()==before+1;
    std::thread controller([&]{std::this_thread::sleep_for(std::chrono::milliseconds(400));probe.release();});
    const bool ack=j2melog::drain(),early=probe.blocked();controller.join();detach_probe(original);
    const auto text=probe.contents();const size_t lines=size_t(std::count(text.begin(),text.end(),'\n'));
    const bool ok=fixture&&dropped&&ack&&!early&&probe.overlaps()==0&&lines==j2melog::kQueueCap+1&&text.find("discarded-hf")==std::string::npos;
    std::cout<<"case=queue_cap_hf_drop dropped="<<dropped<<" records="<<lines<<" acknowledged="<<ack<<" "<<(ok?"PASS":"FAIL")<<"\n";return ok;
}
static bool drain_timeout(const std::string& path) {
    BlockedLogBuffer probe;auto* original=attach_probe(path,probe);
    j2melog::enqueue("LOG timeout blocked",true);const bool fixture=probe.wait_for_write();
    std::thread controller([&]{std::this_thread::sleep_for(std::chrono::milliseconds(2700));probe.release();});
    const auto begin=std::chrono::steady_clock::now();const bool ack=j2melog::drain();
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
    const bool blocked=probe.blocked();const auto overlaps=probe.overlaps();controller.join();
    const bool recovered=j2melog::drain();detach_probe(original);
    const bool ok=fixture&&!ack&&blocked&&overlaps==0&&elapsed>=1500&&elapsed<2500&&recovered;
    std::cout<<"case=drain_timeout elapsed_ms="<<elapsed<<" acknowledged="<<ack<<" writer_still_blocked="<<blocked<<" recovered="<<recovered<<" "<<(ok?"PASS":"FAIL")<<"\n";return ok;
}
static std::string contents(const std::string& path) {std::ifstream file(path);return std::string(std::istreambuf_iterator<char>(file),{});}
static bool reopen(const std::string& path,bool sync) {
    const std::string first=path+"-first.log",second=path+"-second.log";
    {std::ofstream old(second);old<<"stale-content\n";}
    setenv("J2ME_LOG_SYNC",sync?"1":"0",1);
    log_open(first.c_str(),true);log_line("LOG reopen first");log_shutdown();
    const bool stopped=!j2melog::g_started;
    setenv("J2ME_LOG_SYNC","0",1);
    log_open(second.c_str(),true);const bool async_started=j2melog::g_started&&!j2melog::g_sync;
    log_line("LOG reopen second");const bool ack=j2melog::drain();log_shutdown();
    VMS_PLAT(g_log).close();j2melog::g_path.clear();const auto a=contents(first),b=contents(second);
    const bool ok=stopped&&async_started&&ack&&!j2melog::g_started&&a.find("LOG reopen first\n")!=std::string::npos&&
        b.find("stale-content")==std::string::npos&&b.find("LOG reopen second\n")!=std::string::npos&&b.find("LOGASYNC")<b.find("LOG reopen second");
    std::cout<<"case="<<(sync?"sync_fallback_reopen":"reopen")<<" stopped="<<stopped<<" async_started="<<async_started<<" "<<(ok?"PASS":"FAIL")<<"\n";return ok;
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    setenv("J2ME_LOG_SYNC","0",1);
    int failures=0;
    for(const std::string mode:{"host_turn","p1_reset","drain"}) {
        const auto r=check_boundary(std::string(argv[1])+"/"+mode+".log",mode);
        const bool ok=r.fixture&&r.queue_empty&&r.overlaps==0&&
            (mode!="drain"||(!r.early&&r.flushed_at_return));
        std::cout<<"case="<<mode<<" writer_blocked="<<r.fixture<<" batch_queue_empty="<<r.queue_empty
                 <<" foreign_sync_during_write="<<r.overlaps<<" returned_before_write_complete="<<r.early
                 <<" writer_flushed_at_return="<<r.flushed_at_return<<" "<<(ok?"PASS":"FAIL")<<"\n";
        if(!ok)++failures;
    }
    if(!slow_flush(std::string(argv[1])+"/slow-flush.log"))++failures;
    if(!multi_batch(std::string(argv[1])+"/multi-batch.log"))++failures;
    if(!queue_drop(std::string(argv[1])+"/queue-drop.log"))++failures;
    if(!drain_timeout(std::string(argv[1])+"/timeout.log"))++failures;
    if(!reopen(std::string(argv[1])+"/reopen",false))++failures;
    if(!reopen(std::string(argv[1])+"/sync-reopen",true))++failures;
    std::cout<<(failures?"FAIL":"PASS")<<" async log ownership "<<(9-failures)<<"/9\n";
    return failures?1:0;
}
