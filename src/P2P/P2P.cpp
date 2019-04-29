#include "P2P.h"

#include "check.h"

#include "parallel_for.h"
#include "BlockedQueue.h"
#include "Thread.h"

using namespace common;

namespace torrent_node_lib {

std::vector<P2P::Segment> P2P::makeSegments(size_t countSegments, size_t size, size_t minSize) {
    const size_t step = std::min(size, std::max(size / countSegments, minSize));
    CHECK(step != 0, "step == 0");
    
    std::vector<Segment> answer;
    answer.reserve(countSegments);
    size_t prevByte = 0;
    for (size_t i = 0; i < countSegments - 1; i++) {
        const size_t nextByte = std::min(prevByte + step, size);
        answer.emplace_back(prevByte, nextByte, i);
        prevByte = nextByte;
    }
    answer.emplace_back(prevByte, size, countSegments - 1);
    
    return answer;
}

bool P2P::process(const std::vector<std::reference_wrapper<const Server>> &requestServers, const std::vector<Segment> &segments, const MakeQsAndPostFunction &makeQsAndPost, const RequestFunction &requestFunction) {
    size_t countSuccessRequests = 0;
    size_t countExitThreads = 0;
    std::mutex mut;
    std::condition_variable cond;
    
    BlockedQueue<Segment, 1000> blockedQueue;
       
    std::vector<Thread> threads;
    threads.reserve(requestServers.size());
    for (const Server &server: requestServers) {
        threads.emplace_back([&blockedQueue, &countSuccessRequests, &countExitThreads, &mut, &cond, &requestFunction, &makeQsAndPost, threadNumber=threads.size()](const Server &server){
            try {
                while (true) {
                    Segment segment;
                    const bool isStopped = !blockedQueue.pop(segment);
                    if (isStopped) {
                        break;
                    }
                    
                    const auto &[qs, post] = makeQsAndPost(segment.fromByte, segment.toByte);
                    try {
                        requestFunction(threadNumber, qs, post, server.server, segment);
                        std::lock_guard<std::mutex> lock(mut);
                        countSuccessRequests++;
                        cond.notify_one();
                    } catch (const exception &e) {
                        LOGWARN << "Error " << e;
                        blockedQueue.push(segment);
                        break;
                    }
                    
                    checkStopSignal();
                }
            } catch (const exception &e) {
                LOGERR << e;
            } catch (const StopException &e) {
                LOGINFO << "Stop p2p::request thread";
            } catch (const std::exception &e) {
                LOGERR << e.what();
            } catch (...) {
                LOGERR << "Unknown error";
            }
            std::lock_guard<std::mutex> lock(mut);
            countExitThreads++;
            cond.notify_one();
        },
        server
        );
    }
    
    for (const Segment &segment: segments) {
        blockedQueue.push(segment);
    }
    
    std::unique_lock<std::mutex> lock(mut);
    conditionWait(cond, lock, [&countSuccessRequests, &countExitThreads, countSegments=segments.size(), &threads](){return countSuccessRequests == countSegments || countExitThreads == threads.size(); });
    lock.unlock();
    blockedQueue.stop();
    for (auto &thread: threads) {
        thread.join();
    }
    
    checkStopSignal();
    
    return countSuccessRequests == segments.size();
}

SendAllResult P2P::process(const std::vector<std::reference_wrapper<const Server>> &requestServers, const std::string &qs, const std::string &post, const std::string &header, const RequestFunctionSimple &requestFunction) {
    SendAllResult result;
    std::mutex mutResult;
    
    parallelFor(8, requestServers.begin(), requestServers.end(), [&result, &mutResult, &qs, &post, &header, &requestFunction](const Server &server) {
        Timer tt;
        ResponseParse r;
        try {
            r.response = requestFunction(qs, post, header, server.server);
        } catch (const exception &e) {
            r.error = e;
        }
        tt.stop();
        
        checkStopSignal();
        
        std::lock_guard<std::mutex> lock(mutResult);
        result.results.emplace_back(server.server, r, tt.count());
    });
    
    return result;
}

}
