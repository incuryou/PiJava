#include "../src/auto_fps.h"
#include <cassert>
#include <iostream>
int main() {
    GameClock clock;clock.start(1000000,2000);
    assert(clock.value_us(1010000)==2010000);
    clock.rate(1010000,4);assert(clock.value_us(1010000)==2010000);
    assert(clock.value_us(1020000)==2050000);
    clock.pause(1020000,true);assert(clock.value_us(2020000)==2050000);
    clock.pause(2020000,false);assert(clock.value_us(2030000)==2090000);
    clock.start(0,INT64_MAX/1000,16);assert(clock.value_us(INT64_MAX)==INT64_MAX);
    clock.start(0,1,NAN);assert(clock.scale==1);
    AutoFpsLearner learner;int64_t time=1000000;double rate=1;
    learner.drawing(time,16667,rate);
    for(int cycle=0;cycle<5;cycle++) {
        for(int poll=0;poll<20;poll++){learner.poll(time+poll*2500);learner.cooperate();}
        time+=50000;double next=learner.drawing(time,16667,rate);if(next)rate=next;
    }
    assert(rate>2.99 && rate<3.01);
    // A new scene adds a helper drawing and cap waiting consumes the gate.
    time+=50000;assert(!learner.drawing(time,16667,rate));
    learner.poll(time);learner.poll(time+1);time+=50000;
    rate=learner.drawing(time,16667,rate);assert(rate>1.49 && rate<1.51);
    for(int cycle=0;cycle<10;cycle++) {
        time+=25000;assert(!learner.drawing(time,16667,rate));
        learner.poll(time);learner.poll(time+1);time+=25000;
        assert(!learner.drawing(time,16667,rate));
    }
    learner.reset();time=1000000;rate=1;learner.drawing(time,16667,rate);
    for(int cycle=0;cycle<5;cycle++) {
        for(int poll=0;poll<100;poll++)learner.poll(time+poll*700);
        time+=70000;double next=learner.drawing(time,16667,rate);if(next)rate=next;
    }
    assert(rate>4.19 && rate<4.21);
    // 90 ms of real scheduling delay is 378 ms on a 4.2x Java clock.
    // It is within the bridge's real 120 ms bound.
    time+=int64_t(90000*rate);assert(learner.bridge_sleep_gap(time,rate,true));
    learner.poll(time+1);learner.poll(time+2);learner.cooperate();
    time+=70000;assert(!learner.bridge_sleep_gap(time,rate,true));
    learner.drawing(time,16667,rate);
    // Mixed loop: an occasional sleep consumes the following poll. Preserve
    // the proven period without measuring that sleep as a longer gate.
    for(int cycle=0;cycle<40;cycle++) {
        time+=70000;
        assert(learner.bridge_sleep_gap(time,rate,true));
        assert(learner.candidate==70000 && learner.stable>=5);
        learner.poll(time+1);learner.poll(time+2);learner.cooperate();
        time+=70000;
        assert(!learner.bridge_sleep_gap(time,rate,true));
        learner.drawing(time,16667,rate);
    }
    time+=70000;assert(learner.bridge_sleep_gap(time,rate,true));
    time+=70000;assert(learner.bridge_sleep_gap(time,rate,true));
    time+=70000;assert(!learner.bridge_sleep_gap(time,rate,true));
    assert(!learner.bridge_sleep_gap(time+int64_t(121000*rate),rate,true));
    assert(!learner.bridge_sleep_gap(time,1,true));
    assert(!learner.bridge_sleep_gap(time,rate,false));
    learner.reset();time=1000000;rate=1;learner.drawing(time,16667,rate);
    for(int cycle=0;cycle<20;cycle++){time+=90000;assert(!learner.drawing(time,16667,rate));}
    // A verified threshold must not learn enforced cap waits or load/paint time.
    learner.reset();time=1000000;rate=1;learner.drawing(time,16667,rate);
    for(int cycle=0;cycle<5;cycle++) {
        time+=200000;learner.gate_period(50000);
        const double next=learner.drawing(time,16667,rate);
        if(cycle<4)assert(!next);else assert(next>2.99 && next<3.01);
    }
    // Exact gates target logical updates even when an update paints twice.
    for(int i=0;i<8;i++) {
        time+=50000;assert(!learner.drawing(time,16667,3));
        time+=50000;learner.gate_period(50000);
        assert(!learner.drawing(time,16667,3));
    }
    assert(learner.helper_drawing());
    learner.reset();learner.drawing(time,16667,1);
    for(int i=0;i<16;i++) {
        if(i%2){time+=20000;learner.drawing(time,16667,1);}
        time+=50000;learner.gate_period(50000);
        rate=learner.drawing(time,16667,1);
        if(i>=4)assert(rate>2.99 && rate<3.01);
    }
    const auto stable_before=learner.stable;
    assert(!learner.gate_period(50000,learner.gate_previous,nullptr,-1));
    assert(learner.stable==stable_before && learner.helper_drawing());
    assert(!learner.gate_period(7000,learner.gate_previous+1,nullptr,-1));
    learner.invalidate_gate();assert(!learner.helper_drawing() && !learner.started);
    learner.reset();learner.drawing(time,16667,3);
    for(int i=0;i<5;i++){time+=50000;learner.gate_period(16667);rate=learner.drawing(time,16667,3);}
    assert(rate==1);
    std::cout<<"PASS automatic FPS cycle learning and continuous clock\n";
}
