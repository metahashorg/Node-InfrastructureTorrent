#include "NetworkBlockSource.h"

#include "utils/FileSystem.h"
#include "GetNewBlocksFromServers.h"
#include "log.h"
#include "check.h"
#include "PrivateKey.h"
#include "BlockchainRead.h"
#include "BlockInfo.h"
#include "parallel_for.h"

using namespace common;

namespace torrent_node_lib {

const static size_t COUNT_ADVANCED_BLOCKS = 8;
    
NetworkBlockSource::NetworkBlockSource(const std::string &folderPath, size_t maxAdvancedLoadBlocks, size_t countBlocksInBatch, P2P &p2p, bool saveAllTx, bool isValidate, bool isVerifySign) 
    : getterBlocks(maxAdvancedLoadBlocks, countBlocksInBatch, p2p)
    , folderPath(folderPath)
    , saveAllTx(saveAllTx)
    , isValidate(isValidate)
    , isVerifySign(isVerifySign)
{}


void NetworkBlockSource::initialize() {
    createDirectories(folderPath);
}

std::pair<bool, size_t> NetworkBlockSource::doProcess(size_t countBlocks, const std::string &lastBlockHash) {
    nextBlockToRead = countBlocks + 1;
    const GetNewBlocksFromServer::LastBlockResponse lastBlock = getterBlocks.getLastBlock();
    CHECK(!lastBlock.error.has_value(), lastBlock.error.value());
    lastBlockInBlockchain = lastBlock.lastBlock;
    servers = lastBlock.servers;

    advancedBlocks.clear();
    getterBlocks.clearAdvanced();
    
    return std::make_pair(lastBlockInBlockchain >= nextBlockToRead, lastBlockInBlockchain);
}

size_t NetworkBlockSource::knownBlock() {
    return lastBlockInBlockchain;
}

bool NetworkBlockSource::process(BlockInfo &bi, std::string &binaryDump) {
    const bool isContinue = lastBlockInBlockchain >= nextBlockToRead;
    if (!isContinue) {
        return false;
    }
    
    const auto processAdvanced = [&bi, &binaryDump, this](const AdvancedBlock &advanced) {
        if (advanced.exception) {
            std::rethrow_exception(advanced.exception);
        }
        bi = advanced.bi;
        binaryDump = advanced.dump;
        
        nextBlockToRead++;
    };
    
    const auto foundBlock = advancedBlocks.find(nextBlockToRead);
    if (foundBlock != advancedBlocks.end()) {
        processAdvanced(foundBlock->second);
        return true;
    }
    
    advancedBlocks.clear();
    
    const size_t countAdvanced = std::min(COUNT_ADVANCED_BLOCKS, lastBlockInBlockchain - nextBlockToRead + 1);
    
    //LOGDEBUG << "Get block " << nextBlockToRead;
    CHECK(!servers.empty(), "Servers empty");
    for (size_t i = 0; i < countAdvanced; i++) {
        const size_t currBlock = nextBlockToRead + i;
        AdvancedBlock advanced;
        try {
            advanced.header = getterBlocks.getBlockHeader(currBlock, lastBlockInBlockchain, servers[0]);
            advanced.dump = getterBlocks.getBlockDump(advanced.header.hash, advanced.header.blockSize, servers, isVerifySign);
        } catch (...) {
            advanced.exception = std::current_exception();
        }
        advancedBlocks[currBlock] = advanced;
    }
    parallelFor(countAdvanced, advancedBlocks.begin(), advancedBlocks.end(), [this](auto &pair) {
        if (pair.second.exception) {
            return;
        }
        try {
            if (isVerifySign) {
                const BlockSignatureCheckResult signBlock = checkSignatureBlock(pair.second.dump);
                pair.second.dump = signBlock.block;
                pair.second.bi.header.senderSign.assign(signBlock.sign.begin(), signBlock.sign.end());
                pair.second.bi.header.senderPubkey.assign(signBlock.pubkey.begin(), signBlock.pubkey.end());
                pair.second.bi.header.senderAddress.assign(signBlock.address.begin(), signBlock.address.end());
            }
            CHECK(pair.second.dump.size() == pair.second.header.blockSize, "binaryDump.size() == nextBlockHeader.blockSize");
            pair.second.bi.header.filePos.fileName = getFullPath(getBasename(pair.second.header.fileName), folderPath);
            readNextBlockInfo(pair.second.dump.data(), pair.second.dump.data() + pair.second.dump.size(), 0, pair.second.bi, isValidate, saveAllTx, 0, 0);
        } catch (...) {
            pair.second.exception = std::current_exception();
        }
    });
    
    CHECK(advancedBlocks.find(nextBlockToRead) != advancedBlocks.end(), "Incorrect results");
    processAdvanced(advancedBlocks[nextBlockToRead]);
    return true;
}

void NetworkBlockSource::getExistingBlock(const BlockHeader& bh, BlockInfo& bi, std::string &blockDump) const {
    CHECK(bh.blockNumber.has_value(), "Block number not set");
    const GetNewBlocksFromServer::LastBlockResponse lastBlock = getterBlocks.getLastBlock();
    CHECK(!lastBlock.error.has_value(), lastBlock.error.value());
    const MinimumBlockHeader nextBlockHeader = getterBlocks.getBlockHeaderWithoutAdvanceLoad(bh.blockNumber.value(), lastBlock.servers[0]);
    blockDump = getterBlocks.getBlockDumpWithoutAdvancedLoad(nextBlockHeader.hash, nextBlockHeader.blockSize, lastBlock.servers, isVerifySign);
    if (isVerifySign) {
        const BlockSignatureCheckResult signBlock = checkSignatureBlock(blockDump);
        blockDump = signBlock.block;
        bi.header.senderSign.assign(signBlock.sign.begin(), signBlock.sign.end());
        bi.header.senderPubkey.assign(signBlock.pubkey.begin(), signBlock.pubkey.end());
        bi.header.senderAddress.assign(signBlock.address.begin(), signBlock.address.end());
    }
    CHECK(blockDump.size() == nextBlockHeader.blockSize, "binaryDump.size() == nextBlockHeader.blockSize");
    readNextBlockInfo(blockDump.data(), blockDump.data() + blockDump.size(), bh.filePos.pos, bi, isValidate, saveAllTx, 0, 0);
    bi.header.filePos.fileName = bh.filePos.fileName;
    for (auto &tx : bi.txs) {
        tx.filePos.fileName = bh.filePos.fileName;
    }
    bi.header.blockNumber = bh.blockNumber;
}

}
