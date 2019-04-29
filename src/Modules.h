#ifndef MODULES_H_
#define MODULES_H_

#include <bitset>
#include <string>
#include <set>

namespace torrent_node_lib {
    
using Modules = std::bitset<8>;

extern const int MODULE_BLOCK;
extern const int MODULE_BLOCK_RAW;
extern const int MODULE_USERS;
extern const int MODULE_NODE_TEST;

extern const std::string MODULE_BLOCK_STR;
extern const std::string MODULE_BLOCK_RAW_STR;
extern const std::string MODULE_USERS_STR;
extern const std::string MODULE_NODE_TEST_STR;

void parseModules(const std::set<std::string> &modulesStrs);

extern Modules modules;

}

#endif // MODULES_H_
