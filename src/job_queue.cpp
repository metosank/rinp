#include "job_queue.h"

bool JobQueue::push(InputJob job) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) return false;
        jobs_.push(std::move(job));
    }

    cv_.notify_one();
    return true;
}

bool JobQueue::waitPop(InputJob& job) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
        return stopped_ || !jobs_.empty();
    });

    if (jobs_.empty()) return false;

    job = std::move(jobs_.front());
    jobs_.pop();
    return true;
}

void JobQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::queue<InputJob> empty;
    jobs_.swap(empty);
}

void JobQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        std::queue<InputJob> empty;
        jobs_.swap(empty);
    }

    cv_.notify_all();
}
