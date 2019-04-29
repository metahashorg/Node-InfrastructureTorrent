#ifndef GENERATE_JSON_H_
#define GENERATE_JSON_H_

#include <string>
#include <variant>
#include <functional>

namespace torrent_node_lib {
class BlockChainReadInterface;
struct BlockHeader;
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

std::string blockHeaderToJson(const RequestId &requestId, const torrent_node_lib::BlockHeader &bh, const std::optional<std::reference_wrapper<const torrent_node_lib::BlockHeader>> &nextBlock, bool isFormat, BlockTypeInfo type, const JsonVersion &version);

std::string genCountBlockJson(const RequestId &requestId, size_t countBlocks, bool isFormat, const JsonVersion &version);

std::string genBlockDumpJson(const RequestId &requestId, const std::string &blockDump, bool isFormat);

std::string genTestSignStringJson(const RequestId &requestId, const std::string &responseHex);

#endif // GENERATE_JSON_H_
