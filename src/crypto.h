#pragma once

#include <string>

namespace crypto {
std::string random_key_hex(std::size_t bytes = 32);
std::string digest(const std::string &material, const std::string &keyHex = "");
std::string encrypt(const std::string &plain, const std::string &keyHex, const std::string &salt);
std::string decrypt(const std::string &cipherB64, const std::string &keyHex, const std::string &salt);
}
