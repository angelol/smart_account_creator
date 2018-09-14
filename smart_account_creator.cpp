#include "smart_account_creator.hpp"

using namespace eosio;
using namespace std;

class sac : public contract {
public:
  sac(account_name self) : eosio::contract(self), orders(_self, _self) {}
  
  const uint32_t EXPIRE_TIMEOUT = 60*60*3;

  void transfer(const currency::transfer &transfer) {
    array<char, 33> owner_pubkey_char;
    array<char, 33> active_pubkey_char;
    
    if (transfer.from == _self || transfer.to != _self) {
      // this is an outgoing transfer, do nothing
      return;
    }
    
    // don't do anything on transfers from our reference account
    if (transfer.from == N(ge4dknjtgqge)) {
      return;
    }
    
    eosio_assert(transfer.quantity.symbol == CORE_SYMBOL, "Must be CORE_SYMBOL");
    eosio_assert(transfer.quantity.is_valid(), "Invalid token transfer");
    eosio_assert(transfer.quantity.amount > 0, "Quantity must be positive");
    
    // check if memo contains order
    checksum256 result;
    sha256((char *)transfer.memo.c_str(), transfer.memo.length(), &result);

    
    auto idx = orders.template get_index<N(by_key)>();
    auto itr = idx.find(order::to_key256(result));
    boost::optional<order> order;
    
    if(itr != idx.end()) {
      order = idx.get(order::to_key256(result));
      owner_pubkey_char = order->owner_key.data;
      active_pubkey_char = order->active_key.data;
    } else {
      /* Parse Memo
       * Memo must have format "account_name:owner_key:active_key"
       *
       */
      eosio_assert(transfer.memo.length() == 120 || transfer.memo.length() == 66, "Malformed Memo (not right length)");
      
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

      copy(owner_pubkey.data.begin(), owner_pubkey.data.end(),
           owner_pubkey_char.begin());

      
      copy(active_pubkey.data.begin(), active_pubkey.data.end(),
           active_pubkey_char.begin());
    }
    
    
    
    const string account_string = transfer.memo.substr(0, 12);
    const account_name account_to_create =
        string_to_name(account_string.c_str());
    

    const auto owner_auth = authority{
        1, {{{(uint8_t)abieos::key_type::k1, owner_pubkey_char}, 1}}, {}, {}};
    const auto active_auth = authority{
        1, {{{(uint8_t)abieos::key_type::k1, active_pubkey_char}, 1}}, {}, {}};

    const auto amount = buyrambytes(4 * 1024);
    const auto ram_replace_amount = buyrambytes(256);
    const auto cpu = asset(1500);
    const auto net = asset(500);

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
    INLINE_ACTION_SENDER(eosiosystem::system_contract, buyram)
    (N(eosio), {{_self, N(active)}}, {_self, account_to_create, amount});
    
    // replace lost ram
    INLINE_ACTION_SENDER(eosiosystem::system_contract, buyram)
    (N(eosio), {{_self, N(active)}}, {_self, _self, ram_replace_amount});

    // delegate and transfer cpu and net
    INLINE_ACTION_SENDER(eosiosystem::system_contract, delegatebw)
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
    
    if(order) {
      // account has successfully been created, so we can release the RAM taken by this order
      orders.erase(orders.find(order->primary_key()));
    }
  };
  
  void regaccount(const regaccount_args &args) {
    require_auth(args.sender);
    
    auto idx = orders.template get_index<N(by_key)>();
    auto itr = idx.find(order::to_key256(args.hash));
    if(itr != idx.end()) {
      // fail gracefully if it already exists
      return;
    }
    
    orders.emplace(args.sender, [&](auto& order) {
      order.id = orders.available_primary_key();
      order.expires_at = now() + EXPIRE_TIMEOUT;
      order.hash = args.hash;
      order.owner_key = args.owner_key;
      order.active_key = args.active_key;
    });
    
  };
  
  //@abi action
  void clearexpired(const clearexpired_args &args) {
    // if user orders and account, but never creates it, we need a way to reclaim that RAM
    // can be called by anyone by design
    std::vector<order> l;
    auto idx = orders.template get_index<N(expires_at)>();
    
    // idx is now sorted by expires_at, oldest first
    for( const auto& item : idx ) {
      if(item.expires_at < now()) {
        l.push_back(item);
      } else {
        break;
      }

    }

    // delete in second pass
    for (order item : l) {
      orders.erase(orders.find(item.primary_key()));
    }
  };
  
  /*
  //@abi action
  void clearall(const account_name sender) {
    // used mostly during development
    require_auth(_self);
    std::vector<order> l;

    for( const auto& item : orders ) {
      l.push_back(item);
    }

    // delete in second pass
    for (order item : l) {
      orders.erase(orders.find(item.primary_key()));
    }
  };
  */
  
  private:
    // @abi table order i64
    struct order {
      uint64_t id;
      time expires_at;
      checksum256 hash;
      eosio::public_key owner_key;
      eosio::public_key active_key;
      
      uint64_t primary_key()const { return id; }
      
      key256 by_key256()const { return to_key256(hash); }
      uint64_t get_expires_at()const { return (uint64_t)expires_at; }
      
      static key256 to_key256(const checksum256& checksum) {
            const uint64_t *p64 = reinterpret_cast<const uint64_t *>(&checksum);
            return key256::make_from_word_sequence<uint64_t>(p64[0], p64[1], p64[2], p64[3]);
      }
      EOSLIB_SERIALIZE(order, (id)(expires_at)(hash)(owner_key)(active_key))
    };
    typedef eosio::multi_index< N(order), order,
         indexed_by< N(by_key), const_mem_fun<order, key256,  &order::by_key256> >,
         indexed_by< N(expires_at), const_mem_fun<order, uint64_t,  &order::get_expires_at> >
      > order_index;
    order_index orders;
    
};


// EOSIO_ABI(sac, (transfer)(regaccount)(clearexpired))

extern "C"
{
    [[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        sac _sac(receiver);

        switch (action)
        {
        case N(transfer):
            if (code == N(eosio.token))
            {
                _sac.transfer(unpack_action_data<currency::transfer>());
            }
            break;
        case N(regaccount):
            _sac.regaccount(unpack_action_data<regaccount_args>());
            break;
        case N(clearexpired):
            _sac.clearexpired(unpack_action_data<clearexpired_args>());
            break;
        default:
            break;
        }
        eosio_exit(0);
    }
}
