#include "libeventWrapper.h"

#include <sniper/http/SyncClient.h>

#include "check.h"
#include "OopUtils.h"
#include "duration.h"

#include "log.h"

#include <memory>
#include <algorithm>

std::mutex LibEvent::initializeMut;
std::atomic<bool> LibEvent::isInitialized(false);


void LibEvent::initialize() {
    std::lock_guard<std::mutex> lock(initializeMut);
    if (isInitialized.load()) {
        return;
    }

    isInitialized = true;
}

void LibEvent::destroy() {
    //c не удаляем, чтобы один поток не почистил ресурсы, пока им пользуется другой поток
    // curl_global_cleanup();
}

LibEvent::LibEventInstance LibEvent::getInstance() {
    CHECK(isInitialized.load(), "not initialized");
    
    std::unique_ptr<sniper::http::SyncClient> libevent = std::make_unique<sniper::http::SyncClient>(10s);
    CHECK(libevent != nullptr, "libevent == nullptr");
    
    return LibEventInstance(std::move(libevent));
}

std::string LibEvent::request(const LibEvent::LibEventInstance& instance, const std::string& url, const std::string& postData, size_t timeoutSec) {
    CHECK(isInitialized.load(), "not initialized");
    
    std::unique_lock<std::mutex> lock(instance.mut, std::try_to_lock);
    CHECK(lock.owns_lock(), "Curl instanse one of thread");
    
    CHECK(instance.libevent != nullptr, "Incorrect curl instance");
    sniper::http::SyncClient &libevent = *instance.libevent.get();

    const auto response = libevent.post(url, postData);

    return std::string(response->data());
}

LibEvent::LibEventInstance::LibEventInstance()
    : libevent(nullptr)
{}

LibEvent::LibEventInstance::LibEventInstance(std::unique_ptr<sniper::http::SyncClient> &&libevent)
    : libevent(std::move(libevent))
{}

LibEvent::LibEventInstance::LibEventInstance(LibEvent::LibEventInstance &&second)
    : libevent(std::move(second.libevent))
{}

LibEvent::LibEventInstance &LibEvent::LibEventInstance::operator=(LibEvent::LibEventInstance &&second) {
    this->libevent = std::move(second.libevent);
    return *this;
}

LibEvent::LibEventInstance::~LibEventInstance() = default;

