#include "Modules.h"

#include "check.h"

using namespace common;

namespace torrent_node_lib {

const int MODULE_BLOCK = 0;
const int MODULE_BLOCK_RAW = 4;
const int MODULE_USERS = 6;
const int MODULE_NODE_TEST = 7;

const std::string MODULE_BLOCK_STR = "block";
const std::string MODULE_BLOCK_RAW_STR = "block_raw";
const std::string MODULE_USERS_STR = "users";
const std::string MODULE_NODE_TEST_STR = "node_tests";

Modules modules;

void parseModules(const std::set<std::string>& modulesStrs) {
    modules = Modules();
    for (const std::string &str: modulesStrs) {
        if (str == MODULE_BLOCK_STR) {
            modules.set(MODULE_BLOCK);
        } else if (str == MODULE_BLOCK_RAW_STR) {
            modules.set(MODULE_BLOCK_RAW);
        } else if (str == MODULE_USERS_STR) {
            modules.set(MODULE_USERS);
        } else if (str == MODULE_NODE_TEST_STR) {
            modules.set(MODULE_NODE_TEST);
        } else {
            throwErr("Incorrect module " + str);
        }
    }
}

}
