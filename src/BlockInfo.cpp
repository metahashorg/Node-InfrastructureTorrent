#include "BlockInfo.h"

#include "check.h"
#include "utils/serialize.h"
#include "convertStrings.h"
#include "stringUtils.h"
#include <log.h>

using namespace common;

namespace torrent_node_lib {

const std::string Address::INITIAL_WALLET_TRANSACTION = "InitialWalletTransaction";

const static uint64_t BLOCK_TYPE = 0xEFCDAB8967452301;
const static uint64_t BLOCK_TYPE_COMMON = 0x0000000067452301;
const static uint64_t BLOCK_TYPE_STATE = 0x1100000067452301;
const static uint64_t BLOCK_TYPE_FORGING = 0x2200000067452301;

const static uint64_t BLOCK_TYPE_COMMON_2 =  0x0001000067452301;
const static uint64_t BLOCK_TYPE_STATE_2 =   0x1101000067452301;
const static uint64_t BLOCK_TYPE_FORGING_2 = 0x2201000067452301;

const static uint64_t TX_STATE_APPROVE = 1; //block approve transaction
const static uint64_t TX_STATE_ACCEPT = 20; // transaction accepted (data & move)
const static uint64_t TX_STATE_WRONG_MONEY = 30; // transaction not accepted (insuficent funds)
const static uint64_t TX_STATE_WRONG_DATA = 40; // transaction not accepted (data method rejected)
const static uint64_t TX_STATE_FORGING = 100; // forging transaction
const static uint64_t TX_STATE_FORGING_W = 101; // wallet forging transaction
const static uint64_t TX_STATE_FORGING_N = 102; // node forging transaction
const static uint64_t TX_STATE_FORGING_C = 103; // coin forging transaction
const static uint64_t TX_STATE_FORGING_A = 104; // ???
const static uint64_t TX_STATE_STATE = 200; // state block transaction
const static uint64_t TX_STATE_TECH_NODE_STAT = 0x1101;

std::string FilePosition::serialize() const {
    //CHECK(!fileName.empty(), "Not initialized");
    
    std::string res;
    res.reserve(50);
    res += serializeString(fileName);
    res += serializeInt<size_t>(pos);
    return res;
}

void FilePosition::serialize(std::vector<char>& buffer) const {
    serializeString(fileName, buffer);
    serializeInt<size_t>(pos, buffer);
}

FilePosition FilePosition::deserialize(const std::string& raw, size_t from, size_t &nextFrom) {
    FilePosition result;
    
    result.fileName = deserializeString(raw, from, nextFrom);
    CHECK(nextFrom != from, "Incorrect raw ");
    from = nextFrom;
    
    result.pos = deserializeInt<size_t>(raw, from, nextFrom);
    CHECK(nextFrom != from, "Incorrect raw ");
    from = nextFrom;
    
    return result;
}

FilePosition FilePosition::deserialize(const std::string& raw, size_t& from) {
    size_t endPos = from;
    const FilePosition res = FilePosition::deserialize(raw, from, endPos);
    CHECK(endPos != from, "Incorrect raw");
    from = endPos;
    return res;
}

FilePosition FilePosition::deserialize(const std::string& raw) {
    size_t tmp;
    return deserialize(raw, 0, tmp);
}

void TransactionInfo::serialize(std::vector<char>& buffer) const {
    buffer.clear();
    filePos.serialize(buffer);
    serializeInt<size_t>(blockNumber, buffer);
}

void TransactionInfo::calcRealFee() {
    CHECK(sizeRawTx > 0, "Size raw tx not set");
    realFees = sizeRawTx > 255 ? sizeRawTx - 255 : 0;
}

bool TransactionInfo::isIntStatusNoBalance() const {
    return intStatus.has_value() && intStatus.value() == TX_STATE_WRONG_MONEY;
}

bool TransactionInfo::isIntStatusNotSuccess() const {
    return intStatus.has_value() && (intStatus.value() == TX_STATE_WRONG_MONEY || intStatus.value() == TX_STATE_WRONG_DATA);
}

std::set<uint64_t> TransactionInfo::getForgingIntStatuses() {
    return {TX_STATE_FORGING, TX_STATE_FORGING_C, TX_STATE_FORGING_N, TX_STATE_FORGING_W, TX_STATE_FORGING_A};
}

bool TransactionInfo::isIntStatusForging() const {
    const auto set = TransactionInfo::getForgingIntStatuses();
    return intStatus.has_value() && set.find(intStatus.value()) != set.end();
}

bool TransactionInfo::isIntStatusNodeTest() const {
    return intStatus.has_value() && intStatus.value() == TX_STATE_TECH_NODE_STAT;
}

TransactionInfo TransactionInfo::deserialize(const std::string& raw, size_t &from) {
    TransactionInfo result;
    
    result.filePos = FilePosition::deserialize(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    
    return result;
}

TransactionInfo TransactionInfo::deserialize(const std::string& raw) {
    size_t from = 0;
    return deserialize(raw, from);
}

void TransactionStatus::UnDelegate::serialize(std::vector<char>& buffer) const {
    serializeInt<uint64_t>(value, buffer);
    serializeString(delegateHash, buffer);
}

TransactionStatus::UnDelegate TransactionStatus::UnDelegate::deserialize(const std::string &raw, size_t &fromPos) {
    TransactionStatus::UnDelegate result;
    result.value = deserializeInt<uint64_t>(raw, fromPos);
    result.delegateHash = deserializeString(raw, fromPos);
    return result;
}

void TransactionStatus::V8Status::serialize(std::vector<char>& buffer) const {
    serializeInt<uint8_t>(isServerError, buffer);
    serializeInt<uint8_t>(isScriptError, buffer);
    if (!compiledContractAddress.isEmpty()) {
        serializeString(compiledContractAddress.getBinaryString(), buffer);
    } else {
        serializeString("", buffer);
    }
}

TransactionStatus::V8Status TransactionStatus::V8Status::deserialize(const std::string &raw, size_t &fromPos) {
    TransactionStatus::V8Status result;
    result.isServerError = deserializeInt<uint8_t>(raw, fromPos);
    result.isScriptError = deserializeInt<uint8_t>(raw, fromPos);
    const std::string addressString = deserializeString(raw, fromPos);
    if (!addressString.empty()) {
        result.compiledContractAddress = Address(addressString.begin(), addressString.end());
    } else {
        result.compiledContractAddress.setEmpty();
    }
    return result;
}

template<size_t I>
void TransactionStatus::tryParse(const std::string &raw, size_t &fromPos, size_t number) {
    if (I == number) {
        using TypeElement = std::decay_t<decltype(std::get<I>(status))>;
        if constexpr (!std::is_same_v<TypeElement, std::monostate>) {
            status = TypeElement::deserialize(raw, fromPos);
        }
    }
}

template <std::size_t ... I>
void TransactionStatus::parseVarint(const std::string &raw, size_t &fromPos, size_t number, std::index_sequence<I ... >) {    
    (tryParse<I>(raw, fromPos, number), ...);
}

void TransactionStatus::serialize(std::vector<char>& buffer) const {
    buffer.clear();
    serializeInt<uint8_t>(isSuccess, buffer);
    serializeInt<size_t>(blockNumber, buffer);
    serializeString(transaction, buffer);
    CHECK(status.index() != 0, "Status not set");
    serializeInt<uint64_t>(status.index(), buffer);
    std::visit([&buffer](auto &&arg){
        if constexpr (!std::is_same_v<std::decay_t<decltype(arg)>, std::monostate>) {
            arg.serialize(buffer);
        }
    }, status);
}

TransactionStatus TransactionStatus::deserialize(const std::string& raw) {
    TransactionStatus result;
    
    size_t from = 0;
    result.isSuccess = deserializeInt<uint8_t>(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.transaction = deserializeString(raw, from);
    constexpr size_t varintSize = std::variant_size_v<std::decay_t<decltype(result.status)>>;
    const size_t number = deserializeInt<size_t>(raw, from);
    CHECK(number != 0 && number < varintSize, "Incorrect type in transaction status");
    result.parseVarint(raw, from, number, std::make_index_sequence<varintSize>{});
    
    return result;
}

void AddressInfo::serialize(std::vector<char>& buffer) const {
    buffer.clear();
    filePos.serialize(buffer);
    serializeInt<size_t>(blockNumber, buffer);
    if (undelegateValue.has_value()) {
        serializeInt<uint64_t>(undelegateValue.value(), buffer);
    }
}

AddressInfo AddressInfo::deserialize(const std::string& raw) {
    AddressInfo result;
    
    size_t from = 0;
    result.filePos = FilePosition::deserialize(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    if (from < raw.size()) {
        result.undelegateValue = deserializeInt<uint64_t>(raw, from);
    }
       
    return result;
}

bool BlockHeader::isStateBlock() const {
    return blockType == BLOCK_TYPE_STATE || blockType == BLOCK_TYPE_STATE_2;
}

bool BlockHeader::isSimpleBlock() const {
    return blockType == BLOCK_TYPE || blockType == BLOCK_TYPE_COMMON || blockType == BLOCK_TYPE_COMMON_2;
}

bool BlockHeader::isForgingBlock() const {
    return blockType == BLOCK_TYPE_FORGING || blockType == BLOCK_TYPE_FORGING_2;
}

std::string BlockHeader::getBlockType() const {
    if (isStateBlock()) {
        return "state";
    } else if (isSimpleBlock()) {
        return "block";
    } else if (isForgingBlock()) {
        return "forging";
    } else {
        throwErr("Unknown block type " + std::to_string(blockType));
    }
}

std::string BlockHeader::serialize() const {
    CHECK(!hash.empty(), "empty hash");
    CHECK(!prevHash.empty(), "empty prevHash");
    
    std::string res;
    res += filePos.serialize();
    res += serializeString(prevHash);
    res += serializeString(hash);
    res += serializeString(txsHash);
    res += serializeString(std::string(signature.begin(), signature.end()));
    res += serializeInt<uint64_t>(blockSize);
    res += serializeInt<uint64_t>(blockType);
    res += serializeInt<size_t>(timestamp);
    CHECK(countTxs.has_value(), "Count txs not set");
    res += serializeInt<size_t>(countTxs.value());
    
    res += serializeString(std::string(senderSign.begin(), senderSign.end()));
    res += serializeString(std::string(senderPubkey.begin(), senderPubkey.end()));
    res += serializeString(std::string(senderAddress.begin(), senderAddress.end()));
    
    std::vector<char> buffer;
    for (const TransactionInfo &tx: blockSignatures) {
        res += serializeInt<unsigned char>('s');
        
        buffer.clear();
        tx.serialize(buffer);
        res.insert(res.end(), buffer.begin(), buffer.end());
    }
    return res;
}

BlockHeader BlockHeader::deserialize(const std::string& raw) {
    BlockHeader result;
    
    size_t from = 0;
    result.filePos = FilePosition::deserialize(raw, from);
    result.prevHash = deserializeString(raw, from);
    result.hash = deserializeString(raw, from);
    result.txsHash = deserializeString(raw, from);
    const std::string sign = deserializeString(raw, from);
    result.signature = std::vector<unsigned char>(sign.begin(), sign.end());
    result.blockSize = deserializeInt<uint64_t>(raw, from);
    result.blockType = deserializeInt<uint64_t>(raw, from);
    result.timestamp = deserializeInt<size_t>(raw, from);
    result.countTxs = deserializeInt<size_t>(raw, from);
    
    const std::string senderSign = deserializeString(raw, from);
    result.senderSign = std::vector<unsigned char>(senderSign.begin(), senderSign.end());
    const std::string senderPubkey = deserializeString(raw, from);
    result.senderPubkey = std::vector<unsigned char>(senderPubkey.begin(), senderPubkey.end());
    const std::string senderAddress = deserializeString(raw, from);
    result.senderAddress = std::vector<unsigned char>(senderAddress.begin(), senderAddress.end());
    
    while (from < raw.size()) {
        const unsigned char nextType = deserializeInt<unsigned char>(raw, from);
        if (nextType == 's') {
            const TransactionInfo info = TransactionInfo::deserialize(raw, from);
            result.blockSignatures.emplace_back(info);
        } else {
            throwErr("Incorrect type " + std::to_string(nextType));
        }
    }
    return result;
}

std::string BlocksMetadata::serialize() const {
    CHECK(!blockHash.empty(), "Incorrect metadata");
    CHECK(!prevBlockHash.empty(), "Incorrect metadata");
    
    std::string res;
    res += serializeString(blockHash);
    res += serializeString(prevBlockHash);
    res += serializeInt<size_t>(blockNumber);
    return res;
}

BlocksMetadata BlocksMetadata::deserialize(const std::string& raw) {
    BlocksMetadata result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.blockHash = deserializeString(raw, from);
    result.prevBlockHash = deserializeString(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    return result;
}

std::string MainBlockInfo::serialize() const {
    CHECK(!blockHash.empty(), "Incorrect metadata");
    
    std::string res;
    res += serializeString(blockHash);
    res += serializeInt<size_t>(blockNumber);
    res += serializeInt<size_t>(countVal);
    return res;
}

MainBlockInfo MainBlockInfo::deserialize(const std::string& raw) {
    MainBlockInfo result;
    
    if (raw.empty()) {
        return result;
    }
    
    size_t from = 0;
    result.blockHash = deserializeString(raw, from);
    result.blockNumber = deserializeInt<size_t>(raw, from);
    result.countVal = deserializeInt<size_t>(raw, from);
    return result;
}

std::string FileInfo::serialize() const {
    return filePos.serialize();
}

FileInfo FileInfo::deserialize(const std::string& raw) {
    FileInfo result;
    
    result.filePos = FilePosition::deserialize(raw);
    
    return result;
}

size_t getMaxBlockNumber(const std::vector<TransactionInfo> &infos) {
    if (!infos.empty()) {
        const TransactionInfo &first = infos.front();
        const TransactionInfo &last = infos.back();
        const size_t maxBlockNum = std::max(first.blockNumber, last.blockNumber); // хз уже помнит, где находится последняя транзакция, спереди или сзади, так что поверим обе
        return maxBlockNum;
    } else {
        return 0;
    }
}

size_t getMaxBlockNumber(const std::vector<TransactionStatus> &infos) {
    if (!infos.empty()) {
        const TransactionStatus &first = infos.front();
        const TransactionStatus &last = infos.back();
        const size_t maxBlockNum = std::max(first.blockNumber, last.blockNumber); // хз уже помнит, где находится последняя транзакция, спереди или сзади, так что поверим обе
        return maxBlockNum;
    } else {
        return 0;
    }
}

template<>
bool isGreater<std::vector<TransactionInfo>>(const std::vector<TransactionInfo> &infos, size_t blockNumber) {
    if (infos.empty()) {
        return false;
    }
    return getMaxBlockNumber(infos) > blockNumber;
}

template<>
bool isGreater<std::vector<TransactionStatus>>(const std::vector<TransactionStatus> &infos, size_t blockNumber) {
    if (infos.empty()) {
        return false;
    }
    return getMaxBlockNumber(infos) > blockNumber;
}

template Address::Address<const char*>(const char*, const char*);
template Address::Address<std::string::const_iterator>(std::string::const_iterator, std::string::const_iterator);

}
