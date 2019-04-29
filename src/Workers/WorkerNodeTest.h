#ifndef WORKER_NODE_TEST_H_
#define WORKER_NODE_TEST_H_

#include "BlockedQueue.h"

#include "Worker.h"

#include "LevelDb.h"
#include "Thread.h"

#include "ConfigOptions.h"

#include <map>

namespace torrent_node_lib {

struct BlockInfo;
struct AllCaches;

struct NodeTestResult;
struct NodeTestCount;
struct NodeTestTrust;
struct NodeTestExtendedStat;

class BlockChain;

class WorkerNodeTest : public Worker {   
public:
    
    explicit WorkerNodeTest(const BlockChain &blockchain, const LevelDbOptions &leveldbOptNodeTest);
    
    void start() override;
    
    void process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) override;
    
    std::optional<size_t> getInitBlockNumber() const override;
        
    ~WorkerNodeTest() override = default;

public:
        
    std::map<std::string, std::string> getAllNodes() const;
    
private:
    
    void work();
    
private:
    
    const BlockChain &blockchain;
    
    size_t initializeScriptBlockNumber = 0;
    
    common::BlockedQueue<std::shared_ptr<BlockInfo>, 1> queue;
    
    LevelDb leveldbNodeTest;
        
    common::Thread thread;
    
};

}

#endif // WORKER_NODE_TEST_H_
