#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include <unordered_map>
#include <string>
#include <vector>

namespace torrent_node_lib {

struct FileInfo;

std::string getBasename(const std::string &path);

struct CroppedFileName {
public:
    
    CroppedFileName() = default;
    
    explicit CroppedFileName(const std::string &name);
    
    const std::string& str() const;
    
    bool operator==(const CroppedFileName &second) const;
    
    bool operator<(const CroppedFileName &second) const;
    
private:
    
    std::string name;
};

void createDirectories(const std::string& folder);

std::string getFullPath(const std::string &fileName, const std::string &folder);

FileInfo getNextFile(const std::unordered_map<CroppedFileName, FileInfo> &processedFiles, const std::string &folder);

std::vector<std::string> getAllFilesRelativeInDir(const std::string &dirPath);

void saveToFile(const std::string &folder, const std::string &fileName, const std::string &data);

void removeFile(const std::string &folder, const std::string &fileName);

std::string loadFile(const std::string &folder, const std::string &fileName);

bool isFileExist(const std::string &file);

}

namespace std {
    
template <>
struct hash<torrent_node_lib::CroppedFileName> {
    std::size_t operator()(const torrent_node_lib::CroppedFileName& k) const noexcept {
        return std::hash<std::string>()(k.str());
    }
};
    
}

#endif // FILESYSTEM_H_
