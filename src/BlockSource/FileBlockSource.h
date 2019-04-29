#ifndef FILE_BLOCK_SOURCE_H_
#define FILE_BLOCK_SOURCE_H_

#include "BlockSource.h"

#include "OopUtils.h"
#include "utils/FileSystem.h"

#include "BlockInfo.h"

#include <string>
#include <fstream>
#include <unordered_map>

namespace torrent_node_lib {
   
class LevelDb;

class FileBlockSource: public BlockSource, common::no_copyable, common::no_moveable {
public:
    
    FileBlockSource(LevelDb &leveldb, const std::string &folderPath, bool isValidate);
    
    void initialize() override;
    
    std::pair<bool, size_t> doProcess(size_t countBlocks, const std::string &lastBlockHash) override;
    
    size_t knownBlock() override;
    
    bool process(BlockInfo &bi, std::string &binaryDump) override;
    
    static void getExistingBlockS(const BlockHeader &bh, BlockInfo &bi, std::string &blockDump, bool isValidate);
    
    void getExistingBlock(const BlockHeader &bh, BlockInfo &bi, std::string &blockDump) const override;
    
    ~FileBlockSource() override = default;
    
private:
    
    LevelDb &leveldb;
    
    const std::string folderPath;
       
    std::unordered_map<CroppedFileName, FileInfo> allFiles;
    
    size_t currPos = 0;
    std::ifstream file;
    std::string fileName;
    
    const bool isValidate;
    
};

}

#endif // FILE_BLOCK_SOURCE_H_
