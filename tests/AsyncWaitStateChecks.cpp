#include <cstddef>
#include "../src/wait_frame.h"
#include <cassert>
#include <iostream>
int main() {
    FrameWaitState proof;
    proof.request=17;proof.active=proof.complete=proof.requested=proof.awaiting_paint=true;
    proof.reset();
    assert(!proof.active&&!proof.complete&&!proof.requested&&!proof.awaiting_paint);
    // A paint callback may reset modes and enqueue another request before the
    // old callback returns. Its token must not become the new request's token.
    ++proof.request;
    assert(proof.request!=17&&proof.request>17);
    std::cout<<"PASS async request identity survives reset\n";
}
