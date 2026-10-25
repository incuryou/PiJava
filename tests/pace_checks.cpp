#include "../src/pace.h"
#include <cstdlib>
#include <iostream>
int main() {
    int n = 0;
    auto check = [&](bool ok) { if (!ok) { std::cerr << "FAIL\n"; std::exit(1); } ++n; };
    check(pace_align_sleep(10, true, 100, 90));
    check(pace_align_sleep(8, true, 20, 19));
    check(pace_align_sleep(20, true, 50, 30));
    check(!pace_align_sleep(7, true, 100, 90));
    check(!pace_align_sleep(21, true, 100, 90));
    check(!pace_align_sleep(50, true, 100, 90));
    check(!pace_align_sleep(10, false, 100, 90));
    check(!pace_align_sleep(10, true, 90, 90));
    check(!pace_align_sleep(10, true, 80, 90));
    check(!pace_align_sleep(0, true, 100, 90));
    std::cout << "PASS PaceChecks " << n << "\n";
}
