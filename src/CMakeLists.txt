include_directories(. "/usr/local/include")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/secp256k1/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/openssl/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/leveldb/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/rapidjson/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/libconfig/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/lz4/include/)
include_directories(${SNIPER_INCLUDE_DIRS})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../common_utils/")
include(common_utils)

add_definitions(-DSNIPER_LOG_TRACE -DSNIPER_LOG_DEBUG)

add_definitions(-DVERSION="0.80")

set(COMMON_SOURCES
    ${COMMON_UTILS_SOURCES_WITHOUT_GITSHA}
)

set(COMMON_LIBRARY_HEADERS
    check.h
    duration.h
    log.h
    parallel_for.h
    OopUtils.h
    stopProgram.h
    stringUtils.h
    jsonUtils.h
    convertStrings.h
    BigInteger.h
    network_utils.h
    Thread.h
    logger/easylogging++.h
    logger/inc.h
)

set(COMMON_LIBRARY_DIR
    ../common_utils/
)

set(LIBRARY_HEADERS
    BlockInfo.h
    BlockChainReadInterface.h
    P2P/P2P.h
    P2P/P2P_Ips.h
    P2P/P2P_Simple.h
    Modules.h
    ConfigOptions.h
    synchronize_blockchain.h
)

set(LIBRARY_SOURCES   
    Modules.cpp
    BlockChain.cpp

    BlockchainUtils.cpp
    BlockchainRead.cpp
    PrivateKey.cpp
    
    P2P/P2P.cpp
    P2P/P2P_Ips.cpp
    
    BlockSource/GetNewBlocksFromServers.cpp
    BlockSource/FileBlockSource.cpp
    BlockSource/NetworkBlockSource.cpp

    Address.cpp
    BlockInfo.cpp

    Cache/Cache.cpp
    Cache/LocalCache.cpp

    synchronize_blockchain.cpp
    SyncImpl.cpp
    
    Workers/NodeTestsBlockInfo.cpp
    Workers/WorkerMain.cpp
    Workers/WorkerCache.cpp
    Workers/WorkerNodeTest.cpp

    TestP2PNodes.cpp
    
    LevelDb.cpp
    libeventWrapper.cpp
    
    utils/FileSystem.cpp
    utils/checkVirtual.cpp
    utils/Heap.cpp
    utils/utils.cpp
    utils/benchmarks.cpp
    utils/compress.cpp
    utils/SystemInfo.cpp
)

set(PROJECT_MAIN
    main.cpp
    
    ${COMMON_UTILS_GITSHA}
        
    generate_json.cpp
    Server.cpp
        
    utils/Graph.cpp
    P2P/P2P_Graph.cpp
)

#Threads
find_package(Threads)

find_library(LEVELDB NAMES libleveldb.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/leveldb/lib)
find_library(LEVELDB_SNAPPY NAMES libsnappy.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/leveldb/lib)
find_library(LEVELDB_MEMENV NAMES libmemenv.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/leveldb/lib)

find_library(SECP_LIB NAMES libsecp256k1.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/secp256k1/lib/)

find_library(SSL_LIB_1 NAMES libcrypto.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/openssl/lib/)
find_library(SSL_LIB_2 NAMES libssl.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/openssl/lib/)
find_library(LIBCONFIG NAMES libconfig++.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/libconfig/lib/)

find_library(CONFIG1_LIB NAMES libconfig++.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/libconfig/lib/)
find_library(CONFIG2_LIB NAMES libconfig.a HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../3rdParty/libconfig/lib/)

find_library(LZ4_LIB NAMES lib_lz4.a HINTS ../3rdParty/lz4/lib/)

find_library(FMT_LIB NAMES libfmt.a HINTS /usr/local/lib)

set(PROJECT_LIBS
    ${LZ4_LIB}
    
    ${CONFIG1_LIB}
    ${CONFIG2_LIB}

    stdc++fs.a
    
    ${LEVELDB_MEMENV}
    ${LEVELDB}
    ${LEVELDB_SNAPPY}
        
    ${SNIPER_LIBRARIES}

    ${SECP_LIB}
    ${SSL_LIB_2}
    ${SSL_LIB_1}

    gmp

    dl
        
    event.a
    microhttpd.a
    gcrypt
    gnutls
    uriparser.a

    curl

    ${CMAKE_THREAD_LIBS_INIT}

    ${FMT_LIB}
)

add_library(${PROJECT_NAME}_lib STATIC ${LIBRARY_HEADERS} ${LIBRARY_SOURCES})
target_compile_options(${PROJECT_NAME}_lib PRIVATE -Wno-suggest-final-types )
target_compile_options(${PROJECT_NAME}_lib PRIVATE -Wno-unused-parameter -g -rdynamic -Wno-unused-variable)
#target_compile_options(${PROJECT_NAME}_lib PRIVATE -no-pie -D_GLIBCXX_USE_CXX11_ABI=0)
Install(TARGETS ${PROJECT_NAME}_lib DESTINATION ${CMAKE_BINARY_DIR}/lib/sync)
foreach ( file ${LIBRARY_HEADERS} )
    get_filename_component( dir ${file} DIRECTORY )
    install( FILES ${file} DESTINATION ${CMAKE_BINARY_DIR}/lib/sync/${dir} )
endforeach()
Install(FILES ${LEVELDB} DESTINATION ${CMAKE_BINARY_DIR}/lib/sync)
Install(FILES ${LEVELDB_SNAPPY} DESTINATION ${CMAKE_BINARY_DIR}/lib/sync)
Install(FILES ${LEVELDB_MEMENV} DESTINATION ${CMAKE_BINARY_DIR}/lib/sync)

add_library(common STATIC ${COMMON_SOURCES})
target_compile_options(common PRIVATE -Wno-suggest-final-types)
target_compile_options(common PRIVATE -Wno-unused-parameter -g -rdynamic -Wno-unused-variable)
#target_compile_options(common PRIVATE -no-pie -D_GLIBCXX_USE_CXX11_ABI=0)
Install(TARGETS common DESTINATION ${CMAKE_BINARY_DIR}/lib/common)
foreach ( file ${COMMON_LIBRARY_HEADERS} )
    get_filename_component( dir ${file} DIRECTORY )
    install( FILES ${COMMON_LIBRARY_DIR}/${file} DESTINATION ${CMAKE_BINARY_DIR}/lib/common/${dir} )
endforeach()

#APP
add_executable(${PROJECT_NAME} ${PROJECT_MAIN})
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-suggest-final-types)
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unused-parameter -g -rdynamic)
#target_compile_options(${PROJECT_NAME} PRIVATE -no-pie -D_GLIBCXX_USE_CXX11_ABI=0)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-g -rdynamic")
#set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-no-pie -D_GLIBCXX_USE_CXX11_ABI=0")
target_link_libraries(${PROJECT_NAME} ${PROJECT_NAME}_lib common)
target_link_libraries(${PROJECT_NAME} ${PROJECT_LIBS})
