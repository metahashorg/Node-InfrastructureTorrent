#include "NodeTestsBlockInfo.h"

#include "utils/serialize.h"
#include "check.h"

namespace torrent_node_lib {
    
std::string NodeTestType::serialize() const {
    std::string res;
    uint8_t typeInt = 0;
    if (type == Type::torrent) {
        typeInt = 1;
    } else if (type == Type::proxy) {
        typeInt = 2;
    } else {
        throwErr("Incorrect nodeTestType");
    }
    res += serializeInt(typeInt);
    return res;
}

torrent_node_lib::NodeTestType NodeTestType::deserialize(const std::string& raw, size_t &from) {
    NodeTestType result;
    
    if (from >= raw.size()) {
        return result;
    }
    
    const uint8_t typeInt = deserializeInt<uint8_t>(raw, from);
    if (typeInt == 1) {
        result.type = Type::torrent;
    } else if (typeInt == 2) {
        result.type = Type::proxy;
    } else {
        throwErr("Incorrect type " + std::to_string(typeInt));
    }
    
    return result;
}

std::string NodeTestResult::serialize() const {
    std::string res;
    res += serializeString(std::string(result.begin(), result.end()));
    res += serializeInt(timestamp);
    res += type.serialize();
    res += serializeString(ip);
    res += serializeInt(day);
    return res;
}

torrent_node_lib::NodeTestResult NodeTestResult::deserialize(const std::string& raw) {
    NodeTestResult result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.result = deserializeString(raw, from);
    result.timestamp = deserializeInt<size_t>(raw, from);
    result.type = NodeTestType::deserialize(raw, from);
    result.ip = deserializeString(raw, from);
    result.day = deserializeInt<size_t>(raw, from);
    
    return result;
}

std::string NodeTestTrust::serialize() const {
    std::string res;
    res += serializeString(trustJson);
    res += serializeInt(timestamp);
    res += serializeInt<size_t>(trust);
    return res;
}

torrent_node_lib::NodeTestTrust NodeTestTrust::deserialize(const std::string& raw) {
    NodeTestTrust result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.trustJson = deserializeString(raw, from);
    result.timestamp = deserializeInt<size_t>(raw, from);
    result.trust = deserializeInt<size_t>(raw, from);
    
    return result;
}

size_t NodeTestCount::countSuccess() const {
    return countAll - countFailure;
}

std::string NodeTestCount::serialize() const {
    std::string res;
    res += serializeInt(countAll);
    res += serializeInt(countFailure);
    res += serializeInt(day);
    
    res += serializeInt(testers.size());
    for (const Address &tester: testers) {
        res += serializeString(tester.toBdString());
    }
    return res;
}

torrent_node_lib::NodeTestCount NodeTestCount::deserialize(const std::string& raw) {
    if (raw.empty()) {
        return NodeTestCount(0);
    }
    NodeTestCount result;
    
    size_t pos = 0;
    
    result.countAll = deserializeInt<size_t>(raw, pos);
    result.countFailure = deserializeInt<size_t>(raw, pos);
    result.day = deserializeInt<size_t>(raw, pos);
    
    const size_t countTesters = deserializeInt<size_t>(raw, pos);
    for (size_t i = 0; i < countTesters; i++) {
        const std::string addr = deserializeString(raw, pos);
        const std::vector<unsigned char> address(addr.begin(), addr.end());
        result.testers.insert(Address(address));
    }
    return result;
}

NodeTestCount& NodeTestCount::operator+=(const NodeTestCount &second) {
    countAll += second.countAll;
    countFailure += second.countFailure;
    testers.insert(second.testers.begin(), second.testers.end());
    day = std::max(day, second.day);
    return *this;
}

NodeTestCount operator+(const NodeTestCount &first, const NodeTestCount &second) {
    NodeTestCount result = first;
    result += second;
    return result;
}

std::string NodeStatBlockInfo::serialize() const {
    CHECK(!blockHash.empty(), "Incorrect metadata");
    
    std::string res;
    res += serializeString(blockHash);
    res += serializeInt<size_t>(blockNumber);
    res += serializeInt<size_t>(countVal);
    return res;
}

NodeStatBlockInfo NodeStatBlockInfo::deserialize(const std::string& raw) {
    NodeStatBlockInfo result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.blockHash = deserializeString(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.countVal = deserializeInt<size_t>(raw, from);
    return result;
}

std::string NodeTestDayNumber::serialize() const {
    std::string res;
    res += serializeInt(dayNumber);
    return res;
}

NodeTestDayNumber NodeTestDayNumber::deserialize(const std::string &raw) {
    if (raw.empty()) {
        return NodeTestDayNumber();
    }
    NodeTestDayNumber result;
    
    size_t pos = 0;
    
    result.dayNumber = deserializeInt<size_t>(raw, pos);
    return result;
}

std::string AllTestedNodes::serialize() const {
    std::string res;
    res += serializeInt(day);
    res += serializeInt(nodes.size());
    for (const std::string &node: nodes) {
        res += serializeString(node);
    }
    return res;
}

torrent_node_lib::AllTestedNodes AllTestedNodes::deserialize(const std::string& raw) {
    if (raw.empty()) {
        return AllTestedNodes(0);
    }
    AllTestedNodes result;
    
    size_t pos = 0;
    
    result.day = deserializeInt<size_t>(raw, pos);
    const size_t count = deserializeInt<size_t>(raw, pos);
    for (size_t i = 0; i < count; i++) {
        result.nodes.insert(deserializeString(raw, pos));
    }
    return result;
}

std::string AllNodes::serialize() const {
    std::string res;
    res += serializeInt(nodes.size());
    for (const auto &[node, name]: nodes) {
        res += serializeString(node);
        res += serializeString(name);
    }
    return res;
}

torrent_node_lib::AllNodes AllNodes::deserialize(const std::string& raw) {
    if (raw.empty()) {
        return AllNodes();
    }
    AllNodes result;
    
    size_t pos = 0;
    
    const size_t count = deserializeInt<size_t>(raw, pos);
    for (size_t i = 0; i < count; i++) {
        const std::string node = deserializeString(raw, pos);
        const std::string name = deserializeString(raw, pos);
        result.nodes[node] = name;
    }
    return result;
}

std::string NodeRps::serialize() const {
    std::string res;
    res += serializeInt(rps.size());
    for (const uint64_t &rp: rps) {
        res += serializeInt(rp);
    }
    return res;
}

torrent_node_lib::NodeRps NodeRps::deserialize(const std::string& raw) {
    if (raw.empty()) {
        return NodeRps();
    }
    NodeRps result;
    
    size_t pos = 0;
    
    const size_t count = deserializeInt<size_t>(raw, pos);
    for (size_t i = 0; i < count; i++) {
        result.rps.push_back(deserializeInt<uint64_t>(raw, pos));
    }
    return result;
}

}
