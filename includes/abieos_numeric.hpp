// copyright defined in abieos/LICENSE.txt

#include <algorithm>
#include <array>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <string_view>

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
std::array<uint8_t, size> decimal_to_binary(std::string_view s) {
    std::array<uint8_t, size> result{{0}};
    for (auto& src_digit : s) {
        if (src_digit < '0' || src_digit > '9')
            eosio_assert(0, "invalid number");
        uint8_t carry = src_digit - '0';
        for (auto& result_byte : result) {
            int x = result_byte * 10 + carry;
            result_byte = x;
            carry = x >> 8;
        }
        if (carry)
            eosio_assert(0, "number is out of range");
    }
    return result;
}

template <size_t size>
std::string binary_to_decimal(const std::array<uint8_t, size>& bin) {
    std::string result("0");
    for (auto byte_it = bin.rbegin(); byte_it != bin.rend(); ++byte_it) {
        int carry = *byte_it;
        for (auto& result_digit : result) {
            int x = ((result_digit - '0') << 8) + carry;
            result_digit = '0' + x % 10;
            carry = x / 10;
        }
        while (carry) {
            result.push_back('0' + carry % 10);
            carry = carry / 10;
        }
    }
    std::reverse(result.begin(), result.end());
    return result;
}

template <size_t size>
std::array<uint8_t, size> base58_to_binary(std::string_view s) {
    std::array<uint8_t, size> result{{0}};
    for (auto& src_digit : s) {
        int carry = get_base58_map()[src_digit];
        if (carry < 0)
            eosio_assert(0, "invalid base-58 value");
        for (auto& result_byte : result) {
            int x = result_byte * 58 + carry;
            result_byte = x;
            carry = x >> 8;
        }
        if (carry)
            eosio_assert(0, "base-58 value is out of range");
    }
    std::reverse(result.begin(), result.end());
    return result;
}

template <size_t size>
std::string binary_to_base58(const std::array<uint8_t, size>& bin) {
    std::string result("");
    for (auto byte : bin) {
        int carry = byte;
        for (auto& result_digit : result) {
            int x = (get_base58_map()[result_digit] << 8) + carry;
            result_digit = base58_chars[x % 58];
            carry = x / 58;
        }
        while (carry) {
            result.push_back(base58_chars[carry % 58]);
            carry = carry / 58;
        }
    }
    for (auto byte : bin)
        if (byte)
            break;
        else
            result.push_back('1');
    std::reverse(result.begin(), result.end());
    return result;
}

enum class key_type : uint8_t {
    k1 = 0,
    r1 = 1,
};

struct public_key {
    key_type type{};
    std::array<uint8_t, 33> data{};
};


template <typename Key, int suffix_size>
Key string_to_key(std::string_view s, key_type type, const char (&suffix)[suffix_size]) {
    static const auto size = std::tuple_size<decltype(Key::data)>::value;
    auto whole = base58_to_binary<size + 4>(s);
    Key result{type};
    memcpy(result.data.data(), whole.data(), result.data.size());
    return result;
}


public_key string_to_public_key(std::string_view s) {
    if (s.size() >= 3 && s.substr(0, 3) == "EOS") {
        auto whole = base58_to_binary<37>(s.substr(3));
        public_key key{key_type::k1};
        static_assert(whole.size() == key.data.size() + 4, "Error: whole.size() != key.data.size() + 4");
        memcpy(key.data.data(), whole.data(), key.data.size());
        return key;
    } else if (s.size() >= 7 && s.substr(0, 7) == "PUB_R1_") {
        return string_to_key<public_key>(s.substr(7), key_type::r1, "R1");
    } else {
        eosio_assert(0, "unrecognized public key format");
    }
}

} // namespace abieos
