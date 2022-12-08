#include <sam.hpp>
using namespace eosio;
using namespace abieos;

ACTION sam::whitelist(vector<name> user){
  require_auth( get_self() );
  // create the kams singleton that tracks contract information
  kams_singleton kams(get_self(), get_self().value);
  auto kam = kams.get_or_create(get_self());

  kam.whitelist.insert(kam.whitelist.end(), user.begin(), user.end());
  kams.set(kam, get_self());
}


ACTION sam::regaccw(string account, string key){
    require_auth(_self);
    vector<name> acc{name(account)};
    whitelist(acc);
    regacc(account, key);
}

ACTION sam::regacc(string account, string key){
    require_auth(_self);
    kams_singleton kams(get_self(), get_self().value);
    auto kam = kams.get_or_create(get_self());
    check(std::find(kam.whitelist.begin(), kam.whitelist.end(), name(account)) != kam.whitelist.end(), "Account not allowed.");

    account_t n_account;
    n_account.name = wam_to_sam(account);

    public_key pubkey = string_to_public_key(key);
    n_account.active_key = pubkey;
    create_account(n_account);

}

void sam::create_account(struct account_t& data) {

    permission_level owner = permission_level(get_self(), name("accounts"));
    permission_level_weight owner_perm {
        .permission = owner,
        .weight = 1 
    };

    authority owner_auth {
        .threshold = 1,
        .keys = {},
        .accounts = {owner_perm},
        .waits = {}
    };

    key_weight active_pubkey_weight{
        .key = data.active_key,
        .weight = 1
    };

    authority active_auth {
        .threshold = 1,
        .keys = {active_pubkey_weight},
        .accounts = {},
        .waits = {}
    };

    newaccount new_account {
        .creator = _self,
        .name = data.name,
        .owner = owner_auth,
        .active = active_auth
    };

    buy_ram ram_args{
        .payer = _self,
        .receiver = data.name,
        .bytes = default_ram_amount_bytes
    };
    
    deleg_bw deleg_args {
        .from = _self,
        .receiver = data.name,
        .net = default_net_stake,
        .cpu = default_cpu_stake
    };

    permission_level permissionLevel = permission_level(get_self(), name("active"));

    action create = action(
        permissionLevel,
        "eosio"_n,
        "newaccount"_n,
        std::move(new_account)
    );
    create.send();

    action buyRam = action(
        permissionLevel,
        "eosio"_n,
        "buyrambytes"_n,
        std::move(ram_args)
    );
    buyRam.send();

    action delegate = action(
        permissionLevel,
        "eosio"_n,
        "delegatebw"_n,
        std::move(deleg_args)
    );
    delegate.send();

}


name sam::wam_to_sam(string account) {

    check(account.size() > 4, "only Cloud Wallet Accounts Supported");
    string suffix = account.substr((account.size()-4));
    check(suffix == ".wam", "only Cloud Wallet Accounts Supported");
    string prefix = account.substr(0, account.size()-4);
    prefix+=YOUR_SUFFIX;
    check(prefix.size()<13,"Account not supported. (length)");

    name sam_acc = name(prefix);

    return sam_acc;
}


ACTION sam::recann(string account, string key){

    require_auth( get_self() );

    // get the kams singleton
    kams_singleton kams(get_self(), get_self().value);
    auto kam = kams.get_or_create(get_self());
    if (kam.recanns.size() > 9) {
        kam.recanns.clear();
    }
    kam.recanns.push_back(string_to_public_key(key));
    kams.set(kam, get_self());
}


ACTION sam::recacc(string account, string key){
    name acc = name(account);
    require_auth(acc);
    
    // get the kams singleton
    kams_singleton kams(get_self(), get_self().value);
    auto kam = kams.get_or_create(get_self());
    
    
    account_t n_account;
    n_account.name = wam_to_sam(account);
    n_account.active_key = string_to_public_key(key);

    check(std::find(kam.recanns.begin(), kam.recanns.end(), n_account.active_key) != kam.recanns.end(), "Your princess is in another castle.");
    //TODO: remove used keys

    recover_account(n_account);
}

ACTION sam::setactivekey(string account, string key){
    require_auth(get_self());
    name acc = name(account);

    account_t n_account;
    n_account.name = wam_to_sam(account);
    n_account.active_key = string_to_public_key(key);

    recover_account(n_account);
}

void sam::recover_account(struct account_t& data) {

    key_weight active_pubkey_weight{
        .key = data.active_key,
        .weight = 1
    };

    authority active_auth {
        .threshold = 1,
        .keys = {active_pubkey_weight},
        .accounts = {},
        .waits = {}
    };

    recover_args recovery {
        .account = data.name,
        .permission = name("active"),
        .parent = name("owner"),
        .auth = active_auth
    };

    action recover = action(
        permission_level(data.name, name("owner")),
        "eosio"_n,
        "updateauth"_n,
        std::move(recovery)
    );
    recover.send();
}

ACTION sam::giftram(string account, uint32_t bytes){
    require_auth(_self);
    buy_ram ram_args{
        .payer = _self,
        .receiver = name(account),
        .bytes = bytes
    };

    action buyRam = action(
        permission_level(get_self(), name("active")),
        "eosio"_n,
        "buyrambytes"_n,
        std::move(ram_args)
    );
    buyRam.send();
}

ACTION sam::boostacc(string account){
    require_auth(_self);

    deleg_bw deleg_args {
        .from = _self,
        .receiver = name(account),
        .net = default_net_stake,
        .cpu = default_cpu_stake
    };

    action delegate = action(
        permission_level(get_self(), name("active")),
        "eosio"_n,
        "delegatebw"_n,
        std::move(deleg_args)
    );
    delegate.send();

}

ACTION sam::link(name account, uint64_t key) {
    require_auth(account);

}


//=================================== admin functions ===================================

void sam::takeowner(name account) {
    require_auth(_self);

    permission_level owner = permission_level(get_self(), name("accounts"));
    permission_level_weight owner_perm {
        .permission = owner,
        .weight = 1 
    };

    authority owner_auth {
        .threshold = 1,
        .keys = {},
        .accounts = {owner_perm},
        .waits = {}
    };

    recover_args recovery {
        .account = account,
        .permission = name("owner"),
        .parent = {},
        .auth = owner_auth
    };

    action recover = action(
        permission_level(account, name("owner")),
        "eosio"_n,
        "updateauth"_n,
        std::move(recovery)
    );
    recover.send();
}
