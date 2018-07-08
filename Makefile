CPP_IN=smart_account_creator
PK=EOS6KAV4we7bezueZ31pWf26aa8JpTe1Edp5kj298ZKBuN7c7t8D5
CLEOS=./cleos.sh
CONTRACT_ACCOUNT=sac123451235
EOS_CONTRACTS_DIR=/Users/al/Documents/eos/eos/build/contracts
ACCOUNT_NAME := $(shell python gen.py)
CLEOS=./cleos.sh

build:
	eosiocpp -o $(CPP_IN).wast $(CPP_IN).cpp

abi:
	eosiocpp -g $(CPP_IN).abi $(CPP_IN).cpp

all: build abi

deploy: build
	$(CLEOS) set contract $(CONTRACT_ACCOUNT) ../smart_account_creator

setup:
	$(CLEOS) system newaccount --stake-net "1.0000 EOS" --stake-cpu "1.0000 EOS" --buy-ram-kbytes 8000 eosio $(CONTRACT_ACCOUNT) $(PK) $(PK)
	$(CLEOS) set account permission $(CONTRACT_ACCOUNT) active '{"threshold": 1,"keys": [{"key": "$(PK)","weight": 1}],"accounts": [{"permission":{"actor":"$(CONTRACT_ACCOUNT)","permission":"eosio.code"},"weight":1}]}' owner -p $(CONTRACT_ACCOUNT)
	$(CLEOS) system newaccount --stake-net "1.0000 EOS" --stake-cpu "1.0000 EOS" --buy-ram-kbytes 8000 eosio saccountfees $(PK) $(PK)

test:
	$(CLEOS) transfer angelo $(CONTRACT_ACCOUNT) "1.0000 EOS" "$(ACCOUNT_NAME):EOS7R6HoUvevAtoLqUMSix74x9Wk4ig75tA538HaGXLFKpquKCPkH:EOS6bWFTECWtssKrHQVrkKKf68EydHNyr1ujv23KCZMFUxqwcGqC3" -p $(CONTRACT_ACCOUNT)@active -p angelo@active
	$(CLEOS) get account $(ACCOUNT_NAME)
	
clean:
	rm -f $(CPP_IN).wast $(CPP_IN).wasm
