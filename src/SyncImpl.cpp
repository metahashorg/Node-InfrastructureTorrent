#include "SyncImpl.h"

#include "BlockchainRead.h"
#include "PrivateKey.h"

#include "parallel_for.h"
#include "stopProgram.h"
#include "duration.h"
#include "check.h"
#include "log.h"
#include "convertStrings.h"
#include "stringUtils.h"

#include "BlockSource/FileBlockSource.h"
#include "BlockSource/NetworkBlockSource.h"

#include "Workers/WorkerCache.h"
#include "Workers/WorkerNodeTest.h"
#include "Workers/WorkerMain.h"

#include "Workers/NodeTestsBlockInfo.h"

using namespace common;

namespace torrent_node_lib {
    
const static std::string VERSION_DB = "v3.4";
    
bool isInitialized = false;

void SyncImpl::initialize(const std::string& folderPath, const GetterBlockOptions &getterBlocksOpt) {
    CHECK(!modules[MODULE_USERS] || !getterBlocksOpt.getBlocksFromFile, "Options saveOnlyUsers and getBlocksFromFile not compatible");
    
    if (getterBlocksOpt.getBlocksFromFile) {
        isSaveBlockToFiles = false;
        getBlockAlgorithm = std::make_unique<FileBlockSource>(leveldb, folderPath, isValidate);
    } else {
        CHECK(getterBlocksOpt.p2p != nullptr, "p2p nullptr");
        isSaveBlockToFiles = modules[MODULE_BLOCK_RAW];
        const bool isSaveAllTx = modules[MODULE_USERS];
        getBlockAlgorithm = std::make_unique<NetworkBlockSource>(folderPath, getterBlocksOpt.maxAdvancedLoadBlocks, getterBlocksOpt.countBlocksInBatch, getterBlocksOpt.isCompress, *getterBlocksOpt.p2p, isSaveAllTx, getterBlocksOpt.isValidate, getterBlocksOpt.isValidateSign);
    }
}

SyncImpl::SyncImpl(const std::string& folderPath, const LevelDbOptions& leveldbOpt, const CachesOptions& cachesOpt, const GetterBlockOptions &getterBlocksOpt, const std::string &signKeyName, const TestNodesOptions &testNodesOpt)
    : leveldb(leveldbOpt.writeBufSizeMb, leveldbOpt.isBloomFilter, leveldbOpt.isChecks, leveldbOpt.folderName, leveldbOpt.lruCacheMb)
    , caches(cachesOpt.maxCountElementsBlockCache, cachesOpt.maxCountElementsTxsCache, cachesOpt.macLocalCacheElements)
    , isValidate(getterBlocksOpt.isValidate)
    , testNodes(getterBlocksOpt.p2p, testNodesOpt.myIp, testNodesOpt.testNodesServer, testNodesOpt.defaultPortTorrent)
{
    if (getterBlocksOpt.isValidate) {
        CHECK(!getterBlocksOpt.getBlocksFromFile, "validate and get_blocks_from_file options not compatible");
    }
    initialize(folderPath, getterBlocksOpt);
    
    if (!signKeyName.empty()) {
        const std::string signKeyPath = signKeyName + ".raw.prv";
        const std::string result = trim(loadFile("./", signKeyPath));
        CHECK(!result.empty(), "File with private key not found");
        privateKey = std::make_unique<PrivateKey>(fromHex(result), signKeyName);
        
        testNodes.setPrivateKey(privateKey.get());
    }
}

void SyncImpl::setLeveldbOptScript(const LevelDbOptions &leveldbOpt) {
    this->leveldbOptScript = leveldbOpt;
}

void SyncImpl::setLeveldbOptNodeTest(const LevelDbOptions &leveldbOpt) {
    this->leveldbOptNodeTest = leveldbOpt;
}

SyncImpl::~SyncImpl() = default;

void SyncImpl::addUsers(const std::set<Address>& addresses) {
    CHECK(modules[MODULE_USERS], "saveOnlyUsers not set");
    std::lock_guard<std::mutex> lock(usersMut);
    users.insert(addresses.begin(), addresses.end());
}

void SyncImpl::saveTransactions(BlockInfo& bi, const std::string &binaryDump, bool saveBlockToFile) {
    if (!saveBlockToFile) {
        return;
    }
    
    const std::string &fileName = bi.header.filePos.fileName;
    CHECK(!fileName.empty(), "File name not set");
    
    if (!modules[MODULE_USERS]) {
        const size_t currPos = saveBlockToFileBinary(fileName, binaryDump);
        bi.header.filePos.pos = currPos;
        for (TransactionInfo &tx : bi.txs) {
            tx.filePos.fileName = fileName;
            tx.filePos.pos += currPos;
        }
    } else {
        std::ofstream file;
        
        openFile(file, fileName);
        for (TransactionInfo &tx: bi.txs) {
            if (tx.isSaveToBd) {
                const size_t filePos = saveTransactionToFile(file, tx.allRawTx);
                
                tx.filePos.fileName = fileName;
                tx.filePos.pos = filePos;
            }
            tx.allRawTx.clear();
            tx.allRawTx.shrink_to_fit();
        }
    }   
}

void SyncImpl::filterTransactionsToSave(BlockInfo& bi) {
    std::unique_lock<std::mutex> lock(usersMut);
    const std::set<Address> copyUsers = users;
    lock.unlock();
    for (TransactionInfo &tx: bi.txs) {
        if (!modules[MODULE_USERS]) {
            tx.isSaveToBd = true;
        } else if (modules[MODULE_USERS] && (copyUsers.find(tx.fromAddress) != copyUsers.end() || copyUsers.find(tx.toAddress) != copyUsers.end())) {
            tx.isSaveToBd = true;
        } else if (modules[MODULE_USERS] && tx.isSignBlockTx) {
            tx.isSaveToBd = true;
        } else {
            tx.isSaveToBd = false;
        }
    }
}

void SyncImpl::saveSignBlockTransactionsToHeader(BlockInfo& bi) {
    for (const TransactionInfo &tx: bi.txs) {
        if (tx.isSignBlockTx) {
            bi.header.blockSignatures.emplace_back(tx);
        }
    }
}

void SyncImpl::fillSignedTransactionsInBlock(BlockHeader& bh) const {
    std::ifstream file;
    std::string saveFileName;
    for (TransactionInfo &tx: bh.blockSignatures) {
        if (!tx.filePos.fileName.empty() && !tx.isInitialized) {
            if (saveFileName != tx.filePos.fileName) {
                closeFile(file);
                openFile(file, tx.filePos.fileName);
                saveFileName = tx.filePos.fileName;
            }
            const bool res = readOneTransactionInfo(file, tx.filePos.pos, tx, false);
            CHECK(res, "Incorrect read transaction info");
            CHECK(tx.isInitialized, "Incorrect read transaction info 2 " + tx.filePos.fileName + " " + std::to_string(tx.filePos.pos));
        }
    }
}

void SyncImpl::saveBlockToLeveldb(const BlockInfo &bi) {
    Batch batch;
    if (modules[MODULE_BLOCK]) {
        batch.addBlockHeader(bi.header.hash, bi.header.serialize());
    }
    
    const std::string blockMetadata = findBlockMetadata(leveldb);
    const BlocksMetadata metadata = BlocksMetadata::deserialize(blockMetadata);
    BlocksMetadata newMetadata;
    newMetadata.prevBlockHash = bi.header.prevHash;
    if (metadata.prevBlockHash == bi.header.prevHash) {
        if (metadata.blockHash < bi.header.hash) {
            newMetadata.blockHash = metadata.blockHash;
        } else {
            newMetadata.blockHash = bi.header.hash;
        }
    } else {
        newMetadata.blockHash = bi.header.hash;
    }
    batch.addBlockMetadata(newMetadata.serialize());
    
    FileInfo fi;
    fi.filePos.fileName = bi.header.filePos.fileName;
    fi.filePos.pos = bi.header.endBlockPos;
    batch.addFileMetadata(CroppedFileName(fi.filePos.fileName), fi.serialize());
    
    addBatch(batch, leveldb);
}

void SyncImpl::synchronize(int countThreads) {
    this->countThreads = countThreads;
    this->isSync = isSync;
    
    CHECK(isInitialized, "Not initialized");
    CHECK(modules[MODULE_BLOCK], "module " + MODULE_BLOCK_STR + " not set");
    
    try {
        const std::string modulesStr = findModules(leveldb);
        if (!modulesStr.empty()) {
            const Modules oldModules(modulesStr);
            CHECK(modules == oldModules, "Modules changed in this database");
        } else {
            saveModules(modules.to_string(), leveldb);
        }
        
        const std::string versionDb = findVersionDb(leveldb);
        if (!versionDb.empty()) {
            CHECK(versionDb == VERSION_DB, "Version database not matches");
        } else {
            saveVersionDb(VERSION_DB, leveldb);
        }
        
        const std::string blockMetadata = findBlockMetadata(leveldb);
        const BlocksMetadata metadata = BlocksMetadata::deserialize(blockMetadata);
        
        getBlockAlgorithm->initialize();
        
        blockchain.clear();
        
        const std::set<std::string> blocksRaw = getAllBlocks(leveldb);
        for (const std::string &blockRaw: blocksRaw) {
            BlockHeader bh = BlockHeader::deserialize(blockRaw);
            blockchain.addWithoutCalc(bh);
        }
        
        if (!metadata.blockHash.empty()) {
            const size_t countBlocks = blockchain.calcBlockchain(metadata.blockHash);
            LOGINFO << "Last block " << countBlocks << " " << metadata.blockHash;
        }
           
        std::vector<Worker*> workers;
        cacheWorker = std::make_unique<WorkerCache>(caches);
        workers.emplace_back(cacheWorker.get());
        mainWorker = std::make_unique<WorkerMain>(leveldb, caches, blockchain, users, usersMut, countThreads);
        workers.emplace_back(mainWorker.get());
        if (modules[MODULE_NODE_TEST]) {
            CHECK(leveldbOptNodeTest.isValid, "Leveldb node test options not setted");
            nodeTestWorker = std::make_unique<WorkerNodeTest>(blockchain, leveldbOptNodeTest);
            workers.emplace_back(nodeTestWorker.get());
            testNodes.addWorkerTest(nodeTestWorker.get());
        }
    
        for (Worker* &worker: workers) {
            worker->start();
        }
        
        testNodes.start();
        
        const auto minElement = std::min_element(workers.begin(), workers.end(), [](const Worker* first, const Worker* second) {
            if (!first->getInitBlockNumber().has_value()) {
                return false;
            } else if (!second->getInitBlockNumber().has_value()) {
                return true;
            } else {
                return first->getInitBlockNumber().value() < second->getInitBlockNumber().value();
            }
        });
        if (minElement != workers.end() && minElement.operator*()->getInitBlockNumber().has_value()) {
            const size_t fromBlockNumber = minElement.operator*()->getInitBlockNumber().value() + 1;
            LOGINFO << "Retry from block " << fromBlockNumber;
            for (size_t blockNumber = fromBlockNumber; blockNumber <= blockchain.countBlocks(); blockNumber++) {
                const BlockHeader &bh = blockchain.getBlock(blockNumber);
                std::shared_ptr<BlockInfo> bi = std::make_shared<BlockInfo>();
                std::shared_ptr<std::string> blockDump = std::make_shared<std::string>();
                try {
                    FileBlockSource::getExistingBlockS(bh, *bi, *blockDump, isValidate);
                } catch (const exception &e) {
                    LOGWARN << "Dont get existing block " << e;
                    getBlockAlgorithm->getExistingBlock(bh, *bi, *blockDump);
                } catch (const std::exception &e) {
                    LOGWARN << "Dont get existing block " << e.what();
                    getBlockAlgorithm->getExistingBlock(bh, *bi, *blockDump);
                } catch (...) {
                    LOGWARN << "Dont get existing block " << "Unknown";
                    getBlockAlgorithm->getExistingBlock(bh, *bi, *blockDump);
                }
                filterTransactionsToSave(*bi);
                for (Worker* &worker: workers) {
                    if (worker->getInitBlockNumber().has_value() && worker->getInitBlockNumber() < blockNumber) { // TODO добавить сюда поле getToBlockNumberRetry
                        worker->process(bi, blockDump);
                    }
                }
            }
        }
        
        while (true) {
            const time_point beginWhileTime = ::now();
            std::shared_ptr<BlockInfo> prevBi = nullptr;
            std::shared_ptr<std::string> prevDump = nullptr;
            try {
                auto [isContinue, knownLstBlk] = getBlockAlgorithm->doProcess(blockchain.countBlocks(), blockchain.getLastBlock().hash);
                knownLastBlock = knownLstBlk;
                while (isContinue) {
                    Timer tt;
                    
                    std::shared_ptr<BlockInfo> nextBi = std::make_shared<BlockInfo>();
                    
                    nextBi->times.timeBegin = ::now();
                    nextBi->times.timeBeginGetBlock = ::now();
                    
                    std::shared_ptr<std::string> nextBlockDump = std::make_shared<std::string>();
                    isContinue = getBlockAlgorithm->process(*nextBi, *nextBlockDump);
                    if (!isContinue) {
                        break;
                    }
                    
                    if (prevBi == nullptr) {
                        CHECK(prevDump == nullptr, "Ups");
                        prevBi = nextBi;
                        prevDump = nextBlockDump;
                        
                        if (isValidate) {
                            continue;
                        }
                    } else {
                        if (isValidate) {
                            const auto thisHashFromHex = fromHex(prevBi->header.hash);
                            for (const TransactionInfo &tx: nextBi->txs) {
                                if (tx.isSignBlockTx) {
                                    CHECK(thisHashFromHex == tx.data, "Block signatures not confirmed");
                                }
                            }
                        } else {
                            prevBi = nextBi;
                            prevDump = nextBlockDump;
                        }
                    }
                    
                    filterTransactionsToSave(*prevBi);
                    saveTransactions(*prevBi, *prevDump, isSaveBlockToFiles);
                    saveSignBlockTransactionsToHeader(*prevBi);
                    
                    const size_t currentBlockNum = blockchain.addBlock(prevBi->header);
                    CHECK(currentBlockNum != 0, "Incorrect block number");
                    prevBi->header.blockNumber = currentBlockNum;
                    
                    for (TransactionInfo &tx: prevBi->txs) {
                        tx.blockNumber = prevBi->header.blockNumber.value();
                        
                        if (tx.fromAddress.isInitialWallet()) {
                            prevBi->txsStatistic.countInitTxs++;
                        } else {
                            prevBi->txsStatistic.countTransferTxs++;
                        }
                    }
                                        
                    tt.stop();
                    
                    prevBi->times.timeEndGetBlock = ::now();
                    
                    LOGINFO << "Block " << currentBlockNum << " getted. Count txs " << prevBi->txs.size() << ". Time ms " << tt.countMs() << " current block " << prevBi->header.hash << ". Parent hash " << prevBi->header.prevHash;
                    
                    for (Worker* worker: workers) {
                        worker->process(prevBi, prevDump);
                    }
                    
                    saveBlockToLeveldb(*prevBi);
                    
                    if (isValidate) {
                        prevBi = nextBi;
                        prevDump = nextBlockDump;
                    }
                    
                    checkStopSignal();
                }
            } catch (const exception &e) {
                LOGERR << e;
            } catch (const std::exception &e) {
                LOGERR << e.what();
            }
            const time_point endWhileTime = ::now();
            
            const static milliseconds MAX_PENDING = milliseconds(1s) / 2;
            milliseconds pending = std::chrono::duration_cast<milliseconds>(endWhileTime - beginWhileTime);
            pending = MAX_PENDING - pending;
            if (pending < 0ms) {
                pending = 0ms;
            }
            
            sleepMs(pending);
        }
    } catch (const StopException &e) {
        LOGINFO << "Stop synchronize thread";
    } catch (const exception &e) {
        LOGERR << e;
    } catch (const std::exception &e) {
        LOGERR << e.what();
    } catch (...) {
        LOGERR << "Unknown error";
    }
}

std::string SyncImpl::getBlockDump(const BlockHeader &bh, size_t fromByte, size_t toByte, bool isHex, bool isSign) const {
    CHECK(modules[MODULE_BLOCK] && modules[MODULE_BLOCK_RAW] && !modules[MODULE_USERS], "modules " + MODULE_BLOCK_STR + " " + MODULE_BLOCK_RAW_STR + " not set");
       
    const std::optional<std::shared_ptr<std::string>> cache = caches.blockDumpCache.getValue(bh.hash);
    std::string res;
    size_t realSizeBlock;
    std::string fullBlockDump;
    if (!cache.has_value()) {
        CHECK(!bh.filePos.fileName.empty(), "Empty file name in block header");
        std::ifstream file;
        openFile(file, bh.filePos.fileName);
        const auto &[size_block, dumpBlock] = torrent_node_lib::getBlockDump(file, bh.filePos.pos, fromByte, toByte);
        res = dumpBlock;
        realSizeBlock = size_block;
        
        if (isSign) {
            if (toByte >= realSizeBlock) {
                if (fromByte == 0) {
                    fullBlockDump = res;
                } else {
                    const auto &[size_block, dumpBlock] = torrent_node_lib::getBlockDump(file, bh.filePos.pos, 0, toByte);
                    fullBlockDump = dumpBlock;
                }
            }
        }
    } else {
        std::shared_ptr<std::string> element = cache.value();
        res = element->substr(fromByte, toByte - fromByte);
        realSizeBlock = element->size();
        if (isSign) {
            if (toByte >= realSizeBlock) {
                fullBlockDump = *element;
            }
        }
    }
    
    if (isSign) {
        CHECK(privateKey != nullptr, "Private key not set");
        
        if (fromByte == 0) {
            res = makeFirstPartBlockSign(realSizeBlock) + res;
        }
        
        if (toByte >= realSizeBlock) {
            res += makeBlockSign(fullBlockDump, *privateKey);
        }
    }
    
    if (isHex) {
        return toHex(res.begin(), res.end());
    } else {
        return res;
    }
}

std::string SyncImpl::signTestString(const std::string &str, bool isHex) const {
    CHECK(privateKey != nullptr, "Private key not set");
    const std::string res = makeTestSign(str, *privateKey);
    if (isHex) {
        return toHex(res.begin(), res.end());
    } else {
        return res;
    }
}

size_t SyncImpl::getKnownBlock() const {
    return knownLastBlock.load();
}

}
