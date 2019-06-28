#include "generate_json.h"

#include <sstream>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "BlockChainReadInterface.h"

#include "jsonUtils.h"
#include "check.h"
#include "convertStrings.h"
#include "utils/serialize.h"
#include "utils/compress.h"

#include "BlockInfo.h"
#include "Workers/NodeTestsBlockInfo.h"

using namespace common;
using namespace torrent_node_lib;

static void addIdToResponse(const RequestId &requestId, rapidjson::Value &json, rapidjson::Document::AllocatorType &allocator) {
    if (requestId.isSet) {
        if (std::holds_alternative<std::string>(requestId.id)) {
            json.AddMember("id", strToJson(std::get<std::string>(requestId.id), allocator), allocator);
        } else {
            json.AddMember("id", std::get<size_t>(requestId.id), allocator);
        }
    }
}

template<typename Int>
static rapidjson::Value intOrString(Int intValue, bool isString, rapidjson::Document::AllocatorType &allocator) {
    if (isString) {
        return strToJson(std::to_string(intValue), allocator);
    } else {
        rapidjson::Value strVal;
        strVal.Set(intValue, allocator);
        return strVal;
    }
}

std::string genErrorResponse(const RequestId &requestId, int code, const std::string &error) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    addIdToResponse(requestId, jsonDoc, allocator);
    
    rapidjson::Value errorJson(rapidjson::kObjectType);
    errorJson.AddMember("code", code, allocator);
    errorJson.AddMember("message", strToJson(error, allocator), allocator);
    jsonDoc.AddMember("error", errorJson, allocator);
    return jsonToString(jsonDoc, false);
}

std::string genStatusResponse(const RequestId &requestId, const std::string &version, const std::string &gitHash) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    addIdToResponse(requestId, jsonDoc, allocator);
    
    jsonDoc.AddMember("result", strToJson("ok", allocator), allocator);
    jsonDoc.AddMember("version", strToJson(version, allocator), allocator);
    jsonDoc.AddMember("git_hash", strToJson(gitHash, allocator), allocator);
    return jsonToString(jsonDoc, false);
}

std::string genStatisticResponse(const RequestId &requestId, size_t statistic, double proc, unsigned long long int memory, int connections) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    addIdToResponse(requestId, jsonDoc, allocator);
    
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("request_stat", statistic, allocator);
    resultJson.AddMember("proc", proc, allocator);
    resultJson.AddMember("memory", strToJson(std::to_string(memory), allocator), allocator);
    resultJson.AddMember("connections", connections, allocator);
    jsonDoc.AddMember("result", resultJson, allocator);
    return jsonToString(jsonDoc, false);
}

std::string genStatisticResponse(size_t statistic) {
    rapidjson::Document jsonDoc(rapidjson::kObjectType);
    auto &allocator = jsonDoc.GetAllocator();
    jsonDoc.AddMember("result", statistic, allocator);
    return jsonToString(jsonDoc, false);
}

static rapidjson::Value blockHeaderToJson(const BlockHeader &bh, rapidjson::Document::AllocatorType &allocator, BlockTypeInfo type, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    
    CHECK(bh.blockNumber.has_value(), "Block header not set");
    rapidjson::Value resultValue(rapidjson::kObjectType);
    if (type == BlockTypeInfo::Simple) {
        resultValue.AddMember("type", strToJson(bh.getBlockType(), allocator), allocator);
    }
    resultValue.AddMember("hash", strToJson(bh.hash, allocator), allocator);
    resultValue.AddMember("prev_hash", strToJson(bh.prevHash, allocator), allocator);
    if (type == BlockTypeInfo::Simple) {
        resultValue.AddMember("tx_hash", strToJson(bh.txsHash, allocator), allocator);
    }
    resultValue.AddMember("number", intOrString(bh.blockNumber.value(), isStringValue, allocator), allocator);
    if (type == BlockTypeInfo::Simple) {
        resultValue.AddMember("timestamp", intOrString(bh.timestamp, isStringValue, allocator), allocator);
        CHECK(bh.countTxs.has_value(), "Count txs not set");
        resultValue.AddMember("count_txs", intOrString(bh.countTxs.value(), isStringValue, allocator), allocator);
        resultValue.AddMember("sign", strToJson(toHex(bh.signature), allocator), allocator);
    }
    if (type != BlockTypeInfo::Small) {
        resultValue.AddMember("size", bh.blockSize, allocator);
        resultValue.AddMember("fileName", strToJson(bh.filePos.fileName, allocator), allocator);
    }
    
    return resultValue;
}

std::string blockHeaderToJson(const RequestId &requestId, const BlockHeader &bh, const std::optional<std::reference_wrapper<const BlockHeader>> &nextBlock, bool isFormat, BlockTypeInfo type, const JsonVersion &version) {
    if (bh.blockNumber == 0) {
        return genErrorResponse(requestId, -32603, "Incorrect block number: 0. Genesis block begin with number 1");
    }
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    doc.AddMember("result", blockHeaderToJson(bh, allocator, type, version), allocator);
    return jsonToString(doc, isFormat);
}

std::string genCountBlockJson(const RequestId &requestId, size_t countBlocks, bool isFormat, const JsonVersion &version) {
    const bool isStringValue = version == JsonVersion::V2;
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("count_blocks", intOrString(countBlocks, isStringValue, allocator), allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string genBlockDumpJson(const RequestId &requestId, const std::string &blockDump, bool isFormat) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultValue(rapidjson::kObjectType);
    resultValue.AddMember("dump", strToJson(blockDump, allocator), allocator);
    doc.AddMember("result", resultValue, allocator);
    return jsonToString(doc, isFormat);
}

std::string genTestSignStringJson(const RequestId &requestId, const std::string &responseHex) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value resultJson(rapidjson::kObjectType);
    resultJson.AddMember("data", strToJson(responseHex, allocator), allocator);
    doc.AddMember("result", resultJson, allocator);
    return jsonToString(doc, false);
}

static MinimumBlockHeader parseBlockHeader(const rapidjson::Value &resultJson) {    
    MinimumBlockHeader result;
    CHECK(resultJson.HasMember("number") && resultJson["number"].IsInt64(), "number field not found");
    result.number = resultJson["number"].GetInt64();
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

MinimumBlockHeader parseBlockHeader(const std::string &response) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(response.c_str());
    CHECK(pr, "rapidjson parse error. Data: " + response);
    
    CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
    CHECK(doc.HasMember("result") && doc["result"].IsObject(), "result field not found");
    const auto &resultJson = doc["result"];
    
    return parseBlockHeader(resultJson);
}

std::vector<MinimumBlockHeader> parseBlocksHeader(const std::string &response) {
    rapidjson::Document doc;
    const rapidjson::ParseResult pr = doc.Parse(response.c_str());
    CHECK(pr, "rapidjson parse error. Data: " + response);
    
    CHECK(!doc.HasMember("error") || doc["error"].IsNull(), jsonToString(doc["error"], false));
    CHECK(doc.HasMember("result") && doc["result"].IsArray(), "result field not found");
    const auto &resultJson = doc["result"].GetArray();
    
    std::vector<MinimumBlockHeader> result;
    for (const auto &rJson: resultJson) {
        CHECK(rJson.IsObject(), "result field not found");
        result.emplace_back(parseBlockHeader(rJson));
    }
    
    return result;
}

std::string blockHeadersToJson(const RequestId &requestId, const std::vector<BlockHeader> &bh, BlockTypeInfo type, bool isFormat, const JsonVersion &version) {
    rapidjson::Document doc(rapidjson::kObjectType);
    auto &allocator = doc.GetAllocator();
    addIdToResponse(requestId, doc, allocator);
    rapidjson::Value vals(rapidjson::kArrayType);
    for (size_t i = 0; i < bh.size(); i++) {
        const BlockHeader &b  = bh[i];

        if (b.blockNumber == 0) {
            return genErrorResponse(requestId, -32603, "Incorrect block number: 0. Genesis block begin with number 1");
        }
        vals.PushBack(blockHeaderToJson(b, allocator, type, version), allocator);
    }
    doc.AddMember("result", vals, allocator);
    return jsonToString(doc, isFormat);
}

std::string genDumpBlockBinary(const std::string &block, bool isCompress) {
    if (!isCompress) {
        return block;
    } else {
        return compress(block);
    }
}

std::string genDumpBlocksBinary(const std::vector<std::string> &blocks, bool isCompress) {
    std::string res;
    if (!blocks.empty()) {
        res.reserve((8 + blocks[0].size() + 10) * blocks.size());
    }
    for (const std::string &block: blocks) {
        res += serializeStringBigEndian(block);
    }
    if (!isCompress) {
        return res;
    } else {
        return compress(res);
    }
}

std::string parseDumpBlockBinary(const std::string &response, bool isCompress) {
    if (!isCompress) {
        return response;
    } else {
        return decompress(response);
    }
}

std::vector<std::string> parseDumpBlocksBinary(const std::string &response, bool isCompress) {
    std::vector<std::string> res;
    const std::string r = isCompress ? decompress(response) : response;
    size_t from = 0;
    while (from < r.size()) {
        res.emplace_back(deserializeStringBigEndian(r, from));
    }
    return res;
}
