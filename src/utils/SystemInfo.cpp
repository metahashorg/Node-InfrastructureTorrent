#include "SystemInfo.h"

#include <array>

#include <sys/time.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <iostream>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>

static std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

typedef struct {
    long size=0,resident=0,share=0,text=0,lib=0,data=0,dt=0;
} statm_t;

static void read_off_memory_status(statm_t& result) {
    const char* statm_path = "/proc/self/statm";
    
    FILE *f = fopen(statm_path,"r");
    if(f){
        if(7 != fscanf(f,"%ld %ld %ld %ld %ld %ld %ld",
            &result.size,&result.resident,&result.share,&result.text,&result.lib,&result.data,&result.dt))
        {
        }
    }
    fclose(f);
}

unsigned long long getTotalSystemMemory() {
    statm_t result;
    read_off_memory_status(result);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return result.size * page_size;
}

double getProcLoad() {
    std::array<double, 3> stat;
    const int res = getloadavg(stat.data(), stat.size());
    if (res == -1) {
        return 0.;
    }
    return stat[0];
}

int getOpenedConnections() {
    const std::string result = exec("netstat -tnap | wc -l");
    return std::stoi(result);
}
