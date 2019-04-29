#include "BlockchainUtils.h"

#include <memory>

#include <cstring>
#include <openssl/ripemd.h>
#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/x509v3.h>
#include <openssl/x509.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/opensslconf.h>

#include <secp256k1.h>

#include <atomic>

#include "log.h"
#include "check.h"
#include "stringUtils.h"

using namespace common;

namespace torrent_node_lib {
    
static bool isInitialized = false;

static const secp256k1_context* getCtx() {
    static thread_local std::unique_ptr<secp256k1_context, decltype(&secp256k1_context_destroy)> s_ctx{
        secp256k1_context_create(SECP256K1_CONTEXT_VERIFY | SECP256K1_CONTEXT_SIGN),
        &secp256k1_context_destroy
    };
    return s_ctx.get();
}

std::vector<unsigned char> hex2bin(const std::string & src) {
    static const unsigned char DecLookup[256] = {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // gap before first hex digit
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,1,2,3,4,5,6,7,8,9,       // 0123456789
        0,0,0,0,0,0,0,             // :;<=>?@ (gap)
        10,11,12,13,14,15,         // ABCDEF
        0,0,0,0,0,0,0,0,0,0,0,0,0, // GHIJKLMNOPQRS (gap)
        0,0,0,0,0,0,0,0,0,0,0,0,0, // TUVWXYZ[/]^_` (gap)
        10,11,12,13,14,15,         // abcdef
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 // fill zeroes
    };
    
    uint i = 0;
    if (src.size() > 2 && src[0] == '0' && src[1] == 'x') {
        i = 2;
    }
    
    std::vector<unsigned char> dest;
    dest.reserve(src.length()/2);
    for (; i < src.length(); i += 2 ) {
        unsigned char d =  DecLookup[(unsigned char)src[i]] << 4;
        d |= DecLookup[(unsigned char)src[i + 1]];
        dest.push_back(d);
    }
    
    return dest;
}

void ssl_init() {
    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();
}


std::array<unsigned char, 32> get_double_sha256(unsigned char * data, size_t size) {
    CHECK(isInitialized, "Not initialized");
    
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash1;
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash2;
    
    // First pass
    SHA256(data, size, hash1.data());
    
    // Second pass
    SHA256(hash1.data(), SHA256_DIGEST_LENGTH, hash2.data());
    
    return hash2;
}


bool IsValidECKey(EVP_PKEY* key) {
    CHECK(isInitialized, "Not initialized");
    
    bool rslt = false;
    EC_KEY *ec_key = EVP_PKEY_get1_EC_KEY(key);
    if (ec_key) {
        rslt = (EC_KEY_check_key(ec_key) == 1);
        EC_KEY_free(ec_key);
    }
    return rslt;
}

EVP_PKEY* ReadPublicKey(const std::vector<unsigned char> & binary) {
    CHECK(isInitialized, "Not initialized");
    
    const unsigned char * data = binary.data();
    if (binary.size()) {
        EVP_PKEY* key = d2i_PUBKEY(NULL, const_cast<const unsigned char **>(&data), binary.size());
        if (key) {
            return key;
        }
    }
    
    return NULL;
}


std::string get_address(const std::string & pubk) {
    std::vector<unsigned char> binary = hex2bin(pubk);
    return get_address(binary);
}

std::string makeAddressFromSecpKey(const std::vector<unsigned char> &pubkey) {
    secp256k1_pubkey pubkeySecp;
    CHECK(secp256k1_ec_pubkey_parse(getCtx(), &pubkeySecp, pubkey.data(), pubkey.size()) == 1, "Incorrect pubkey");
    std::vector<unsigned char> pubkeyData(65);
    size_t size = pubkeyData.size();
    CHECK(secp256k1_ec_pubkey_serialize(getCtx(), pubkeyData.data(), &size, &pubkeySecp, SECP256K1_EC_UNCOMPRESSED) == 1, "Incorrect pubkey");
    const std::string calculatedAddress = torrent_node_lib::get_address(pubkeyData);
    return calculatedAddress;
}

static bool get_address_comp(const std::vector<unsigned char> & bpubk, std::array<unsigned char, RIPEMD160_DIGEST_LENGTH + 1> &wide_h, std::array<unsigned char, SHA256_DIGEST_LENGTH> &hash2) {
    CHECK(isInitialized, "Not initialized");
    
    std::vector<unsigned char> binary(bpubk);
    unsigned char* data = binary.data();
    int datasize = binary.size();
    if (data && datasize >= 65) {
        data[datasize - 65] = 0x04;
        
        std::array<unsigned char, SHA256_DIGEST_LENGTH> sha_1;
        std::array<unsigned char, RIPEMD160_DIGEST_LENGTH> r160;
        
        SHA256(data + (datasize - 65), 65, sha_1.data());
        RIPEMD160(sha_1.data(), SHA256_DIGEST_LENGTH, r160.data());
        
        wide_h[0] = 0;
        for (size_t i = 0; i < RIPEMD160_DIGEST_LENGTH; i++) {
            wide_h[i + 1] = r160[i];
        }
        
        std::array<unsigned char, SHA256_DIGEST_LENGTH> hash1;
        SHA256(wide_h.data(), RIPEMD160_DIGEST_LENGTH + 1, hash1.data());
        
        SHA256(hash1.data(), SHA256_DIGEST_LENGTH, hash2.data());
        return true;
    }
    
    return false;
}

std::string get_address(const std::vector<unsigned char> & bpubk) {
    std::array<unsigned char, RIPEMD160_DIGEST_LENGTH + 1> wide_h;
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash2;
    const bool res = get_address_comp(bpubk, wide_h, hash2);
    if (!res) {
        return "Invalid";
    }
            
    std::string address;
    {
        address.reserve(55);
        
        address.insert(address.end(), '0');
        address.insert(address.end(), 'x');
        
        static const char HexLookup[513]= {
            "000102030405060708090a0b0c0d0e0f"
            "101112131415161718191a1b1c1d1e1f"
            "202122232425262728292a2b2c2d2e2f"
            "303132333435363738393a3b3c3d3e3f"
            "404142434445464748494a4b4c4d4e4f"
            "505152535455565758595a5b5c5d5e5f"
            "606162636465666768696a6b6c6d6e6f"
            "707172737475767778797a7b7c7d7e7f"
            "808182838485868788898a8b8c8d8e8f"
            "909192939495969798999a9b9c9d9e9f"
            "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
            "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
            "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
            "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
            "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
            "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"
        };
        
        for (uint i = 0; i < wide_h.size(); i++) {
            const char * hex = HexLookup + 2 * wide_h[i];
            address.insert(address.end(), hex, hex + 2);
        }
        
        
        for (size_t i = 0; i < 4; i++) {
            const char * hex = HexLookup + 2 * hash2[i];
            address.insert(address.end(), hex, hex + 2);
        }
    }
    
    return address;
}

std::vector<unsigned char> get_address_bin(const std::vector<unsigned char> & bpubk) {
    std::array<unsigned char, RIPEMD160_DIGEST_LENGTH + 1> wide_h;
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash2;
    const bool res = get_address_comp(bpubk, wide_h, hash2);
    if (!res) {
        return {};
    }
    
    std::vector<unsigned char> result(wide_h.begin(), wide_h.end());
    result.insert(result.end(), hash2.begin(), hash2.begin() + 4);
    
    return result;
}

/*std::vector<unsigned char> crypto_sign_data(
    const std::vector<unsigned char>& private_key, 
    const unsigned char *data,
    size_t data_size)
{
    secp256k1_pubkey pubkeySecp;
    CHECK(secp256k1_ec_pubkey_parse(getCtx(), &pubkeySecp, public_key.data() + public_key.size() - 65, 65) == 1, "Incorrect publik key");
    secp256k1_ecdsa_signature signSecp;
    CHECK(secp256k1_ecdsa_signature_parse_der(getCtx(), &signSecp, (const unsigned char*)sign.data(), sign.size()) == 1, "Incorrect sign key");
    secp256k1_ecdsa_signature sig_norm;
    secp256k1_ecdsa_signature_normalize(getCtx(), &sig_norm, &signSecp);
    
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash;
    SHA256(data, data_size, hash.data());
    
    const int result = secp256k1_ecdsa_verify(getCtx(), &sig_norm, hash.data(), &pubkeySecp);
    if (result == 1) {
        return true;
    } else if (result == 0) {
        return false;
    } else {
        throwErr("Serious error of signature verification");
    }
}*/

static std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> crypto_load_key(
    const std::vector<unsigned char> &key_buf,
    bool pubkey)
{
    CHECK(!key_buf.empty(), "empty key_buf");
    std::unique_ptr<BIO, decltype(&BIO_free)> bio(BIO_new_mem_buf(key_buf.data(), key_buf.size()), BIO_free);
    CHECK(bio != nullptr, "Cannot create BIO object");
    
    if(pubkey) {
        return std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(d2i_PUBKEY_bio(bio.get(), NULL), EVP_PKEY_free);
    } else {
        return std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(d2i_PrivateKey_bio(bio.get(), NULL), EVP_PKEY_free);
    }
}

static std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> crypto_load_public_key(const std::vector<unsigned char> &public_key_buf) {
    return crypto_load_key(public_key_buf, true);
}

bool crypto_check_sign_data2(
    const std::vector<char>& sign, 
    const std::vector<unsigned char>& public_key, 
    const unsigned char *data,
    size_t data_size)
{
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> mdctx(EVP_MD_CTX_create(), EVP_MD_CTX_free);
    CHECK(mdctx != nullptr, "Cannot create digest context");
    
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey = crypto_load_public_key(public_key);
    CHECK(pkey != nullptr, "Cannot load pulic key");
    
    const bool res1 = EVP_DigestVerifyInit(mdctx.get(), NULL, EVP_sha256(), NULL, pkey.get());
    CHECK(res1 == 1, "Cannot initialise SHA-256 verifying");
    
    const bool res2 = EVP_DigestVerifyUpdate(mdctx.get(), data, data_size);
    CHECK(res2 == 1, "Cannot verify the data");
    
    const int rc = EVP_DigestVerifyFinal(mdctx.get(), (const unsigned char*)sign.data(), sign.size());
    if(rc == 0) {
        return false;
    } else if(rc == 1) {
        return true;
    } else {
        throwErr("Serious error of signature verification");
    }
}

bool crypto_check_sign_data(
    const std::vector<char>& sign, 
    const std::vector<unsigned char>& public_key, 
    const unsigned char *data,
    size_t data_size)
{
    secp256k1_pubkey pubkeySecp;
    if (secp256k1_ec_pubkey_parse(getCtx(), &pubkeySecp, public_key.data() + public_key.size() - 65, 65) != 1) {
        return crypto_check_sign_data2(sign, public_key, data, data_size);
    }
    
    secp256k1_ecdsa_signature signSecp;
    CHECK(secp256k1_ecdsa_signature_parse_der(getCtx(), &signSecp, (const unsigned char*)sign.data(), sign.size()) == 1, "Incorrect sign key");
    secp256k1_ecdsa_signature sig_norm;
    secp256k1_ecdsa_signature_normalize(getCtx(), &sig_norm, &signSecp);
    
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash;
    SHA256(data, data_size, hash.data());
        
    const int result = secp256k1_ecdsa_verify(getCtx(), &sig_norm, hash.data(), &pubkeySecp);
    if (result == 1) {
        return true;
    } else {
        return false;
    }
}

void initBlockchainUtilsImpl() {
    ssl_init();
    isInitialized = true;
}

}
