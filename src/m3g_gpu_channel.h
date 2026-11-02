#pragma once
#include "m3g_gpu_types.h"
#include <condition_variable>
#include <memory>
#include <mutex>
namespace m3g_gpu {
// One result per request, independent of the renderer's coalescing telemetry.
template<class Result> class ResultState {
    std::mutex mutex_;std::condition_variable wake_;
    bool done_=false,consumed_=false;Result value_;
public:
    bool complete(Result&& value){
        std::lock_guard<std::mutex> lock(mutex_);if(done_)return false;
        value_=std::move(value);done_=true;wake_.notify_all();return true;
    }
    Result wait(){
        std::unique_lock<std::mutex> lock(mutex_);wake_.wait(lock,[&]{return done_;});
        if(consumed_){Result error;error.code=Code::Failed;error.reason="result_already_consumed";return error;}
        consumed_=true;return std::move(value_);
    }
};
template<class Request,class Result> struct ResolveJob {
    Request request;
    std::shared_ptr<ResultState<Result>> result;
    ResolveJob(Request&& input,std::shared_ptr<ResultState<Result>> output):request(std::move(input)),result(std::move(output)){}
    ResolveJob(const ResolveJob&)=delete;ResolveJob& operator=(const ResolveJob&)=delete;
    ~ResolveJob(){if(result){Result canceled;canceled.code=Code::Cancelled;canceled.reason="owner_retired_without_result";result->complete(std::move(canceled));}}
};
using GeometryJob=ResolveJob<GeometryRequest,GeometryResult>;
using SkinJob=ResolveJob<SkinRequest,SkinResult>;
}
