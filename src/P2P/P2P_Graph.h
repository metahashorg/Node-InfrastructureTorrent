#ifndef P2P_GRAPH_H_
#define P2P_GRAPH_H_

#include "P2P.h"

#include <vector>

#include "utils/Graph.h"

#include "curlWrapper.h"

using GraphString = Graph<std::string>;

class P2P_Graph: public torrent_node_lib::P2P {   
public:
    
    P2P_Graph(const std::vector<std::pair<std::string, std::string>> &graphVec, const std::string &thisIp, size_t countConnections);
    
    /**
     * c Выполняет запрос по всем серверам. Результаты возвращает в callback.
     *c callback должен быть готов к тому, что его будут вызывать из нескольких потоков.
     */
    void broadcast(const std::string &qs, const std::string &postData, const std::string &header, const torrent_node_lib::BroadcastResult &callback) const override;
    
    std::string request(size_t responseSize, bool isPrecisionSize, const torrent_node_lib::MakeQsAndPostFunction &makeQsAndPost, const std::string &header, const torrent_node_lib::ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) const override;
       
    std::vector<std::string> requests(size_t countRequests, const torrent_node_lib::MakeQsAndPostFunction2 &makeQsAndPost, const std::string &header, const torrent_node_lib::ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) const override;
    
    std::string runOneRequest(const std::string &server, const std::string &qs, const std::string &postData, const std::string &header) const override;
   
    torrent_node_lib::SendAllResult requestAll(const std::string &qs, const std::string &postData, const std::string &header, const std::set<std::string> &additionalServers) const override;
    
private:
    
    std::vector<std::reference_wrapper<const Server>> getServersList(const Server &server) const;
    
    std::string request(const common::Curl::CurlInstance &curl, const std::string &qs, const std::string &postData, const std::string &header, const std::string &server) const;
    
    std::vector<std::string> requestImpl(size_t responseSize, size_t minResponseSize, bool isPrecisionSize, const torrent_node_lib::MakeQsAndPostFunction &makeQsAndPost, const std::string &header, const torrent_node_lib::ResponseParseFunction &responseParse, const std::vector<std::string> &hintsServers) const;
    
private:
    
    GraphString graph;
    
    const GraphString::Element *parent;
    
    size_t countConnections;
    
    std::vector<common::Curl::CurlInstance> curls;
    
};

#endif // P2P_GRAPH_H_
