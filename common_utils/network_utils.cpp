#include "network_utils.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>

#include "check.h"

namespace common {

std::string getMyIp() {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, "eth0");
    
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    ioctl(s, SIOCGIFADDR, &ifr);
    close(s);
    
    struct sockaddr_in *sa = (struct sockaddr_in*)&ifr.ifr_addr;
    
    return std::string(inet_ntoa(sa->sin_addr));
}

std::string getHostName() {
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    return std::string(hostname);
}

std::string getMyIp2(const std::string &externalServer) {
    /*sockaddr_in srvAddr;
    srvAddr.sin_family = AF_INET;
    //inet_pton(AF_INET, host.c_str(), &(srvAddr.sin_addr));
    hostent *he = gethostbyname(externalServer.c_str());
    CHECK(he != nullptr, "I can not connect to " + externalServer);
    memcpy(&srvAddr.sin_addr, he->h_addr_list[0], he->h_length);*/
    
    const size_t findDots = externalServer.find(":");
    std::string statistics_server = externalServer;
    int statistics_port = 80;
    if (findDots != externalServer.npos) {
        statistics_server = externalServer.substr(0, findDots);
        statistics_port = std::stoi(externalServer.substr(findDots + 1));
    }
    
    struct sockaddr_in serv;
    
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    //Socket could not be created
    if (sock < 0) {
        perror("Socket error");
    }
    
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(statistics_server.c_str());
    serv.sin_port = htons(statistics_port);
    
    int err = connect(sock, (const struct sockaddr*) &serv , sizeof(serv));
    
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (struct sockaddr*) &name, &namelen);
    (void)err;
    
    char buffer[100];
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, 100);
    
    close(sock);
    
    if (p != nullptr) {
        return std::string(buffer);
    } else {
        return "0.0.0.0";
    }
}

}
