#ifndef P2P_SIMPLE_H_
#define P2P_SIMPLE_H_

#include "P2P.h"

#include <vector>

#include "curlWrapper.h"

namespace torrent_node_lib {

class P2P_Simple: public P2P {   
public:
    
    P2P_Simple(const std::vector<std::string> &ips);
    
    void broadcast(const std::string &qs, const std::string &postData, const std::string &header, const torrent_node_lib::BroadcastResult &callback) const override;
    
    std::string request(size_t responseSize, bool isPrecisionSize, const torrent_node_lib::MakeQsAndPostFunction &makeQsAndPost, const std::string &header, const torrent_node_lib::ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) const override;
    
    std::string runOneRequest(const std::string &server, const std::string &qs, const std::string &postData, const std::string &header) const override;
   
    SendAllResult requestAll(const std::string &qs, const std::string &postData, const std::string &header, const std::set<std::string> &additionalServers) const override;
    
private:
        
    std::string request(const common::Curl::CurlInstance &curl, const std::string &qs, const std::string &postData, const std::string &header, const std::string &server) const;
    
private:

    const std::vector<std::string> ips;
    
    common::Curl::CurlInstance curls;
    
};

}

#endif // P2P_SIMPLE_H_
