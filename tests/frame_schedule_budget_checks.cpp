#include "../src/frame_schedule.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

static int checks=0;
static void check(bool ok,const char* reason) {
    ++checks;
    if(!ok) {std::cerr<<"FAIL FrameScheduleBudgetChecks "<<reason<<'\n';std::exit(1);}
}

static void budget_checks() {
    FrameWaitBudget budget;
    check(budget.cap_ms()==2,"startup retains a two millisecond rounding bridge");
    budget.observe(8000,500);
    check(budget.cap_ms()==6,"moderate advance and submit leave a bounded six millisecond bridge");
    check(budget.cap_ms(2000)==5 && budget.cap_ms(7500)==2,
          "elapsed slot work consumes extended wait budget while preserving the legacy bridge");
    check(budget.cap_ms(-1)==6,"a backwards mock clock never enlarges the hard cap");
    budget.observe(12000,500);
    check(budget.cap_ms()==3,"work cost reduces the cap instead of always waiting six milliseconds");
    for(unsigned i=0;i<FrameWaitBudget::history_size-1;++i)budget.observe(8000,500);
    check(budget.cap_ms()==3,"a recent expensive frame survives seven cheap observations");
    budget.observe(8000,500);
    check(budget.cap_ms()==6,"expired peak costs age out after eight new frames");
    budget.observe(16000,0);
    check(budget.cap_ms()==2,"near-period advance keeps only the legacy bridge");
    budget.observe(std::numeric_limits<int64_t>::max(),std::numeric_limits<int64_t>::max());
    check(budget.cap_ms(std::numeric_limits<int64_t>::max())==2,
          "huge durations cannot overflow into an extended wait budget");
    budget={};budget.observe(-100,-200);
    check(budget.cap_ms()==6 && budget.recent_work_us()==0 && budget.recent_submit_us()==0,
          "negative samples clamp to zero while the hard upper bound remains");
    budget.observe(8000,8000);
    check(budget.cap_ms()==2,"submit overrun disables extension without removing the legacy bridge");
    budget={};budget.observe(11000,666);
    check(budget.cap_ms()==4,"millisecond cap rounds down at a budget boundary");
    budget.observe(11000,667);
    check(budget.cap_ms()==4,"an exact four millisecond budget is usable");
    budget.observe(11000,668);
    check(budget.cap_ms()==3,"one microsecond below the boundary cannot overspend a full millisecond");
}

static void callback_checks() {
    FrameWaitBudget budget;budget.observe(8000,500);
    for(int64_t remaining:{-1,0,1,2,3,4,5,6,7,10,16,60000}) {
        int64_t now=1000,requested=0;
        int acquires=0,delays=0,waits=0,advances=0,clocks=0;
        std::string order;WakeStats stats;
        scheduled_frame([&]{++acquires;now+=100000;order+='A';},
            [&]{++delays;order+='D';return remaining;},
            [&](int64_t ms){++waits;requested=ms;order+='W';},
            [&]{++advances;order+='J';},budget,
            [&]{++clocks;return now;},&stats);
        const bool near=remaining>0 && remaining<=6;
        check(order==(near?"ADWJ":"ADJ"),"slot acquisition precedes the optional deadline bridge");
        check(acquires==1 && delays==1 && waits==(near?1:0) && advances==1 && clocks==2,
              "there is one deadline query, one wait at most and one Java phase");
        check(!near || requested==remaining,"wait preserves the full original Java delay");
        check(stats.budget_samples==1 && stats.budget_cap_min_ms==6 && stats.budget_cap_max_ms==6,
              "blocking acquire time is excluded from the new slot's budget");
        check(stats.deferred_wakes==(remaining>6?1u:0u) &&
              stats.extended_waits==(remaining>=3 && remaining<=6?1u:0u),
              "diagnostics distinguish extended waits and rejected distant deadlines");
    }
    int64_t now=0;int waits=0,advances=0;
    scheduled_frame([]{},[&]{now+=3000;return int64_t(5);},[&](int64_t){++waits;},
        [&]{++advances;},budget,[&]{return now;});
    check(waits==0 && advances==1,"deadline-query overhead can reject an otherwise affordable wait");
    scheduled_frame([]{},[]{return int64_t(4);},[&](int64_t){++waits;},
        [&]{++advances;},budget,[&]{return now;});
    check(waits==1 && advances==2,"a stationary fake clock still advances after one wait without polling");
    budget.observe(16000,500);
    scheduled_frame([]{},[]{return int64_t(3);},[&](int64_t){++waits;},
        [&]{++advances;},budget,[&]{return now;});
    check(waits==1 && advances==3,"high load rejects a three millisecond extension");
    scheduled_frame([]{},[]{return int64_t(2);},[&](int64_t){++waits;},
        [&]{++advances;},budget,[&]{return now;});
    check(waits==2 && advances==4,"high load preserves the existing two millisecond bridge");
}

struct Simulation {
    int frames=0,updates=0,waits=0;
    int64_t elapsed_us=0,wait_us=0,max_wait_us=0;
    WakeStats stats;
};
// Deterministic model only: fixed 60Hz slot acquisition, millisecond Java clock,
// a single worker doing work then Thread.sleep(10), and a 0.5ms host submission.
// It models cadence and timer lateness, not interpreter or console performance.
static Simulation simulate(bool adaptive,int64_t update_work_us) {
    Simulation result;FrameWaitBudget budget;
    int64_t now=0,wake_ms=0,slot_us=0;
    for(int frame=0;frame<180;++frame) {
        int64_t advance_us=0;
        auto acquire=[&] {now=std::max(now,slot_us);};
        auto delay=[&] {return std::max<int64_t>(0,wake_ms-now/1000);};
        auto wait=[&](int64_t ms) {
            const int64_t us=ms*1000;
            ++result.waits;result.wait_us+=us;result.max_wait_us=std::max(result.max_wait_us,us);
            now+=us;result.stats.waited(us);
        };
        auto advance=[&] {
            if(now/1000>=wake_ms) {
                if(result.updates)result.stats.dispatched(wake_ms,now/1000);
                ++result.updates;now+=update_work_us;advance_us=update_work_us;
                wake_ms=now/1000+10;
            } else {advance_us=100;now+=advance_us;}
        };
        if(adaptive)scheduled_frame(acquire,delay,wait,advance,budget,[&]{return now;},&result.stats);
        else scheduled_frame(acquire,delay,wait,advance,2);
        now+=500;budget.observe(advance_us,500);
        // Overrun skips occupied screen slots, never runs extra Java slices.
        slot_us=(now/FrameWaitBudget::frame_period_us+1)*FrameWaitBudget::frame_period_us;
        ++result.frames;
    }
    result.elapsed_us=now;
    return result;
}
static void print_simulation(const char* name,const Simulation& old,const Simulation& current) {
    auto output=[](const Simulation& s) {
        std::cout<<"{\"frames\":"<<s.frames<<",\"updates\":"<<s.updates
                 <<",\"elapsed_us\":"<<s.elapsed_us<<",\"waits\":"<<s.waits
                 <<",\"wait_us\":"<<s.wait_us<<",\"max_wait_us\":"<<s.max_wait_us
                 <<",\"wake_late_avg_ms\":"<<(s.stats.sleep_wakes?double(s.stats.wake_late_ms)/s.stats.sleep_wakes:0)
                 <<",\"wake_late_max_ms\":"<<s.stats.wake_late_max_ms
                 <<",\"wake_late_3ms_count\":"<<s.stats.wake_late_3ms_count
                 <<",\"extended_waits\":"<<s.stats.extended_waits
                 <<",\"deferred_wakes\":"<<s.stats.deferred_wakes<<'}';
    };
    std::cout<<"MODEL {\"scenario\":\""<<name<<"\",\"old\":";output(old);
    std::cout<<",\"bounded\":";output(current);std::cout<<"}\n";
}
static void simulation_checks() {
    const Simulation low_old=simulate(false,2000),low=simulate(true,2000);
    check(low_old.updates==180 && low.updates==180,"low load retains one update per 60Hz screen slot");
    check(low.elapsed_us==low_old.elapsed_us && low.waits==0,"low-load cadence incurs no new wait");
    print_simulation("work2ms_sleep10ms",low_old,low);
    const Simulation medium_old=simulate(false,8000),medium=simulate(true,8000);
    check(medium.updates>medium_old.updates,"bounded waiting avoids moderate-load extra missed slots");
    check(medium.stats.wake_late_ms*medium_old.stats.sleep_wakes <
          medium_old.stats.wake_late_ms*medium.stats.sleep_wakes,
          "moderate load has lower average Java dispatch lateness");
    check(medium.stats.wake_late_3ms_count<medium_old.stats.wake_late_3ms_count && medium.stats.extended_waits>0,
          "three-to-six millisecond bridges reduce missed deadlines");
    check(medium.max_wait_us<=FrameWaitBudget::hard_wait_us,"model never requests a wait above six milliseconds");
    print_simulation("work8ms_sleep10ms",medium_old,medium);
    for(int64_t work:{16000,18000,25000}) {
        const Simulation high_old=simulate(false,work),high=simulate(true,work);
        check(high.waits==high_old.waits && high.wait_us==high_old.wait_us && high.max_wait_us<=2000,
              "high load preserves existing waits without adding an extended bridge");
        check(high.frames==180 && high.wait_us<=high_old.wait_us,
              "high load still advances once per acquired slot without adding deadline waits");
        check(high.updates==high_old.updates && high.elapsed_us==high_old.elapsed_us &&
              high.stats.wake_late_ms==high_old.stats.wake_late_ms &&
              high.stats.wake_late_max_ms==high_old.stats.wake_late_max_ms &&
              high.stats.wake_late_3ms_count==high_old.stats.wake_late_3ms_count && !high.stats.extended_waits,
              "all high-load deadlines retain old cadence and lateness, including work25ms");
        print_simulation((std::string("work")+std::to_string(work/1000)+"ms_sleep10ms").c_str(),high_old,high);
    }
}
int main() {
    budget_checks();callback_checks();simulation_checks();
    std::cout<<"PASS FrameScheduleBudgetChecks "<<checks<<" bounded deadline, telemetry and 60Hz model checks\n";
}
