#include "FileSystem.h"

#include <iostream>
#include <set>
#include <fstream>
#include <experimental/filesystem>

#include "check.h"

#include "stringUtils.h"

#include "BlockInfo.h"

using namespace common;

namespace torrent_node_lib {

namespace fs = std::experimental::filesystem;

CroppedFileName::CroppedFileName(const std::string& name) 
    : name(getBasename(name))
{}

const std::string& CroppedFileName::str() const {
    return name;
}

bool CroppedFileName::operator==(const CroppedFileName& second) const {
    return name == second.name;
}

bool CroppedFileName::operator<(const CroppedFileName& second) const {
    return name < second.name;
}

void createDirectories(const std::string& folder) {
    fs::create_directories(folder);
}

std::string getBasename(const std::string &path) {
    const auto p = fs::path(path);
    return p.filename();
}

std::string getFullPath(const std::string &fileName, const std::string &folder) {
    return fs::path(folder) / fs::path(fileName);
}

FileInfo getNextFile(const std::unordered_map<CroppedFileName, FileInfo> &processedFiles, const std::string &folder) {
    std::set<CroppedFileName> files;
    for(const auto& p: fs::directory_iterator(folder)) {
        const CroppedFileName baseName(p.path().filename());
        if (baseName.str().find(".blk") != baseName.str().npos) {
            files.insert(baseName);
        }
    }
    
    for (const CroppedFileName &file: files) {
        auto p = fs::path(folder);
        p /= file.str();
        
        if (processedFiles.find(file) == processedFiles.end()) {
            FileInfo fileInfo;
            fileInfo.filePos.fileName = p;
            fileInfo.filePos.pos = 0;
            
            return fileInfo;
        }
        const size_t fsize = fs::file_size(p);
        const FileInfo &fileInfo = processedFiles.at(file);
        if (fileInfo.filePos.pos < fsize) {
            return fileInfo;
        }
    }
    return FileInfo();
}

std::vector<std::string> getAllFilesRelativeInDir(const std::string &dirPath) {
    std::vector<std::string> listOfFiles;
    if (fs::exists(dirPath) && fs::is_directory(dirPath)) {
        fs::recursive_directory_iterator iter(dirPath);
        fs::recursive_directory_iterator end;
        
        while (iter != end) {
            if (fs::is_directory(iter->path())) {
                iter.disable_recursion_pending();
            } else {
                std::error_code ec;
                std::string r = fs::absolute(iter->path().string());
                r = r.substr(fs::absolute(dirPath).string().size());
                listOfFiles.push_back(r);
            }
            
            std::error_code ec;
            iter.increment(ec);
            CHECK(!ec, "Error While Accessing : " + iter->path().string() + " :: " + ec.message());
        }
    }
    return listOfFiles;
}

void saveToFile(const std::string& folder, const std::string& fileName, const std::string& data) {
    const std::string fullPath = getFullPath(fileName, folder);
    
    fs::create_directories(fs::path(fullPath).parent_path());
    
    std::ofstream file(fullPath);
    file << data;
    file.close();
}

void removeFile(const std::string& folder, const std::string& fileName) {
    const std::string fullPath = getFullPath(fileName, folder);
       
    if (fs::exists(fullPath)) {
        fs::remove(fullPath);
    }
}

std::string loadFile(const std::string& folder, const std::string& fileName) {
    const std::string fullPath = getFullPath(fileName, folder);
    
    std::ifstream file(fullPath);
    std::string content((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));
    return content;
}

bool isFileExist(const std::string &file) {
    return fs::exists(file);
}

}
