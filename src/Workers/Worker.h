#ifndef WORKER_H_
#define WORKER_H_

#include <memory>
#include <optional>

#include "OopUtils.h"

namespace torrent_node_lib {

struct BlockInfo;
    
class Worker: public common::no_copyable, common::no_moveable{
public:
    
    virtual void start() = 0;
    
    virtual void process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) = 0;
    
    virtual std::optional<size_t> getInitBlockNumber() const = 0;
    
    virtual ~Worker() = default;
    
};
    
}

#endif // WORKER_H_
