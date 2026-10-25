#include "../src/p7_worker.h"
#include <cassert>
#include <iostream>
int main(){ p7::g_count_on=true;
  std::cout<<p7::configure(true,1)<<"\n";
  auto& p=p7::pool(); std::atomic<int> sum{0};
  for(int r=0;r<200;++r){ std::vector<uint64_t> t; for(int i=0;i<16;++i) t.push_back(p.submit([&]{sum.fetch_add(1);})); p.wait(t[3]); p.drain(); }
  assert(sum==3200);
  // epoch clear while tasks queued
  for(int i=0;i<64;++i) p.submit([&]{ sum.fetch_add(1);});
  size_t d=p.epoch_clear(2); p.drain(); std::cout<<"dropped="<<d<<" sum="<<sum<<"\n"; assert(sum+d==3264);
  // exception propagation

  { P7_SCOPE(Interp); { P7_SCOPE(Gc); } P7_SCOPE(Decode); }
  std::cout<<p7::counters_summary("test")<<"\n";
  p.shutdown(); assert(!p.enabled());
  std::cout<<"PASS\n";
}
