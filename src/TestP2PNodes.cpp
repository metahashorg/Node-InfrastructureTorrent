#include "TestP2PNodes.h"

#include <random>
#include <iomanip>

#include "P2P/P2P.h"
#include "Workers/WorkerNodeTest.h"

#include "PrivateKey.h"

#include "stopProgram.h"
#include "check.h"
#include "log.h"
#include "convertStrings.h"
#include "curlWrapper.h"
#include "libeventWrapper.h"
#include "stringUtils.h"
#include "utils/utils.h"

using namespace common;

namespace torrent_node_lib {
    
static std::map<std::string, std::string> readFileTestedNodes(const std::string &fileName) {
    std::map<std::string, std::string> nodes;
    std::ifstream file(fileName);
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (!line.empty()) {
            const auto foundDelim = line.find(" ");
            
            nodes.emplace(line.substr(0, foundDelim), line.substr(foundDelim + 1));
        }
    }
    file.close();
    return nodes;
}
    
TestP2PNodes::TestP2PNodes(P2P *p2p, const std::string &myIp, const std::string &resultServer, size_t defaultPortTorrent)
    : p2p(p2p)
    , myIp(myIp)
    , resultServer(resultServer)
    , defaultPortTorrent(defaultPortTorrent)
{
    testedNodes = readFileTestedNodes("tested_nodes.txt");
    for(auto iter = testedNodes.begin(); iter != testedNodes.end(); ) {
        if (iter->first.find(myIp) != iter->first.npos) {
            testedNodes.erase(iter++);
        } else {
            ++iter;
        }
    }
    
    LibEvent::initialize();
}

static std::string makeRandomString(size_t size) {
    std::random_device rd;
    std::mt19937 rnd(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::string result;
    result.reserve(size);
    for (size_t i = 0; i < size; i++) {
        result += (char)dis(rnd);
    }
    return result;
}

struct TestResult {
    std::optional<milliseconds> time;
    size_t size;
    
    TestResult(const milliseconds &time, size_t size) 
        : time(time)
        , size(size)
    {}
    
    TestResult(size_t size) 
        : size(size)
    {}
};

struct TestForSend {
    std::string fromAddress;
    std::string fromName;
    std::string toAddress;
    std::string toName;
    size_t sizeData;
    milliseconds time;
    bool isSuccess;
    
    std::string sign;
    std::string pubkey;
    
    std::string messageFromSign() const {
        return fromAddress + ";" + fromName + ";" + toAddress + ";" + toName + ";" + std::to_string(sizeData) + ";" + std::to_string(time.count()) + ";" + std::to_string(isSuccess ? 1 : 0);
    }
};

static std::string genTestsResultResponse(const std::vector<TestForSend> &testsResults) {
    std::stringstream ss;
    ss << "{'result': [";
    bool isFirst = true;
    for (const TestForSend &result: testsResults) {
        if (!isFirst) {
            ss << ",";
        }
        
        ss << "{";
        ss << "'fromAddress': " << std::quoted(result.fromAddress, '\'') << ",";
        ss << "'fromName': " << std::quoted(result.fromName, '\'') << ",";
        ss << "'toAddress': " << std::quoted(result.toAddress, '\'') << ",";
        ss << "'toName': " << std::quoted(result.toName, '\'') << ",";
        ss << "'sizeData': " << "'" << result.sizeData << "'" << ",";
        ss << "'time': " << "'" << result.time.count() << "'" << ",";
        ss << "'isSuccess': " << "'" << result.isSuccess << "'" << ",";
        ss << "'sign': " << "'" << result.sign << "'" << ",";
        ss << "'pubkey': " << "'" << result.pubkey << "'";
        ss << "}";
        
        isFirst = false;
    }
    ss << "]}";
    return ss.str();
}

static std::map<std::string, std::string> transformMap(const std::map<std::string, std::string> &map, size_t defaultPortTorrent) {
    std::map<std::string, std::string> result;
    for (const auto &[key, value]: map) {
        result.emplace(addDefaultPort(key, defaultPortTorrent), value);
    }
    return result;
}

static void sendTests(LibEvent::LibEventInstance &libevent, const std::map<std::string, std::vector<TestResult>> &results, const PrivateKey &privateKey, const std::string &myIp, const std::map<std::string, std::string> &nodesForTest, const std::string &resultServer, size_t size) {
    std::vector<TestForSend> resultsForSend;
    for (const auto &[key, value]: results) {
        for (const TestResult &res: value) {
            if (res.size != size) {
                continue;
            }
            TestForSend r;
            r.fromName = privateKey.get_address();
            r.fromAddress = myIp;
            r.isSuccess = res.time.has_value();
            r.pubkey = toHex(privateKey.public_key());
            r.sizeData = res.size;
            if (res.time.has_value()) {
                r.time = res.time.value();
            }
            r.toAddress = key;
            const auto found = nodesForTest.find(key);
            if (found != nodesForTest.end()) {
                r.toName = found->second;
            }
            
            const std::string toSign = r.messageFromSign();
            r.sign = makeTestResultSign(toSign, privateKey);
            
            resultsForSend.emplace_back(r);
        }
    }
    
    const std::string toSend = genTestsResultResponse(resultsForSend);
    
    const std::string response = LibEvent::request(libevent, resultServer, toSend, 10);
    //const std::string response = Curl::request(resultServer, toSend, "", "", 10);
}

void TestP2PNodes::work() {
    while (true) {
        try {
            while (true) {
                CHECK(p2p != nullptr, "P2P not set");
                
                std::unique_lock<std::mutex> lock(workerTestPtrMut);
                CHECK(workerTestPtr != nullptr, "Worker test not setted");
                const WorkerNodeTest &workerTest = *workerTestPtr;
                lock.unlock();
                
                //const std::map<std::string, std::string> nodesForTest = transformMap(workerTest.getAllNodes(), defaultPortTorrent);
                const std::map<std::string, std::string> nodesForTest = transformMap(testedNodes, defaultPortTorrent);
                
                std::set<std::string> nodesForTestSet;
                using MapIter = std::decay_t<decltype(*nodesForTest.begin())>;
                std::transform(nodesForTest.begin(), nodesForTest.end(), std::inserter(nodesForTestSet, nodesForTestSet.end()), std::mem_fn(&MapIter::first));
                
                std::map<std::string, std::vector<TestResult>> testsResults;
                
                LOGINFO << "Torrents test start. " << nodesForTest.size();
                
                LibEvent::LibEventInstance libevent = LibEvent::getInstance();
                
                Timer tt;
                const auto tests = {1 * 1024, 2 * 1024, 4 * 1024, 8 * 1024, 16 * 1024};
                for (size_t sizeStr: tests) {
                    const std::string body = makeRandomString(sizeStr);
                    const std::string qs = "sign-test-string";
                    
                    const SendAllResult testResult = p2p->requestAll(qs, body, "", nodesForTestSet);
                    for (const SendAllResult::SendOneResult &res: testResult.results) {
                        if (!res.response.error.has_value()) {
                            bool isSuccess = true;
                            try {
                                checkSignatureTest(body, res.response.response);
                            } catch (const exception &e) {
                                isSuccess = false;
                            }
                            if (isSuccess) {
                                testsResults[res.server].emplace_back(res.time, sizeStr);
                            } else {
                                testsResults[res.server].emplace_back(sizeStr);
                            }
                        } else {
                            testsResults[res.server].emplace_back(sizeStr);
                        }
                    }
                    
                    CHECK(privateKey != nullptr, "Private key not setted");
                    sendTests(libevent, testsResults, *privateKey, myIp, nodesForTest, resultServer, sizeStr);                   
                    
                    checkStopSignal();
                }
                tt.stop();
                
                const size_t allServers = testsResults.size();
                const size_t lifeServers = std::accumulate(testsResults.begin(), testsResults.end(), 0, [](size_t prev, const auto &pair) {
                    const size_t countAnswers = std::accumulate(pair.second.begin(), pair.second.end(), 0, [](size_t prev, const TestResult &res) {
                        return prev + (res.time.has_value() ? 1 : 0);
                    });
                    return prev + (countAnswers > 0 ? 1 : 0);
                });
                const size_t completeServers = std::accumulate(testsResults.begin(), testsResults.end(), 0, [&tests](size_t prev, const auto &pair) {
                    const size_t countAnswers = std::accumulate(pair.second.begin(), pair.second.end(), 0, [](size_t prev, const TestResult &res) {
                        return prev + (res.time.has_value() ? 1 : 0);
                    });
                    return prev + (countAnswers == tests.size() ? 1 : 0);
                });
                                
                LOGINFO << "Torrents test complete. " << tt.countMs() << ". " << allServers << " " << lifeServers << " " << completeServers;
                
                sleep(hours(1));
            }
        } catch (const exception &e) {
            LOGERR << "Error: " << e;
        } catch (const std::exception &e) {
            LOGERR << "Error: " << e.what();
        } catch (const StopException &) {
            break;
        } catch (...) {
            LOGERR << "Error: Unknown exception";
        }
        
        sleep(3s);
    }
    
    LOGINFO << "TestP2PNodes thread stopped";
}
    
void TestP2PNodes::start() {
    if (!resultServer.empty()) {
        thread = Thread(&TestP2PNodes::work, this);
    }
}

void TestP2PNodes::addWorkerTest(const WorkerNodeTest *workerTest) {
    std::lock_guard<std::mutex> lock(workerTestPtrMut);
    workerTestPtr = workerTest;
}

} // namespace torrent_node_lib
