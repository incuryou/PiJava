// P8.7 (0.2.77-p8.11-r2): async durable log. Main thread only enqueues; a low-priority writer
// thread (not core 0, not gated by p7) writes to the card. No per-line flush; batch/~1s.
// Key lines are never dropped; when the queue is full high-frequency lines are shed.
#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
namespace j2melog {
static std::string g_path;
static std::atomic<uint64_t> g_bytes{0},g_failures{0};
static uint64_t g_shed=0,g_shed_reported=0,g_fault_at=0;
static constexpr uint64_t kSoftBudget=24ull<<20;
static bool key(const std::string& m){
    static const char* const k[]={"BOOT","==== BOOT","LAUNCH","JAR ","SPEED ","P1END","P1STAT","P1FRAME","P1LASTEND","P7COUNT","P7GC_STATS","P7PRESENT",
        "P7AUDIO_STATS","P7WORKER","P8CAUGHT","P8ENABLE","P8FRAME","P86GFX","P86WAKE","LIFECYCLE","SHEET","EX ","JAVA ","exit","LOG","RENDER","SAVE","LOAD","J2S","CITYEND","P1EVENT schema=1","LOGCOST","LOGRECOVER","LOGASYNC","LOGDROP"};
    for(const char* p:k)if(m.rfind(p,0)==0){
        if(p[0]=='P'&&p[1]=='1'&&p[2]=='E'&&p[3]=='V')return m.find(" launch begin ")!=std::string::npos||m.find(" worker finish ")!=std::string::npos||m.find(" first_live_to_zero")!=std::string::npos;
        return true;}
    return false;
}
// Main-thread time spent inside log_line (format + enqueue, or sync write when J2ME_LOG_SYNC=1).
static std::atomic<uint64_t> g_main_us{0};
static uint64_t main_us_snapshot(){return g_main_us.load(std::memory_order_relaxed);}

struct Item{std::string line; bool key;uint64_t seq;};
static constexpr size_t kQueueCap=1024;
static std::mutex g_mu;
static std::condition_variable g_cv,g_drained;
static std::deque<Item> g_q;
static std::atomic<uint64_t> g_drops{0};
static uint64_t g_drops_reported=0;
static bool g_stop=false,g_started=false,g_sync=false;
// Protected by g_mu. A written watermark excludes the writer's in-flight
// batch; a flush acknowledgement is published only after that I/O returns.
static uint64_t g_enqueued=0,g_written=0,g_flush_request=0,g_flush_target=0,g_flush_ack=0;
#ifdef __SWITCH__
static Thread g_thr{};
static bool g_thr_ok=false;
#else
static std::thread g_thr;
#endif

static bool write_raw(const std::string& line,bool flush){
    auto& o=VMS_PLAT(g_log);
    if(!o.is_open()&&g_path.empty())return false;
    if(o){ o<<line<<"\n"; if(flush)o.flush(); }
    if(g_fault_at&&g_bytes>=g_fault_at){g_fault_at=0;o.setstate(std::ios::badbit);}
    if(o){ g_bytes+=line.size()+1; return true; }
    const int e=errno; ++g_failures;
    o.clear(); if(o.is_open())o.close();
    if(g_path.empty())return false;
    o.open(g_path,std::ios::out|std::ios::app);
    if(!o)return false;
    o<<"LOGRECOVER failures="<<g_failures<<" errno="<<e<<" bytes="<<g_bytes<<"\n"<<line<<"\n"; o.flush();
    if(!o)return false;
    g_bytes+=line.size()+1; return true;
}

static void flush_raw(){if(VMS_PLAT(g_log).is_open())VMS_PLAT(g_log).flush();}
static bool enabled(){return !g_path.empty()||((!g_started||g_sync)&&VMS_PLAT(g_log).is_open());}
static bool sync_requested(){
#ifdef __SWITCH__
    return false;
#else
    const char* s=std::getenv("J2ME_LOG_SYNC");return s&&std::strcmp(s,"1")==0;
#endif
}
static void writer_body(){
    std::vector<Item> batch; batch.reserve(64);
    auto last_flush=std::chrono::steady_clock::now();
    for(;;){
        bool stopping=false;
        {
            std::unique_lock<std::mutex> lk(g_mu);
            g_cv.wait_for(lk,std::chrono::milliseconds(200),[]{return g_stop||!g_q.empty()||g_flush_request>g_flush_ack;});
            stopping=g_stop;
            while(!g_q.empty()&&batch.size()<64){ batch.push_back(std::move(g_q.front())); g_q.pop_front(); }
        }
        bool has_key=false;
        for(auto& it:batch){ if(it.key)has_key=true; write_raw(it.line,false); }
        uint64_t acknowledge=0;
        {
            std::lock_guard<std::mutex> lk(g_mu);
            if(!batch.empty())g_written=batch.back().seq;
            if(g_written>=g_flush_target)acknowledge=g_flush_request;
        }
        auto now=std::chrono::steady_clock::now();
        bool requested=false;
        {std::lock_guard<std::mutex> lk(g_mu);requested=acknowledge>g_flush_ack;}
        if(has_key||requested||stopping||now-last_flush>=std::chrono::seconds(1)||batch.size()>=32){
            flush_raw(); // Never hold the queue mutex during SD I/O.
            last_flush=now;
            std::lock_guard<std::mutex> lk(g_mu);
            g_flush_ack=std::max(g_flush_ack,acknowledge);g_drained.notify_all();
        }
        batch.clear();
        {
            std::unique_lock<std::mutex> lk(g_mu);
            if(stopping&&g_q.empty()&&g_flush_ack>=g_flush_request)return;
        }
    }
}

#ifdef __SWITCH__
static void writer_entry(void*){ writer_body(); }
#endif

static void register_atexit();
static void start_writer(){
    if(g_started)return;
    g_started=true;register_atexit();g_sync=sync_requested();
    {std::lock_guard<std::mutex> lk(g_mu);g_stop=false;g_enqueued=g_written=g_flush_request=g_flush_target=g_flush_ack=0;}
#ifdef __SWITCH__
    g_sync=false; // Switch always async; sync path is a desktop A/B hook only
    // Core 2, priority below the game/p7 workers (0x2C). Not core 0.
    Result rc=threadCreate(&g_thr,&writer_entry,nullptr,nullptr,0x8000,0x30,2);
    if(R_SUCCEEDED(rc)){ rc=threadStart(&g_thr); if(R_SUCCEEDED(rc)) g_thr_ok=true; else threadClose(&g_thr); }
    if(!g_thr_ok){ g_sync=true; g_started=true; } // fall back to sync if threadCreate fails
#else
    if(!g_sync) g_thr=std::thread(&writer_body);
#endif
}

static void enqueue(std::string line,bool is_key){
    if(g_sync||!g_started){ write_raw(line,is_key); return; }
    std::unique_lock<std::mutex> lk(g_mu);
    auto drop_one_hf=[&]()->bool{
        for(auto it=g_q.begin();it!=g_q.end();++it)if(!it->key){ g_q.erase(it); ++g_drops; return true; }
        return false;
    };
    while(g_q.size()>=kQueueCap){
        if(is_key){ if(!drop_one_hf()) break; }
        else { if(!drop_one_hf()){ ++g_drops; return; } }
    }
    if(g_q.size()>=kQueueCap){ // queue is all keys
        if(!is_key){ ++g_drops; return; }
        // still enqueue key by dropping oldest key (last resort) — prefer keeping newest BOOT/SAVE
        g_q.pop_front(); ++g_drops;
    }
    g_q.push_back(Item{std::move(line),is_key,++g_enqueued});
    lk.unlock(); g_cv.notify_one();
}

static bool drain(){
    if(g_sync||!g_started){flush_raw();return true;}
    std::unique_lock<std::mutex> lk(g_mu);
    const uint64_t request=++g_flush_request;g_flush_target=g_enqueued;g_cv.notify_one();
    return g_drained.wait_for(lk,std::chrono::seconds(2),[&]{return g_flush_ack>=request;});
}

static void shutdown(){
    if(!g_started)return;
    if(g_sync){flush_raw();g_started=false;g_stop=false;return;}
    {
        std::unique_lock<std::mutex> lk(g_mu);
        g_stop=true; g_cv.notify_one();
    }
#ifdef __SWITCH__
    if(g_thr_ok){ threadWaitForExit(&g_thr); threadClose(&g_thr); g_thr_ok=false; }
#else
    if(g_thr.joinable()) g_thr.join();
#endif
    g_started=false; g_stop=false;
    // The writer flushed its final batch before exiting; only now may the
    // lifecycle owner close or reopen the stream.
}
static void atexit_shutdown(){ shutdown(); }
static void register_atexit(){ static bool reg=false; if(!reg){reg=true; std::atexit(&atexit_shutdown);} }
} // namespace j2melog
