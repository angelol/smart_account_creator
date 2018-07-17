#include "smart_account_creator.hpp"

using namespace eosio;
using namespace std;

class sac : public contract {
public:
  sac(account_name self) : eosio::contract(self) {}

  void transfer(const account_name sender, const account_name receiver) {
    const auto transfer = unpack_action_data<currency::transfer>();
    if (transfer.from == _self || transfer.to != _self) {
      // this is an outgoing transfer, do nothing
      return;
    }
    
    // don't do anything on transfers from our reference account
    if (transfer.from == N(ge4dknjtgqge)) {
      return;
    }
    
    /* Parse Memo
     * Memo must have format "account_name:owner_key:active_key"
     *
     */
    eosio_assert(transfer.quantity.symbol == string_to_symbol(4, "EOS"),
                 "Must be EOS");
    eosio_assert(transfer.quantity.is_valid(), "Invalid token transfer");
    eosio_assert(transfer.quantity.amount > 0, "Quantity must be positive");

    eosio_assert(transfer.memo.length() == 120 || transfer.memo.length() == 66, "Malformed Memo (not right length)");
    const string account_string = transfer.memo.substr(0, 12);
    const account_name account_to_create =
        string_to_name(account_string.c_str());
    eosio_assert(transfer.memo[12] == ':' || transfer.memo[12] == '-', "Malformed Memo [12] == : or -");

    const string owner_key_str = transfer.memo.substr(13, 53);
    string active_key_str;
    if(transfer.memo[66] == ':' || transfer.memo[66] == '-') {
      // active key provided
      active_key_str = transfer.memo.substr(67, 53);
    } else {
      // active key is the same as owner
      active_key_str =  owner_key_str;
    }
    

    const abieos::public_key owner_pubkey =
        abieos::string_to_public_key(owner_key_str);
    const abieos::public_key active_pubkey =
        abieos::string_to_public_key(active_key_str);

    array<char, 33> owner_pubkey_char;
    copy(owner_pubkey.data.begin(), owner_pubkey.data.end(),
         owner_pubkey_char.begin());

    array<char, 33> active_pubkey_char;
    copy(active_pubkey.data.begin(), active_pubkey.data.end(),
         active_pubkey_char.begin());

    const auto owner_auth = authority{
        1, {{{(uint8_t)abieos::key_type::k1, owner_pubkey_char}, 1}}, {}, {}};
    const auto active_auth = authority{
        1, {{{(uint8_t)abieos::key_type::k1, active_pubkey_char}, 1}}, {}, {}};

    const auto amount = buyrambytes(4 * 1024);
    const auto ram_replace_amount = buyrambytes(256);
    const auto cpu = asset(1000);
    const auto net = asset(1000);

    const auto fee =
        asset(std::max((transfer.quantity.amount + 119) / 200, 1000ll));
        
    eosio_assert(cpu + net + amount + fee + ram_replace_amount <= transfer.quantity,
                 "Not enough money");

    const auto remaining_balance = transfer.quantity - cpu - net - amount - fee - ram_replace_amount;

    // create account
    INLINE_ACTION_SENDER(call::eosio, newaccount)
    (N(eosio), {{_self, N(active)}},
     {_self, account_to_create, owner_auth, active_auth});

    // buy ram
    INLINE_ACTION_SENDER(call::eosio, buyram)
    (N(eosio), {{_self, N(active)}}, {_self, account_to_create, amount});
    
    // replace lost ram
    INLINE_ACTION_SENDER(call::eosio, buyram)
    (N(eosio), {{_self, N(active)}}, {_self, _self, ram_replace_amount});

    // delegate and transfer cpu and net
    INLINE_ACTION_SENDER(call::eosio, delegatebw)
    (N(eosio), {{_self, N(active)}}, {_self, account_to_create, net, cpu, 1});
    // fee
    INLINE_ACTION_SENDER(eosio::token, transfer)
    (N(eosio.token), {{_self, N(active)}},
     {_self, string_to_name("saccountfees"), fee,
      std::string("Account creation fee")});

    if (remaining_balance.amount > 0) {
      // transfer remaining balance to new account
      INLINE_ACTION_SENDER(eosio::token, transfer)
      (N(eosio.token), {{_self, N(active)}},
       {_self, account_to_create, remaining_balance,
        std::string("Initial balance")});
    }
  }
};

// EOSIO_ABI(sac, (transfer))

#define EOSIO_ABI_EX(TYPE, MEMBERS)                                            \
  extern "C" {                                                                 \
  void apply(uint64_t receiver, uint64_t code, uint64_t action) {              \
    if (action == N(onerror)) {                                                \
      /* onerror is only valid if it is for the "eosio" code account and       \
       * authorized by "eosio"'s "active permission */                         \
      eosio_assert(code == N(eosio), "onerror action's are only valid from "   \
                                     "the \"eosio\" system account");          \
    }                                                                          \
    auto self = receiver;                                                      \
    if (code == self || code == N(eosio.token) || action == N(onerror)) {      \
      TYPE thiscontract(self);                                                 \
      switch (action) { EOSIO_API(TYPE, MEMBERS) }                             \
      /* does not allow destructor of thiscontract to run: eosio_exit(0); */   \
    }                                                                          \
  }                                                                            \
  }

EOSIO_ABI_EX(sac, (transfer))
