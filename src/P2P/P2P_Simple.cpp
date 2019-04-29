#include "P2P_Simple.h"

#include "curlWrapper.h"

#include "check.h"
#include "log.h"

#include "parallel_for.h"

#include "stopProgram.h"

using namespace common;

namespace torrent_node_lib {

P2P_Simple::P2P_Simple(const std::vector<std::string> &ips)
    : ips(ips)
{
    Curl::initialize();
    
    CHECK(!ips.empty(), "ips empty");
    
    curls = Curl::getInstance();
}

std::string P2P_Simple::request(const Curl::CurlInstance &curl, const std::string& qs, const std::string& postData, const std::string& header, const std::string& server) const {
    std::string url = server;
    CHECK(!url.empty(), "server empty");
    if (url[url.size() - 1] != '/') {
        url += '/';
    }
    url += qs;
    const std::string response = Curl::request(curl, url, postData, header, "", 5);
    return response;
}

void P2P_Simple::broadcast(const std::string &qs, const std::string &postData, const std::string &header, const BroadcastResult& callback) const {
    parallelFor(8, ips.begin(), ips.end(), [&qs, &postData, &header, &callback, this](const std::string &server) {
        try {
            const std::string response = request(curls, qs, postData, header, server);
            callback(server, response, {});
        } catch (const exception &e) {
            callback(server, "", CurlException(e));
            return;
        }
        
        checkStopSignal();
    });
    
    checkStopSignal();
}

std::string P2P_Simple::runOneRequest(const std::string& server, const std::string& qs, const std::string& postData, const std::string& header) const {
    return request(curls, qs, postData, header, server);
}

std::string P2P_Simple::request(size_t responseSize, bool isPrecisionSize, const MakeQsAndPostFunction& makeQsAndPost, const std::string& header, const ResponseParseFunction& responseParse, const std::vector<std::string> &hintsServers) const {   
    CHECK(hintsServers.size() > 0, "Incorrect hint servers");
    
    const auto &[qs, post] = makeQsAndPost(0, responseSize);
    const std::string response = request(curls, qs, post, header, hintsServers[0]);
    const ResponseParse parsed = responseParse(response);
    CHECK(!parsed.error.has_value(), parsed.error.value());
    
    return parsed.response;
}

SendAllResult P2P_Simple::requestAll(const std::string &qs, const std::string &postData, const std::string &header, const std::set<std::string> &additionalServers) const {
    const auto &mainElementsGraph = ips;
    //const std::vector<Server> mainServers(mainElementsGraph.begin(), mainElementsGraph.end());
    const std::vector<Server> otherServers(additionalServers.begin(), additionalServers.end());
    std::vector<std::reference_wrapper<const Server>> allServers;
    //allServers.insert(allServers.end(), mainServers.begin(), mainServers.end());
    allServers.insert(allServers.end(), otherServers.begin(), otherServers.end());
    
    const auto requestFunction = [this](const std::string &qs, const std::string &post, const std::string &header, const std::string &server) -> std::string {
        return request(Curl::getInstance(), qs, post, header, server);
    };

    return P2P::process(allServers, qs, postData, header, requestFunction);
}

}
