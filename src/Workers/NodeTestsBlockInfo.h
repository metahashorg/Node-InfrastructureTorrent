#ifndef NODE_TESTS_BLOCK_INFO_H_
#define NODE_TESTS_BLOCK_INFO_H_

#include <string>
#include <vector>
#include <set>
#include <map>

#include "Address.h"

namespace torrent_node_lib {
    
struct NodeTestType {
    enum class Type {
        unknown, torrent, proxy
    };
    
    Type type = Type::unknown;
    
    NodeTestType() = default;
    
    NodeTestType(Type type)
        : type(type)
    {}
    
    std::string serialize() const;
    
    static NodeTestType deserialize(const std::string &raw, size_t &from);
    
};

struct NodeTestResult {
    std::string result;
    size_t timestamp = 0;
    NodeTestType type;
    std::string ip;
    size_t day = 0;
    
    uint64_t avgRps = 0;
    
    NodeTestResult() = default;
    
    NodeTestResult(const std::string &result, size_t timestamp, NodeTestType type, const std::string &ip, size_t day)
        : result(result)
        , timestamp(timestamp)
        , type(type)
        , ip(ip)
        , day(day)
    {}
    
    std::string serialize() const;
    
    static NodeTestResult deserialize(const std::string &raw);
    
};

struct NodeTestTrust {
    std::string trustJson;
    
    size_t timestamp = 0;
    
    int trust = 1; // default
    
    NodeTestTrust() = default;
    
    NodeTestTrust(size_t blockNumber, const std::vector<unsigned char> &trustJson, int trust)
        : trustJson(trustJson.begin(), trustJson.end())
        , timestamp(blockNumber)
        , trust(trust)
    {}
    
    std::string serialize() const;
    
    static NodeTestTrust deserialize(const std::string &raw);
    
};

struct NodeTestCount {
    
    size_t countAll = 0;
    
    size_t countFailure = 0;
    
    size_t day = 0;
    
    std::set<Address> testers;
    
    NodeTestCount() = default;
    
    NodeTestCount(size_t day)
        : day(day)
    {}
    
    size_t countSuccess() const;
    
    std::string serialize() const;
    
    static NodeTestCount deserialize(const std::string &raw);
    
    NodeTestCount& operator+=(const NodeTestCount &second);
    
};

NodeTestCount operator+(const NodeTestCount &first, const NodeTestCount &second);

struct NodeStatBlockInfo {
    size_t blockNumber = 0;
    std::string blockHash;
    size_t countVal = 0;
    
    NodeStatBlockInfo() = default;
    
    NodeStatBlockInfo(size_t blockNumber, const std::string &blockHash, size_t countVal)
        : blockNumber(blockNumber)
        , blockHash(blockHash)
        , countVal(countVal)
    {}
    
    std::string serialize() const;
    
    static NodeStatBlockInfo deserialize(const std::string &raw);
    
};

struct NodeTestExtendedStat {
    
    NodeTestCount count;
    
    NodeTestType type;
    
    std::string ip;
    
    NodeTestExtendedStat() = default;
    
    NodeTestExtendedStat(const NodeTestCount &count, const NodeTestType &type, const std::string &ip)
        : count(count)
        , type(type)
        , ip(ip)
    {}
    
};

struct NodeTestDayNumber {
    size_t dayNumber = 0;
    
    std::string serialize() const;
    
    static NodeTestDayNumber deserialize(const std::string &raw);
    
};

struct AllTestedNodes {
    
    std::set<std::string> nodes;
    
    size_t day = 0;
    
    AllTestedNodes() = default;
    
    AllTestedNodes(size_t day)
        : day(day)
    {}
    
    std::string serialize() const;
    
    static AllTestedNodes deserialize(const std::string &raw);
    
};

struct AllNodes {
    
    std::map<std::string, std::string> nodes;
    
    AllNodes() = default;
    
    std::string serialize() const;
    
    static AllNodes deserialize(const std::string &raw);
    
};

struct NodeRps {
    
    std::vector<uint64_t> rps;
    
    std::string serialize() const;
    
    static NodeRps deserialize(const std::string &raw);
    
};
}
    
#endif // NODE_TESTS_BLOCK_INFO_H_
