#ifndef GENERATE_JSON_H_
#define GENERATE_JSON_H_

#include <string>
#include <variant>
#include <functional>

namespace torrent_node_lib {
class BlockChainReadInterface;
struct BlockHeader;
struct MinimumBlockHeader;
}

struct RequestId {
    std::variant<std::string, size_t> id;
    bool isSet = false;
};

enum class JsonVersion {
    V1, V2
};

enum class BlockTypeInfo {
    ForP2P, Small, Simple, Hashes, Full
};

std::string genErrorResponse(const RequestId &requestId, int code, const std::string &error);

std::string genStatusResponse(const RequestId &requestId, const std::string &version, const std::string &gitHash);

std::string genStatisticResponse(const RequestId &requestId, size_t statistic, double proc, unsigned long long int memory, int connections);

std::string genStatisticResponse(size_t statistic);

std::string blockHeaderToJson(const RequestId &requestId, const torrent_node_lib::BlockHeader &bh, const std::optional<std::reference_wrapper<const torrent_node_lib::BlockHeader>> &nextBlock, bool isFormat, BlockTypeInfo type, const JsonVersion &version);

std::string genCountBlockJson(const RequestId &requestId, size_t countBlocks, bool isFormat, const JsonVersion &version);

std::string genBlockDumpJson(const RequestId &requestId, const std::string &blockDump, bool isFormat);

std::string genTestSignStringJson(const RequestId &requestId, const std::string &responseHex);

std::string genDumpBlockBinary(const std::string &block, bool isCompress);

std::string genDumpBlocksBinary(const std::vector<std::string> &blocks, bool isCompress);

std::string parseDumpBlockBinary(const std::string &response, bool isCompress);

std::vector<std::string> parseDumpBlocksBinary(const std::string &response, bool isCompress);

std::string blockHeadersToJson(const RequestId &requestId, const std::vector<torrent_node_lib::BlockHeader> &bh, BlockTypeInfo type, bool isFormat, const JsonVersion &version);

torrent_node_lib::MinimumBlockHeader parseBlockHeader(const std::string &response);

std::vector<torrent_node_lib::MinimumBlockHeader> parseBlocksHeader(const std::string &response);

#endif // GENERATE_JSON_H_
