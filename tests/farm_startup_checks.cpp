#include "../src/farm_startup.h"
#include <cstdlib>
#include <iostream>
static void check(bool ok, const char* label) {
    if (!ok) { std::cerr << "FAIL " << label << '\n'; std::exit(1); }
}
int main() {
    const char* farm = "251e9f36de9bac05c4fa63003b107cdbc43f5f4ae3bf6b5815e3c09af26d8e66";
    FarmStartup policy;
    policy.start("other-jar", 0);
    check(!policy.waiting() && !policy.enqueue(53,false), "unselected titles unaffected");
    policy.start(farm,100);
    check(policy.waiting() && policy.enqueue(53,false) && policy.enqueue(53,true), "startup queues press and release");
    check(!policy.update(200,false) && policy.waiting(), "readiness drives startup boundary");
    check(policy.update(201,true) && !policy.waiting() && policy.keys.size()==2,
          "resource readiness releases pending input");
    check(policy.keys[0].code==53 && !policy.keys[0].release && policy.keys[1].release, "input ordering preserved");
    check(!policy.update(40000,false) && policy.state==FarmStartup::State::Ready, "readiness permanently ends special handling");
    policy.start(farm,100);
    policy.enqueue(53,false); policy.enqueue(53,true);
    policy.pause(1100);
    check(policy.waiting() && policy.paused() && policy.keys.empty(), "pause cancels pending startup keys");
    check(!policy.update(61100,true) && policy.waiting(), "60 second pause cannot finish or expire startup");
    policy.pause(61100); // Repeated pause must retain its original timestamp.
    policy.resume(61100);
    check(!policy.paused() && policy.deadline==90100, "pause freezes remaining 29 second deadline");
    policy.resume(70000);
    check(policy.deadline==90100 && !policy.update(90099,false) && policy.waiting(), "resume is idempotent and deadline remains bounded");
    check(!policy.update(90100,false) && policy.state==FarmStartup::State::Failed, "timeout occurs after resumed active time");
    policy.start(farm,100);
    check(!policy.update(30100,false) && policy.state==FarmStartup::State::Failed && !policy.enqueue(53,false),
          "timeout exposes ordinary callbacks instead of hiding errors");
    policy.start(farm,0);
    for(int i=0;i<64;++i) check(policy.enqueue(53,i%2),"bounded queue capacity");
    check(!policy.enqueue(53,false) && policy.state==FarmStartup::State::Failed && policy.keys.empty(),"overflow fails policy");
    policy.start("other-jar",1);
    check(policy.keys.empty() && policy.state==FarmStartup::State::Off,"relaunch resets policy");
    check(FarmStartup::selected("bff8e05c4b2d5fbb4e3d04b12b4e8ef424751be75e28e5b230b6ed8a91c4fc80"),"observed Switch JAR identity selected");
    std::cout << "PASS FarmStartupChecks exact identity, readiness, ordered input, reset, timeout, pause and overflow\n";
}
