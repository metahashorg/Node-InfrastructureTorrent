#include "WorkerMain.h"

#include "Cache/Cache.h"
#include "LevelDb.h"
#include "BlockChain.h"

#include "BlockchainRead.h"

#include "parallel_for.h"
#include "stopProgram.h"
#include "duration.h"
#include "check.h"
#include "log.h"
#include "convertStrings.h"
#include "utils/FileSystem.h"

#include "Modules.h"

using namespace common;

namespace torrent_node_lib {

WorkerMain::WorkerMain(LevelDb &leveldb, AllCaches &caches, BlockChain &blockchain, const std::set<Address> &users, std::mutex &usersMut, int countThreads)
    : leveldb(leveldb)
    , caches(caches)
    , blockchain(blockchain)
    , countThreads(countThreads)
    , users(users)
    , usersMut(usersMut)
{    
    const std::string oldBlockMetadata = findMainBlock(leveldb);
    const MainBlockInfo oldMetadata = MainBlockInfo::deserialize(oldBlockMetadata);
    countVal.store(oldMetadata.countVal);
    
    lastSavedBlock = oldMetadata.blockNumber;
}

WorkerMain::~WorkerMain() = default;

bool WorkerMain::checkAddressToSave(const TransactionInfo& tx, const Address& address) const {
    if (!tx.isSaveToBd) {
        return false;
    }
    if (modules[MODULE_USERS]) {
        std::lock_guard<std::mutex> lock(usersMut);
        if (users.find(address) == users.end()) {
            return false;
        }
    }
    return true;
}

void WorkerMain::worker() {    
    LocalCache &localCache = caches.localCache;
    while (true) {
        try {
            std::shared_ptr<BlockInfo> biSP;
            
            const bool isStopped = !queue.pop(biSP);
            if (isStopped) {
                return;
            }
            BlockInfo &bi = *biSP;
            Timer tt;
                        
            const std::string attributeTxStatusCache = std::to_string(bi.header.blockNumber.value());
            
            const std::string oldBlockMetadata = findMainBlock(leveldb);
            const MainBlockInfo oldMetadata = MainBlockInfo::deserialize(oldBlockMetadata);
            const std::string prevHash = oldMetadata.blockHash;
            
            if (bi.header.blockNumber.value() <= oldMetadata.blockNumber) {
                continue;
            }
            
            CHECK(prevHash.empty() || prevHash == bi.header.prevHash, "Incorrect prev hash. Expected " + prevHash + ", received " + bi.header.prevHash);
            
            bi.times.timeBeginSaveBlock = ::now();
            
            Batch batch;
                        
            batch.addMainBlock(MainBlockInfo(bi.header.blockNumber.value(), bi.header.hash, countVal.load()).serialize());
            
            addBatch(batch, leveldb);
            
            tt.stop();
            
            bi.times.timeEndSaveBlock = ::now();
            bi.times.timeEnd = ::now();
            
            LOGINFO << "Block " << bi.header.blockNumber.value() << " saved. Count txs " << bi.txs.size() << ". Time ms " << tt.countMs();
            
            caches.txsStatusCache.remove(std::to_string(bi.header.blockNumber.value() - caches.maxCountElementsTxsCache));
            
            processLocalCache();
            
            localCache.setLastBlockNum(bi.header.blockNumber.value());
            
            checkStopSignal();
        } catch (const exception &e) {
            LOGERR << e;
        } catch (const StopException &e) {
            LOGINFO << "Stop processBlockInfo thread";
            return;
        } catch (const std::exception &e) {
            LOGERR << e.what();
        } catch (...) {
            LOGERR << "Unknown error";
        }
    }
}

void WorkerMain::start() {
    workerThread = Thread(&WorkerMain::worker, this);
}

void WorkerMain::process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) {   
    if (bi->header.blockNumber.value() > lastSavedBlock) {
        queue.push(bi);
    }
}

std::optional<size_t> WorkerMain::getInitBlockNumber() const {
    return lastSavedBlock;
}

BlockInfo WorkerMain::getFullBlock(const BlockHeader &bh, size_t beginTx, size_t countTx) const {
    CHECK(modules[MODULE_BLOCK_RAW] && !modules[MODULE_USERS], "module " + MODULE_BLOCK_RAW_STR + " not set");
    
    BlockInfo bi;
    bi.header = bh;
    if (bh.blockNumber == 0) {
        return bi;
    }
    
    const std::optional<std::shared_ptr<std::string>> cache = caches.blockDumpCache.getValue(bh.hash);
    if (!cache.has_value()) {
        CHECK(!bh.filePos.fileName.empty(), "Empty file name in block header");
        std::ifstream file;
        openFile(file, bh.filePos.fileName);
        std::string tmp;
        const size_t nextPos = readNextBlockInfo(file, bh.filePos.pos, bi, tmp, false, false, beginTx, countTx);
        CHECK(nextPos != bh.filePos.pos, "Ups");
    } else {
        std::shared_ptr<std::string> element = cache.value();
        readNextBlockInfo(element->data(), element->data() + element->size(), bh.filePos.pos, bi, true, false, beginTx, countTx);
    }
    
    for (TransactionInfo &tx: bi.txs) {
        tx.blockNumber = bi.header.blockNumber.value();
    }
    
    return bi;
}

void WorkerMain::processLocalCache() {

}

}
