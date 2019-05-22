#include "sac.hpp"

ACTION sac::regaccount(const name sender, const checksum256 hash, const eosio::public_key owner_key, const eosio::public_key active_key) {
    require_auth(sender);
    
    do_clearexpired();

    auto idx = orders.template get_index<"bykey"_n>();
    auto itr = idx.find(hash);
    if(itr != idx.end()) {
      // fail gracefully if it already exists
      return;
    }
    
    orders.emplace(sender, [&](auto& order) {
      order.id = orders.available_primary_key();
      order.expires_at = now() + EXPIRE_TIMEOUT;
      order.hash = hash;
      order.owner_key = owner_key;
      order.active_key = active_key;
    });
};

ACTION sac::clearexpired(const name sender) {
    // if user orders and account, but never creates it, we need a way to reclaim that RAM
    // can be called by anyone by design
    do_clearexpired();
};

ACTION sac::transfer(const name from, const name to, const asset quantity, const std::string memo) {
    // only respond to incoming transfers
    if (from == _self || to != _self) {
      return;
    }
    
    // don't do anything on transfers from our reference account
    if (from == "ge4dknjtgqge"_n) {
      return;
    }
    
    // only handle EOS transfers, ignore anything else
    if(quantity.symbol != core_symbol) {
      return;
    }
    
    eosio_assert(quantity.is_valid(), "Are you trying to corrupt me?");
    eosio_assert(quantity.amount > 0, "Amount must be > 0");
    
    // check if memo contains order
    const auto hash = sha256(trim(memo).c_str(), memo.length());
    auto idx = orders.template get_index<"bykey"_n>();
    auto itr = idx.find(hash);
    
    struct account_t data;
    if(itr != idx.end()) {
      data.name = name(memo.substr(0, 12));
      data.owner_key = itr->owner_key;
      data.active_key = itr->active_key;
      data.stake_cpu = default_cpu_stake;
      data.ram_amount_bytes = default_ram_amount_bytes;
      idx.erase(itr);
    } else {
      parse_memo(memo, data);
    }
    create_account(quantity, data);
}

extern "C" {
  [[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
    if (action == "transfer"_n.value && code == sac::eosio_token_account.value) {
      execute_action(eosio::name(receiver), eosio::name(code), &sac::transfer);
    }
    
    if (code == receiver) {
      switch (action) { 
        EOSIO_DISPATCH_HELPER(sac, 
          (regaccount)
          (clearexpired)
        ) 
      }    
    }
    eosio_exit(0);
  }
}