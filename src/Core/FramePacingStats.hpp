#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <mutex>
namespace flappy12 {
struct PacingSnapshot {uint64_t frames;double p99Milliseconds;};
class FramePacingStats {
    std::mutex mutex;
    std::array<double,256> intervals{};
    size_t size=0,cursor=0;
    uint64_t frames=0;
    double last=0;
public:
    void reset(){std::lock_guard<std::mutex> lock(mutex);size=cursor=0;frames=0;last=0;}
    void presented(double time) {
        if(time<=0)return;
        std::lock_guard<std::mutex> lock(mutex);
        if(time<=last)return;
        if(last>0&&time-last<.25){intervals[cursor]=(time-last)*1000;cursor=(cursor+1)%intervals.size();size=std::min(size+1,intervals.size());}
        last=time;++frames;
    }
    PacingSnapshot take() {
        std::array<double,256> copy;size_t count;uint64_t captured;
        {std::lock_guard<std::mutex> lock(mutex);copy=intervals;count=size;captured=frames;frames=0;}
        std::sort(copy.begin(),copy.begin()+count);
        return {captured,count?copy[(count-1)*99/100]:0};
    }
};
}
