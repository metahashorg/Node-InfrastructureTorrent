#include "Heap.h"

#include <algorithm>

#include "HashedString.h"

#include "duration.h"
#include "check.h"

using namespace common;

namespace torrent_node_lib {

template<typename Key, typename Value>
void Heap<Key, Value>::addElement(const Key& key, const Value& value) {
    std::lock_guard<std::mutex> lock(mut);
    auto found = keys.find(key);
    if (found != keys.end()) {
        const Value &lastValue = found->second;
        const auto &eqRange = heap.equal_range(lastValue);
        const auto saveIt = std::find_if(eqRange.first, eqRange.second, [&key](const auto &it) {return it.second == key;});
        CHECK(saveIt != eqRange.second, "Not found element");
        heap.erase(saveIt);
    }
    keys[key] = value;
    heap.insert({value, key});
}

template<typename Key, typename Value> 
std::vector<Key> Heap<Key, Value>::reduceToMax(size_t newSize) {
    CHECK(newSize != 0, "Incorrect new size");
    std::vector<Key> result;
    const Value minValue = Value::min();
    std::lock_guard<std::mutex> lock(mut);
    while (keys.size() > newSize) {
        const auto it = heap.lower_bound(minValue);
        const Key &key = result.emplace_back(it->second);
        keys.erase(key);
        heap.erase(it);
    }
    return result;
}

template class Heap<HashedString, time_point>;

}
