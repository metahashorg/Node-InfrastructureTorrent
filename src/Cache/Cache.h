#ifndef CACHE_H_
#define CACHE_H_

#include <list>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <shared_mutex>
#include <optional>
#include <memory>

#include "LocalCache.h"

#include "HashedString.h"

namespace torrent_node_lib {

template<typename Value>
class Cache {
public:
    
    using Key = common::HashedString;
    
    using Attribute = std::string;
    
public:

    void addValue(const Key &key, const Attribute &attribute, const Value &value);
       
    std::optional<Value> getValue(const Key &key) const;
    
    void remove(const Attribute &attribute);
    
private:
    
    std::unordered_map<Key, Value> map;
    std::unordered_map<Attribute, std::vector<Key>> attributes;
    
    mutable std::shared_mutex mutex;
};

struct AllCaches {   
    size_t maxCountElementsBlockCache;
    size_t maxCountElementsTxsCache;
    size_t macLocalCacheElements;
    
    Cache<std::shared_ptr<std::string>> blockDumpCache;
    Cache<TransactionInfo> txsCache;
    Cache<TransactionStatus> txsStatusCache;
    
    LocalCache localCache;
    
    AllCaches(size_t maxCountElementsBlockCache, size_t maxCountElementsTxsCache, size_t macLocalCacheElements)
        : maxCountElementsBlockCache(maxCountElementsBlockCache)
        , maxCountElementsTxsCache(maxCountElementsTxsCache)
        , macLocalCacheElements(macLocalCacheElements)
        , localCache(macLocalCacheElements)
    {}
};

}

#endif // CACHE_H_
