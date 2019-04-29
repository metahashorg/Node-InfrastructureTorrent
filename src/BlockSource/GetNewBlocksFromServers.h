#ifndef GET_NEW_BLOCKS_FROM_SERVER_H_
#define GET_NEW_BLOCKS_FROM_SERVER_H_

#include "P2P/P2P.h"

namespace torrent_node_lib {

class GetNewBlocksFromServer {
public:
    
    struct LastBlockResponse {
        std::vector<std::string> servers;
        size_t lastBlock;
        std::optional<std::string> error;
    };
    
    struct BlockHeader {
        size_t blockSize;
        std::string hash;
        std::string parentHash;
        std::string fileName;
    };
    
public:
    
    static std::pair<std::string, std::string> makeRequestForDumpBlock(const std::string &blockHash, size_t fromByte, size_t toByte);
    
    static std::pair<std::string, std::string> makeRequestForDumpBlockSign(const std::string &blockHash);
    
    static ResponseParse parseDumpBlockResponse(const std::string &result);
    
public:
    
    GetNewBlocksFromServer(size_t maxAdvancedLoadBlocks, const P2P &p2p)
        : maxAdvancedLoadBlocks(maxAdvancedLoadBlocks)
        , p2p(p2p)
    {}
        
    LastBlockResponse getLastBlock() const;
        
    BlockHeader getBlockHeader(size_t blockNum, size_t maxBlockNum, const std::string &server) const;
    
    BlockHeader getBlockHeaderWithoutAdvanceLoad(size_t blockNum, const std::string &server) const;
    
    std::string getBlockDump(const std::string &blockHash, size_t blockSize, const std::vector<std::string> &hintsServers, bool isSign) const;
    
    std::string getBlockDumpWithoutAdvancedLoad(const std::string &blockHash, size_t blockSize, const std::vector<std::string> &hintsServers, bool isSign) const;
    
    void clearAdvanced();
    
private:
    
    const size_t maxAdvancedLoadBlocks;
    
    const P2P &p2p;
    
    mutable std::vector<std::pair<size_t, BlockHeader>> advancedLoadsBlocksHeaders;
    
    mutable std::unordered_map<std::string, std::string> advancedLoadsBlocksDumps;
    
};

}

#endif // GET_NEW_BLOCKS_FROM_SERVER_H_
