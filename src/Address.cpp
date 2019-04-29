#include "Address.h"

#include "check.h"
#include "convertStrings.h"

using namespace common;

namespace torrent_node_lib {
    
Address::Address(const std::vector<unsigned char> &address, bool isBlockedAddress)
    : address(address.begin(), address.end())
    , isSet(true)
{
    if (isBlockedAddress) {
        this->address[0] = 1;
    }
}

Address::Address(const std::string& hexAddress) {
    CHECK(!hexAddress.empty(), "Empty address");
    CHECK(hexAddress.compare(0, 2, "0x") == 0, "Incorrect address " + hexAddress);
    const std::vector<unsigned char> tmp = fromHex(hexAddress);
    address = std::string(tmp.begin(), tmp.end());
    isSet = true;
}

bool Address::operator==(const Address& second) const {
    CHECK(isSet, "Address not set");
    CHECK(second.isSet, "Address not set");
    return address == second.address;
}

bool Address::operator<(const Address& second) const {
    CHECK(isSet, "Address not set");
    CHECK(second.isSet, "Address not set");
    return address < second.address;
}

const std::string& Address::getBinaryString() const {
    CHECK(isSet, "Address not set");
    if (address.empty()) {
        return INITIAL_WALLET_TRANSACTION;
    }
    return address;
}

const std::string& Address::toBdString() const {
    return getBinaryString();
}

std::string Address::calcHexString() const {
    CHECK(isSet, "Address not set");
    if (address.empty()) {
        return INITIAL_WALLET_TRANSACTION;
    }
    return "0x" + toHex(address.begin(), address.end());
}

void Address::setEmpty() {
    isSet = true;
    address.clear();
    isInitialW = true;
}

bool Address::isEmpty() const {
    CHECK(isSet, "Address not set");
    return address.empty();
}

bool Address::isInitialWallet() const {
    CHECK(isSet, "Address not set");
    return isInitialW;
}

bool Address::isScriptAddress() const {
    CHECK(isSet, "Address not set");
    return address[0] == 8;
}
    
}
