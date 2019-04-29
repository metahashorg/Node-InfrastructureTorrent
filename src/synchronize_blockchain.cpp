#include "synchronize_blockchain.h"

#include "check.h"

#include "BlockchainUtils.h"
#include "BlockchainRead.h"

#include "SyncImpl.h"

#include "Workers/NodeTestsBlockInfo.h"

#include "utils/checkVirtual.h"

using namespace common;

namespace torrent_node_lib {

void initBlockchainUtils(const BlockVersion &blockVersion) {
    isInitialized = true;
    initBlockchainUtilsImpl();
    
    //initializeIsVirtualMachine();
    
    if (blockVersion == BlockVersion::V1) {
        BLOCK_VERSION = BlockVersionImpl::V1;
    } else if (blockVersion == BlockVersion::V2) {
        BLOCK_VERSION = BlockVersionImpl::V2;
    } else {
        throwErr("Incorrect block version");
    }
}

Sync::Sync(const std::string& folderPath, const LevelDbOptions& leveldbOpt, const CachesOptions& cachesOpt, const GetterBlockOptions &getterBlocksOpt, const std::string &signKeyName, const TestNodesOptions &testNodesOpt)
    : impl(std::make_unique<SyncImpl>(folderPath, leveldbOpt, cachesOpt, getterBlocksOpt, signKeyName, testNodesOpt))
{}

void Sync::setLeveldbOptScript(const LevelDbOptions &leveldbOptScript) {
    impl->setLeveldbOptScript(leveldbOptScript);
}

void Sync::setLeveldbOptNodeTest(const LevelDbOptions &leveldbOptScript) {
    impl->setLeveldbOptNodeTest(leveldbOptScript);
}

std::string Sync::getBlockDump(const BlockHeader& bh, size_t fromByte, size_t toByte, bool isHex, bool isSign) const {
    return impl->getBlockDump(bh, fromByte, toByte, isHex, isSign);
}

bool Sync::isVirtualMachine() const {
    return torrent_node_lib::isVirtualMachine();
}

size_t Sync::getKnownBlock() const {
    return impl->getKnownBlock();
}

void Sync::synchronize(int countThreads) {
    impl->synchronize(countThreads);
}

void Sync::addUsers(const std::set<Address>& addresses) {
    impl->addUsers(addresses);
}

const BlockChainReadInterface & Sync::getBlockchain() const {
    return impl->getBlockchain();
}

void Sync::fillSignedTransactionsInBlock(BlockHeader& bh) const {
    impl->fillSignedTransactionsInBlock(bh);
}

std::string Sync::signTestString(const std::string &str, bool isHex) const {
    return impl->signTestString(str, isHex);
}

Sync::~Sync() = default;

}
