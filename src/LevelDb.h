#ifndef LEVELDB_H_
#define LEVELDB_H_

#include <string>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <vector>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

namespace std {
template <>
struct hash<std::vector<char>> {
    std::size_t operator()(const std::vector<char>& k) const noexcept {
        return std::hash<std::string>()(std::string(k.begin(), k.end()));
    }
};
}

namespace torrent_node_lib {

struct FileInfo;
struct CroppedFileName;
class LevelDb;

class Batch {
    friend void addBatch(Batch &batch, LevelDb &leveldb);
    
public:
    
    Batch() = default;
    
    explicit Batch(bool isSave)
        : isSave(isSave)
    {}
    
public:
       
    void addBlockHeader(const std::string &blockHash, const std::string &value);
    
    void addBlockMetadata(const std::string &value);
    
    void addFileMetadata(const CroppedFileName &fileName, const std::string &value);
    
    void addMainBlock(const std::string &value);
    
    void addNodeStatBlock(const std::string &value);
        
    void addAllNodes(const std::string &value);
    
private:
    
    template<class Key, class Value>
    void addKey(const Key &key, const Value &value);
    
    std::optional<std::vector<char>> findValueInBatch(const std::vector<char> &key) const;
    
    void removeValue(const std::vector<char> &key);
    
private:
    
    bool isSave = false;
    
    leveldb::WriteBatch batch;
    std::unordered_map<std::vector<char>, std::vector<char>> save;
    std::unordered_set<std::vector<char>> deleted;
    mutable std::mutex mut;
    
    thread_local static std::vector<char> buffer;
};

class LevelDb {
public:
       
    LevelDb(size_t writeBufSizeMb, bool isBloomFilter, bool isChecks, std::string_view folderName, size_t lruCacheMb);
    
    ~LevelDb();
    
    template<class Key, class Value>
    void saveValue(const Key &key, const Value &value, bool isSync = false);
    
    template<class Key>
    std::vector<std::string> findKey(const Key &keyFrom, const Key &keyTo, size_t from = 0, size_t count = 0) const;
    
    std::vector<std::pair<std::string, std::string>> findKey2(const std::string &keyFrom, const std::string &keyTo, size_t from = 0, size_t count = 0) const;
    
    template<class Key>
    std::string findOneValue(const Key &key) const;
    
    template<class Key>
    std::string findOneValueWithoutCheck(const Key &key) const;
    
    std::pair<std::string, std::string> findFirstOf(const std::string &key, const std::unordered_set<std::string> &excluded) const;
    
    void addBatch(leveldb::WriteBatch &batch);
        
    void removeKey(const std::string &key);
    
private:
    
    template<class Key>
    std::string findOneValueInternal(const Key &key, bool isCheck) const;
    
    template<typename Result, class Key, typename Func>
    std::vector<Result> findKeyInternal(const Key &keyFrom, const Key &keyTo, size_t from, size_t count, const Func &func) const;
    
private:
    
    leveldb::DB* db;
    leveldb::Options options;
    
};

std::string findVersionDb(const LevelDb &leveldb);

void addBatch(Batch &batch, LevelDb &leveldb);

void saveModules(const std::string &modules, LevelDb& leveldb);

void saveVersionDb(const std::string &value, LevelDb &leveldb);

std::string findBlockMetadata(const LevelDb &leveldb);

std::unordered_map<CroppedFileName, FileInfo> getAllFiles(const LevelDb &leveldb);

std::set<std::string> getAllBlocks(const LevelDb &leveldb);

std::string findModules(const LevelDb &leveldb);

std::string findMainBlock(const LevelDb &leveldb);

std::string findAllNodes(const LevelDb &leveldb);

std::string findNodeStatBlock(const LevelDb& leveldb);

}

#endif // LEVELDB_H_
