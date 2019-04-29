#include "libeventWrapper.h"

#include <mh/libevent/LibEvent.h>

#include "check.h"
#include "OopUtils.h"

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
    
    std::unique_ptr<mh::libevent::LibEvent> libevent = std::make_unique<mh::libevent::LibEvent>();
    CHECK(libevent != nullptr, "libevent == nullptr");
    
    return LibEventInstance(std::move(libevent));
}

static std::string deleteFromBeginUri(const std::string &uri, const std::string &deletedString) {
    if (uri.substr(0, deletedString.size()) == deletedString) {
        return uri.substr(deletedString.size());
    } else {
        return uri;
    }
}

static std::string deleteHttpShemeFromUri(const std::string &uri) {
    const std::string HTTP_SHEME("http://");
    return deleteFromBeginUri(uri, HTTP_SHEME);
}

std::string getDomainFromUri(const std::string &uri) {
    const std::string &uriWithoutSheme = deleteHttpShemeFromUri(uri);
    const size_t dotsPos = uriWithoutSheme.find(":");
    if (dotsPos != uriWithoutSheme.npos) {
        return uriWithoutSheme.substr(0, dotsPos);
    } else {
        const size_t slashPos = uriWithoutSheme.find("/");
        if (slashPos != uriWithoutSheme.npos) {
            return uriWithoutSheme.substr(0, slashPos);
        } else {
            return uriWithoutSheme;
        }
    }
}

std::string getPortFromUri(const std::string &uri) {
    const std::string &uriWithoutSheme = deleteHttpShemeFromUri(uri);
    const std::string &domain = getDomainFromUri(uri);
    const std::string &uriWithoutDomain = deleteFromBeginUri(uriWithoutSheme, domain);
    
    if (uriWithoutDomain.empty()) {
        return "";
    }
    if (uriWithoutDomain[0] != ':') {
        return "";
    }
    size_t slashPos = uriWithoutDomain.find("/");
    if (slashPos == uriWithoutDomain.npos) {
        slashPos = uriWithoutDomain.size();
    }
    return uriWithoutDomain.substr(1, slashPos - 1);
}

std::string getPathFromUri(const std::string &uri) {
    const std::string &uriWithoutSheme = deleteHttpShemeFromUri(uri);
    const std::string &domain = getDomainFromUri(uri);
    const std::string &uriWithoutDomain = deleteFromBeginUri(uriWithoutSheme, domain);
    const std::string &port = getPortFromUri(uri);
    std::string uriWithoutPort = uriWithoutDomain;
    if (!port.empty()) {
        uriWithoutPort = deleteFromBeginUri(uriWithoutDomain, ":" + port);
    }
    
    if (uriWithoutPort.empty()) {
        return "/";
    } else {
        return uriWithoutPort;
    }
}

std::string LibEvent::request(const LibEvent::LibEventInstance& instance, const std::string& url, const std::string& postData, size_t timeoutSec) {
    CHECK(isInitialized.load(), "not initialized");
    
    std::unique_lock<std::mutex> lock(instance.mut, std::try_to_lock);
    CHECK(lock.owns_lock(), "Curl instanse one of thread");
    
    CHECK(instance.libevent != nullptr, "Incorrect curl instance");
    mh::libevent::LibEvent &libevent = *instance.libevent.get();
    
    const std::string host = getDomainFromUri(url);
    const std::string portStr = getPortFromUri(url);
    const std::string path = getPathFromUri(url);
    int port = 80;
    if (!portStr.empty()) {
        port = std::stoi(portStr);
    }
        
    std::string response;
    libevent.post_keep_alive(host, port, host, path, postData, response, timeoutSec * 1000);
    
    return response;
}

LibEvent::LibEventInstance::LibEventInstance()
    : libevent(nullptr)
{}

LibEvent::LibEventInstance::LibEventInstance(std::unique_ptr< mh::libevent::LibEvent > &&libevent)
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

