#pragma once
#include "game_names.h"
#include "host_menu_settings.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <sys/stat.h>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <pthread.h>
#endif
namespace game_titles {
// Display metadata only: never an alias, save identity, or JAR mutation.
struct Record { std::string path, name, detail; uint64_t size=uint64_t(-1); int64_t mtime=0; };
struct Hooks {
    std::function<bool(const std::string&,uint64_t&,int64_t&)> stat;
    std::function<bool(const std::string&,std::string&)> manifest;
    std::function<bool(const std::string&,const std::string&)> save;
};
struct Limits { size_t records=8192, results=128; unsigned delay_ms=4, save_after=64, save_delay_ms=1000; };
class Service {
    static constexpr size_t max_file=8*1024*1024, max_records=8192, max_path=1024;
    using Clock=std::chrono::steady_clock;
    struct Entry { Record record; bool known=false, pending=false; };
    Hooks hooks_;
    Limits limits_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
#ifdef __SWITCH__
    Thread worker_{};
    static void entry(void* self) {static_cast<Service*>(self)->run();}
#else
    pthread_t worker_{};
    static void* entry(void* self) {static_cast<Service*>(self)->run();return nullptr;}
#endif
    std::map<std::string,Entry> entries_;
    std::vector<std::string> order_;
    std::deque<std::string> priority_, results_;
    std::set<std::string> queued_;
    std::string cache_;
    size_t cursor_=0, dirty_=0;
    uint64_t generation_=0, revision_=0;
    bool running_=false, enabled_=false, stopping_=false, save_failed_=false;
    Clock::time_point last_save_=Clock::now();

    static bool path_ok(const std::string& p) { return !p.empty()&&p.size()<=max_path&&p.find('\0')==std::string::npos; }
    static std::string safe(const std::string& text,size_t chars,size_t bytes) {
        if(text.size()>bytes)return {};
        std::vector<uint32_t> cps;
        if(!game_names::utf8_codepoints(text,&cps)||cps.size()>chars)return {};
        for(uint32_t c:cps)if(c<32||(c>=127&&c<=159)||c==0x2028||c==0x2029)return {};
        return game_names::trim(text);
    }
    static Record read(const std::string& path,const Hooks& hooks) {
        Record r;r.path=path;uint64_t size=uint64_t(-1);int64_t stamp=0;
        const bool exists=hooks.stat(path,size,stamp);
        if(exists){r.size=size;r.mtime=stamp;}
        return r;
    }
    static void describe(Record& r,const Hooks& hooks) {
        std::string manifest,version;
        if(r.size!=uint64_t(-1)&&hooks.manifest(r.path,manifest)) {
            const auto props=game_names::parse_manifest(manifest);
            r.name=safe(game_names::choose_name(props),128,512);
            auto it=props.find("MIDlet-Version");if(it!=props.end())version=safe(it->second,64,256);
        }
        // Malformed path bytes must not leak invalid UTF-8 to the detail renderer.
        std::string label=r.name.empty()?safe(game_names::strip_jar(r.path),256,1024):r.name;
        if(label.empty())label="?";
        r.detail=label+u8" · "+(version.empty()?u8"版本未知":u8"版本 "+version)+u8" · "+
            (r.size==uint64_t(-1)?u8"大小未知":std::to_string(r.size/1024+(r.size%1024!=0))+" KB");
    }
    static void number(std::string& out,uint64_t n) {for(int i=0;i<8;++i){out+=char(n);n>>=8;}}
    static bool number(const std::string& in,size_t& pos,uint64_t& n) {
        if(pos>in.size()||in.size()-pos<8)return false;
        n=0;for(int i=0;i<8;++i)n|=uint64_t((unsigned char)in[pos++])<<(i*8);return true;
    }
    static void field(std::string& out,const std::string& s) {number(out,s.size());out+=s;}
    static bool field(const std::string& in,size_t& pos,std::string& s,size_t max) {
        uint64_t len;if(!number(in,pos,len)||len>max||len>in.size()-pos)return false;s.assign(in,pos,size_t(len));pos+=size_t(len);return true;
    }
    static uint64_t checksum(const std::string& s,size_t length) {
        uint64_t hash=1469598103934665603ull;for(size_t i=0;i<length;++i){hash^=(unsigned char)s[i];hash*=1099511628211ull;}return hash;
    }
    bool load_file(const std::string& path) {
        FILE* f=std::fopen(path.c_str(),"rb");if(!f)return false;
        std::string data;char buffer[4096];size_t count;
        while((count=std::fread(buffer,1,sizeof(buffer),f))!=0){data.append(buffer,count);if(data.size()>max_file)break;}
        bool ok=!std::ferror(f)&&data.size()<=max_file;ok=std::fclose(f)==0&&ok;
        const std::string magic="J2ME_TITLES 1\n";
        if(!ok||data.size()<magic.size()+16||data.compare(0,magic.size(),magic)!=0)return false;
        size_t end=data.size()-8,pos=end;uint64_t sum;
        if(!number(data,pos,sum)||sum!=checksum(data,end))return false;
        pos=magic.size();uint64_t count_records;
        if(!number(data,pos,count_records)||count_records>limits_.records)return false;
        std::map<std::string,Entry> loaded;
        for(uint64_t i=0;i<count_records;++i){
            Record r;uint64_t time;
            if(!field(data,pos,r.path,max_path)||!path_ok(r.path)||!field(data,pos,r.name,512)||
                !field(data,pos,r.detail,2048)||!number(data,pos,r.size)||!number(data,pos,time))return false;
            r.mtime=int64_t(time);
            if(safe(r.name,128,512)!=r.name||safe(r.detail,1024,2048)!=r.detail||loaded.count(r.path))return false;
            loaded[r.path]={r,true,false};
        }
        if(pos!=end)return false;
        entries_.swap(loaded);return true;
    }
    std::string snapshot() {
        // Copy one bounded record per lock; menus never wait for an entire
        // library snapshot. A concurrent reset keeps revision dirty for retry.
        std::string body,last;size_t count=0;
        while(count<limits_.records){
            Record r;
            {
                std::lock_guard<std::mutex> lock(mutex_);auto it=entries_.upper_bound(last);
                while(it!=entries_.end()&&!it->second.known)++it;
                if(it==entries_.end())break;
                last=it->first;r=it->second.record;
            }
            std::string row;field(row,r.path);field(row,r.name);field(row,r.detail);number(row,r.size);number(row,uint64_t(r.mtime));
            if(body.size()+row.size()+64>max_file)break;
            body+=row;++count;}
        std::string result="J2ME_TITLES 1\n";number(result,count);result+=body;number(result,checksum(result,result.size()));return result;
    }
    void persist() {
        uint64_t revision;
        {std::lock_guard<std::mutex> lock(mutex_);if(!dirty_||cache_.empty())return;revision=revision_;}
        const auto data=snapshot();const bool saved=hooks_.save(cache_,data);
        std::lock_guard<std::mutex> lock(mutex_);last_save_=Clock::now();save_failed_=!saved;
        if(saved&&revision==revision_)dirty_=0;
    }
    bool next(std::string& path) {
        while(!priority_.empty()){
            path=std::move(priority_.front());priority_.pop_front();auto it=entries_.find(path);
            if(it!=entries_.end()&&it->second.pending){it->second.pending=false;return true;}
        }
        while(cursor_<order_.size()){
            path=order_[cursor_++];auto it=entries_.find(path);
            if(it!=entries_.end()&&it->second.pending){it->second.pending=false;return true;}
        }
        return false;
    }
    void run() {
        for(;;){
            std::string path;Record old;bool known=false;uint64_t generation=0;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if(stopping_){lock.unlock();persist();return;}
                if(!enabled_){wake_.wait(lock,[&]{return stopping_||enabled_;});continue;}
                const bool due=dirty_&&!cache_.empty()&&Clock::now()-last_save_>=std::chrono::milliseconds(limits_.save_delay_ms);
                if(!cache_.empty()&&(due||(!save_failed_&&dirty_>=limits_.save_after&&Clock::now()-last_save_>=std::chrono::milliseconds(10)))){
                    lock.unlock();persist();continue;
                }
                if(!next(path)){
                    if(dirty_&&!cache_.empty())wake_.wait_until(lock,last_save_+std::chrono::milliseconds(limits_.save_delay_ms));
                    else wake_.wait(lock);
                    continue;
                }
                generation=generation_;const auto& entry=entries_.find(path)->second;known=entry.known;old=entry.record;
            }
            Record result=read(path,hooks_);
            if(!known||old.size!=result.size||old.mtime!=result.mtime)describe(result,hooks_);else result=old;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                auto entry=entries_.find(path);
                if(generation==generation_&&entry!=entries_.end()){
                    const bool changed=!known||old.size!=result.size||old.mtime!=result.mtime||old.name!=result.name||old.detail!=result.detail;
                    entry->second.record=result;entry->second.known=true;
                    if(changed){++dirty_;++revision_;
                        if(!queued_.count(path)){
                            if(results_.size()>=limits_.results){queued_.erase(results_.front());results_.pop_front();}
                            results_.push_back(path);queued_.insert(path);
                        }
                    }
                }
                // disable wakes this throttle immediately; it never starts another job.
                if(limits_.delay_ms)wake_.wait_for(lock,std::chrono::milliseconds(limits_.delay_ms),[&]{return stopping_||!enabled_||generation!=generation_;});
            }
        }
    }
public:
    explicit Service(Hooks hooks={},Limits limits={}):hooks_(std::move(hooks)),limits_(limits) {
        limits_.records=std::max(size_t(1),std::min(limits_.records,max_records));
        limits_.results=std::max(size_t(1),std::min(limits_.results,size_t(128)));
        limits_.save_after=std::max(1u,limits_.save_after);limits_.save_delay_ms=std::max(10u,limits_.save_delay_ms);
        if(!hooks_.stat)hooks_.stat=[](const std::string& path,uint64_t& size,int64_t& time){struct stat s;if(::stat(path.c_str(),&s)!=0||!S_ISREG(s.st_mode))return false;size=uint64_t(s.st_size);time=int64_t(s.st_mtime);return true;};
        if(!hooks_.manifest)hooks_.manifest=[](const std::string& path,std::string& text){return game_names::read_manifest_only(path,text);};
        if(!hooks_.save)hooks_.save=[](const std::string& path,const std::string& data){return host_menu::save_record(path,data);};
    }
    ~Service(){stop();}
    Service(const Service&)=delete;
    Service& operator=(const Service&)=delete;
    // The only synchronous I/O is one bounded cache load at startup. No JAR stat.
    bool start(const std::string& cache_path) {
        std::lock_guard<std::mutex> lock(mutex_);if(running_)return false;
        if(cache_path.find('\0')!=std::string::npos)return false;
        cache_=cache_path;entries_.clear();order_.clear();priority_.clear();results_.clear();queued_.clear();cursor_=dirty_=0;revision_=0;
        if(!cache_.empty()&&!load_file(cache_))load_file(cache_+".previous");
          // puff.c lazily builds mutable fixed-code tables. Initialize them on
          // the owner before metadata and game decoders can run concurrently.
          if(!j2me_image::initialize_puff())return false;
        enabled_=stopping_=save_failed_=false;last_save_=Clock::now();
#ifdef __SWITCH__
        Result rc=threadCreate(&worker_,&Service::entry,this,nullptr,0x20000,0x30,1);
        if(R_FAILED(rc))return false;
        rc=threadStart(&worker_);if(R_FAILED(rc)){threadClose(&worker_);return false;}
#else
        if(pthread_create(&worker_,nullptr,&Service::entry,this)!=0)return false;
#endif
        running_=true;return true;
    }
    // All menu-time calls below perform memory work only.
    void reset(const std::vector<std::string>& paths) {
        std::lock_guard<std::mutex> lock(mutex_);++generation_;std::map<std::string,Entry> next_entries;order_.clear();
        for(const auto& path:paths){if(next_entries.size()>=limits_.records)break;if(!path_ok(path)||next_entries.count(path))continue;
            auto it=entries_.find(path);Entry e;if(it!=entries_.end())e=std::move(it->second);e.pending=true;next_entries.emplace(path,std::move(e));order_.push_back(path);}
        for(const auto& e:entries_)if(e.second.known&&!next_entries.count(e.first)){++dirty_;++revision_;}
        entries_.swap(next_entries);cursor_=0;priority_.clear();results_.clear();queued_.clear();wake_.notify_one();
    }
    void enable(bool enabled) {std::lock_guard<std::mutex> lock(mutex_);if(enabled_==enabled)return;enabled_=enabled;wake_.notify_one();}
    // Call with selected path first, then visible page, before enable(true).
    void prioritize(const std::vector<std::string>& paths) {
        std::lock_guard<std::mutex> lock(mutex_);priority_.clear();
        for(const auto& p:paths){if(priority_.size()>=128)break;auto it=entries_.find(p);if(it!=entries_.end()&&it->second.pending&&std::find(priority_.begin(),priority_.end(),p)==priority_.end())priority_.push_back(p);}wake_.notify_one();
    }
    bool contains(const std::string& path)const {
        std::lock_guard<std::mutex> lock(mutex_);return entries_.find(path)!=entries_.end();
    }
    bool find(const std::string& path,Record& record)const {
        std::lock_guard<std::mutex> lock(mutex_);auto it=entries_.find(path);if(it==entries_.end()||!it->second.known)return false;record=it->second.record;return true;
    }
    // A nonempty batch is a menu refresh signal: overflow coalesces notifications,
    // while find() retains every current record (including a negative name).
    std::vector<Record> poll(size_t limit=8) {
        std::lock_guard<std::mutex> lock(mutex_);std::vector<Record> out;limit=std::min(limit,limits_.results);
        while(!results_.empty()&&out.size()<limit){std::string path=std::move(results_.front());results_.pop_front();queued_.erase(path);auto it=entries_.find(path);if(it!=entries_.end()&&it->second.known)out.push_back(it->second.record);}return out;
    }
    // Shutdown waits for one in-flight I/O and one batched save, never all JARs.
    void stop() {
        {std::lock_guard<std::mutex> lock(mutex_);if(!running_)return;enabled_=false;stopping_=true;wake_.notify_one();}
#ifdef __SWITCH__
        threadWaitForExit(&worker_);threadClose(&worker_);
#else
        pthread_join(worker_,nullptr);
#endif
        std::lock_guard<std::mutex> lock(mutex_);running_=false;
    }
};
}
