#include "LevelDb.h"

#include <iostream>
#include <memory>

#include <leveldb/filter_policy.h>
#include <leveldb/cache.h>

#include "check.h"

#include "BlockInfo.h"

#include "utils/FileSystem.h"
#include "stringUtils.h"
#include "utils/serialize.h"
#include "log.h"

using namespace common;

namespace torrent_node_lib {

const static std::string KEY_BLOCK_METADATA = "?block_meta";
const static std::string VERSION_DB = "?version_db";

const static std::string BLOCK_PREFIX = "b_";
const static std::string MAIN_BLOCK_NUMBER_PREFIX = "ms_";
const static std::string NODE_STAT_BLOCK_NUMBER_PREFIX = "ns_";
const static std::string FILE_PREFIX = "f_";
const static std::string MODULES_KEY = "modules";
const static std::string NODES_STATS_ALL = "nsaa_";

thread_local std::vector<char> Batch::buffer;

LevelDb::LevelDb(long unsigned int writeBufSizeMb, bool isBloomFilter, bool isChecks, std::string_view folderName, size_t lruCacheMb) {
    createDirectories(std::string(folderName));
    options.block_cache = leveldb::NewLRUCache(lruCacheMb * 1024 * 1024);
    options.create_if_missing = true;
    options.compression = leveldb::kNoCompression;
    if (isChecks) {
        options.paranoid_checks = true;
    }
    if (isBloomFilter) {
        options.filter_policy = leveldb::NewBloomFilterPolicy(10);
    }
    options.write_buffer_size = writeBufSizeMb * 1024 * 1024;
    const leveldb::Status status = leveldb::DB::Open(options, folderName.data(), &db);
    CHECK(status.ok(), "Dont open leveldb " + status.ToString());
}

LevelDb::~LevelDb() {
    // WARNING деструктор не сработает, если в кострукторе произойдет исключение
    delete db;
    delete options.filter_policy;
    delete options.block_cache;
}

template<class Key, class Value>
void LevelDb::saveValue(const Key &key, const Value &value, bool isSync) {
    auto writeOptions = leveldb::WriteOptions();
    if (isSync) {
        writeOptions.sync = true;
    }
    const leveldb::Status s = db->Put(writeOptions, leveldb::Slice(key.data(), key.size()), leveldb::Slice(value.data(), value.size()));
    CHECK(s.ok(), "dont add key to bd " + std::string(key.begin(), key.end()) + ". " + s.ToString());
}

template<typename Result, class Key, typename Func>
std::vector<Result> LevelDb::findKeyInternal(const Key &keyFrom, const Key &keyTo, size_t from, size_t count, const Func &func) const {
    std::unique_ptr<leveldb::Iterator> it(db->NewIterator(leveldb::ReadOptions()));
    std::vector<Result> result;
    size_t index = 0;
    for (it->Seek(leveldb::Slice(keyFrom.data(), keyFrom.size())); it->Valid() && it->key().compare(leveldb::Slice(keyTo.data(), keyTo.size())) < 0; it->Next()) {
        if (count != 0 && index >= from + count) {
            break;
        }
        if (index >= from) {
            result.emplace_back(func(it.get()));
        }
        index++;
    }
    return result;
}

template<class Key>
std::vector<std::string> LevelDb::findKey(const Key &keyFrom, const Key &keyTo, size_t from, size_t count) const {
    return findKeyInternal<std::string>(keyFrom, keyTo, from, count, [](const leveldb::Iterator* iter) {
        return iter->value().ToString();
    });
}

std::vector<std::pair<std::string, std::string>> LevelDb::findKey2(const std::string &keyFrom, const std::string &keyTo, size_t from, size_t count) const {
    return findKeyInternal<std::pair<std::string, std::string>>(keyFrom, keyTo, from, count, [](const leveldb::Iterator* iter) {
        return std::make_pair(iter->key().ToString(), iter->value().ToString());
    });
}

template<class Key>
std::string LevelDb::findOneValue(const Key& key) const {
    return findOneValueInternal(key, true);
}

template<class Key>
std::string LevelDb::findOneValueWithoutCheck(const Key& key) const {
    return findOneValueInternal(key, false);
}

template<class Key>
std::string LevelDb::findOneValueInternal(const Key& key, bool isCheck) const {
    std::string value;
    const leveldb::Status s = db->Get(leveldb::ReadOptions(), leveldb::Slice(key.data(), key.size()), &value);
    if (!isCheck && s.IsNotFound()) {
        return "";
    }
    CHECK(s.ok(), "dont add key to bd count val " + s.ToString());
    return value;
}

std::pair<std::string, std::string> LevelDb::findFirstOf(const std::string& key, const std::unordered_set<std::string> &excluded) const {
    std::unique_ptr<leveldb::Iterator> it(db->NewIterator(leveldb::ReadOptions()));
    for (it->Seek(key); it->Valid(); it->Next()) {
        const std::string &keyResult = it->key().ToString();
        if (beginWith(keyResult, key)) {
            if (excluded.find(keyResult) != excluded.end()) {
                continue;
            }
            return std::make_pair(keyResult, it->value().ToString());
        }
        break;
    }
    return std::make_pair("", "");
}

void LevelDb::addBatch(leveldb::WriteBatch& batch) {
    const leveldb::Status s = db->Write(leveldb::WriteOptions(), &batch);
    CHECK(s.ok(), "dont add key to bd. " + s.ToString());
}

void LevelDb::removeKey(const std::string& key) {
    const leveldb::Status s = db->Delete(leveldb::WriteOptions(), key);
    CHECK(s.ok(), "dont delete key to bd. " + s.ToString());
}

static void makeKeyPrefix(const std::string &key, const std::string &prefix, std::vector<char> &buffer) {
    buffer.clear();
    buffer.insert(buffer.end(), prefix.begin(), prefix.end());
    buffer.insert(buffer.end(), key.begin(), key.end());
}

static void makeBlockKey(const std::string& blockHash, std::vector<char> &buffer) {
    CHECK(!blockHash.empty(), "Incorrect blockHash: empty");
    buffer.clear();
    makeKeyPrefix(blockHash, BLOCK_PREFIX, buffer);
}

static void makeFileKey(const std::string& fileName, std::vector<char> &buffer) {
    CHECK(!fileName.empty(), "Incorrect blockHash: empty");
    buffer.clear();
    makeKeyPrefix(fileName, FILE_PREFIX, buffer);
}

void addBatch(Batch& batch, LevelDb& leveldb) {
    leveldb.addBatch(batch.batch);
}

void Batch::addBlockHeader(const std::string& blockHash, const std::string& value) {
    makeBlockKey(blockHash, buffer);
    addKey(buffer, value);
}

void Batch::addBlockMetadata(const std::string& value) {
    addKey(KEY_BLOCK_METADATA, value);
}

void Batch::addFileMetadata(const CroppedFileName &fileName, const std::string& value) {
    makeFileKey(fileName.str(), buffer);
    addKey(buffer, value);
}

void saveModules(const std::string& modules, LevelDb& leveldb) {
    leveldb.saveValue(MODULES_KEY, modules, true);
}

void Batch::addMainBlock(const std::string &value) {
    addKey(MAIN_BLOCK_NUMBER_PREFIX, value);
}

void Batch::addAllNodes(const std::string &value) {
    addKey(NODES_STATS_ALL, value);
}

std::string findBlockMetadata(const LevelDb& leveldb) {
    return leveldb.findOneValueWithoutCheck(KEY_BLOCK_METADATA);
}

std::unordered_map<CroppedFileName, FileInfo> getAllFiles(const LevelDb &leveldb) {
    const std::string &from = FILE_PREFIX;
    std::string to = from.substr(0, from.size() - 1);
    to += (char)(from.back() + 1);
    
    const std::vector<std::string> found = leveldb.findKey(from, to);
    
    std::unordered_map<CroppedFileName, FileInfo> result;
    
    for (const std::string &f: found) {
        const FileInfo fi = FileInfo::deserialize(f);
        result[CroppedFileName(fi.filePos.fileName)] = fi;
    }
    
    return result;
}

std::set<std::string> getAllBlocks(const LevelDb &leveldb) {
    const std::string &from = BLOCK_PREFIX;
    std::string to = from.substr(0, from.size() - 1);
    to += (char)(from.back() + 1);
    
    const std::vector<std::string> res = leveldb.findKey(from, to);
    return std::set<std::string>(res.begin(), res.end());
}

template<class Key, class Value>
void Batch::addKey(const Key &key, const Value& value) {
    std::lock_guard<std::mutex> lock(mut);
    batch.Put(leveldb::Slice(key.data(), key.size()), leveldb::Slice(value.data(), value.size()));
    if (isSave) {
        save.emplace(std::vector<char>(key.begin(), key.end()), std::vector<char>(value.begin(), value.end()));
    }
}

std::optional<std::vector<char>> Batch::findValueInBatch(const std::vector<char> &key) const {
    CHECK(isSave, "is save not set");
    const auto found = save.find(key);
    if (found == save.end()) {
        return std::nullopt;
    } else {
        return found->second;
    }
}

void Batch::removeValue(const std::vector<char> &key) {
    std::lock_guard<std::mutex> lock(mut);
    batch.Delete(leveldb::Slice(key.data(), key.size()));
    if (isSave) {
        save.erase(key);
        deleted.insert(key);
    }
}

void saveAddressStatus(const std::string& addressAndHash, const std::vector<char>& value, LevelDb& leveldb) {
    leveldb.saveValue(addressAndHash, value);
}

void saveVersionDb(const std::string &value, LevelDb &leveldb) {
    leveldb.saveValue(VERSION_DB, value);
}

std::string findModules(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(MODULES_KEY);
}

std::string findMainBlock(const LevelDb &leveldb) {
    const std::string result = leveldb.findOneValueWithoutCheck(MAIN_BLOCK_NUMBER_PREFIX);
    return result;
}

std::string findVersionDb(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(VERSION_DB);
}

std::string findNodeStatBlock(const LevelDb& leveldb) {
    const std::string result = leveldb.findOneValueWithoutCheck(NODE_STAT_BLOCK_NUMBER_PREFIX);
    return result;
}

void Batch::addNodeStatBlock(const std::string &value) {
    addKey(NODE_STAT_BLOCK_NUMBER_PREFIX, value);
}

std::string findAllNodes(const LevelDb &leveldb) {
    return leveldb.findOneValueWithoutCheck(NODES_STATS_ALL);
}

}
