#include "checkVirtual.h"

#include "check.h"
#include "FileSystem.h"

using namespace common;

namespace torrent_node_lib {

static bool isInitialize = false;

static bool isVirtual = false;

void initializeIsVirtualMachine() {
    isVirtual = true;
    for (size_t i = 0; i < 10; i++) {
        const std::string fileName = "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
        if (isFileExist(fileName)) {
            isVirtual = false;
            break;
        }
    }
    
    isInitialize = true;
}

bool isVirtualMachine() {
    CHECK(isInitialize, "Not initialized");
    return isVirtual;
}

}
