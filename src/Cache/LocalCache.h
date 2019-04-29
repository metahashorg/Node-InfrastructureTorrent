#ifndef LOCAL_CACHE_H_
#define LOCAL_CACHE_H_

#include "BlockInfo.h"
#include "HashedString.h"

#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <atomic>
#include <functional>
#include <variant>

#include "OopUtils.h"
#include "duration.h"

#include "utils/Heap.h"

namespace torrent_node_lib {

template<class Value>
struct LocalCacheElement {
    
    using ValueType = Value;
    
    Value value;
    
    size_t blockUnklesOrNewNum = 0;
    
    bool isSyncBlockchainThreadUpdated;
    
    LocalCacheElement() = default;
    
    template<typename ValueElement>
    LocalCacheElement(const ValueElement &value, bool isSaveFull, bool isSyncBlockchainThreadUpdated);
    
};

using LocalCacheElementTxs = LocalCacheElement<std::variant<size_t, std::vector<TransactionInfo>>>;
using LocalCacheElementTxsStatus = LocalCacheElement<std::variant<size_t, std::vector<TransactionStatus>>>;

template<class Element>
class LocalCacheInternal: public common::no_copyable, common::no_moveable {
public:
    
    const static size_t DISABLED_CACHE = 0;
    
    const static size_t WITHOUT_LIMITATION = std::numeric_limits<size_t>::max();
    
    LocalCacheInternal(size_t maxCountElements)
        : maxCountElements(maxCountElements)
    {}
    
    virtual ~LocalCacheInternal() = default;
    
    template<typename ValueElement>
    void addElement(const common::HashedString &address, const ValueElement &value, bool isSyncBlockchainThreadUpdated);
       
    template<typename ValueElement>
    void addIfNoExist(const common::HashedString &address, const std::function<ValueElement(const std::string &address)> &getter, bool isSyncBlockchainThreadUpdated, bool isInitialized);
    
    void setLastBlockNum(size_t newLastBlockNum);
    
    std::unordered_set<common::HashedString> getNoSyncBlockchainThreadElements() const;
    
    bool isExist(const common::HashedString &address) const;
    
    bool isSyncBlockchainThreadUpdated(const common::HashedString &address) const;
    
    bool getValueIfExist(const common::HashedString &address, typename Element::ValueType &value) const;        
    
    BatchResults<typename Element::ValueType> findGreaterElements(const std::unordered_set<common::HashedString> &addresses, size_t blockNum) const;
    
private:
    
    template<typename ValueElement>
    void addElementInternal(const common::HashedString &address, const ValueElement &value, bool isSyncBlockchainThreadUpdated, bool isNewElement);
    
    void removeElement(const common::HashedString &address);
    
protected:
    
    std::unordered_map<common::HashedString, Element> cache;
    
    std::unordered_set<common::HashedString> noSyncBlockchainThreadElements;
    
    size_t lastBlockNum = 0;
    
    const size_t maxCountElements;
    
    mutable Heap<common::HashedString, time_point> heap;
    
    mutable std::shared_mutex mut;

    bool isSaveFull = true;
    
};

class LocalCacheTxs: public LocalCacheInternal<LocalCacheElementTxs> {
public:
    
    LocalCacheTxs(size_t maxCountElements)
        : LocalCacheInternal<LocalCacheElementTxs>(maxCountElements)
    {}
    
    void setTransactions(const common::HashedString &address, const std::vector<TransactionInfo> &transactions, bool isSyncBlockchainThreadUpdated);
    
    void addTransactions(const common::HashedString &address, const std::vector<TransactionInfo> &transactions);
    
    void removeLastTransactions(const common::HashedString &address);
    
    static std::vector<TransactionInfo> getTransactions(const std::variant<size_t, std::vector<TransactionInfo>> &txs, size_t from, size_t count);
    
    ~LocalCacheTxs() override = default;
        
};

class LocalCacheTxsStatus: public LocalCacheInternal<LocalCacheElementTxsStatus> {
public:
    
    LocalCacheTxsStatus(size_t maxCountElements)
        : LocalCacheInternal<LocalCacheElementTxsStatus>(maxCountElements)
    {}
    
    void setStatuses(const common::HashedString &address, const std::vector<TransactionStatus> &transactions, bool isSyncBlockchainThreadUpdated);
    
    void addStatuses(const common::HashedString &address, const std::vector<TransactionStatus> &transactions);
    
    void removeLastStatuses(const common::HashedString &address);
    
    static std::vector<TransactionStatus> getStatuses(const std::variant<size_t, std::vector<TransactionStatus>> &txs);
    
    ~LocalCacheTxsStatus() override = default;
    
};

class LocalCache {
public:
    
    using GetterTxs = std::function<std::vector<TransactionInfo>(const std::string &address, const std::string &setName)>;
    
public:
    
    LocalCache(size_t maxCountElements)
        : localCacheTxs(maxCountElements)
        , localCacheTxsStatus(maxCountElements)
    {}
    
    void setFunctions(const GetterTxs &getterTxs_, size_t countThreads);
    
    void setAddressPathAndLoad(const std::string &path, const std::function<std::string(std::string)> &processAddress, const std::string &setName);
    
    void addAddresses(const std::unordered_set<std::string> &newAddressesStrs, const std::string &group);
    
    void removeAddresses(const std::unordered_set<std::string> &removedAddressesStrs, const std::string &group);
    
    std::unordered_set<common::HashedString> getAddresses(const std::string &group) const;
    
    void setLastBlockNum(size_t blockNum);
    
public:
    
    LocalCacheTxs localCacheTxs;
    
    LocalCacheTxsStatus localCacheTxsStatus;
    
private:
    
    void processFile();
    
private:
    
    GetterTxs getterTxs;
    
    bool isSetFunctions = false;
    
    std::string addressesFile;
    
    std::function<std::string(std::string)> processAddr;
    
    std::function<std::vector<TransactionInfo>(const std::string &address)> getterTxs2;
    
    size_t countThreadsForLoad;
    
    std::atomic<bool> isInitialized = false;
    
    std::atomic<bool> isSetAddressesPath = false;
    
    std::unordered_map<std::string, std::unordered_set<common::HashedString>> addresses;
        
    mutable std::mutex mut;
};

}

#endif // LOCAL_CACHE_H_
