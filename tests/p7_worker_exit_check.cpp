#include "../src/p7_worker.h"
#include <iostream>
int main(){ std::cout<<p7::configure(true,1)<<"\n"; return 0; } // no explicit shutdown: must exit promptly
