#ifndef BLOCK_SOURCE_H_
#define BLOCK_SOURCE_H_

#include <string>

namespace torrent_node_lib {

struct BlockInfo;
struct BlockHeader;

class BlockSource {
public:
    
    virtual void initialize() = 0;
    
    virtual std::pair<bool, size_t> doProcess(size_t countBlocks, const std::string &lastBlockHash) = 0;
    
    virtual size_t knownBlock() = 0;
    
    virtual bool process(BlockInfo &bi, std::string &binaryDump) = 0;
    
    virtual void getExistingBlock(const BlockHeader &bh, BlockInfo &bi, std::string &blockDump) const = 0;
    
    virtual ~BlockSource() = default;
    
};

}

#endif // BLOCK_SOURCE_H_
