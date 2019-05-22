#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/crypto.hpp>
#include "exchange_state.hpp"
#include "public_key.hpp"

using namespace eosio;

CONTRACT sac : public contract {
  public:
    using contract::contract;
    sac(name self, name code, datastream<const char*> ds) : 
      eosio::contract(self,code,ds),
      orders(_self, _self.value)
      {}
    
    const uint32_t EXPIRE_TIMEOUT = 60*60*3;
    
    static constexpr symbol core_symbol{"EOS", 4};
    static constexpr name system_account{"eosio"};
    static constexpr name eosio_token_account{"eosio.token"};
    static constexpr symbol RAM_symbol{"RAM", 0};
    static constexpr symbol RAMCORE_symbol{"RAMCORE", 4};
    
    const asset default_cpu_stake{1500, core_symbol};
    const asset default_net_stake{500, core_symbol};
    const uint32_t default_ram_amount_bytes{3000};
    
    struct permission_level_weight {
      permission_level permission;
      uint16_t weight;
    };
    struct key_weight {
      eosio::public_key key;
      uint16_t weight;
    };
    struct wait_weight {
      uint32_t wait_sec;
      uint16_t weight;
    };
    struct authority {
      uint32_t threshold;
      std::vector<key_weight> keys;
      std::vector<permission_level_weight> accounts;
      std::vector<wait_weight> waits;
    };
    struct newaccount {
      name creator;
      name name;
      authority owner;
      authority active;
    };
    
    struct account_t {
      name name;
      eosio::public_key owner_key;
      eosio::public_key active_key;
      asset stake_cpu;
      uint32_t ram_amount_bytes;
    };
    
    
    TABLE order {
      uint64_t id;
      uint32_t expires_at;
      checksum256 hash;
      eosio::public_key owner_key;
      eosio::public_key active_key;
      
      uint64_t primary_key()const { return id; }
      
      checksum256 by_checksum()const { return hash; }
      uint64_t get_expires_at()const { return (uint64_t)expires_at; }
    };
    typedef eosio::multi_index< name("order"), order,
         indexed_by< "bykey"_n, const_mem_fun<order, checksum256,  &order::by_checksum> >,
         indexed_by< "expiresat"_n, const_mem_fun<order, uint64_t,  &order::get_expires_at> >
      > order_index;
    order_index orders;
    
    
    ACTION regaccount(const name sender, const checksum256 hash, const eosio::public_key owner_key, const eosio::public_key active_key);
    ACTION clearexpired(const name sender);
    ACTION transfer(const name from, const name to, const asset quantity, const std::string memo);

  private:
    void do_clearexpired() {
      std::vector<order> l;
      auto idx = orders.template get_index<"expiresat"_n>();
      
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
    }
    
    asset determine_ram_price(uint32_t bytes) {
      rammarket rammarkettable(system_account, system_account.value);
      auto market = rammarkettable.get(RAMCORE_symbol.raw());
      auto ram_price = market.convert(asset{bytes, RAM_symbol}, core_symbol);
      ram_price.amount = (ram_price.amount * 200 + 199) / 199; // add ram fee
      return ram_price;
    }
    
    /**
      * Memo format 1: ACCOUNT_NAME:PUBLIC_KEY:EOS_FOR_CPU:RAM_AMOUNT_KB
      * First 2 parameters are mandatory, the rest are optional
      * If EOS_FOR_CPU and RAM_AMOUNT_KB is not given, default values are used.
      * So either 2 params, or 4
      *
      * Memo format 2: ACCOUNT_NAME:OWNER_KEY:ACTIVE_KEY:EOS_FOR_CPU:RAM_AMOUNT_KB
      * This is with separate owner and active keys.
      * So either 3 params or 5
      */
    void parse_memo(const std::string& memo, struct account_t& out) {
      std::vector<std::string> v;
      
      split(trim(memo), ":-", v);
      
      /* This is true for all above cases */
      out.name = name{v[0]};
      out.owner_key = abieos::string_to_public_key(v[1]);

      /* default values unless overwritten below */
      out.stake_cpu = default_cpu_stake;
      out.ram_amount_bytes = default_ram_amount_bytes;
      
      size_t size = v.size();
      switch(size) {
        case 2:
          out.active_key = out.owner_key;
          break;
        case 3:
          out.active_key = abieos::string_to_public_key(v[2]);
          break;
        case 4:
          out.active_key = out.owner_key;
          out.stake_cpu = asset_from_string(v[2]);
          out.ram_amount_bytes = bytes_from_string(v[3]);
          break;
        case 5:
          out.active_key = abieos::string_to_public_key(v[2]);
          out.stake_cpu = asset_from_string(v[3]);
          out.ram_amount_bytes = bytes_from_string(v[4]);
          break;
      }
    }

    uint32_t bytes_from_string(std::string str) {
      const auto ram_amount_kb = std::strtoul(str.c_str(), nullptr, 10);
      const auto ram_amount_bytes = ram_amount_kb * 1024;
      check(ram_amount_bytes > default_ram_amount_bytes, "Accounts require at least 3 KB of RAM");
      return ram_amount_bytes;
    }
    
    asset asset_from_string(std::string str) {
      const auto amount = std::atoll(str.c_str());
      asset money{amount, core_symbol};
      money *= 10000;
      check(money.amount > 0, "Please stake a positive amount");
      return money;
    }
    
    std::string trim(const string& str) {
      const size_t first = str.find_first_not_of(' ');
      const size_t last = str.find_last_not_of(' ');
      return (const std::string)str.substr(first, (last-first+1));
    }

    void split(const string& s, const string& delimiters, vector<string>& v) {
      string::size_type i = 0;
      string::size_type j = find_any(s, delimiters);

      while(j != string::npos) {
        v.push_back(s.substr(i, j-i));
        i = ++j;
        j = find_any(s, delimiters, j);

        if(j == string::npos) {
          v.push_back(s.substr(i, s.length()));
        }
      }
    }
  
    string::size_type find_any(const string& s, const string& delimiters, const string::size_type start=0) {
      for(std::string::size_type i = start; i < s.size(); ++i) {
        if(is_any(s[i], delimiters)) {
          return i;
        }
      }
      return string::npos;
    }
  
    bool is_any(const char& s, const string& candidates) {
      for(const char& c : candidates) {
        if(s == c) {
          return true;
        }
      }
      return false;
    }
    
    
    
    void create_account(const asset quantity, struct account_t& data) {
      const key_weight owner_pubkey_weight{
        .key = data.owner_key,
        .weight = 1,
      };
      const key_weight active_pubkey_weight{
          .key = data.active_key,
          .weight = 1,
      };
      const authority owner{
          .threshold = 1,
          .keys = {owner_pubkey_weight},
          .accounts = {},
          .waits = {}
      };
      const authority active{
          .threshold = 1,
          .keys = {active_pubkey_weight},
          .accounts = {},
          .waits = {}
      };
      const newaccount new_account{
          .creator = _self,
          .name = data.name,
          .owner = owner,
          .active = active
      };
      const auto stake_cpu = data.stake_cpu;
      const auto stake_net = default_net_stake;
      const auto rent_cpu_amount = asset{10, core_symbol};
      const auto ram_price = determine_ram_price(data.ram_amount_bytes);
      const auto ram_replace_amount = determine_ram_price(800);
      const auto fee = asset{std::max((quantity.amount + 119) / 200, 1000ll), core_symbol};      
      const auto remaining_balance = quantity - stake_cpu - stake_net - ram_price - fee - ram_replace_amount - rent_cpu_amount;
      check(remaining_balance.amount >= 0, "Not enough money");
      
      action(
        permission_level{ _self, "active"_n, },
        "eosio"_n,
        "newaccount"_n,
        new_account
      ).send();

      action(
        permission_level{ _self, "active"_n},
        "eosio"_n,
        "buyram"_n,
        make_tuple(_self, data.name, ram_price)
      ).send();
      
      action(
        permission_level{ _self, "active"_n},
        "eosio"_n,
        "buyram"_n,
        make_tuple(_self, _self, ram_replace_amount)
      ).send();
      
      action(
        permission_level{ _self, "active"_n},
        "eosio.token"_n,
        "transfer"_n,
        make_tuple(_self, "saccountfees"_n, fee, std::string("Account creation fee"))
      ).send();

      action(
        permission_level{ _self, "active"_n},
        "eosio"_n,
        "delegatebw"_n,
        make_tuple(_self, data.name, stake_net, stake_cpu, true)
      ).send();
      
      action(
        permission_level{ _self, "active"_n},
        "eosio"_n,
        "deposit"_n,
        make_tuple(_self, rent_cpu_amount)
      ).send();
      
      action(
        permission_level{ _self, "active"_n},
        "eosio"_n,
        "rentcpu"_n,
        make_tuple(_self, data.name, rent_cpu_amount, asset{0, core_symbol})
      ).send();
      
      if(remaining_balance.amount > 0) {
        action(
          permission_level{ _self, "active"_n},
          "eosio.token"_n,
          "transfer"_n,
          make_tuple(_self, data.name, remaining_balance, std::string("Initial balance"))
        ).send();
      }
    }
};
