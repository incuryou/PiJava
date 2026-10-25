#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Only System.currentTimeMillis uses this clock. Host scheduling and audio
// always use their existing real clocks. Reanchor before changing the rate so
// Java timestamps, including timestamps kept in fields, remain continuous.
struct GameClock {
    int64_t real_us = 0, game_us = 0;
    double scale = 1.0;
    bool initialized = false, paused = false;
    int64_t value_us(int64_t now) const {
        if (!initialized) return 0;
        const long double value=game_us + (paused ? 0.0L : std::max(0.0L,static_cast<long double>(now)-real_us)*scale);
        return value>=INT64_MAX?INT64_MAX:static_cast<int64_t>(value);
    }
    void start(int64_t now, int64_t millis, double rate=1.0) {
        real_us=now; game_us=std::max<int64_t>(0,std::min<int64_t>(INT64_MAX/1000,millis))*1000; initialized=true; paused=false;
        scale=std::isfinite(rate)?std::max(1.0,std::min(16.0,rate)):1.0;
    }
    void rate(int64_t now, double next) {
        game_us=value_us(now); real_us=now;
        scale=std::isfinite(next)?std::max(1.0,std::min(16.0,next)):1.0;
    }
    void pause(int64_t now, bool next) {
        game_us=value_us(now); real_us=now; paused=next;
    }
};

// A gated update may synchronously draw more than once. Accumulate all its
// owned drawings before measuring the next clock-polling cycle, rather than
// interpreting a helper repaint as a new update rate.
struct AutoFpsLearner {
    int64_t cycle_game_us=0, first_poll_us=0, last_poll_us=0;
    unsigned draws=0, polls=0, yields=0, stable=0;
    unsigned sleep_gaps=0;
    double candidate=0;
    int64_t verified_period_us=0;
    const void* gate_method=nullptr;
    int gate_pc=-1;
    int64_t gate_previous=0;
    bool gate_key_set=false, group_open=false, group_drawn=false, group_complete=false;
    bool started=false, verified_gate_seen=false;
    void reset() { *this={}; }
    void poll(int64_t game_us) {
        if(started && polls<1000000) {
            if(!polls)first_poll_us=game_us;
            last_poll_us=game_us;++polls;
        }
    }
    void cooperate() { if(started && yields<1000000)++yields; }
    // A certified threshold counts logical updates, independently of helper
    // paintings. The same ready read/anchor cannot open an additional group.
    bool gate_period(int64_t requested_us,int64_t previous,const void* method,int pc) {
        if(!started || requested_us<8000 || requested_us>120000)return false;
        if(gate_key_set && gate_method==method && gate_pc==pc && gate_previous==previous)return false;
        if(!verified_gate_seen || gate_method!=method || gate_pc!=pc) {candidate=0;stable=0;}
        gate_method=method;gate_pc=pc;gate_previous=previous;gate_key_set=true;
        verified_period_us=requested_us;verified_gate_seen=true;
        group_open=true;group_drawn=group_complete=false;
        return true;
    }
    void gate_period(int64_t requested_us) {
        gate_period(requested_us,gate_previous+1,nullptr,-1);
    }
    bool grouped_update() const {return verified_gate_seen && stable>=5 && group_open;}
    bool helper_drawing() const {return grouped_update() && group_drawn;}
    void invalidate_gate() {if(verified_gate_seen)reset();}
    bool polling_cycle(int64_t game_us,double active_scale) const {
        if(verified_period_us>0)return true;
        const int64_t duration=game_us-cycle_game_us;
        const bool busy=polls>=32 && last_poll_us-first_poll_us>=duration/2;
        return (polls>=2 && (yields>=1 || (stable>=5 && active_scale>1.01))) || busy;
    }
    // A short sleep in an established mixed loop can consume the next poll
    // completely. Keep its learned rate for at most two such drawings. Do not
    // feed the sleep duration back as a newly measured gate period. A scene
    // that becomes sleep-only still drops the old rate on the third drawing.
    bool bridge_sleep_gap(int64_t game_us,double active_scale,bool was_mixed) {
        if(polling_cycle(game_us,active_scale)) {sleep_gaps=0;return false;}
        if(!was_mixed || verified_gate_seen || stable<5 || active_scale<=1.01 ||
           candidate<8000 || candidate>120000 || sleep_gaps>=2 ||
           double(game_us-cycle_game_us)>120000.0*active_scale || game_us<cycle_game_us)return false;
        ++sleep_gaps;cycle_game_us=game_us;draws=polls=yields=0;
        return true;
    }
    double target(int period_us,unsigned cycle_draws,double active_scale) const {
        if(stable<5 || candidate<=0 || !cycle_draws || period_us<=0)return 0;
        const double divisor=verified_gate_seen?1.0:double(cycle_draws);
        const double rate=std::max(1.0,std::min(8.0,candidate/(divisor*period_us)));
        return std::fabs(rate-active_scale)>std::max(0.03,active_scale*0.01)?rate:0;
    }
    double drawing(int64_t game_us,int period_us,double active_scale) {
        if(!started) {started=true;cycle_game_us=game_us;return 0;}
        ++draws;
        // No repeated polling means this can be another drawing in the same
        // Java update. Bound the group; loading and unpaced animation do not
        // qualify simply because they draw occasionally.
        const int64_t duration=game_us-cycle_game_us;
        if(verified_period_us) {
            const double next=verified_period_us;
            verified_period_us=0;
            cycle_game_us=game_us;draws=0;polls=yields=0;
            if(candidate>0 && std::fabs(next-candidate)<=500.0) {
                candidate=(candidate*stable+next)/(stable+1);if(stable<1000000)++stable;
            } else {candidate=next;stable=1;}
            group_drawn=true;
            return target(period_us,1,active_scale);
        }
        // Helper drawings still execute Java. They provide neither a new
        // logical update nor a host elapsed-time sample for the learned rate.
        if(verified_gate_seen) {polls=yields=0;return 0;}
        const bool busy_gate=polls>=32 && last_poll_us-first_poll_us>=duration/2;
        const bool gated=polling_cycle(game_us,active_scale);
        if(!gated) {
            polls=yields=0;
            if(draws>=8 || game_us-cycle_game_us>960000)reset();
            return 0;
        }
        const unsigned cycle_draws=draws;
        sleep_gaps=0;
        const double per_draw=double(duration)/draws;
        cycle_game_us=game_us;draws=0;
        const bool evidence=polls>=8 || yields>=2 || busy_gate;
        polls=yields=0;
        // Once pacing has consumed the gate's spare time, few polls remain.
        // That is success, not evidence that the learned clock must decay.
        if(!evidence && active_scale>1.01) {
            // A load gap is not a frame. Do not turn a decode stall into a 7x clock.
            if(candidate<=0 || candidate>120000.0*cycle_draws) return 0;
            return target(period_us,cycle_draws,active_scale);
        }
        if(per_draw<8000 || per_draw>120000 || !evidence) {stable=0;candidate=0;return 0;}
        const double next=duration;
        if(candidate>0 && std::fabs(next-candidate)<=std::max(1500.0,candidate*0.20)) {
            candidate=(candidate*stable+next)/(stable+1);++stable;
        } else {candidate=next;stable=1;}
        return target(period_us,cycle_draws,active_scale);
    }
};
