#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <utility>

struct InputJob {
    std::wstring text;
    uint16_t delayMs = 0;
};

class JobQueue {
public:
    bool push(InputJob job);
    bool waitPop(InputJob& job);
    void clear();
    void stop();

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<InputJob> jobs_;
    bool stopped_ = false;
};
