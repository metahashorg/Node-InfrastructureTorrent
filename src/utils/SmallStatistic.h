#ifndef SMALL_STATISTIC_H_
#define SMALL_STATISTIC_H_

#include "duration.h"

#include <mutex>

struct SmallStatisticElement {
    size_t stat = 0;
    time_point timer;
};

struct SmallStatistic {
    SmallStatisticElement prev;
    SmallStatisticElement current;
    
    mutable std::mutex mut;
    
    void addStatistic(size_t plus, const time_point &now, const milliseconds &updateTime) {
        std::lock_guard<std::mutex> lock(mut);
        if (current.timer == time_point()) {
            current.timer = now;
        } else if (now - current.timer >= updateTime) {
            prev = current;
            current = SmallStatisticElement();
            current.timer = now;
        }
        current.stat += plus;
    }
    
    SmallStatisticElement getStatistic() const {
        std::lock_guard<std::mutex> lock(mut);
        return prev;
    }
};

#endif // SMALL_STATISTIC_H_
