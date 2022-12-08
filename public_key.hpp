// copyright defined in abieos/LICENSE.txt
// Adapted from https://github.com/EOSIO/abieos/blob/master/src/abieos_numeric.hpp

#include <eosio/crypto.hpp>

using namespace eosio;
namespace abieos {

const char base58_chars[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

bool map_initialized = false;
std::array<int8_t, 256> base58_map{{0}};
auto get_base58_map() {
    if(!map_initialized) {
      for (unsigned i = 0; i < base58_map.size(); ++i)
          base58_map[i] = -1;
      for (unsigned i = 0; i < sizeof(base58_chars); ++i)
          base58_map[base58_chars[i]] = i;
      map_initialized = true;
    }
    return base58_map;
}

template <size_t size>
std::array<uint8_t, size> base58_to_binary(std::string_view s) {
    std::array<uint8_t, size> result{{0}};
    for (auto& src_digit : s) {
        int carry = get_base58_map()[src_digit];
        if (carry < 0)
            check(0, "invalid base-58 value");
        for (auto& result_byte : result) {
            int x = result_byte * 58 + carry;
            result_byte = x;
            carry = x >> 8;
        }
        if (carry)
            check(0, "base-58 value is out of range");
    }
    std::reverse(result.begin(), result.end());
    return result;
}

enum class key_type : uint8_t {
    k1 = 0,
    r1 = 1,
};

ecc_public_key string_to_ecc(std::string_view s, key_type type) {
    static const auto size = std::tuple_size<ecc_public_key>::value;
    auto whole = base58_to_binary<size + 4>(s);
    ecc_public_key ecc_key;
    check(whole.size() == ecc_key.size() + 4, "Error: whole.size() != ecc_key.size() + 4");
    memcpy(ecc_key.data(), whole.data(), ecc_key.size());
    return ecc_key;
}

eosio::public_key string_to_public_key(std::string_view s) {
    if (s.size() >= 3 && s.substr(0, 3) == "EOS") {
        ecc_public_key ecc_key = string_to_ecc(s.substr(3), key_type::k1);
        return public_key{ std::in_place_index<0>, ecc_key };

    } else if (s.size() >= 7 && s.substr(0, 7) == "PUB_K1_") {
        ecc_public_key ecc_key = string_to_ecc(s.substr(7), key_type::k1);
        return public_key{ std::in_place_index<0>, ecc_key };

    } else if (s.size() >= 7 && s.substr(0, 7) == "PUB_R1_") {
        ecc_public_key ecc_key = string_to_ecc(s.substr(7), key_type::r1);
        return public_key{ std::in_place_index<1>, ecc_key };
        
    } else {
        check(0, "unrecognized public key format");
        return eosio::public_key{}; // this is never returned, but shuts up compiler warnings
    }
}
} //namepace abieos
