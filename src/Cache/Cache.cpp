#include "Cache.h"

using namespace common;

namespace torrent_node_lib {

template<typename Value>
void Cache<Value>::addValue(const Key& key, const Attribute& attribute, const Value &value) {
    std::lock_guard<std::shared_mutex> lock(mutex);
    attributes[attribute].emplace_back(key);
    map.insert({key, value}); // Not emplace
}

template<typename Value>
std::optional<Value> Cache<Value>::getValue(const Key& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex);
    const auto found = map.find(key);
    if (found == map.end()) {
        return std::nullopt;
    } else {
        return found->second;
    }
}

template<typename Value>
void Cache<Value>::remove(const Attribute& attribute) {
    std::lock_guard<std::shared_mutex> lock(mutex);
    auto found = attributes.find(attribute);
    if (found == attributes.end()) {
        return;
    }
    for (const Key &key: found->second) {
        map.erase(key);
    }
    attributes.erase(attribute);
}

template class Cache<std::shared_ptr<std::string>>;
template class Cache<TransactionInfo>;
template class Cache<TransactionStatus>;

}
