#include "PrivateKey.h"

#include <array>
#include <memory>

#include <secp256k1.h>
#include <openssl/sha.h>

#include "check.h"
#include "stringUtils.h"
#include "utils/serialize.h"
#include "convertStrings.h"

#include "BlockchainUtils.h"

using namespace common;

namespace torrent_node_lib {

static const secp256k1_context* getCtx() {
    static thread_local std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)> s_ctx{
        secp256k1_context_create(SECP256K1_CONTEXT_VERIFY | SECP256K1_CONTEXT_SIGN),
        &secp256k1_context_destroy
    };
    return s_ctx.get();
}
    
PrivateKey::PrivateKey(const std::vector<unsigned char>& key, const std::string &address) 
    : privateKey(key.begin() + 7, key.end())
    , pubk(65)
    , address(address)
{
    secp256k1_pubkey pubkey;
    CHECK(secp256k1_ec_pubkey_create(getCtx(), &pubkey, (const uint8_t*)privateKey.data()) == 1, "Incorrect private key");
    size_t size = pubk.size();
    CHECK(secp256k1_ec_pubkey_serialize(getCtx(), pubk.data(), &size, &pubkey, SECP256K1_EC_UNCOMPRESSED) == 1, "Incorrect pubkey");
    const std::string calculatedAddress = torrent_node_lib::get_address(pubk);
    CHECK(calculatedAddress == toLower(address), "Incorrect private key or address");
}
    
const std::vector<unsigned char>& PrivateKey::public_key() const {
    return pubk;
}

const std::string& PrivateKey::get_address() const {
    return address;
}

std::vector<unsigned char> PrivateKey::sign(const std::string& data) const {
    return sign((const unsigned char*)data.data(), data.size());
}

std::vector<unsigned char> PrivateKey::sign(const std::vector<unsigned char>& data) const {
    return sign(data.data(), data.size());
}

std::vector<unsigned char> PrivateKey::sign(const unsigned char *data, size_t size) const {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash;
    SHA256(data, size, hash.data());
    
    secp256k1_ecdsa_signature signature;
    CHECK(secp256k1_ecdsa_sign(getCtx(), &signature, hash.data(), privateKey.data(), nullptr, nullptr) == 1, "Dont create signature");
    std::vector<unsigned char> result(100);
    size_t lenResult = result.size();
    CHECK(secp256k1_ecdsa_signature_serialize_der(getCtx(), result.data(), &lenResult, &signature) == 1, "Dont create signature");
    result.resize(lenResult);
    return result;
}

std::string makeFirstPartBlockSign(size_t blockSize) {
    return serializeInt(blockSize);
}

std::string makeBlockSign(const std::string &blockDump, const PrivateKey &privateKey) {
    const std::vector<unsigned char> sign = privateKey.sign(blockDump);
    const std::vector<unsigned char> &pubkey = privateKey.public_key();
    const std::string &address = privateKey.get_address();
    
    std::string res;
    
    res.reserve(8 + sign.size() + 8 + pubkey.size() + 8 + address.size() + 1);
    
    res += serializeString(std::string(sign.begin(), sign.end()));
    res += serializeString(std::string(pubkey.begin(), pubkey.end()));
    res += serializeString(address);
    
    return res;
}

BlockSignatureCheckResult checkSignatureBlock(const std::string &blockDump) {
    BlockSignatureCheckResult result;
    
    size_t from = 0;
    result.block = deserializeString(blockDump, from);
    result.sign = deserializeString(blockDump, from);
    result.pubkey = deserializeString(blockDump, from);
    result.address = deserializeString(blockDump, from);
    
    CHECK(crypto_check_sign_data(std::vector<char>(result.sign.begin(), result.sign.end()), std::vector<unsigned char>(result.pubkey.begin(), result.pubkey.end()), (const unsigned char*)result.block.data(), result.block.size()), "Not validate");
    const std::string calculatedAddress = makeAddressFromSecpKey(std::vector<unsigned char>(result.pubkey.begin(), result.pubkey.end()));
    CHECK(calculatedAddress == result.address, "Not validate");
    
    return result;
}

std::string makeTestSign(const std::string &str, const PrivateKey &privateKey) {
    const std::vector<unsigned char> sign = privateKey.sign(str);
    const std::vector<unsigned char> &pubkey = privateKey.public_key();
    const std::string &address = privateKey.get_address();
    
    std::string res;
    
    res.reserve(8 + sign.size() + 8 + pubkey.size() + 8 + address.size() + 1);
    
    res += serializeString(std::string(sign.begin(), sign.end()));
    res += serializeString(std::string(pubkey.begin(), pubkey.end()));
    res += serializeString(address);
    
    return res;
}

std::string makeTestResultSign(const std::string &str, const PrivateKey &privateKey) {
    const std::vector<unsigned char> sign = privateKey.sign(str);
    return toHex(sign);
}

void checkSignatureTest(const std::string &text, const std::string &str) {
    size_t from = 0;
    const std::string sign = deserializeString(str, from);
    const std::string pubkey = deserializeString(str, from);
    const std::string address = deserializeString(str, from);
    
    CHECK(crypto_check_sign_data(std::vector<char>(sign.begin(), sign.end()), std::vector<unsigned char>(pubkey.begin(), pubkey.end()), (const unsigned char*)text.data(), text.size()), "Not validate");
    const std::string calculatedAddress = makeAddressFromSecpKey(std::vector<unsigned char>(pubkey.begin(), pubkey.end()));
    CHECK(calculatedAddress == address, "Not validate");
}

} // namespace torrent_node_lib
