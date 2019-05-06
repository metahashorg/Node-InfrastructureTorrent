#include "GetNewBlocksFromServers.h"

#include <functional>
using namespace std::placeholders;

#include "generate_json.h"

#include <mutex>

#include "BlockInfo.h"

#include "check.h"
#include "log.h"
#include "jsonUtils.h"

using namespace common;

namespace torrent_node_lib {

const static size_t ESTIMATE_SIZE_SIGNATURE = 250;

const static size_t MAX_BLOCK_SIZE_WITHOUT_ADVANCE = 100 * 1000;

GetNewBlocksFromServer::LastBlockResponse GetNewBlocksFromServer::getLastBlock() const {
    std::optional<size_t> lastBlock;
    std::string error;
    std::vector<std::string> serversSave;
    std::mutex mut;
    const BroadcastResult function = [&lastBlock, &error, &mut, &serversSave](const std::string &server, const std::string &result, const std::optional<CurlException> &curlException) {
        if (curlException.has_value()) {
            std::lock_guard<std::mutex> lock(mut);
            error = curlException.value().message;
            return;
        }
        
        try {
            rapidjson::Document doc;
            const rapidjson::ParseResult pr = doc.Parse(result.c_str());
            CHECK(pr, "rapidjson parse error. Data: " + result);
            
            CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
            CHECK(doc.HasMember("result") && doc["result"].IsObject(), "result field not found");
            const auto &resultJson = doc["result"];
            CHECK(resultJson.HasMember("count_blocks") && resultJson["count_blocks"].IsInt(), "count_blocks field not found");
            const size_t countBlocks = resultJson["count_blocks"].GetInt();
            
            std::lock_guard<std::mutex> lock(mut);
            if (!lastBlock.has_value()) {
                lastBlock = 0;
            }
            if (lastBlock < countBlocks) {
                lastBlock = countBlocks;
                serversSave.clear();
                serversSave.emplace_back(server);
            } else if (lastBlock == countBlocks) {
                serversSave.emplace_back(server);
            }
        } catch (const exception &e) {
            std::lock_guard<std::mutex> lock(mut);
            error = e;
        }
    };
    
    p2p.broadcast("get-count-blocks", "{\"id\": 1}", "", function);
    
    LastBlockResponse response;
    if (lastBlock.has_value()) {
        response.lastBlock = lastBlock.value();
        response.servers = serversSave;
    } else {
        response.error = error;
    }
    return response;
}

void GetNewBlocksFromServer::clearAdvanced() {
    advancedLoadsBlocksHeaders.clear();
    advancedLoadsBlocksDumps.clear();
}

MinimumBlockHeader GetNewBlocksFromServer::getBlockHeader(size_t blockNum, size_t maxBlockNum, const std::string &server) const {
    const auto foundBlock = std::find_if(advancedLoadsBlocksHeaders.begin(), advancedLoadsBlocksHeaders.end(), [blockNum](const auto &pair) {
        return pair.first == blockNum;
    });
    if (foundBlock != advancedLoadsBlocksHeaders.end()) {
        return foundBlock->second;
    }
    
    advancedLoadsBlocksHeaders.clear();
    
    const size_t countBlocks = std::min(maxBlockNum - blockNum + 1, maxAdvancedLoadBlocks);
    const size_t countParts = (countBlocks + countBlocksInBatch - 1) / countBlocksInBatch;
    CHECK(countBlocks != 0 && countParts != 0, "Incorrect count blocks");
    const auto makeQsAndPost = [blockNum, countBlocksInBatch=this->countBlocksInBatch, maxCountBlocks=countBlocks](size_t number) {
        const size_t beginBlock = blockNum + number * countBlocksInBatch;
        const size_t countBlocks = std::min(countBlocksInBatch, maxCountBlocks - number * countBlocksInBatch);
        if (countBlocks != 1) {
            return std::make_pair("get-blocks", "{\"id\":1,\"params\":{\"beginBlock\": " + std::to_string(beginBlock) + ", \"countBlocks\": " + std::to_string(countBlocks) + ", \"type\": \"forP2P\", \"direction\": \"forward\"}}");
        } else {
            return std::make_pair("get-block-by-number", "{\"id\":1,\"params\":{\"number\": " + std::to_string(blockNum + number) + ", \"type\": \"forP2P\"}}");
        }
    };
    
    const std::vector<std::string> answer = p2p.requests(countParts, makeQsAndPost, "", [](const std::string &result) {
        ResponseParse r;
        r.response = result;
        return r;
    }, {server});
    
    CHECK(answer.size() == countParts, "Incorrect answer");
    
    for (size_t i = 0; i < answer.size(); i++) {
        const size_t currBlockNum = blockNum + i * countBlocksInBatch;
        const size_t blocksInPart = std::min(countBlocksInBatch, countBlocks - i * countBlocksInBatch);
        if (blocksInPart != 1) {
            const std::vector<MinimumBlockHeader> blocks = parseBlocksHeader(answer[i]);
            CHECK(blocks.size() == blocksInPart, "Incorrect answers");
            for (size_t j = 0; j < blocks.size(); j++) {
                CHECK(blocks[j].number == currBlockNum + j, "Incorrect block number in answer: " + std::to_string(blocks[j].number) + " " + std::to_string(currBlockNum + j));
                advancedLoadsBlocksHeaders.emplace_back(currBlockNum + j, blocks[j]);
            }
        } else {
            advancedLoadsBlocksHeaders.emplace_back(currBlockNum, parseBlockHeader(answer[i]));
        }
    }
    
    return advancedLoadsBlocksHeaders.front().second;
}

MinimumBlockHeader GetNewBlocksFromServer::getBlockHeaderWithoutAdvanceLoad(size_t blockNum, const std::string &server) const {
    const std::string response = p2p.runOneRequest(server, "get-block-by-number", "{\"id\":1,\"params\":{\"number\": " + std::to_string(blockNum) + ", \"type\": \"forP2P\"}}", "");
    
    return parseBlockHeader(response);
}

std::pair<std::string, std::string> GetNewBlocksFromServer::makeRequestForDumpBlock(const std::string &blockHash, size_t fromByte, size_t toByte) {
    const static std::string QS = "get-dump-block-by-hash";
    const std::string post = "{\"id\":1,\"params\":{\"hash\": \"" + blockHash + "\" , \"fromByte\": " + std::to_string(fromByte) + ", \"toByte\": " + std::to_string(toByte) + ", \"isHex\": false}}"; // TODO если isHex true, нужно менять код ниже
    return std::make_pair(QS, post);
}

std::pair<std::string, std::string> GetNewBlocksFromServer::makeRequestForDumpBlockSign(const std::string &blockHash, size_t fromByte, size_t toByte) {
    const static std::string QS = "get-dump-block-by-hash";
    const std::string post = "{\"id\":1,\"params\":{\"hash\": \"" + blockHash + "\" , \"fromByte\": " + std::to_string(fromByte) + ", \"toByte\": " + std::to_string(toByte) + "\", \"isHex\": false, \"isSign\": true, " + 
    "\"compress\": " + "false" + 
    "}}"; // TODO если isHex true, нужно менять код ниже
    return std::make_pair(QS, post);
}

ResponseParse GetNewBlocksFromServer::parseDumpBlockResponse(const std::string& result) {
    ResponseParse parsed;
    if (result.empty()) {
        return parsed;
    }
    if (result.size() <= 512 && result[0] == '{' && result[result.size() - 1] == '}') { // скорее всего, это json. Проверка в предположении, что isHex false
        try {
            rapidjson::Document doc;
            const rapidjson::ParseResult pr = doc.Parse(result.c_str());
            CHECK(pr, "rapidjson parse error. Data: " + result);
            
            CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
            CHECK(doc.HasMember("result") && doc["result"].IsObject(), "result field not found");
            const auto &resultJson = doc["result"];
            CHECK(resultJson.HasMember("dump") && resultJson["dump"].IsString(), "dump field not found");
            parsed.response = resultJson["dump"].GetString();
        } catch (const exception &e) {
            parsed.error = e;
        }
    } else {
        parsed.response = result;
    }
    return parsed;
}

std::string GetNewBlocksFromServer::getBlockDumpWithoutAdvancedLoad(const std::string &blockHash, size_t blockSize, const std::vector<std::string> &hintsServers, bool isSign) const {
    if (!isSign) {
        return p2p.request(blockSize, true, std::bind(makeRequestForDumpBlock, std::ref(blockHash), _1, _2), "", parseDumpBlockResponse, hintsServers);
    } else {
        const std::string result = p2p.request(blockSize + ESTIMATE_SIZE_SIGNATURE, false, std::bind(makeRequestForDumpBlockSign, std::ref(blockHash), _1, _2), "", parseDumpBlockResponse, hintsServers);
        return result;
    }
}

std::string GetNewBlocksFromServer::getBlockDump(const std::string& blockHash, size_t blockSize, const std::vector<std::string> &hintsServers, bool isSign) const {
    const auto foundDump = advancedLoadsBlocksDumps.find(blockHash);
    if (foundDump != advancedLoadsBlocksDumps.end()) {
        return foundDump->second;
    }
       
    if (blockSize > MAX_BLOCK_SIZE_WITHOUT_ADVANCE) {
        return getBlockDumpWithoutAdvancedLoad(blockHash, blockSize, hintsServers, isSign);
    }
    
    advancedLoadsBlocksDumps.clear();
    
    const auto foundHeader = std::find_if(advancedLoadsBlocksHeaders.begin(), advancedLoadsBlocksHeaders.end(), [&blockHash](const auto &pair) {
        return pair.second.hash == blockHash;
    });
   
    std::vector<std::string> blocksHashs;
    for (auto iterHeader = foundHeader ; iterHeader != advancedLoadsBlocksHeaders.end() && iterHeader->second.blockSize <= MAX_BLOCK_SIZE_WITHOUT_ADVANCE; iterHeader++) {
        blocksHashs.emplace_back(iterHeader->second.hash);
    }
    
    CHECK(!blocksHashs.empty(), "advanced blocks not loaded");
    
    const size_t countParts = (blocksHashs.size() + countBlocksInBatch - 1) / countBlocksInBatch;
    
    const auto makeQsAndPost = [&blocksHashs, isSign, countBlocksInBatch=this->countBlocksInBatch, isCompress=this->isCompress](size_t number) {
        CHECK(blocksHashs.size() > number * countBlocksInBatch, "Incorrect number");
        const size_t beginBlock = number * countBlocksInBatch;
        const size_t countBlocks = std::min(countBlocksInBatch, blocksHashs.size() - number * countBlocksInBatch);
        if (countBlocks == 1) {
            return std::make_pair("get-dump-block-by-hash", "{\"id\":1,\"params\":{\"hash\": \"" + blocksHashs[number] + "\" , \"isHex\": false, " + 
                "\"isSign\": " + (isSign ? "true" : "false") + 
                ", \"compress\": " + (isCompress ? "true" : "false") + 
                "}}");
        } else {            
            std::string r;
            r += "{\"id\":1,\"params\":{\"hashes\": [";
            bool isFirst = true;
            for (size_t i = beginBlock; i < beginBlock + countBlocks; i++) {
                if (!isFirst) {
                    r += ", ";
                }
                
                r += "\"" + blocksHashs[i] + "\"";
                
                isFirst = false;
            }
            r += std::string("], \"isSign\": ") + (isSign ? "true" : "false") + 
            ", \"compress\": " + (isCompress ? "true" : "false") + 
            "}}";
            
            return std::make_pair("get-dumps-blocks-by-hash", r);
        }
    };
    
    const std::vector<std::string> responses = p2p.requests(countParts, makeQsAndPost, "", parseDumpBlockResponse, hintsServers);
    CHECK(responses.size() == countParts, "Incorrect responses");
    
    for (size_t i = 0; i < responses.size(); i++) {
        const size_t beginBlock = i * countBlocksInBatch;
        const size_t blocksInPart = std::min(countBlocksInBatch, blocksHashs.size() - i * countBlocksInBatch);
        
        if (blocksInPart == 1) {
            advancedLoadsBlocksDumps[blocksHashs[i]] = parseDumpBlockBinary(responses[i], isCompress);
        } else {
            const std::vector<std::string> blocks = parseDumpBlocksBinary(responses[i], isCompress);
            CHECK(blocks.size() == blocksInPart, "Incorrect answer");
            CHECK(beginBlock + blocks.size() <= blocksHashs.size(), "Incorrect answer");
            for (size_t j = 0; j < blocks.size(); j++) {
                advancedLoadsBlocksDumps[blocksHashs[beginBlock + j]] = blocks[j];
            }
        }
    }
    
    return advancedLoadsBlocksDumps[blockHash];
}

}
