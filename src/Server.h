#ifndef SERVER_H_
#define SERVER_H_

#include <sniper/mhd/MHD.h>

#include <string>
#include <atomic>

#include "utils/SmallStatistic.h"

namespace torrent_node_lib {
class Sync;
}

class Server: public sniper::mhd::MHD {
public:
    Server(const torrent_node_lib::Sync &sync, int port, std::atomic<int> &countRunningThreads) 
        : sync(sync)
        , port(port)
        , countRunningThreads(countRunningThreads)
        , isStoped(false)
    {}
    
    ~Server() override {}
    
    bool run(int thread_number, Request& mhd_req, Response& mhd_resp) override;
    
    bool init() override;
    
private:
    
    const torrent_node_lib::Sync &sync;
    
    const int port;

    std::atomic<int> &countRunningThreads;
    
    std::atomic<bool> isStoped;
        
    SmallStatistic smallRequestStatistics;
    
};

#endif // SERVER_H_
