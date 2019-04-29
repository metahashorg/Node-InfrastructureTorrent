#include "WorkerCache.h"

#include "check.h"
#include "log.h"

#include "Cache/Cache.h"

using namespace common;

namespace torrent_node_lib {
    
WorkerCache::WorkerCache(AllCaches &caches) 
    : caches(caches)
{}
    
void WorkerCache::work() {
    while (true) {
        try {
            std::pair<std::shared_ptr<BlockInfo>, std::shared_ptr<std::string>> element;
            
            const bool isStopped = !queue.pop(element);
            if (isStopped) {
                return;
            }
            BlockInfo &bi = *element.first;
            std::shared_ptr<std::string> blockDump = element.second;
            
            Timer tt;
            
            Timer tFirst;
            const std::string attribute = std::to_string(bi.header.blockNumber.value());
            
            if (caches.maxCountElementsBlockCache != 0) {
                caches.blockDumpCache.addValue(bi.header.hash, attribute, blockDump);
                caches.blockDumpCache.remove(std::to_string(bi.header.blockNumber.value() - caches.maxCountElementsBlockCache));
            }
            
            tFirst.stop();
            
            Timer tt2;
            if (caches.maxCountElementsTxsCache != 0) {
                for (const TransactionInfo &tx: bi.txs) {
                    if (tx.isIntStatusNodeTest()) {
                        continue;
                    }
                    
                    if (tx.isSaveToBd) {
                        caches.txsCache.addValue(tx.hash, attribute, tx);
                    }
                }
                tt2.stop();
                caches.txsCache.remove(std::to_string(bi.header.blockNumber.value() - caches.maxCountElementsTxsCache));
            }
            
            tt.stop();
            
            LOGINFO << "Block " << bi.header.blockNumber.value() << " saved to cache. Time: " << tFirst.countMs() << " " << tt.countMs() << " " << tt2.countMs();
            
            checkStopSignal();
        } catch (const exception &e) {
            LOGERR << e;
        } catch (const StopException &e) {
            LOGINFO << "Stop fillCacheWorker thread";
            return;
        } catch (const std::exception &e) {
            LOGERR << e.what();
        } catch (...) {
            LOGERR << "Unknown error";
        }
    }
}
    
void WorkerCache::start() {
    thread = Thread(&WorkerCache::work, this);
}
    
void WorkerCache::process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) {
    queue.push(std::make_pair(bi, dump));
}
    
std::optional<size_t> WorkerCache::getInitBlockNumber() const {
    return std::nullopt;
}

}
