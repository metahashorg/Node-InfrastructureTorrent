#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

#include <string>
#include <thread>
#include <atomic>
#include <fstream>

#include <libconfig.h++>

#include "cmake_modules/GitSHA1.h"

#include "check.h"
#include "stringUtils.h"
#include "utils/utils.h"
#include "log.h"
#include "utils/FileSystem.h"

#include "duration.h"

#include "synchronize_blockchain.h"

#include "Server.h"

#include "stopProgram.h"
#include "network_utils.h"

#include "P2P/P2P.h"
#include "P2P/P2P_Ips.h"
#include "P2P/P2P_Graph.h"
#include "P2P/P2P_Simple.h"

#include "Modules.h"

using namespace common;
using namespace torrent_node_lib;

static std::atomic<int> countRunningServerThreads(-1);

void crash_handler(int sig) {
    void *array[50];
    const size_t size = backtrace(array, 50);
    
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    signal(SIGINT, nullptr);
    exit(1);
}

static void serverThreadFunc(const Sync &sync, int port) {
    try {
        Server server(sync, port, countRunningServerThreads);
        std::this_thread::sleep_for(1s); // Небольшая задержка сервера перед запуском
        server.start("./");
    } catch (const exception &e) {
        LOGERR << e;
    } catch (const std::exception &e) {
        LOGERR << e.what();
    } catch (...) {
        LOGERR << "Server thread error";
    }
    countRunningServerThreads = 0;
}

static std::vector<std::pair<std::string, std::string>> readServers(const std::string &fileName, size_t port) {
    std::ifstream file(fileName);
    std::string line;
    
    std::vector<std::pair<std::string, std::string>> result;
    while (std::getline(file, line)) {
        line = trim(line);
        if (!line.empty()) {
            const size_t findSpace = line.find(",");
            CHECK(findSpace != line.npos, "Incorrect graph file");
            std::string srv1 = trim(line.substr(0, findSpace));
            srv1 = addDefaultPort(srv1, port);
            std::string srv2 = trim(line.substr(findSpace + 1));
            srv2 = addDefaultPort(srv2, port);
            result.emplace_back(srv1, srv2);
        }
    }

    return result;
}

struct SettingsDb {
    size_t writeBufSizeMb;
    bool isBloomFilter;
    bool isChecks;
    size_t lruCacheMb;
    
    bool isSet = false;
};

static SettingsDb parseSettingsDb(const libconfig::Setting &allSettings, const std::string &prefix) {
    SettingsDb settings;
    if (allSettings.exists(prefix + "write_buffer_size_mb")) {
        settings.writeBufSizeMb = static_cast<int>(allSettings[(prefix + "write_buffer_size_mb").c_str()]);
        settings.lruCacheMb = static_cast<int>(allSettings[(prefix + "lru_cache_mb").c_str()]);
        settings.isBloomFilter = allSettings[(prefix + "is_bloom_filter").c_str()];
        settings.isChecks = allSettings[(prefix + "is_checks").c_str()];
        
        settings.isSet = true;
    }
    return settings;
}

int main (int argc, char *const *argv) {
    //signal(SIGSEGV, crash_handler);
    //signal(SIGABRT, crash_handler);
    
    initializeStopProgram();

    if (argc != 2) {
        std::cout << "path_to_config" << std::endl;
        return -1;
    }
    
    configureLog("./", true, true, false, false);
    
    LOGINFO << "Repository version " << g_GIT_SHA1;
    LOGINFO << "Is local changes " << g_GIT_IS_LOCAL_CHANGES;
    LOGINFO << "Branch " << g_GIT_REFSPEC;
   
    const std::string path_to_config(argv[1]);
    libconfig::Config config;
    try {
        config.readFile(path_to_config.c_str());
    } catch(const libconfig::FileIOException &fioex) {
        LOGERR << std::string("I/O error while reading file. ") + path_to_config;
        return -1;
    } catch(const libconfig::ParseException &pex) {
        std::stringstream sst;
        sst << "Parse error at " << pex.getFile() << ":" << pex.getLine() <<
        " - " << pex.getError() << std::endl;
        LOGERR << sst.str();
        return -1;
    }
    
    try {
        const libconfig::Setting &allSettings = config.getRoot()["Base"];
        
        const std::string pathToBd = allSettings["path_to_bd"];
        const std::string pathToFolder = allSettings["path_to_folder"];
        const size_t countWorkers = static_cast<int>(allSettings["count_threads"]);
        const SettingsDb settingsDb = parseSettingsDb(allSettings, "");
        CHECK(settingsDb.isSet, "settings db not found");
        const SettingsDb settingsStateDb = parseSettingsDb(allSettings, "st_");
        const size_t port = static_cast<int>(allSettings["port"]);
        const bool getBlocksFromFile = allSettings["get_blocks_from_file"];
        const size_t countConnections = static_cast<int>(allSettings["count_connections"]);
        const std::string thisServer = getHostName();
        const std::string latencyFile = allSettings["latency_file"];       
        size_t maxCountElementsBlockCache = 0;
        if (allSettings.exists("max_count_elements_block_cache")) {
            maxCountElementsBlockCache = static_cast<int>(allSettings["max_count_elements_block_cache"]);
        }
        size_t maxCountElementsTxsCache = 0;
        if (allSettings.exists("max_count_blocks_txs_cache")) {
            maxCountElementsTxsCache = static_cast<int>(allSettings["max_count_blocks_txs_cache"]);
        }
        size_t maxLocalCacheElements = 0;
        if (allSettings.exists("mac_local_cache_elements")) {
            maxLocalCacheElements = static_cast<int>(allSettings["mac_local_cache_elements"]);
        }
        if (allSettings.exists("max_local_cache_elements")) {
            maxLocalCacheElements = static_cast<int>(allSettings["max_local_cache_elements"]);
        }
        std::string signKey;
        if (allSettings.exists("sign_key")) {
            signKey = static_cast<const char*>(allSettings["sign_key"]);
        }
        
        BlockVersion blockVersion = BlockVersion::V1;
        if (allSettings.exists("block_version")) {
            const std::string v = allSettings["block_version"];
            if (v == "v1" || v == "V1") {
                blockVersion = BlockVersion::V1;
            } else if (v == "v2" || v == "V2") {
                blockVersion = BlockVersion::V2;
            }
        } else {
            blockVersion = BlockVersion::V1;
        }
        initBlockchainUtils(blockVersion);
        
        bool isValidate = false;
        if (allSettings.exists("validate")) {
            isValidate = allSettings["validate"];
        }
        bool isValidateSign = false;
        if (allSettings.exists("validateSign")) {
            isValidateSign = allSettings["validateSign"];
        }
                
        std::string testNodesServer;
        if (allSettings.exists("test_nodes_result_server")) {
            testNodesServer = static_cast<const char*>(allSettings["test_nodes_result_server"]);
        }
               
        size_t maxAdvancedLoadBlocks = 10;
        if (allSettings.exists("advanced_load_blocks")) {
            maxAdvancedLoadBlocks = static_cast<int>(allSettings["advanced_load_blocks"]);
        }
               
        std::set<std::string> modulesStr;
        for (const std::string &moduleStr: allSettings["modules"]) {
            modulesStr.insert(moduleStr);
        }
        
        parseModules(modulesStr);
        LOGINFO << "Modules " << modules;
        
        std::unique_ptr<P2P> p2p = nullptr;
        
        size_t otherPortTorrent = port;
        if (allSettings.exists("other_torrent_port")) {
            otherPortTorrent = static_cast<int>(allSettings["other_torrent_port"]);
        }

        const std::string myIp = trim(getMyIp2("172.104.236.166:5797")) + ":" + std::to_string(port);

        if (allSettings["servers"].isArray()) {
            std::vector<std::string> serversStr;
            for (const std::string &serverStr: allSettings["servers"]) {
                serversStr.push_back(serverStr);
            }
            p2p = std::make_unique<P2P_Ips>(serversStr, countConnections);
        } else {
            const std::string &fileName = allSettings["servers"];
            const std::vector<std::pair<std::string, std::string>> serversGraph = readServers(fileName, otherPortTorrent);
            p2p = std::make_unique<P2P_Graph>(serversGraph, myIp, countConnections);
        }

        Sync sync(
            pathToFolder, 
            LevelDbOptions(settingsDb.writeBufSizeMb, settingsDb.isBloomFilter, settingsDb.isChecks, getFullPath("simple", pathToBd), settingsDb.lruCacheMb),
            CachesOptions(maxCountElementsBlockCache, maxCountElementsTxsCache, maxLocalCacheElements),
            GetterBlockOptions(maxAdvancedLoadBlocks, p2p.get(), getBlocksFromFile, isValidate, isValidateSign),
            signKey,
            TestNodesOptions(otherPortTorrent, myIp, testNodesServer)
        );
        if (settingsStateDb.isSet) {
            sync.setLeveldbOptScript(LevelDbOptions(settingsStateDb.writeBufSizeMb, settingsStateDb.isBloomFilter, settingsStateDb.isChecks, getFullPath("states", pathToBd), settingsStateDb.lruCacheMb));
        }
        if (settingsStateDb.isSet) {
            sync.setLeveldbOptNodeTest(LevelDbOptions(settingsStateDb.writeBufSizeMb, settingsStateDb.isBloomFilter, settingsStateDb.isChecks, getFullPath("nodeTest", pathToBd), settingsStateDb.lruCacheMb));
        }
        
        //LOGINFO << "Is virtual machine: " << sync.isVirtualMachine();
        
        std::thread serverThread(serverThreadFunc, std::cref(sync), port);
        serverThread.detach();
        
        //sync.addUsers({Address("0x0049704639387c1ae22283184e7bc52d38362ade0f977030e6"), Address("0x0034d209107371745c6f5634d6ed87199bac872c310091ca56")});
        
        sync.synchronize(countWorkers);
        
        while(countRunningServerThreads.load() != 0);
        LOGINFO << "Stop server thread";       
    } catch (const libconfig::SettingNotFoundException &e) {
        LOGERR << std::string("Attribute ") + std::string(e.getPath()) + std::string(" not found");
    } catch (const libconfig::SettingException &e) {
        LOGERR << std::string(e.what()) + " " + std::string(e.getPath());
    } catch (const libconfig::ConfigException &e) {
        LOGERR << std::string(e.what());
    } catch (const exception &e) {
        LOGERR << e;
    } catch (const std::runtime_error &e) {
        LOGERR << e.what();
    } catch (...) {
        LOGERR << "Unknown error ";
    }
    return 0;
}
