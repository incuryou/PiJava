#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
// Menu-only blocking observations. Main and GPU have separate single writers.
// Logger reads atomic numbers and static enum labels, never VM/menu containers.
namespace menu_stall {
enum class Stage:uint64_t {Loop,Applet,Focus,Input,TitleEnable,Draw,Preferences,Page,Titles,Select,Prioritize,DrawDone,PollTitles,PollDone,Present,Compose,ComposeDone,CpuAcquire,CpuPresent,OwnerCredit,OwnerSubmit,PresentDone,ModalReturn,GpuAcquire,GpuAcquireDone,GpuFence,GpuUpload,GpuCommands,GpuSubmit,GpuPresent,GpuDone,GpuFinish,GpuClose};
inline const char* name(Stage s){switch(s){
#define MENU_STAGE(id,label) case Stage::id:return label;
 MENU_STAGE(Loop,"loop") MENU_STAGE(Applet,"applet") MENU_STAGE(Focus,"focus") MENU_STAGE(Input,"input") MENU_STAGE(TitleEnable,"title_enable")
 MENU_STAGE(Draw,"draw") MENU_STAGE(Preferences,"preferences") MENU_STAGE(Page,"page") MENU_STAGE(Titles,"titles") MENU_STAGE(Select,"select") MENU_STAGE(Prioritize,"prioritize") MENU_STAGE(DrawDone,"draw_done")
 MENU_STAGE(PollTitles,"poll_titles") MENU_STAGE(PollDone,"poll_done") MENU_STAGE(Present,"present") MENU_STAGE(Compose,"compose") MENU_STAGE(ComposeDone,"compose_done") MENU_STAGE(CpuAcquire,"cpu_acquire") MENU_STAGE(CpuPresent,"cpu_present")
 MENU_STAGE(OwnerCredit,"owner_credit") MENU_STAGE(OwnerSubmit,"owner_submit") MENU_STAGE(PresentDone,"present_done") MENU_STAGE(ModalReturn,"modal_return")
 MENU_STAGE(GpuAcquire,"gpu_acquire") MENU_STAGE(GpuAcquireDone,"gpu_acquire_done") MENU_STAGE(GpuFence,"gpu_fence") MENU_STAGE(GpuUpload,"gpu_upload") MENU_STAGE(GpuCommands,"gpu_commands") MENU_STAGE(GpuSubmit,"gpu_submit") MENU_STAGE(GpuPresent,"gpu_present") MENU_STAGE(GpuDone,"gpu_done") MENU_STAGE(GpuFinish,"gpu_finish") MENU_STAGE(GpuClose,"gpu_close_wait_idle")
#undef MENU_STAGE
 }return "unknown";}
inline int64_t now_us(){return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Snapshot{int64_t when=0;Stage stage=Stage::Loop;uint64_t epoch=0,sequence=0;int selected=-1,count=0;};
class Live {
 std::atomic<uint64_t> sequence_{0};std::array<std::atomic<uint64_t>,5> words_{};
public:
 void publish(int64_t when,Stage stage,uint64_t epoch,int selected=-1,int count=0){
  sequence_.fetch_add(1,std::memory_order_acq_rel);
  const uint64_t words[]={uint64_t(when),uint64_t(stage),epoch,uint64_t(int64_t(selected)),uint64_t(count)};
  for(size_t i=0;i<5;++i)words_[i].store(words[i],std::memory_order_relaxed);
  sequence_.fetch_add(1,std::memory_order_release);
 }
 bool read(Snapshot& out)const{
  for(int attempt=0;attempt<4;++attempt){const auto seq=sequence_.load(std::memory_order_acquire);if(seq&1)continue;
   uint64_t words[5];for(size_t i=0;i<5;++i)words[i]=words_[i].load(std::memory_order_relaxed);
   std::atomic_thread_fence(std::memory_order_acquire);if(seq!=sequence_.load(std::memory_order_acquire))continue;
   out={int64_t(words[0]),Stage(words[1]),words[2],seq,int(int64_t(words[3])),int(words[4])};return true;
  }return false;
 }
};
class Progress {
 std::atomic<bool> enabled_{false},paused_{false};std::atomic<uint64_t> epoch_{0};
 int selected_=-1,count_=0;unsigned pause_depth_=0; // main-thread-only context
public:
 Live main,gpu;
 bool enabled()const{return enabled_.load(std::memory_order_acquire);}
 bool paused()const{return paused_.load(std::memory_order_acquire);}
 uint64_t epoch()const{return epoch_.load(std::memory_order_acquire);}
 void context(int selected,int count){selected_=selected;count_=count;}
 void menu_at(bool on,int64_t when){
  if(!on){enabled_.store(false,std::memory_order_release);return;}
  if(enabled())return;
  const auto next=epoch_.fetch_add(1,std::memory_order_acq_rel)+1;
  main.publish(when,Stage::Loop,next,selected_,count_);enabled_.store(true,std::memory_order_release);
 }
 void main_at(Stage s,int64_t when){if(enabled()&&!paused())main.publish(when,s,epoch(),selected_,count_);}
 void gpu_at(Stage s,int64_t when){if(enabled()&&!paused())gpu.publish(when,s,epoch());}
 void pause_at(int64_t){if(!pause_depth_++)paused_.store(true,std::memory_order_release);}
 void resume_at(int64_t when){if(pause_depth_&&!--pause_depth_){if(enabled())main.publish(when,Stage::ModalReturn,epoch(),selected_,count_);paused_.store(false,std::memory_order_release);}}
};
inline Progress progress;
inline void menu(bool on){progress.menu_at(on,now_us());}
inline void main(Stage s){if(progress.enabled()&&!progress.paused())progress.main_at(s,now_us());}
inline void gpu(Stage s){if(progress.enabled()&&!progress.paused())progress.gpu_at(s,now_us());}
class Pause{public:Pause(){progress.pause_at(now_us());}~Pause(){progress.resume_at(now_us());}Pause(const Pause&)=delete;Pause& operator=(const Pause&)=delete;};
class Watchdog {
 struct Lane{uint64_t sequence=0,epoch=0;int64_t last_report=0;};Lane lanes_[2];
public:
 template<class Sink>void poll(int64_t now,const Progress& p,Sink&& sink){
  if(!p.enabled()||p.paused())return;
  Snapshot main;if(!p.main.read(main))return;
  for(int lane=0;lane<2;++lane){Snapshot v;if(lane==0)v=main;else if(!p.gpu.read(v))continue;
   if(v.epoch!=p.epoch()||!v.when||now<v.when||(lane==1&&v.stage==Stage::GpuDone))continue;
   auto& last=lanes_[lane];if(last.sequence!=v.sequence||last.epoch!=v.epoch){last={v.sequence,v.epoch,0};}
   if(now-v.when<2000000||(last.last_report&&now-last.last_report<10000000))continue;
   if(!p.enabled()||p.paused()||p.epoch()!=v.epoch)return;
   last.last_report=now;char line[384];std::snprintf(line,sizeof(line),"MENU_STALL schema=1 lane=%s stage=%s selected=%d count=%d age_us=%lld mono_us=%lld sequence=%llu epoch=%llu threshold_us=2000000 scope=last_progress_not_root_cause",
    lane?"gpu":"main",name(v.stage),main.selected,main.count,(long long)(now-v.when),(long long)now,(unsigned long long)v.sequence,(unsigned long long)v.epoch);sink(line);
  }
 }
};
}
