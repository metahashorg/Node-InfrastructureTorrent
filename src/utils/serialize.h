#ifndef SERIALIZE_H_
#define SERIALIZE_H_

#include <string>
#include <vector>
#include <array>
#include <iostream>

#include "check.h"

namespace torrent_node_lib {

// Процедуры сериализации сделаны таким образом, чтобы младшие разряды оказывались в конце
template<typename T>
inline std::string toBinaryString(T val) {
    std::string result(sizeof(T), 0);
    int i = result.size() - 1;
    while (val != 0 && i >= 0) {
        result[i] = val % 256;
        val /= 256;
        i--;
    }
    return result;
}

// Процедуры сериализации сделаны таким образом, чтобы младшие разряды оказывались в конце
template<typename T>
inline void toBinaryString(T val, std::vector<char> &buffer) {
    int i = sizeof(val) - 1;
    const size_t oldSize = buffer.size();
    buffer.resize(oldSize + sizeof(val), 0);
    while (val != 0 && i >= 0) {
        buffer[oldSize + i] = val % 256;
        val /= 256;
        i--;
    }
}

template<typename T>
inline T fromBinaryString(const std::string &raw, size_t fromPos, size_t &endPos) {
    endPos = fromPos;
    constexpr size_t sizeField = sizeof(T);
    if (raw.size() < sizeField + fromPos) {
        return 0;
    }
    
    T val = 0;
    for (size_t i = 0; i < sizeField; i++) {
        val *= 256;
        val += (unsigned char)raw[i + fromPos];
    }
    endPos = fromPos + sizeField;
    return val;
}

template<typename T>
inline std::string serializeInt(T intValue) {
    return toBinaryString<T>(intValue);
}

template<typename T>
inline void serializeInt(T intValue, std::vector<char> &buffer) {
    toBinaryString<T>(intValue, buffer);
}

inline std::string serializeString(const std::string &str) {
    std::string res;
    res.reserve(str.size() + 10);
    res += serializeInt<size_t>(str.size());
    res += str;
    return res;
}

inline void serializeString(const std::string &str, std::vector<char> &buffer) {
    serializeInt<size_t>(str.size(), buffer);
    buffer.insert(buffer.end(), str.begin(), str.end());
}

template<typename T>
inline T deserializeInt(const std::string &raw, size_t fromPos, size_t &endPos) {
    const T val = fromBinaryString<T>(raw, fromPos, endPos);
    return val;
}

template<typename T>
inline T deserializeInt(const std::string &raw, size_t &fromPos) {
    size_t endPos = fromPos;
    const T val = deserializeInt<T>(raw, fromPos, endPos);
    CHECK(endPos != fromPos, "Incorrect raw");
    fromPos = endPos;
    return val;
}

inline std::string deserializeString(const std::string &raw, size_t fromPos, size_t &endPos) {
    const size_t sizeString = deserializeInt<size_t>(raw, fromPos, endPos);
    if (fromPos == endPos) {
        return "";
    }
    fromPos = endPos;
    
    const std::string str = raw.substr(fromPos, sizeString);
    endPos += sizeString;
    
    return str;
}

inline std::string deserializeString(const std::string &raw, size_t &fromPos) {
    size_t endPos = fromPos;
    const std::string str = deserializeString(raw, fromPos, endPos);
    CHECK(endPos != fromPos, "Incorrect raw");
    fromPos = endPos;
    
    return str;
}

}

#endif // SERIALIZE_H_
