#include <algorithm>
#include <cmath>
#include <boost/optional.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/currency.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/public_key.hpp>
#include <eosio.system/exchange_state.hpp>
#include <eosio.system/exchange_state.cpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio.system/eosio.system.hpp>
#include <eosio.system/native.hpp>
#include <eosiolib/public_key.hpp>
#include <eosiolib/crypto.h>
#include "includes/abieos_numeric.hpp"


namespace eosio {

// Temporary authority until native is fixed. Ref: https://github.com/EOSIO/eos/issues/4669
struct wait_weight {
  uint32_t wait_sec;
  weight_type weight;
};
struct authority {
  uint32_t threshold;
  vector<eosiosystem::key_weight> keys;
  vector<eosiosystem::permission_level_weight> accounts;
  vector<wait_weight> waits;
};


// eosiosystem::native::newaccount Doesn't seem to want to take authorities.
struct call {
  struct eosio {
    void newaccount(account_name creator, account_name name,
                    authority owner, authority active);
  };
};


asset buyrambytes(uint32_t bytes) {
  eosiosystem::rammarket market(N(eosio), N(eosio));
  auto itr = market.find(S(4, RAMCORE));
  eosio_assert(itr != market.end(), "RAMCORE market not found");
  auto tmp = *itr;
  return tmp.convert(asset(bytes, S(0, RAM)), CORE_SYMBOL);
}
}