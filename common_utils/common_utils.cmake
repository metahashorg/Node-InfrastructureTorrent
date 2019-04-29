include_directories(${CMAKE_CURRENT_LIST_DIR})

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake_modules/")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
git_local_changes(GIT_LOCAL_CHANGES)

configure_file("${CMAKE_CURRENT_LIST_DIR}/cmake_modules/GitSHA1.cpp.in" "${CMAKE_BINARY_DIR}/GitSHA1.cpp" @ONLY)

set(COMMON_UTILS_SOURCES 
    ${CMAKE_CURRENT_LIST_DIR}/curlWrapper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/jsonUtils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/log.cpp
    ${CMAKE_CURRENT_LIST_DIR}/network_utils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/stopProgram.cpp
    ${CMAKE_CURRENT_LIST_DIR}/logger/easylogging++.cc
    ${CMAKE_BINARY_DIR}/GitSHA1.cpp
)

set(COMMON_UTILS_SOURCES_WITHOUT_GITSHA 
    ${CMAKE_CURRENT_LIST_DIR}/curlWrapper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/jsonUtils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/log.cpp
    ${CMAKE_CURRENT_LIST_DIR}/network_utils.cpp
    ${CMAKE_CURRENT_LIST_DIR}/stopProgram.cpp
    ${CMAKE_CURRENT_LIST_DIR}/logger/easylogging++.cc
)

set(COMMON_UTILS_GITSHA
    ${CMAKE_BINARY_DIR}/GitSHA1.cpp
)
