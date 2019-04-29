#ifndef HEAP_H_
#define HEAP_H_

#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace torrent_node_lib {

template<typename Key, typename Value>
class Heap {
public:
    
    void addElement(const Key &key, const Value &value);
    
    std::vector<Key> reduceToMax(size_t newSize);
    
private:
    
    std::unordered_map<Key, Value> keys;
    std::multimap<Value, Key> heap;
    
    std::mutex mut;
};

}

#endif // HEAP_H_
