#ifndef BLOCKED_QUEUE_H_
#define BLOCKED_QUEUE_H_

#include <mutex>
#include <condition_variable>
#include <queue>

#include "stopProgram.h"

namespace common {

template <typename T, size_t MAX_SIZE = 1>
class BlockedQueue {
private:
    
    mutable std::mutex d_mutex;
    std::condition_variable cond_pop;
    std::condition_variable cond_push;
    std::queue<T> d_queue;
    bool isStopped = false;
    
public:
    
    void push(const T& value) {
        std::unique_lock<std::mutex> lock(d_mutex);
        conditionWait(cond_push, lock, [this]{ return d_queue.size() < MAX_SIZE || isStopped; });
        if (isStopped) {
            return;
        }
        d_queue.push(value);
        cond_pop.notify_one();
    }
    
    void push(T&& value) {
        std::unique_lock<std::mutex> lock(d_mutex);
        conditionWait(cond_push, lock, [this]{ return d_queue.size() < MAX_SIZE || isStopped; });
        if (isStopped) {
            return;
        }
        d_queue.push(std::forward<T>(value));
        cond_pop.notify_one();
    }
    
    bool pop(T &t) {
        std::unique_lock<std::mutex> lock(d_mutex);
        conditionWait(cond_pop, lock, [this]{ return !d_queue.empty() || isStopped; });
        if (isStopped) {
            return false;
        }
        t = std::move(d_queue.front());
        d_queue.pop();
        cond_push.notify_one();
        return true;
    }
    
    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(d_mutex);
        return d_queue.empty();
    }
    
    void stop() {
        std::lock_guard<std::mutex> lock(d_mutex);
        isStopped = true;
        cond_pop.notify_all();
        cond_push.notify_all();
    }
    
};

}

#endif // BLOCKED_QUEUE_H_
