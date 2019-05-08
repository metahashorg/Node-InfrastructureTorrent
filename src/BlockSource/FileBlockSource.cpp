#include "FileBlockSource.h"

#include "BlockchainRead.h"
#include "LevelDb.h"

#include "log.h"
#include "check.h"

using namespace common;

namespace torrent_node_lib {

FileBlockSource::FileBlockSource(LevelDb &leveldb, const std::string &folderPath, bool isValidate)
    : leveldb(leveldb)
    , folderPath(folderPath)
    , isValidate(isValidate)
{}

void FileBlockSource::initialize() {
    allFiles = getAllFiles(leveldb);
}

std::pair<bool, size_t> FileBlockSource::doProcess(size_t countBlocks, const std::string &lastBlockHash) {
    return std::make_pair(true, 0);
}

size_t FileBlockSource::knownBlock() {
    return 0;
}

bool FileBlockSource::process(BlockInfo &bi, std::string &binaryDump) {
    if (fileName.empty()) {
        const FileInfo fi = getNextFile(allFiles, folderPath);
        fileName = fi.filePos.fileName;
        if (fileName.empty()) {
            return false;
        }
        openFile(file, fileName);
        currPos = fi.filePos.pos;
        LOGINFO << "Open next file " << fileName << " " << currPos;
    }
    const size_t nextCurrPos = readNextBlockInfo(file, currPos, bi, binaryDump, isValidate, false, 0, 0);
    if (currPos == nextCurrPos) {
        closeFile(file);
        fileName.clear();
        return false;
    } else {
        bi.header.filePos.fileName = fileName;
        for (auto &tx : bi.txs) {
            tx.filePos.fileName = fileName;
        }
    }

    currPos = nextCurrPos;
    allFiles[CroppedFileName(fileName)].filePos.pos = currPos;
    allFiles[CroppedFileName(fileName)].filePos.fileName = fileName;

    return true;
}

void FileBlockSource::getExistingBlockS(const BlockHeader& bh, BlockInfo& bi, std::string &blockDump, bool isValidate) {
    CHECK(!bh.filePos.fileName.empty(), "Incorrect file name");
    std::ifstream file;
    openFile(file, bh.filePos.fileName);
    const size_t nextCurrPos = readNextBlockInfo(file, bh.filePos.pos, bi, blockDump, isValidate, false, 0, 0);
    CHECK(nextCurrPos != bh.filePos.pos, "File incorrect");
    bi.header.filePos.fileName = bh.filePos.fileName;
    for (auto &tx : bi.txs) {
        tx.filePos.fileName = bh.filePos.fileName;
        tx.blockNumber = bh.blockNumber.value();
    }
    bi.header.blockNumber = bh.blockNumber;
}

void FileBlockSource::getExistingBlock(const BlockHeader& bh, BlockInfo& bi, std::string &blockDump) const {
    getExistingBlockS(bh, bi, blockDump, isValidate);
}

}
