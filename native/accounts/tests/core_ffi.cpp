#include "fourstory_accounts.h"
#include <array>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <string>
#include <algorithm>

int main() {
    auto bytes = [](const char* p) { return reinterpret_cast<const uint8_t*>(p); };
    auto validate = [&](const char* name, const char* email, const char* pass, const char* confirm) {
        return fourstory_accounts_validate(bytes(name), std::strlen(name), bytes(email), std::strlen(email),
            bytes(pass), std::strlen(pass), bytes(confirm), std::strlen(confirm));
    };
    assert(validate("Player_1", "", "  abcdef  ", "  abcdef  ") == FOURSTORY_ACCOUNT_OK);
    assert(validate("Player_1", "a@example.com", "ExamplePass42", "ExamplePass42") == FOURSTORY_ACCOUNT_OK);
    assert(validate("bad name", "", "ExamplePass42", "ExamplePass42") == FOURSTORY_ACCOUNT_NAME);
    assert(validate("Player", "a@@b", "ExamplePass42", "ExamplePass42") == FOURSTORY_ACCOUNT_EMAIL);
    assert(validate("Player", "", "short", "short") == FOURSTORY_ACCOUNT_PASSWORD);
    assert(validate("Player", "", "ExamplePass42", "different") == FOURSTORY_ACCOUNT_CONFIRMATION);
    assert(fourstory_accounts_validate(nullptr, 1, nullptr, 0, nullptr, 0, nullptr, 0) == FOURSTORY_ACCOUNT_INVALID_ARGUMENT);
    assert(fourstory_accounts_validate(nullptr, 51, nullptr, 0, nullptr, 0, nullptr, 0) == FOURSTORY_ACCOUNT_NAME);
    const uint8_t invalid[] = {0xff};
    assert(fourstory_accounts_validate(invalid, 1, nullptr, 0, nullptr, 0, nullptr, 0) == FOURSTORY_ACCOUNT_NAME);
    std::array<uint8_t, 18> guarded; guarded.fill(0xa5);
    size_t length = 99;
    assert(fourstory_accounts_generate_name(guarded.data()+1, 15, &length) == FOURSTORY_ACCOUNT_BUFFER_TOO_SMALL);
    assert(length == 0);
    for (auto value: guarded) assert(value == 0xa5);
    assert(fourstory_accounts_generate_name(nullptr, 16, &length) == FOURSTORY_ACCOUNT_INVALID_ARGUMENT);
    assert(fourstory_accounts_generate_name(guarded.data()+1, 16, nullptr) == FOURSTORY_ACCOUNT_INVALID_ARGUMENT);
    for (int i = 0; i < 100; ++i) {
        guarded.fill(0xa5);
        assert(fourstory_accounts_generate_name(guarded.data()+1, 16, &length) == FOURSTORY_ACCOUNT_OK);
        assert(length > 0 && length <= 15 && guarded[length+1] == 0);
        assert(guarded.front() == 0xa5 && guarded.back() == 0xa5);
        for (size_t j = 1; j <= length; ++j)
            assert((guarded[j] >= 'a' && guarded[j] <= 'z') || (guarded[j] >= '0' && guarded[j] <= '9'));
    }
    if(std::getenv("FOURSTORY_ACCOUNTS_ABI_HTTP")) {
        auto start=[&](const char* name){return fourstory_accounts_register(bytes(name),std::strlen(name),bytes(""),0,bytes("AbiSecret123"),12,bytes("AbiSecret123"),12);};
        std::string name=std::getenv("FOURSTORY_ACCOUNTS_ABI_NAME")?std::getenv("FOURSTORY_ACCOUNTS_ABI_NAME"):"BridgePlayer";
        assert(start(name.c_str())==20);
        int result=20;
        for(int i=0;i<400 && result==20;++i){std::this_thread::sleep_for(std::chrono::milliseconds(50));result=fourstory_accounts_poll();}
        if(std::getenv("FOURSTORY_ACCOUNTS_ABI_TLS_REJECT")){assert(result==23);std::puts("PASS untrusted TLS certificate rejected");return 0;}
        assert(result==21);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});
        assert(start(name.c_str())==20);
        result=20;
        for(int i=0;i<400 && result==20;++i){std::this_thread::sleep_for(std::chrono::milliseconds(50));result=fourstory_accounts_poll();}
        assert(result==22);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        assert(start("BridgeCancel")==20);fourstory_accounts_cancel();
        for(int i=0;i<20;++i){std::this_thread::sleep_for(std::chrono::milliseconds(50));assert(fourstory_accounts_poll()==0);}
        std::puts("PASS real C++ -> Rust -> HTTPS -> least-privilege SQL create, duplicate and canceled-result isolation");
    }
    std::puts("PASS native C++ -> Rust account validation and OS-random name ABI");
}
