#pragma once

#include <string>

class PasswordHasher {
public:
    static bool Hash(const std::string& password, std::string& encoded_hash);
    static bool Verify(const std::string& password, const std::string& encoded_hash);

private:
    static bool EnsureInitialized();
};
