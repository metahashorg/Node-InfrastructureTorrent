#ifndef BLOCKCHAIN_H_
#define BLOCKCHAIN_H_

#include "BlockInfo.h"

#include <unordered_map>
#include <shared_mutex>

#include "OopUtils.h"

#include "BlockChainReadInterface.h"

namespace torrent_node_lib {

class BlockChain : public BlockChainReadInterface {
public:
    
    BlockChain();
    
    bool addWithoutCalc(const BlockHeader &block);
    
    size_t calcBlockchain(const std::string &lastHash);
    
    size_t addBlock(const BlockHeader &block);
    
    BlockHeader getBlock(const std::string &hash) const override;
    
    BlockHeader getBlock(size_t blockNumber) const override;
    
    BlockHeader getLastBlock() const override;
    
    size_t countBlocks() const override;
    
    void clear();
    
private:
    
    void removeBlock(const BlockHeader &block);
    
    BlockHeader getBlockImpl(const std::string &hash) const;
    
    BlockHeader getBlockImpl(size_t blockNumber) const;
    
private:
    
    void initialize();
    
private:
    
    std::unordered_map<std::string, BlockHeader> blocks;
    std::vector<std::string> hashes;
    
    mutable std::shared_mutex mut;
};

}

#endif // BLOCKCHAIN_H_
