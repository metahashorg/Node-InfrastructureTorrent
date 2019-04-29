#ifndef COUNTER_H_
#define COUNTER_H_

#include <atomic>
#include <stddef.h>

namespace torrent_node_lib {
    
template<bool inc>
class Counter {
public:
    
    void store(size_t value) {
        counter.store(value + (inc ? +1 : -1));
    }
    
    size_t load() const {
        return counter.load();
    }
    
    size_t get() {
        if constexpr (inc) {
            return counter++;
        } else {
            return counter--;
        }
    }
    
private:
    
    std::atomic<size_t> counter;
};

}

#endif // COUNTER_H_
