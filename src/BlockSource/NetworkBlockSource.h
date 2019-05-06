#ifndef NETWORK_BLOCK_SOURCE_H_
#define NETWORK_BLOCK_SOURCE_H_

#include "BlockSource.h"

#include "OopUtils.h"

#include "BlockInfo.h"

#include "GetNewBlocksFromServers.h"

#include <string>
#include <map>
#include <vector>

namespace torrent_node_lib {
    
class P2P;
   
class NetworkBlockSource: public BlockSource, common::no_copyable, common::no_moveable {
public:
    
    NetworkBlockSource(const std::string &folderPath, size_t maxAdvancedLoadBlocks, size_t countBlocksInBatch, P2P &p2p, bool saveAllTx, bool isValidate, bool isVerifySign);
    
    void initialize() override;
    
    std::pair<bool, size_t> doProcess(size_t countBlocks, const std::string &lastBlockHash) override;
    
    size_t knownBlock() override;
    
    bool process(BlockInfo &bi, std::string &binaryDump) override;
    
    void getExistingBlock(const BlockHeader &bh, BlockInfo &bi, std::string &blockDump) const override;
    
    ~NetworkBlockSource() override = default;
    
private:
    
    struct AdvancedBlock {
        MinimumBlockHeader header;
        BlockInfo bi;
        std::string dump;
        std::exception_ptr exception;
    };
    
private:
    
    GetNewBlocksFromServer getterBlocks;
    
    const std::string folderPath;
    
    size_t nextBlockToRead = 0;
    
    std::vector<std::string> servers;
    
    size_t lastBlockInBlockchain = 0;
    
    bool saveAllTx;
    
    const bool isValidate;
    
    const bool isVerifySign;
  
    std::map<size_t, AdvancedBlock> advancedBlocks;
    
};

}

#endif // NETWORK_BLOCK_SOURCE_H_
