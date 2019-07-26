#ifndef PRIVATE_KEY_H_
#define PRIVATE_KEY_H_

#include <string>
#include <vector>

namespace torrent_node_lib {
    
class PrivateKey {
public:
    
    PrivateKey(const std::vector<unsigned char> &key, const std::string &address);
    
    std::vector<unsigned char> sign(const std::vector<unsigned char> &data) const;
    
    std::vector<unsigned char> sign(const std::string &data) const;
    
    const std::vector<unsigned char>& public_key() const;
    
    const std::string& get_address() const;
    
private:

    std::vector<unsigned char> sign(const unsigned char *data, size_t size) const;
    
private:
    
    std::vector<unsigned char> privateKey;
    
    std::vector<unsigned char> pubk;
    
    std::string address;
    
    bool isSecp = false;

};
    
struct BlockSignatureCheckResult {
    std::string block;
    std::string sign;
    std::string pubkey;
    std::string address;
};

std::string getAddress(const std::vector<unsigned char> &pubkey);

std::string makeFirstPartBlockSign(size_t blockSize);

std::string makeBlockSign(const std::string &blockDump, const PrivateKey &privateKey);

BlockSignatureCheckResult checkSignatureBlock(const std::string &blockDump);

std::string makeTestResultSign(const std::string &str, const PrivateKey &privateKey);

void checkSignatureTest(const std::string &text, const std::string &str);

bool verifySignature(const std::string &text, const std::vector<unsigned char> &sign, const std::vector<unsigned char> &pubkey);

} // namespace torrent_node_lib

#endif // PRIVATE_KEY_H_
