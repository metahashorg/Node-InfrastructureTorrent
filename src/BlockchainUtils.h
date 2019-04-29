#ifndef BLOCKCHAIN_UTILS_H_
#define BLOCKCHAIN_UTILS_H_

#include <string>
#include <vector>
#include <array>

namespace torrent_node_lib {

std::vector<unsigned char> hex2bin(const std::string & src);

void ssl_init();

std::array<unsigned char, 32> get_double_sha256(unsigned char * data, size_t size);

std::string get_address(const std::string & pubk);

std::string get_address(const std::vector<unsigned char> & bpubk);

std::vector<unsigned char> get_address_bin(const std::vector<unsigned char> & bpubk);

std::string makeAddressFromSecpKey(const std::vector<unsigned char> &pubkey);

bool crypto_check_sign_data(
    const std::vector<char>& sign, 
    const std::vector<unsigned char>& public_key, 
    const unsigned char *data,
    size_t data_size);

void initBlockchainUtilsImpl();

}

#endif // BLOCKCHAIN_UTILS_H_
