#include "curlWrapper.h"

#include <curl/curl.h>

#include "check.h"
#include "OopUtils.h"

#include <memory>

namespace common {

std::mutex Curl::initializeMut;
std::atomic<bool> Curl::isInitialized(false);

static int writer(char *data, size_t size, size_t nmemb, std::string *buffer) {
    int result = 0;
    if (buffer != NULL) {
        buffer->append(data, size * nmemb);
        result = size * nmemb;
    }
    return result;
}

void Curl::initialize() {
    std::lock_guard<std::mutex> lock(initializeMut);
    if (isInitialized.load()) {
        return;
    }
    curl_global_init(CURL_GLOBAL_ALL);
    isInitialized = true;
}

void Curl::destroy() {
    //c не удаляем, чтобы один поток не почистил ресурсы, пока им пользуется другой поток
    // curl_global_cleanup();
}

Curl::CurlInstance Curl::getInstance() {
    CHECK(isInitialized.load(), "not initialized");
    
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), DELETER(curl_easy_cleanup));
    CHECK(curl != nullptr, "curl == nullptr");
    
    return CurlInstance(std::move(curl));
}

std::string Curl::request(const Curl::CurlInstance& instance, const std::string& url, const std::string& postData, const std::string& header, const std::string& password, int timeoutSec) {
    CHECK(isInitialized.load(), "not initialized");
    
    std::unique_lock<std::mutex> lock(instance.mut, std::try_to_lock);
    CHECK(lock.owns_lock(), "Curl instanse one of thread");
    
    CHECK(instance.curl != nullptr, "Incorrect curl instance");
    CURL* curl = instance.curl.get();
    CHECK(curl != nullptr, "curl == nullptr");
    
    /* enable TCP keep-alive for this transfer */
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    /* keep-alive idle time to 120 seconds */
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
    /* interval time between keep-alive probes: 60 seconds */
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
    
    std::string buffer;
    std::unique_ptr<struct curl_slist, decltype(&curl_slist_free_all)> headers(nullptr, curl_slist_free_all);
    if (!header.empty()) {
        headers.reset(curl_slist_append(headers.get(), header.c_str()));
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (!postData.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postData.size());
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    if (!password.empty()) {
        curl_easy_setopt(curl, CURLOPT_USERPWD, password.c_str());
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
    }
    
    if (timeoutSec != 0) {
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    }
    
    const CURLcode res = curl_easy_perform(curl);
    CHECK(res == CURLE_OK, "curl_easy_perform() failed: " + std::string(curl_easy_strerror(res)));
    
    return buffer;
}

std::string Curl::request(const Curl::CurlInstance& instance, const std::string& url, const std::string& postData, const std::string& header, const std::string& password) {
    return request(instance, url, postData, header, password, 0);
}

std::string Curl::request(const std::string& url, const std::string &postData, const std::string& header, const std::string& password) {
    const CurlInstance ci = getInstance();
    return request(ci, url, postData, header, password);
}

std::string Curl::request(const std::string& url, const std::string &postData, const std::string& header, const std::string& password, int timeoutSec) {
    const CurlInstance ci = getInstance();
    return request(ci, url, postData, header, password, timeoutSec);
}

}
