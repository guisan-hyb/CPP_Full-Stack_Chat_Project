#include "PasswordHasher.h"

#include <array>
#include <mutex>
#include <sodium.h>

namespace {
bool initialized = false;
std::once_flag init_flag;
}

bool PasswordHasher::EnsureInitialized()
{
    std::call_once(init_flag, [] {
        initialized = sodium_init() >= 0;
    });
    return initialized;
}

bool PasswordHasher::Hash(const std::string& password, std::string& encoded_hash)
{
    if (!EnsureInitialized() || password.empty()) {
        return false;
    }

    std::array<char, crypto_pwhash_STRBYTES> hash{};
    if (crypto_pwhash_str(
            hash.data(),
            password.data(),
            password.size(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        return false;
    }

    encoded_hash.assign(hash.data());
    sodium_memzero(hash.data(), hash.size());
    return true;
}

bool PasswordHasher::Verify(const std::string& password, const std::string& encoded_hash)
{
    if (!EnsureInitialized() || password.empty() || encoded_hash.empty()) {
        return false;
    }

    return crypto_pwhash_str_verify(
               encoded_hash.c_str(),
               password.data(),
               password.size()) == 0;
}
