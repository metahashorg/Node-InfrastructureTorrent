#ifndef WORKER_CACHE_H_
#define WORKER_CACHE_H_

#include "BlockedQueue.h"

#include "Worker.h"

#include "LevelDb.h"
#include "Modules.h"
#include "utils/Counter.h"
#include "Thread.h"

namespace torrent_node_lib {

struct BlockInfo;
struct AllCaches;

class WorkerCache : public Worker {
public:
    
    explicit WorkerCache(AllCaches &caches);
    
    void start() override;
    
    void process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) override;
    
    std::optional<size_t> getInitBlockNumber() const override;
       
    ~WorkerCache() override = default;
    
private:
    
    void work();
    
private:
    
    common::BlockedQueue<std::pair<std::shared_ptr<BlockInfo>, std::shared_ptr<std::string>>, 3> queue;
       
    AllCaches &caches;
    
    common::Thread thread;
};

}

#endif // WORKER_CACHE_H_
