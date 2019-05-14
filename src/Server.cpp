#include "Server.h"

#include <string_view>
#include <variant>

#include "synchronize_blockchain.h"
#include "BlockInfo.h"
#include "Workers/NodeTestsBlockInfo.h"

#include "check.h"
#include "duration.h"
#include "stringUtils.h"
#include "log.h"
#include "jsonUtils.h"
#include "convertStrings.h"

#include "BlockChainReadInterface.h"

#include "cmake_modules/GitSHA1.h"

#include "generate_json.h"

#include "stopProgram.h"

using namespace common;
using namespace torrent_node_lib;

const static std::string GET_BLOCK_BY_HASH = "get-block-by-hash";
const static std::string GET_BLOCK_BY_NUMBER = "get-block-by-number";
const static std::string GET_BLOCKS = "get-blocks";
const static std::string GET_COUNT_BLOCKS = "get-count-blocks";
const static std::string GET_DUMP_BLOCK_BY_HASH = "get-dump-block-by-hash";
const static std::string GET_DUMP_BLOCK_BY_NUMBER = "get-dump-block-by-number";
const static std::string GET_DUMPS_BLOCKS_BY_HASH = "get-dumps-blocks-by-hash";
const static std::string GET_DUMPS_BLOCKS_BY_NUMBER = "get-dumps-blocks-by-number";
const static std::string SIGN_TEST_STRING = "sign-test-string";

const static int HTTP_STATUS_OK = 200;
const static int HTTP_STATUS_METHOD_NOT_ALLOWED = 405;
const static int HTTP_STATUS_BAD_REQUEST = 400;
const static int HTTP_STATUS_NO_CONTENT = 204;
const static int HTTP_STATUS_INTERNAL_SERVER_ERROR = 500;

struct IncCountRunningThread {
  
    IncCountRunningThread(std::atomic<int> &countRunningThreads)
        : countRunningThreads(countRunningThreads)
    {
        countRunningThreads++;
    }
    
    ~IncCountRunningThread() {
        countRunningThreads--;
    }
    
    std::atomic<int> &countRunningThreads;
    
};

template<typename T>
T getJsonField(const rapidjson::Value &json, const std::string_view name) {
    if constexpr (std::is_same_v<T, size_t>) {
        CHECK_USER(json.HasMember(name.data()) && json[name.data()].IsInt64(), std::string(name) + " field not found");
        return json[name.data()].GetInt64();
    } else if constexpr(std::is_same_v<T, std::string>) {
        CHECK_USER(json.HasMember(name.data()) && json[name.data()].IsString(), std::string(name) + " field not found");
        return json[name.data()].GetString();
    }
}

template<typename T>
T getJsonField(const rapidjson::Value &json) {
    if constexpr (std::is_same_v<T, size_t>) {
        CHECK_USER(json.IsInt64(), "field not found");
        return json.GetInt64();
    } else if constexpr(std::is_same_v<T, std::string>) {
        CHECK_USER(json.IsString(), "field not found");
        return json.GetString();
    }
}

template<typename T>
std::string to_string(const T &value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return value;
    } else {
        return std::to_string(value);
    }
}

template<typename T>
std::string getBlock(const RequestId &requestId, const rapidjson::Document &doc, const std::string_view nameParam, const Sync &sync, bool isFormat, const JsonVersion &version) {   
    CHECK_USER(doc.HasMember("params") && doc["params"].IsObject(), "params field not found");
    const auto &jsonParams = doc["params"];
    const T &hashOrNumber = getJsonField<T>(jsonParams, nameParam);
    
    BlockTypeInfo type = BlockTypeInfo::Simple;
    if (jsonParams.HasMember("type") && jsonParams["type"].IsInt()) {
        const int typeInt = jsonParams["type"].GetInt();
        if (typeInt == 0) {
            type = BlockTypeInfo::Simple;
        } else if (typeInt == 4) {
            type = BlockTypeInfo::ForP2P;
        } else if (typeInt == 1) {
            type = BlockTypeInfo::Hashes;
        } else if (typeInt == 2) {
            type = BlockTypeInfo::Full;
        } else if (typeInt == 3) {
            type = BlockTypeInfo::Small;
        }
    } else if (jsonParams.HasMember("type") && jsonParams["type"].IsString()) {
        const std::string typeString = jsonParams["type"].GetString();
        if (typeString == "simple") {
            type = BlockTypeInfo::Simple;
        } else if (typeString == "forP2P") {
            type = BlockTypeInfo::ForP2P;
        } else if (typeString == "hashes") {
            type = BlockTypeInfo::Hashes;
        } else if (typeString == "full") {
            type = BlockTypeInfo::Full;
        } else if (typeString == "small") {
            type = BlockTypeInfo::Small;
        }
    }
    
    const BlockHeader bh = sync.getBlockchain().getBlock(hashOrNumber);
    
    if (!bh.blockNumber.has_value()) {
        return genErrorResponse(requestId, -32603, "block " + to_string(hashOrNumber) + " not found");
    }
    
    BlockHeader nextBh = sync.getBlockchain().getBlock(*bh.blockNumber + 1);
    sync.fillSignedTransactionsInBlock(nextBh);
    if (type == BlockTypeInfo::Simple || type == BlockTypeInfo::ForP2P || type == BlockTypeInfo::Small) {
        return blockHeaderToJson(requestId, bh, nextBh, isFormat, type, version);
    } else {
        return "";
    }
}

template<typename T>
std::string getBlockDump(const rapidjson::Document &doc, const RequestId &requestId, const std::string_view nameParam, const Sync &sync, bool isFormat) {   
    CHECK_USER(doc.HasMember("params") && doc["params"].IsObject(), "params field not found");
    const auto &jsonParams = doc["params"];
    const T &hashOrNumber = getJsonField<T>(jsonParams, nameParam);
    
    size_t fromByte = 0;
    size_t toByte = std::numeric_limits<size_t>::max();
    if (jsonParams.HasMember("fromByte") && jsonParams["fromByte"].IsInt64()) {
        fromByte = jsonParams["fromByte"].GetInt64();
    }
    if (jsonParams.HasMember("toByte") && jsonParams["toByte"].IsInt64()) {
        toByte = jsonParams["toByte"].GetInt64();
    }
    
    bool isHex = false;
    if (jsonParams.HasMember("isHex") && jsonParams["isHex"].IsBool()) {
        isHex = jsonParams["isHex"].GetBool();
    }
    
    bool isSign = false;
    if (jsonParams.HasMember("isSign") && jsonParams["isSign"].IsBool()) {
        isSign = jsonParams["isSign"].GetBool();
    }
    bool isCompress = false;
    if (jsonParams.HasMember("compress") && jsonParams["compress"].IsBool()) {
        isCompress = jsonParams["compress"].GetBool();
    }
    
    const BlockHeader bh = sync.getBlockchain().getBlock(hashOrNumber);
    CHECK(bh.blockNumber.has_value(), "block " + to_string(hashOrNumber) + " not found");
    const std::string res = genDumpBlockBinary(sync.getBlockDump(bh, fromByte, toByte, isHex, isSign), isCompress);
    
    CHECK(!res.empty(), "block " + to_string(hashOrNumber) + " not found");
    if (isHex) {
        return genBlockDumpJson(requestId, res, isFormat);
    } else {
        return res;
    }
}

static std::string getBlocks(const RequestId &requestId, const rapidjson::Document &doc, const Sync &sync, bool isFormat, const JsonVersion &version) {
    CHECK_USER(doc.HasMember("params") && doc["params"].IsObject(), "params field not found");
    const auto &jsonParams = doc["params"];

    int64_t countBlocks = 0;
    if (jsonParams.HasMember("countBlocks") && jsonParams["countBlocks"].IsInt()) {
        countBlocks = jsonParams["countBlocks"].GetInt();
    }
    int64_t beginBlock = 0;
    if (jsonParams.HasMember("beginBlock") && jsonParams["beginBlock"].IsInt()) {
        beginBlock = jsonParams["beginBlock"].GetInt();
    }

    BlockTypeInfo type = BlockTypeInfo::Simple;
    if (jsonParams.HasMember("type") && jsonParams["type"].IsString()) {
        const std::string typeString = jsonParams["type"].GetString();
        if (typeString == "simple") {
            type = BlockTypeInfo::Simple;
        } else if (typeString == "forP2P") {
            type = BlockTypeInfo::ForP2P;
        } else if (typeString == "small") {
            type = BlockTypeInfo::Small;
        } else {
            throwUserErr("Incorrect block type: " + typeString);
        }
    }

    bool isForward = false;
    if (jsonParams.HasMember("direction") && jsonParams["direction"].IsString()) {
        isForward = jsonParams["direction"].GetString() == std::string("forward");
    }

    std::vector<BlockHeader> bhs;
    bhs.reserve(countBlocks);

    const auto processBlock = [&bhs, &sync, type](int64_t i) {
        bhs.emplace_back(sync.getBlockchain().getBlock(i));
    };

    if (!isForward) {
        beginBlock = sync.getBlockchain().countBlocks() - beginBlock;
        for (int64_t i = beginBlock; i > beginBlock - countBlocks && i > 0; i--) {
            processBlock(i);
        }
    } else {
        const int64_t maxBlockNum = sync.getBlockchain().countBlocks();
        if (countBlocks == 0) {
            countBlocks = maxBlockNum;
        }
        for (int64_t i = beginBlock; i < std::min(maxBlockNum + 1, beginBlock + countBlocks); i++) {
            processBlock(i);
        }
    }

    return blockHeadersToJson(requestId, bhs, type, isFormat, version);
}

template<typename T>
std::string getBlockDumps(const rapidjson::Document &doc, const RequestId &requestId, const std::string nameParam, const Sync &sync) {
    CHECK_USER(doc.HasMember("params") && doc["params"].IsObject(), "params field not found");
    const auto &jsonParams = doc["params"];
    bool isSign = false;
    if (jsonParams.HasMember("isSign") && jsonParams["isSign"].IsBool()) {
        isSign = jsonParams["isSign"].GetBool();
    }
    bool isCompress = false;
    if (jsonParams.HasMember("compress") && jsonParams["compress"].IsBool()) {
        isCompress = jsonParams["compress"].GetBool();
    }
    CHECK_USER(jsonParams.HasMember(nameParam.c_str()) && jsonParams[nameParam.c_str()].IsArray(), "hashes field not found");
    const auto &jsonVals = jsonParams[nameParam.c_str()].GetArray();
    std::vector<std::string> result;
    for (const auto &jsonVal: jsonVals) {
        const T &hashOrNumber = getJsonField<T>(jsonVal);

        const size_t fromByte = 0;
        const size_t toByte = std::numeric_limits<size_t>::max();

        const BlockHeader bh = sync.getBlockchain().getBlock(hashOrNumber);
        CHECK(bh.blockNumber.has_value(), "block " + to_string(hashOrNumber) + " not found");
        const std::string res = sync.getBlockDump(bh, fromByte, toByte, false, isSign);

        CHECK(!res.empty(), "block " + to_string(hashOrNumber) + " not found");
        result.emplace_back(res);
    }
    return genDumpBlocksBinary(result, isCompress);
}

static std::string signTestString(const std::string &strBinary, bool isHex, const RequestId &requestId, const Sync &sync) {
    const std::string answer = sync.signTestString(strBinary, isHex);
    if (!isHex) {
        return answer;
    } else {
        return genTestSignStringJson(requestId, answer);
    }
}

bool Server::run(int thread_number, Request& mhd_req, Response& mhd_resp) {
    mhd_resp.headers["Access-Control-Allow-Origin"] = "*";
    
    if (isStoped.load()) {
        return false;
    }
    
    IncCountRunningThread incCountRunningThreads(countRunningThreads);
    try {
        checkStopSignal();
    } catch (const StopException &e) {
        isStoped = true;
        return false;
    } catch (...) {
        throw;
    }
    
    const std::string &url = mhd_req.url;
    const std::string &method = mhd_req.method;
    RequestId requestId;
    
    std::string func;
    try {
        std::string jsonRequest;
        
        if (url == "/status") {            
            rapidjson::Document jsonDoc(rapidjson::kObjectType);
            auto &allocator = jsonDoc.GetAllocator();
            jsonDoc.AddMember("result", strToJson("ok", allocator), allocator);
            jsonDoc.AddMember("version", strToJson(VERSION, allocator), allocator);
            jsonDoc.AddMember("git_hash", strToJson(g_GIT_SHA1, allocator), allocator);
            //jsonDoc.AddMember("is_virtual", sync.isVirtualMachine(), allocator);
            mhd_resp.data = jsonToString(jsonDoc, false);
            mhd_resp.code = HTTP_STATUS_OK;
            
            LOGDEBUG << "get status ok";
            return true;
        } else if (url == "/" + SIGN_TEST_STRING) {
            if (!mhd_req.post.empty()) {
                mhd_resp.data = signTestString(mhd_req.post, false, requestId, sync);
                mhd_resp.code = HTTP_STATUS_OK;
            } else {
                mhd_resp.code = HTTP_STATUS_BAD_REQUEST;
            }
            return true;
        }
        
        if (method == "POST") {
            if (!mhd_req.post.empty()) {
                jsonRequest = mhd_req.post;
            }
        }
                
        rapidjson::Document doc;
        const rapidjson::ParseResult pr = doc.Parse(jsonRequest.c_str());
        CHECK(pr, "rapidjson parse error. Data: " + jsonRequest);
        
        if (url.size() > 1) {
            func = url.substr(1);        
        } else {
            CHECK_USER(doc.HasMember("method") && doc["method"].IsString(), "method field not found");
            func = doc["method"].GetString();
        }
                
        if (doc.HasMember("id") && doc["id"].IsString()) {
            requestId.id = doc["id"].GetString();
            requestId.isSet = true;
        }
        if (doc.HasMember("id") && doc["id"].IsInt64()) {
            requestId.id = doc["id"].GetInt64();
            requestId.isSet = true;
        }
        
        JsonVersion jsonVersion = JsonVersion::V1;
        if (doc.HasMember("version") && doc["version"].IsString()) {
            const std::string jsonVersionString = doc["version"].GetString();
            if (jsonVersionString == "v1" || jsonVersionString == "version1") {
                jsonVersion = JsonVersion::V1;
            } else if (jsonVersionString == "v2" || jsonVersionString == "version2") {
                jsonVersion = JsonVersion::V2;
            }
        }
        
        bool isFormatJson = false;
        if (doc.HasMember("pretty") && doc["pretty"].IsBool()) {
            isFormatJson = doc["pretty"].GetBool();
        }
        
        std::string response;
        
        if (func == GET_BLOCK_BY_HASH) {
            response = getBlock<std::string>(requestId, doc, "hash", sync, isFormatJson, jsonVersion);
        } else if (func == GET_BLOCK_BY_NUMBER) {
            response = getBlock<size_t>(requestId, doc, "number", sync, isFormatJson, jsonVersion);
        } else if (func == GET_BLOCKS) {
            response = getBlocks(requestId, doc, sync, isFormatJson, jsonVersion);
        } else if (func == GET_DUMP_BLOCK_BY_HASH) {
            response = getBlockDump<std::string>(doc, requestId, "hash", sync, isFormatJson);
        } else if (func == GET_DUMP_BLOCK_BY_NUMBER) {
            response = getBlockDump<size_t>(doc, requestId, "number", sync, isFormatJson);
        } else if (func == GET_DUMPS_BLOCKS_BY_HASH) {
            response = getBlockDumps<std::string>(doc, requestId, "hashes", sync);
        } else if (func == GET_DUMPS_BLOCKS_BY_NUMBER) {
            response = getBlockDumps<size_t>(doc, requestId, "numbers", sync);
        } else if (func == GET_COUNT_BLOCKS) {
            const size_t countBlocks = sync.getBlockchain().countBlocks();
            
            response = genCountBlockJson(requestId, countBlocks, isFormatJson, jsonVersion);
        } else if (func == SIGN_TEST_STRING) {
            CHECK_USER(doc.HasMember("params") && doc["params"].IsObject(), "params field not found");
            const auto &jsonParams = doc["params"];
            CHECK_USER(jsonParams.HasMember("data") && jsonParams["data"].IsString(), "data field not found");
            const std::string &data = jsonParams["data"].GetString();
            
            const auto dataBin = fromHex(data);
            response = signTestString(std::string(dataBin.begin(), dataBin.end()), true, requestId, sync);
        } else {
            throwUserErr("Incorrect func " + func);
        }
        
        mhd_resp.data = response;
        mhd_resp.code = HTTP_STATUS_OK;
    } catch (const exception &e) {
        LOGERR << e;
        mhd_resp.data = genErrorResponse(requestId, -32603, e);
        mhd_resp.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    } catch (const UserException &e) {
        LOGERR << e.exception;
        mhd_resp.data = genErrorResponse(requestId, -32602, e.exception + ". Url: " + url);
        mhd_resp.code = HTTP_STATUS_BAD_REQUEST;
    } catch (const std::exception &e) {
        LOGERR << e.what();
        mhd_resp.data = genErrorResponse(requestId, -32603, e.what());
        mhd_resp.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    } catch (...) {
        LOGERR << "Unknown error";
        mhd_resp.data = genErrorResponse(requestId, -32603, "Unknown error");
        mhd_resp.code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
    
    return true;
}

bool Server::init() {
    LOGINFO << "Port " << port;

    const int COUNT_THREADS = 8;
    
    countRunningThreads = 0;
    
    set_threads(COUNT_THREADS);
    set_port(port);

    return true;
}
