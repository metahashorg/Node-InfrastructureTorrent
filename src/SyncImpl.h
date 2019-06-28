#ifndef SYNC_IMPL_H_
#define SYNC_IMPL_H_

#include <atomic>
#include <memory>

#include "Cache/Cache.h"
#include "LevelDb.h"
#include "BlockChain.h"

#include "TestP2PNodes.h"
#include "ConfigOptions.h"

namespace torrent_node_lib {

extern bool isInitialized;

class Statistics;
class WorkerCache;
class WorkerNodeTest;
class WorkerMain;
class BlockSource;
class PrivateKey;

struct V8Details;
struct V8Code;
struct NodeTestResult;
struct NodeTestTrust;
struct NodeTestCount;
struct NodeTestExtendedStat;

class SyncImpl {  
public:
    
    SyncImpl(const std::string &folderPath, const std::string &technicalAddress, const LevelDbOptions &leveldbOpt, const CachesOptions &cachesOpt, const GetterBlockOptions &getterBlocksOpt, const std::string &signKeyName, const TestNodesOptions &testNodesOpt);
       
    const BlockChainReadInterface & getBlockchain() const {
        return blockchain;
    }
    
    void setLeveldbOptScript(const LevelDbOptions &leveldbOpt);
    
    void setLeveldbOptNodeTest(const LevelDbOptions &leveldbOpt);
    
    ~SyncImpl();
    
private:
    
    void initialize(const std::string& folderPath, const GetterBlockOptions &getterBlocksOpt);
        
public:
    
    void synchronize(int countThreads);
    
    void addUsers(const std::set<Address> &addresses);
        
    std::string getBlockDump(const BlockHeader &bh, size_t fromByte, size_t toByte, bool isHex, bool isSign) const;
    
    size_t getKnownBlock() const;

    size_t getLastBlockDay() const;

    std::string signTestString(const std::string &str, bool isHex) const;
    
    bool verifyTechnicalAddressSign(const std::string &binary, const std::vector<unsigned char> &signature, const std::vector<unsigned char> &pubkey) const;

private:
   
    void saveTransactions(BlockInfo &bi, const std::string &binaryDump, bool saveBlockToFile);
    
    void filterTransactionsToSave(BlockInfo &bi);
    
    void saveBlockToLeveldb(const BlockInfo &bi);

private:
    
    LevelDb leveldb;
        
    LevelDbOptions leveldbOptScript;
    
    LevelDbOptions leveldbOptNodeTest;
    
    mutable AllCaches caches;
    
    BlockChain blockchain;
        
    const std::string technicalAddress;

    int countThreads;
    bool isSync;
        
    std::unique_ptr<BlockSource> getBlockAlgorithm;
    
    std::set<Address> users;
    mutable std::mutex usersMut;
    
    bool isSaveBlockToFiles;
    
    const bool isValidate;
    
    std::atomic<size_t> knownLastBlock = 0;
    
    std::unique_ptr<WorkerCache> cacheWorker;
    std::unique_ptr<WorkerNodeTest> nodeTestWorker;
    std::unique_ptr<WorkerMain> mainWorker;
    
    std::unique_ptr<PrivateKey> privateKey;
        
    TestP2PNodes testNodes;
    
};

}

#endif // SYNC_IMPL_H_
