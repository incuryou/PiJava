#include "../src/pace.h"
#include "../src/frame_prep.h"
#include <cstdio>
#include <cstdlib>

static int fails = 0;
static void expect(bool ok, const char* what) {
    if (ok) return;
    std::fprintf(stderr, "FAIL %s\n", what);
    ++fails;
}

int main() {
    int64_t cut = 0, add = 0;
    expect(speed_frame_deadline(0, 0, 1000, 30, cut, add) == 0, "native cap leaves sleep");
    expect(speed_frame_deadline(33333, 0, 1000, 5, cut, add) == 0, "short sleep unchanged");
    expect(speed_frame_deadline(33333, 0, 1000, 200, cut, add) == 0, "long sleep unchanged");

    cut = add = 0;
    const int64_t ahead = speed_frame_deadline(33333, 0, 1000000, 20, cut, add);
    expect(ahead == 1000000 + 33333, "first slot is one period ahead");
    expect(add == 33333 - 20000 && cut == 0, "short sleep extends to the slot");

    cut = add = 0;
    const int64_t limited = speed_frame_deadline(33333, 5000000, 5000000 - 10000, 50, cut, add);
    expect(limited == 5000000, "future slot shortens a long sleep");
    expect(cut == 50000 - 10000 && add == 0, "cut is the removed part only");

    // Deadline already due: a 16ms poll must stay a real sleep, not a zero yield.
    cut = add = 0;
    expect(speed_frame_deadline(33333, 1000, 500000, 16, cut, add) == 0, "late slot keeps a short poll");
    expect(cut == 0 && add == 0, "kept poll is not accounted as a cut");

    cut = add = 0;
    const int64_t boosted = speed_frame_deadline(33333, 1000, 800000, 80, cut, add);
    expect(boosted == 800000 + 33333, "late slot clamps a long sleep to one period");
    expect(cut == 80000 - 33333 && add == 0, "long-sleep boost counts the cut");

    // 1ms late must aim at due+period, not now+period. Re-anchoring to now
    // turns that bias into a permanent 56fps at a 60 cap.
    cut = add = 0;
    const int64_t slipped = speed_frame_deadline(16667, 500000, 501000, 30, cut, add);
    expect(slipped == 500000 + 16667, "a 1ms late wake aims at the next grid point");
    expect(cut == 30000 - (16667 - 1000) && add == 0, "slip cut is only the unused part of the request");

    // Paint already spent 8ms of this 16.667ms slot. A late sleep(50) waits
    // the leftover, not another full period (that stack is what drops 60 to ~40).
    cut = add = 0;
    const int64_t painted = speed_frame_deadline(16667, 1000, 500000, 50, cut, add, 500000 - 8000);
    expect(painted == 500000 + (16667 - 8000), "late sleep after an 8ms paint keeps the remainder");
    expect(cut == 50000 - (16667 - 8000) && add == 0, "remainder cut drops only the unused part of the request");

    cut = add = 0;
    const int64_t overrun = speed_frame_deadline(16667, 1000, 500000, 50, cut, add, 500000 - 20000);
    expect(overrun == 500000 + 1000, "a paint that overran the slot does not stack another period");
    expect(cut == 50000 - 1000 && add == 0, "overrun floor is a 1ms sleep");

    expect(speed_next_paint_deadline(16667,1016667,1033000)==1049667,
           "late paint skips a near grid slot instead of a 334us catch-up");
    expect(speed_next_paint_deadline(16667,1016667,1017867)==1033334,
           "small late wake retains the grid phase");
    expect(speed_next_paint_deadline(33333,0,1000000)==1033333,
           "first paint owns one full period");
    cut=add=0;
    expect(speed_owned_frame_deadline(1000,500000,8,cut,add)==501000,
           "expired owned short sleep adds only a 1ms yield");
    expect(cut==7000 && add==0,"expired short sleep accounting");

    // Samples represent completed wake-to-draw cycles, not repeated drawing
    // calls or planned wake deadlines. Use fixed timestamps to keep this exact.
    FramePrepBudget prep;
    int sleep_site_a=0, sleep_site_b=0;
    auto sample=[&](int64_t elapsed) {
        prep.arm(7,16667,&sleep_site_a,12);
        prep.woke(1000000);
        prep.before_draw(7,16667,1000000+elapsed);
    };
    for(int i=0;i<3;++i)sample(2000+i*1000);
    expect(prep.lead_us(16667)==0,"three prep samples retain the full sleep deadline");
    sample(3000);
    expect(prep.count==4 && prep.lead_us(16667)==4500,"four prep samples use the recent peak plus 500us");
    const auto samples=prep.count;
    prep.before_draw(7,16667,1010000);
    expect(prep.count==samples && prep.lead_us(16667)==4500,"repeated drawing does not add a preparation sample");
    sample(12000);
    expect(prep.lead_us(16667)==12500,"prep peak includes alternating longer work");
    for(int i=0;i<7;++i)sample(2000);
    expect(prep.count==8 && prep.lead_us(16667)==12500,"rolling window retains a peak through seven replacement cycles");
    sample(2000);
    expect(prep.count==8 && prep.lead_us(16667)==2500,"rolling eight-cycle window ages out the old peak");
    expect(prep.wake_deadline(2000000,1900000,16667)==1997500,"preparation lead wakes before the drawing deadline");
    expect(prep.wake_deadline(2000000,1999500,16667)==2000500,"early preparation wake still leaves a 1ms cooperative sleep");
    sample(40000);
    expect(prep.lead_us(16667)==16667*3/4,"preparation lead is bounded to three quarters of the frame");
    prep.arm(7,16667,&sleep_site_a,13);
    expect(prep.count==0 && prep.lead_us(16667)==0,"changing sleep bytecode site restarts warmup");
    sample(2000);prep.arm(7,16667,&sleep_site_b,12);
    expect(prep.count==0,"changing sleep method restarts warmup");
    sample(2000);prep.arm(8,16667,&sleep_site_a,12);
    expect(prep.count==0,"changing Canvas restarts warmup");
    sample(2000);prep.before_draw(8,16667,1010000);
    expect(prep.count==0 && !prep.awaiting_draw,"drawing another Canvas clears pending preparation");
    sample(2000);prep.arm(7,33333,&sleep_site_a,12);
    expect(prep.count==0 && prep.lead_us(16667)==0,"changing frame period restarts warmup");
    sample(2000);prep.before_draw(7,33333,1010000);
    expect(prep.count==0,"drawing after a period change clears preparation");
    for(int i=0;i<4;++i)sample(999);
    expect(prep.lead_us(16667)==0,"sub-millisecond preparation earns no lead");
    sample(16667*4+1);
    expect(prep.count==0 && !prep.awaiting_draw,"a hitch beyond four periods discards stale preparation history");
    for(int i=0;i<4;++i)sample(1000);
    expect(prep.lead_us(16667)==1500,"one millisecond preparation meets the lead threshold");
    prep.arm(7,16667,&sleep_site_a,12);prep.woke(1000000);
    prep.before_draw(7,16667,999999);
    expect(prep.count==0,"a reversed preparation clock discards history");
    prep.arm(7,16667,&sleep_site_a,12);
    prep.before_draw(7,16667,1002000);
    expect(prep.count==0,"drawing before a runnable wake cannot create a sample");

    if (fails) {
        std::fprintf(stderr, "%d failed\n", fails);
        return 1;
    }
    std::puts("PASS speed pace");
    return 0;
}
