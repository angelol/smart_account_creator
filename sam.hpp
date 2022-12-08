#include <eosio/eosio.hpp>
#include <eosio/action.hpp>
#include <eosio/asset.hpp>
#include <eosio/system.hpp>
#include <public_key.hpp>
#include <eosio/singleton.hpp>

using namespace std;
using namespace eosio;

CONTRACT sam : public contract {
    public:
        using contract::contract;

        static constexpr symbol core_symbol = symbol("WAX", 8);
        static constexpr name eosio_token_account{"eosio.token"};
        static constexpr symbol RAM_symbol{"RAM", 0};
        static constexpr symbol RAMCORE_symbol{"RAMCORE", 4};
        static constexpr string YOUR_SUFFIX = ".hive";
        const asset default_cpu_stake{500000000, core_symbol};
        const asset default_net_stake{100000000, core_symbol};
        const uint32_t default_ram_amount_bytes{3000};

        ACTION whitelist(vector<name> account);
        ACTION regacc(string account, string key);
        ACTION regaccw(string account, string key);
        ACTION recann(string account, string key);
        ACTION recacc(string account, string key);
        ACTION setactivekey(string account, string key);

        ACTION giftram(string account, uint32_t bytes);
        ACTION boostacc(string account);

        ACTION link(name account, uint64_t key);
        
        ACTION takeowner(name account);


    private:

        //==================================== structs ====================================

        struct account_t {
            name name;
            public_key active_key;
            EOSLIB_SERIALIZE( account_t, (name)(active_key) )
        };

        struct permission_level_weight {
            permission_level permission;
            uint16_t weight;
        };

        struct key_weight {
            public_key key;
            uint16_t weight;
            EOSLIB_SERIALIZE( key_weight, (key)(weight) )
        };

        struct wait_weight {
            uint32_t wait_sec;
            uint16_t weight;
        };

        struct authority {
            uint32_t threshold;
            vector<key_weight> keys;
            vector<permission_level_weight> accounts;
            vector<wait_weight> waits;
        };

        struct newaccount {
            name creator;
            name name;
            authority owner;
            authority active;
        };

        struct buy_ram { //eosio::buyrambytes
            name payer;
            name receiver;
            uint32_t bytes;
        };

        struct deleg_bw { //eosio::delegatebw
            name from;
            name receiver;
            asset net;
            asset cpu;
            bool transfer = false;
        };

        struct recover_args {
            name account;
            name permission;
            name parent;
            authority auth;
        };

        //==================================== tables ====================================


        // scope: self
        TABLE kams { 
            vector<name> whitelist; // containing whitelisted accounts eligble 
            vector<public_key> recanns;
            EOSLIB_SERIALIZE(kams, (whitelist)(recanns))
        };
        typedef singleton<name("kams"), kams> kams_singleton;

        void create_account(struct account_t& data);
        void recover_account(struct account_t& data);

        name wam_to_sam(string account);
};       
