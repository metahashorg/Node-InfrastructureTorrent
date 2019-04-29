#include "WorkerNodeTest.h"

#include <numeric>

#include "check.h"
#include "utils/utils.h"
#include "log.h"
#include "convertStrings.h"

#include "BlockChain.h"

#include "NodeTestsBlockInfo.h"

#include <rapidjson/document.h>

using namespace common;

namespace torrent_node_lib {
        
WorkerNodeTest::WorkerNodeTest(const BlockChain &blockchain, const LevelDbOptions &leveldbOptNodeTest) 
    : blockchain(blockchain)
    , leveldbNodeTest(leveldbOptNodeTest.writeBufSizeMb, leveldbOptNodeTest.isBloomFilter, leveldbOptNodeTest.isChecks, leveldbOptNodeTest.folderName, leveldbOptNodeTest.lruCacheMb)
{
    const std::string lastScriptBlockStr = findNodeStatBlock(leveldbNodeTest);
    const NodeStatBlockInfo lastScriptBlock = NodeStatBlockInfo::deserialize(lastScriptBlockStr);
    initializeScriptBlockNumber = lastScriptBlock.blockNumber;
}

void WorkerNodeTest::work() {
    while (true) {
        try {
            std::shared_ptr<BlockInfo> biSP;
            
            const bool isStopped = !queue.pop(biSP);
            if (isStopped) {
                return;
            }
            BlockInfo &bi = *biSP;
            
            const std::string lastScriptBlockStr = findNodeStatBlock(leveldbNodeTest);
            const NodeStatBlockInfo lastScriptBlock = NodeStatBlockInfo::deserialize(lastScriptBlockStr);
            const std::string &prevHash = lastScriptBlock.blockHash;

            if (bi.header.blockNumber.value() <= lastScriptBlock.blockNumber) {
                continue;
            }
            
            Timer tt;
            
            CHECK(prevHash.empty() || prevHash == bi.header.prevHash, "Incorrect prev hash. Expected " + prevHash + ", received " + bi.header.prevHash);
                       
            const std::string allNodesStr = findAllNodes(leveldbNodeTest);
            AllNodes allNodes = AllNodes::deserialize(allNodesStr);
            
            Batch batchStates(false);
            
            for (const TransactionInfo &tx: bi.txs) {               
                if (tx.data.size() > 0) {
                    if (tx.data[0] == '{' && tx.data[tx.data.size() - 1] == '}') {
                        rapidjson::Document doc;
                        const rapidjson::ParseResult pr = doc.Parse((const char*)tx.data.data(), tx.data.size());
                        if (pr) { // Здесь специально стоят if-ы вместо чеков, чтобы не крашится, если пользователь захочет послать какую-нибудь фигню
                            if (doc.HasMember("method") && doc["method"].IsString()) {
                                const std::string method = doc["method"].GetString();
                                if (method == "mh-noderegistration") {
                                    if (doc.HasMember("params") && doc["params"].IsObject()) {
                                        const auto &params = doc["params"];
                                        if (params.HasMember("host") && params["host"].IsString() && params.HasMember("name") && params["name"].IsString()) {
                                            const std::string host = params["host"].GetString();
                                            const std::string name = params["name"].GetString();
                                            LOGINFO << "Node register found " << host;
                                            allNodes.nodes[host] = name;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
                        
            batchStates.addNodeStatBlock(NodeStatBlockInfo(bi.header.blockNumber.value(), bi.header.hash, 0).serialize());
            
            batchStates.addAllNodes(allNodes.serialize());
            
            addBatch(batchStates, leveldbNodeTest);
            
            tt.stop();
            
            LOGINFO << "Block " << bi.header.blockNumber.value() << " saved to node test. Time: " << tt.countMs();
            
            checkStopSignal();
        } catch (const exception &e) {
            LOGERR << e;
        } catch (const StopException &e) {
            LOGINFO << "Stop fillCacheWorker thread";
            return;
        } catch (const std::exception &e) {
            LOGERR << e.what();
        } catch (...) {
            LOGERR << "Unknown error";
        }
    }
}
    
void WorkerNodeTest::start() {
    thread = Thread(&WorkerNodeTest::work, this);
}
    
void WorkerNodeTest::process(std::shared_ptr<BlockInfo> bi, std::shared_ptr<std::string> dump) {
    queue.push(bi);
}
    
std::optional<size_t> WorkerNodeTest::getInitBlockNumber() const {
    return initializeScriptBlockNumber;
}

std::map<std::string, std::string> WorkerNodeTest::getAllNodes() const {
    const std::string allNodesStr = findAllNodes(leveldbNodeTest);
    AllNodes allNodes = AllNodes::deserialize(allNodesStr);
    return allNodes.nodes;
}

}
