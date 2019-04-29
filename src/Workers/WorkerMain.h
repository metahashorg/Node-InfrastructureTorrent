#ifndef WORKER_MAIN_H_
#define WORKER_MAIN_H_

#include <memory>
#include <stack>
#include <set>
#include <unordered_map>

#include "Address.h"
#include "utils/Counter.h"
#include "BlockedQueue.h"
#include "Thread.h"

#include "Worker.h"

namespace torrent_node_lib {

struct AllCaches;
class LevelDb;
class Batch;
class BlockChain;
struct TransactionInfo;
struct BalanceInfo;
struct BlockHeader;
struct TransactionStatus;

class WorkerMain: public Worker {  
public:
    
    WorkerMain(LevelDb &leveldb, AllCaches &caches, BlockChain &blockchain, const std::set<Address> &users, std::mutex &usersMut, int countThreads);
       
    ~WorkerMain() override;
    
private:
    
    using DelegateTransactionsCache = std::unordered_map<std::string, std::stack<std::vector<char>>>;
        
public:
    
    void start() override;
    
    void process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) override;
    
    std::optional<size_t> getInitBlockNumber() const override;
    
public:
       
    BlockInfo getFullBlock(const BlockHeader &bh, size_t beginTx, size_t countTx) const;

private:
    
    void worker();
        
private:
    
    void processLocalCache();
           
    [[nodiscard]] bool checkAddressToSave(const TransactionInfo &tx, const Address &address) const;
       
    void readTransactionInFile(TransactionInfo &filePos) const;
    
private:
    
    size_t lastSavedBlock;
    
    LevelDb &leveldb;
    
    AllCaches &caches;
    
    BlockChain &blockchain;
    
    common::Thread workerThread;
    
    const int countThreads;
    
    common::BlockedQueue<std::shared_ptr<BlockInfo>, 1> queue;
    
    Counter<false> countVal;
    
    const std::set<Address> &users;
    std::mutex &usersMut;
    
};

}

#endif // WORKER_MAIN_H_
