#include "GetNewBlocksFromServers.h"

#include <functional>
using namespace std::placeholders;

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <mutex>

#include "check.h"
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

static GetNewBlocksFromServer::BlockHeader parseBlockHeader(const std::string &response) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(response.c_str());
    CHECK(pr, "rapidjson parse error. Data: " + response);
    
    CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
    CHECK(doc.HasMember("result") && doc["result"].IsObject(), "result field not found");
    const auto &resultJson = doc["result"];
    
    GetNewBlocksFromServer::BlockHeader result;
    CHECK(resultJson.HasMember("hash") && resultJson["hash"].IsString(), "hash field not found");
    result.hash = resultJson["hash"].GetString();
    CHECK(resultJson.HasMember("prev_hash") && resultJson["prev_hash"].IsString(), "prev_hash field not found");
    result.parentHash = resultJson["prev_hash"].GetString();
    CHECK(resultJson.HasMember("size") && resultJson["size"].IsInt64(), "size field not found");
    result.blockSize = resultJson["size"].GetInt64();
    CHECK(resultJson.HasMember("fileName") && resultJson["fileName"].IsString(), "fileName field not found");
    result.fileName = resultJson["fileName"].GetString();
    
    return result;
}

void GetNewBlocksFromServer::clearAdvanced() {
    advancedLoadsBlocksHeaders.clear();
    advancedLoadsBlocksDumps.clear();
}

GetNewBlocksFromServer::BlockHeader GetNewBlocksFromServer::getBlockHeader(size_t blockNum, size_t maxBlockNum, const std::string &server) const {
    const auto foundBlock = std::find_if(advancedLoadsBlocksHeaders.begin(), advancedLoadsBlocksHeaders.end(), [blockNum](const auto &pair) {
        return pair.first == blockNum;
    });
    if (foundBlock != advancedLoadsBlocksHeaders.end()) {
        return foundBlock->second;
    }
    
    advancedLoadsBlocksHeaders.clear();
    
    const size_t countBlocks = std::min(maxBlockNum - blockNum + 1, maxAdvancedLoadBlocks);
    CHECK(countBlocks != 0, "Incorrect count blocks");
    const auto makeQsAndPost = [blockNum](size_t number) {
        return std::make_pair("get-block-by-number", "{\"id\":1,\"params\":{\"number\": " + std::to_string(blockNum + number) + ", \"type\": \"forP2P\"}}");
    };
    
    const std::vector<std::string> answer = p2p.requests(countBlocks, makeQsAndPost, "", [](const std::string &result) {
        ResponseParse r;
        r.response = result;
        return r;
    }, {server});
    
    CHECK(answer.size() == countBlocks, "Incorrect answer");
    
    for (size_t i = 0; i < answer.size(); i++) {
        const size_t currBlockNum = blockNum + i;
        advancedLoadsBlocksHeaders.emplace_back(currBlockNum, parseBlockHeader(answer[i]));
    }
    
    return advancedLoadsBlocksHeaders.front().second;
}

GetNewBlocksFromServer::BlockHeader GetNewBlocksFromServer::getBlockHeaderWithoutAdvanceLoad(size_t blockNum, const std::string &server) const {
    const std::string response = p2p.runOneRequest(server, "get-block-by-number", "{\"id\":1,\"params\":{\"number\": " + std::to_string(blockNum) + ", \"type\": \"forP2P\"}}", "");
    
    return parseBlockHeader(response);
}

std::pair<std::string, std::string> GetNewBlocksFromServer::makeRequestForDumpBlock(const std::string &blockHash, size_t fromByte, size_t toByte) {
    const static std::string QS = "get-dump-block-by-hash";
    const std::string post = "{\"id\":1,\"params\":{\"hash\": \"" + blockHash + "\" , \"fromByte\": " + std::to_string(fromByte) + ", \"toByte\": " + std::to_string(toByte) + ", \"isHex\": false}}"; // TODO если isHex true, нужно менять код ниже
    return std::make_pair(QS, post);
}

std::pair<std::string, std::string> GetNewBlocksFromServer::makeRequestForDumpBlockSign(const std::string &blockHash) {
    const static std::string QS = "get-dump-block-by-hash";
    const std::string post = "{\"id\":1,\"params\":{\"hash\": \"" + blockHash + "\", \"isHex\": false, \"isSign\": true}}"; // TODO если isHex true, нужно менять код ниже
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
        return p2p.request(blockSize + ESTIMATE_SIZE_SIGNATURE, false, std::bind(makeRequestForDumpBlockSign, std::ref(blockHash)), "", parseDumpBlockResponse, hintsServers);
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
    
    const auto makeQsAndPost = [&blocksHashs, isSign](size_t number) {
        CHECK(blocksHashs.size() > number, "Incorrect number");
        return std::make_pair("get-dump-block-by-hash", "{\"id\":1,\"params\":{\"hash\": \"" + blocksHashs[number] + "\" , \"isHex\": false, \"isSign\": " + (isSign ? "true" : "false") + "}}");
    };
    
    const std::vector<std::string> responses = p2p.requests(blocksHashs.size(), makeQsAndPost, "", parseDumpBlockResponse, hintsServers);
    CHECK(responses.size() == blocksHashs.size(), "Incorrect responses");
    
    for (size_t i = 0; i < responses.size(); i++) {
        advancedLoadsBlocksDumps[blocksHashs[i]] = responses[i];
    }
    
    return advancedLoadsBlocksDumps[blockHash];
}

}
