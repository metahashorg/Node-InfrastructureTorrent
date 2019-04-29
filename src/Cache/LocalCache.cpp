#include "LocalCache.h"

#include "log.h"
#include "check.h"
#include "stringUtils.h"
#include "convertStrings.h"
#include "stopProgram.h"
#include "parallel_for.h"

#include <fstream>
#include <functional>
using namespace std::placeholders;

using namespace common;

namespace torrent_node_lib {

template<typename VectorElement>
static bool isGreater(const std::variant<size_t, std::vector<VectorElement>> &infos, size_t blockNumber);

template<typename ReturnT, typename T>
ReturnT makeNewElementValue(const T &value, bool isSaveFull) {
    if constexpr (std::is_same_v<T, std::vector<TransactionInfo>>) {
        if (isSaveFull) {
            return value;
        } else {
            return getMaxBlockNumber(value);
        }
    } else if constexpr (std::is_same_v<T, std::vector<TransactionStatus>>) {
        if (isSaveFull) {
            return value;
        } else {
            return getMaxBlockNumber(value);
        }
    }
}

template<class Value>
template<typename ValueElement>
LocalCacheElement<Value>::LocalCacheElement(const ValueElement &value, bool isSaveFull, bool isSyncBlockchainThreadUpdated)
    : value(makeNewElementValue<Value>(value, isSaveFull))
    , isSyncBlockchainThreadUpdated(isSyncBlockchainThreadUpdated)
{}

template<class Element>
template<typename ValueElement>
void LocalCacheInternal<Element>::addElementInternal(const HashedString &address, const ValueElement &value, bool isSyncBlockchainThreadUpdated, bool isNewElement) {
    Element newElement = Element(value, isSaveFull, isSyncBlockchainThreadUpdated);
    if (isNewElement){
        newElement.blockUnklesOrNewNum = lastBlockNum + 1;
    }
    std::lock_guard<std::shared_mutex> lock(mut);
    if (!isSyncBlockchainThreadUpdated) {
        cache[address] = newElement;
        noSyncBlockchainThreadElements.insert(address);
    } else {
        auto found = cache.find(address);
        if (found == cache.end()) {
            return;
        }
        found->second = newElement;
        noSyncBlockchainThreadElements.erase(address);
    }
}

template<class Element>
template<typename ValueElement>
void LocalCacheInternal<Element>::addElement(const HashedString &address, const ValueElement &value, bool isSyncBlockchainThreadUpdated) {
    if (maxCountElements == DISABLED_CACHE) {
        return;
    }
    
    addElementInternal(address, value, isSyncBlockchainThreadUpdated, false);
    
    if (maxCountElements != WITHOUT_LIMITATION) {
        const std::vector<HashedString> addrs = heap.reduceToMax(maxCountElements);
        std::lock_guard<std::shared_mutex> lock(mut);
        for (const HashedString &addr: addrs) {
            removeElement(addr);
        }
    }
}

template<class Element>
template<typename ValueElement>
void LocalCacheInternal<Element>::addIfNoExist(const HashedString &address, const std::function<ValueElement(const std::string &address)> &getter, bool isSyncBlockchainThreadUpdated, bool isInitialized) {
    std::unique_lock<std::shared_mutex> lock(mut);
    auto found = cache.find(address);
    if (found == cache.end()) {
        lock.unlock();
        //c Может оказаться так, что мы добавим один элемент 2 раза. Ну и пофиг
        addElementInternal(address, getter(address), isSyncBlockchainThreadUpdated, true);
    } else {
        if (!isInitialized) {
            found->second.blockUnklesOrNewNum = lastBlockNum + 1;
        }
    }
}

template<class Element>
void LocalCacheInternal<Element>::setLastBlockNum(size_t newLastBlockNum) {
    std::lock_guard<std::shared_mutex> lock(mut);
    if (lastBlockNum == 0) { // До этого setLastBlockNum не вызывался
        for (auto &[key, element]: cache) {
            (void)key;
            element.blockUnklesOrNewNum = newLastBlockNum + 1;
        }
    }
    lastBlockNum = newLastBlockNum;
}

void LocalCacheTxs::setTransactions(const HashedString& address, const std::vector<TransactionInfo>& transactions, bool isSyncBlockchainThreadUpdated) {
    if (!transactions.empty() && transactions.front().blockNumber >= transactions.back().blockNumber) {
        addElement(address, transactions, isSyncBlockchainThreadUpdated);
    } else {
        std::vector<TransactionInfo> cp(transactions);
        std::reverse(cp.begin(), cp.end());
        addElement(address, cp, isSyncBlockchainThreadUpdated);
    }
}

void LocalCacheTxs::addTransactions(const HashedString& address, const std::vector<TransactionInfo>& transactions) {
    std::lock_guard<std::shared_mutex> lock(mut);
    auto found = cache.find(address);
    if (found == cache.end()) {
        return;
    }
    auto &txs = found->second.value;
    if (std::holds_alternative<size_t>(txs)) {
        txs = getMaxBlockNumber(transactions);
    } else {
        std::get<1>(txs).insert(std::get<1>(txs).begin(), transactions.begin(), transactions.end());
    }
}

void LocalCacheTxs::removeLastTransactions(const HashedString& address) {
    // TODO не приспособлена под удаление из кэша
    std::lock_guard<std::shared_mutex> lock(mut);
    auto &element = cache.at(address);
    auto &txs = element.value;
    if (std::holds_alternative<size_t>(txs)) {
        
    } else {
        auto iter = std::get<1>(txs).begin();
        const size_t block_num = iter->blockNumber;
        while (iter != std::get<1>(txs).end() && iter->blockNumber == block_num) {
            iter++;
        }
        std::get<1>(txs).erase(std::get<1>(txs).begin(), iter);
    }
    
    element.blockUnklesOrNewNum = lastBlockNum + 1;
}

std::vector<TransactionInfo> LocalCacheTxs::getTransactions(const std::variant<size_t, std::vector<TransactionInfo>> &txs, size_t from, size_t count) {
    CHECK(std::holds_alternative<std::vector<TransactionInfo>>(txs), "Incorrect txs");
    const auto &res = std::get<std::vector<TransactionInfo>>(txs);
    if (count == 0) {
        return res;
    }
    return std::vector<TransactionInfo>(res.begin() + std::min(from, res.size()), res.begin() + std::min(from + count, res.size()));
}

void LocalCacheTxsStatus::setStatuses(const HashedString& address, const std::vector<TransactionStatus>& transactions, bool isSyncBlockchainThreadUpdated) {
    addElement(address, transactions, isSyncBlockchainThreadUpdated);
}

void LocalCacheTxsStatus::addStatuses(const HashedString& address, const std::vector<TransactionStatus>& transactions) {
    std::lock_guard<std::shared_mutex> lock(mut);
    auto found = cache.find(address);
    if (found == cache.end()) {
        return;
    }
    auto &txs = found->second.value;
    if (std::holds_alternative<size_t>(txs)) {
        txs = getMaxBlockNumber(transactions);
    } else {
        std::get<1>(txs).insert(std::get<1>(txs).begin(), transactions.begin(), transactions.end());
    }
}

void LocalCacheTxsStatus::removeLastStatuses(const HashedString& address) {
    // TODO не приспособлена под удаление из кэша
    std::lock_guard<std::shared_mutex> lock(mut);
    auto &element = cache.at(address);
    auto &txs = element.value;
    if (std::holds_alternative<size_t>(txs)) {
        
    } else {
        auto iter = std::get<1>(txs).begin();
        const size_t block_num = iter->blockNumber;
        while (iter != std::get<1>(txs).end() && iter->blockNumber == block_num) {
            iter++;
        }
        std::get<1>(txs).erase(std::get<1>(txs).begin(), iter);
    }
    
    element.blockUnklesOrNewNum = lastBlockNum + 1;
}

std::vector<TransactionStatus> LocalCacheTxsStatus::getStatuses(const std::variant<size_t, std::vector<TransactionStatus>> &txs) {
    CHECK(std::holds_alternative<std::vector<TransactionStatus>>(txs), "Incorrect txs");
    return std::get<std::vector<TransactionStatus>>(txs);
}

template<class Element>
std::unordered_set<HashedString> LocalCacheInternal<Element>::getNoSyncBlockchainThreadElements() const {
    std::shared_lock<std::shared_mutex> lock(mut);
    return noSyncBlockchainThreadElements;
}

template<class Element>
bool LocalCacheInternal<Element>::isExist(const HashedString& address) const {
    std::shared_lock<std::shared_mutex> lock(mut);
    return cache.find(address) != cache.end();
}

template<class Element>
bool LocalCacheInternal<Element>::isSyncBlockchainThreadUpdated(const HashedString& address) const {
    std::shared_lock<std::shared_mutex> lock(mut);
    auto found = cache.find(address);
    if (found == cache.end()) {
        return false;
    }
    return found->second.isSyncBlockchainThreadUpdated;
}

template<class Element>
bool LocalCacheInternal<Element>::getValueIfExist(const HashedString& address, typename Element::ValueType &value) const {
    if (maxCountElements == DISABLED_CACHE) {
        return false;
    }
    
    std::shared_lock<std::shared_mutex> lock(mut);
    const auto found = cache.find(address);
    const bool result = found != cache.end();
    if (result) {
        value = found->second.value;
    }
    lock.unlock();
    
    if (result && maxCountElements != WITHOUT_LIMITATION) {
        const time_point now = ::now();
        heap.addElement(address, now);
    }
    return result;
}

template<typename Element> 
void LocalCacheInternal<Element>::removeElement(const HashedString& address) {
    cache.erase(address);
    noSyncBlockchainThreadElements.erase(address);
}

template<class Element> 
BatchResults<typename Element::ValueType> LocalCacheInternal<Element>::findGreaterElements(const std::unordered_set<HashedString> &addresses, size_t blockNum) const {
    BatchResults<typename Element::ValueType> result;
    std::shared_lock<std::shared_mutex> lock(mut);
    result.lastBlockNum = lastBlockNum;
    for (const HashedString &address: addresses) {
        const auto found = cache.find(address);
        if (found == cache.end()) {
            continue;
        }
        const auto &element = found->second;
        if (element.blockUnklesOrNewNum > blockNum || isGreater(element.value, blockNum)) {
            result.elements.emplace_back(address, element.value);
        }
    }
    return result;
}

void LocalCache::setFunctions(const GetterTxs& getterTxs_, size_t countThreads) {
    getterTxs = getterTxs_;
    isSetFunctions = true;
    countThreadsForLoad = countThreads;
}

void LocalCache::setAddressPathAndLoad(const std::string& path, const std::function<std::string(std::string)> &processAddress, const std::string &setName) {
    CHECK(addressesFile.empty(), "Double run setAddressPathAndLoad");
    addressesFile = path;
    processAddr = processAddress;
       
    if (!addressesFile.empty()) {
        CHECK(isSetFunctions, "Functions not set");
        
        getterTxs2 = std::bind(getterTxs, _1, std::ref(setName));
        
        processFile();
        
        isSetAddressesPath = true;
    }
    
    isInitialized = true;
}

static void addAddressesToFile(std::ofstream &file, const std::unordered_set<HashedString> &newAddresses, const std::string &group) {
    for (const HashedString &address: newAddresses) {
        file << address.s;
        if (!group.empty()) {
            file << " " << group;
        }
        file << "\n";
    }
}

void LocalCache::addAddresses(const std::unordered_set<std::string> &newAddressesStrs, const std::string &group) {
    CHECK(isInitialized.load(), "not initialized");
    CHECK(isSetAddressesPath.load(), "not initialized for batch requests");
    
    std::unordered_set<HashedString> newAddresses;
    for (const std::string &address: newAddressesStrs) {
        newAddresses.insert(processAddr(address));
    }
    
    for (const HashedString &address: newAddresses) {
        localCacheTxs.addIfNoExist(address, getterTxs2, false, false);
    }
    
    std::lock_guard<std::mutex> lock(mut);
    auto &addrs = addresses[group];
    addrs.insert(newAddresses.begin(), newAddresses.end());
    std::ofstream file(addressesFile, std::ios::app);
    addAddressesToFile(file, newAddresses, group);
}

void LocalCache::removeAddresses(const std::unordered_set<std::string> &removedAddressesStrs, const std::string &group) {
    CHECK(isInitialized.load(), "not initialized");
    CHECK(isSetAddressesPath.load(), "not initialized for batch requests");
    
    std::unordered_set<HashedString> removedAddresses;
    for (const std::string &address: removedAddressesStrs) {
        removedAddresses.insert(processAddr(address));
    }
    
    std::lock_guard<std::mutex> lock(mut);
    const auto found = addresses.find(group);
    CHECK(found != addresses.end(), "Group " + group + " not found in addresses list");
    for (const HashedString &address: removedAddresses) {
        found->second.erase(address);
    }
    std::ofstream file(addressesFile, std::ios::trunc);
    for (const auto &[group, addrs]: addresses) {
        addAddressesToFile(file, addrs, group);
    }
}

std::unordered_set<HashedString> LocalCache::getAddresses(const std::string &group) const {
    CHECK(isInitialized.load(), "not initialized");
    CHECK(isSetAddressesPath.load(), "not initialized for batch requests");
    
    std::lock_guard<std::mutex> lock(mut);
    const auto found = addresses.find(group);
    if (found != addresses.end()) {
        return found->second;
    } else {
        return {};
    }
}

void LocalCache::processFile() {
    Timer tt;
    std::unordered_set<HashedString> addressesInFile;
    std::unique_lock<std::mutex> lock(mut);
    std::ifstream file(addressesFile);
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (!line.empty()) {
            const size_t findSpace = line.find(" ");
            std::string group;
            if (findSpace != line.npos) {
                group = line.substr(findSpace + 1);
                line = line.substr(0, findSpace);
            }
            addressesInFile.insert(line);
            addresses[group].insert(line);
        }
    }
    file.close();
    lock.unlock();
                    
    parallelFor(countThreadsForLoad, addressesInFile.begin(), addressesInFile.end(), [this](const HashedString &address) {
        localCacheTxs.addIfNoExist(address, getterTxs2, false, true);
    });
    
    tt.stop();
    LOGINFO << "Loaded " << addressesInFile.size() << " all addresses. Time " << tt.countMs();
}

void LocalCache::setLastBlockNum(size_t blockNum) {
    localCacheTxs.setLastBlockNum(blockNum);
}

template class LocalCacheInternal<LocalCacheElementTxs>;
template class LocalCacheInternal<LocalCacheElementTxsStatus>;

template void LocalCacheInternal<LocalCacheElementTxs>::addElement<std::vector<TransactionInfo>>(const HashedString &address, const std::vector<TransactionInfo> &value, bool isSyncBlockchainThreadUpdated);
template void LocalCacheInternal<LocalCacheElementTxsStatus>::addElement<std::vector<TransactionStatus>>(const HashedString &address, const std::vector<TransactionStatus> &value, bool isSyncBlockchainThreadUpdated);

template<typename VectorElement>
static bool isGreater(const std::variant<size_t, std::vector<VectorElement>> &infos, size_t blockNumber) {
    if (std::holds_alternative<size_t>(infos)) {
        return std::get<size_t>(infos) > blockNumber;
    } else {
        return isGreater(std::get<std::vector<VectorElement>>(infos), blockNumber);
    }
}

}
