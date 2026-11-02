#pragma once
#include "gpu_filter.h"
namespace j2me_present {
inline bool request_gpu_backend(display_filter::Filter filter,bool owner_requested,bool p1_cpu) {
    return !p1_cpu&&display_filter::valid(filter)&&
        (gpu_filter::supported(filter)||owner_requested);
}
}
