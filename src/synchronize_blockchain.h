#ifndef SYNCHRONIZE_BLOCKCHAIN_H_
#define SYNCHRONIZE_BLOCKCHAIN_H_

#include "OopUtils.h"

#include <string>
#include <vector>
#include <memory>
#include <set>
#include <unordered_map>

#include "ConfigOptions.h"

namespace torrent_node_lib {

class BlockChainReadInterface;
class Address;
struct TransactionInfo;
struct BlockHeader;
struct BlockInfo;
struct BalanceInfo;
struct DelegateState;
struct V8Details;
struct CommonBalance;
struct V8Code;
struct ForgingSums;
struct NodeTestResult;
struct NodeTestTrust;
struct NodeTestCount;
struct NodeTestExtendedStat;

class P2P;

class SyncImpl;

class Sync: public common::no_copyable, common::no_moveable {   
public:
    
    Sync(const std::string &folderPath, const std::string &technicalAddress, const LevelDbOptions &leveldbOpt, const CachesOptions &cachesOpt, const GetterBlockOptions &getterBlocksOpt, const std::string &signKeyName, const TestNodesOptions &testNodesOpt);
       
    void setLeveldbOptScript(const LevelDbOptions &leveldbOptScript);
    
    void setLeveldbOptNodeTest(const LevelDbOptions &leveldbOpt);
    
    const BlockChainReadInterface & getBlockchain() const;
    
    bool verifyTechnicalAddressSign(const std::string &binary, const std::vector<unsigned char> &signature, const std::vector<unsigned char> &pubkey) const;

    ~Sync();
    
public:
    
    bool isVirtualMachine() const;
    
    void synchronize(int countThreads);

    void addUsers(const std::set<Address> &addresses);
    
    std::string getBlockDump(const BlockHeader &bh, size_t fromByte, size_t toByte, bool isHex, bool isSign) const;

    std::vector<TransactionInfo> getLastTxs() const;

    size_t getKnownBlock() const;

    std::string signTestString(const std::string &str, bool isHex) const;
    
private:
    
    std::unique_ptr<SyncImpl> impl;
    
};

void initBlockchainUtils();

}

#endif // SYNCHRONIZE_BLOCKCHAIN_H_
