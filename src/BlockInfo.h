#ifndef BLOCK_INFO_H_
#define BLOCK_INFO_H_

#include <string>
#include <vector>
#include <optional>
#include <variant>
#include <set>
#include <unordered_map>

#include "duration.h"

#include "Address.h"

namespace torrent_node_lib {

struct FilePosition {
    size_t pos;
    std::string fileName;
    
    FilePosition() = default;
    
    FilePosition(const std::string &fileName, size_t pos)
        : pos(pos)
        , fileName(fileName)
    {}
    
    std::string serialize() const;
    
    void serialize(std::vector<char> &buffer) const;
    
    static FilePosition deserialize(const std::string &raw, size_t from, size_t &nextFrom);
    
    static FilePosition deserialize(const std::string &raw, size_t &from);
    
    static FilePosition deserialize(const std::string &raw);
    
};

struct AddressInfo {
    
    AddressInfo() = default;
    
    AddressInfo(size_t pos, const std::string &fileName, size_t blockNumber)
        : filePos(fileName, pos)
        , blockNumber(blockNumber)
    {}
    
    FilePosition filePos;
    
    size_t blockNumber = 0;
    
    std::optional<int64_t> undelegateValue;
    
    void serialize(std::vector<char> &buffer) const;
    
    static AddressInfo deserialize(const std::string &raw);
};

struct TransactionStatus {
    TransactionStatus(const std::string &txHash, size_t blockNumber)
        : transaction(txHash)
        , blockNumber(blockNumber)
    {}
    
    struct Delegate {
        void serialize(std::vector<char> &buffer) const {}
        static Delegate deserialize(const std::string &raw, size_t &fromPos) {
            return Delegate();
        }
    };
       
    struct UnDelegate {
        int64_t value;
        std::string delegateHash;
        
        UnDelegate() = default;
        
        UnDelegate(int64_t value, const std::string &delegateHash)
            : value(value)
            , delegateHash(delegateHash)
        {}
        
        void serialize(std::vector<char> &buffer) const;
        static UnDelegate deserialize(const std::string &raw, size_t &fromPos);
    };
    
    struct V8Status {
        bool isServerError = false;
        bool isScriptError = false;
        
        Address compiledContractAddress;
        
        void serialize(std::vector<char> &buffer) const;
        static V8Status deserialize(const std::string &raw, size_t &fromPos);
    };
    
    bool isSuccess;
    
    std::string transaction;
    
    size_t blockNumber;
    
    std::variant<std::monostate, Delegate, UnDelegate, V8Status> status;
    
    void serialize(std::vector<char> &buffer) const;
    
    static TransactionStatus deserialize(const std::string &raw);
    
private:
    
    TransactionStatus() = default;
    
    template <std::size_t ... I>
    void parseVarint(const std::string &raw, size_t &fromPos, size_t number, std::index_sequence<I ... >);
    
    template<size_t I>
    void tryParse(const std::string &raw, size_t &fromPos, size_t number);
};

struct TransactionInfo {
    struct DelegateInfo {
        int64_t value;
        bool isDelegate;
    };
    
    struct ScriptInfo {
        std::string txRaw;
        bool isInitializeScript;
    };
    
    std::string hash;
    Address fromAddress;
    Address toAddress;
    int64_t value;
    int64_t fees = 0;
    uint64_t nonce = 0;
    size_t blockNumber = 0;
    size_t sizeRawTx = 0;
    bool isSaveToBd = true;
    
    bool isSignBlockTx = false;
        
    std::optional<uint64_t> intStatus;
    
    std::vector<char> sign;
    std::vector<unsigned char> pubKey;
    
    std::vector<unsigned char> data;
    
    std::string allRawTx;
    
    std::optional<int64_t> realFees;
    
    std::optional<DelegateInfo> delegate;
    
    std::optional<ScriptInfo> scriptInfo;
    
    FilePosition filePos;
    
    std::optional<TransactionStatus> status;
    
    bool isModuleNotSet = false;
    
    bool isInitialized = false;
    
    void serialize(std::vector<char> &buffer) const;
    
    static TransactionInfo deserialize(const std::string &raw);
    
    static TransactionInfo deserialize(const std::string &raw, size_t &from);
    
    void calcRealFee();
    
    bool isStatusNeed() const {
        return delegate.has_value() || scriptInfo.has_value();
    }
    
    bool isIntStatusNoBalance() const;
    
    bool isIntStatusNotSuccess() const;
    
    bool isIntStatusForging() const;
    
    bool isIntStatusNodeTest() const;
    
    static std::set<uint64_t> getForgingIntStatuses();
};

struct BlockHeader {
    size_t timestamp;
    uint64_t blockSize;
    uint64_t blockType;
    std::string hash;
    std::string prevHash;
    
    std::vector<unsigned char> signature;
    
    std::string txsHash;
    
    std::optional<size_t> countTxs;
    
    FilePosition filePos;
    
    size_t endBlockPos = 0;
    
    std::optional<size_t> blockNumber;
    
    std::vector<TransactionInfo> blockSignatures;
    
    std::vector<unsigned char> senderSign;  
    std::vector<unsigned char> senderPubkey;
    std::vector<unsigned char> senderAddress;
    
    std::string serialize() const;
    
    static BlockHeader deserialize(const std::string &raw);
    
    bool isStateBlock() const;
    
    bool isSimpleBlock() const;
    
    bool isForgingBlock() const;
    
    std::string getBlockType() const;
};

struct BlockTimes {
    time_point timeBegin;
    time_point timeEnd;
    
    time_point timeBeginGetBlock;
    time_point timeEndGetBlock;
    
    time_point timeBeginSaveBlock;
    time_point timeEndSaveBlock;
};

struct TransactionStatistics {
    size_t countTransferTxs = 0;
    size_t countInitTxs = 0;
};

struct BlockInfo {
    BlockHeader header;
    
    BlockTimes times;
    
    TransactionStatistics txsStatistic;
    
    std::vector<TransactionInfo> txs;
};

struct BlocksMetadata {
    std::string blockHash;
    std::string prevBlockHash;
    size_t blockNumber = 0;
       
    BlocksMetadata() = default;
    
    BlocksMetadata(const std::string &blockHash, const std::string &prevBlockHash, size_t blockNumber)
        : blockHash(blockHash)
        , prevBlockHash(prevBlockHash)
        , blockNumber(blockNumber)
    {}
    
    std::string serialize() const;
       
    static BlocksMetadata deserialize(const std::string &raw);
    
};

struct MainBlockInfo {
    size_t blockNumber = 0;
    std::string blockHash;
    size_t countVal = 0;
    
    MainBlockInfo() = default;
    
    MainBlockInfo(size_t blockNumber, const std::string &blockHash, size_t countVal)
        : blockNumber(blockNumber)
        , blockHash(blockHash)
        , countVal(countVal)
    {}
    
    std::string serialize() const;
    
    static MainBlockInfo deserialize(const std::string &raw);
    
};

struct FileInfo {
    
    FilePosition filePos;
    
    FileInfo()
        : filePos("", 0)
    {}
    
    std::string serialize() const;
    
    static FileInfo deserialize(const std::string &raw);
    
};

template<class ValueType>
struct BatchResults {
    using Address = std::string;
    
    std::vector<std::pair<Address, ValueType>> elements;
    size_t lastBlockNum;
};

size_t getMaxBlockNumber(const std::vector<TransactionInfo> &infos);

size_t getMaxBlockNumber(const std::vector<TransactionStatus> &infos);

template<class Info>
bool isGreater(const Info &info, size_t blockNumber);

}

#endif // BLOCK_INFO_H_
