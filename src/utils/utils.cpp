#include "utils.h"

namespace torrent_node_lib {
    
std::string addDefaultPort(const std::string &host, size_t port) {
    if (host.find(':') == host.npos) {
        return host + ":" + std::to_string(port);
    } else {
        return host;
    }
}
    
} // namespace torrent_node_lib
