#ifndef TEST_P2P_NODES_H_
#define TEST_P2P_NODES_H_

#include "Thread.h"

#include <mutex>
#include <map>

namespace torrent_node_lib {

class P2P;
class WorkerNodeTest;
class PrivateKey;
    
class TestP2PNodes {
public:
    
    TestP2PNodes(P2P *p2p, const std::string &myIp, const std::string &resultServer, size_t defaultPortTorrent);
    
    void start();
    
    void addWorkerTest(const WorkerNodeTest *workerTest);
    
    void setPrivateKey(const PrivateKey *privKey) {
        privateKey = privKey;
    }
    
private:
    
    void work();
    
private:
    
    P2P *p2p;
    
    common::Thread thread;
    
    const PrivateKey *privateKey = nullptr;
    
    const std::string myIp;
    
    const std::string resultServer;
    
    const size_t defaultPortTorrent;
    
    const WorkerNodeTest *workerTestPtr = nullptr;
    mutable std::mutex workerTestPtrMut;
    
    std::map<std::string, std::string> testedNodes;
};
    
} // namespace torrent_node_lib

#endif // TEST_P2P_NODES_H_
