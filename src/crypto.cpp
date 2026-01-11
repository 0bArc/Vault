#include "crypto.h"

#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

namespace {
std::vector<std::uint8_t> hex_to_bytes(const std::string &hex) {
    if (hex.size() % 2 != 0) throw std::runtime_error("Bad hex key");
    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        auto byte = static_cast<std::uint8_t>(std::stoul(byteStr, nullptr, 16));
        out.push_back(byte);
    }
    return out;
}

#ifdef _WIN32
std::vector<std::uint8_t> random_bytes(std::size_t n) {
    std::vector<std::uint8_t> out(n);
    if (BCryptGenRandom(nullptr, out.data(), (ULONG)out.size(), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("BCryptGenRandom failed");
    }
    return out;
}
#endif

std::string bytes_to_hex(const std::vector<std::uint8_t> &bytes) {
    std::ostringstream oss;
    oss.setf(std::ios::hex, std::ios::basefield);
    oss.setf(std::ios::uppercase, std::ios::fmtflags(0));
    oss.fill('0');
    for (auto b : bytes) {
        oss << std::hex;
        oss.width(2);
        oss << static_cast<int>(b);
    }
    return oss.str();
}

std::string base64_encode(const std::string &input) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0;
    int valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string base64_decode(const std::string &input) {
    static const int T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };
    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (c == '=') break;
        int d = T[c];
        if (d == -1) continue;
        val = (val << 6) + d;
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}
}

namespace crypto {

std::string random_key_hex(std::size_t bytes) {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 255);
    std::vector<std::uint8_t> raw(bytes);
    for (auto &b : raw) b = static_cast<std::uint8_t>(dist(rd));
    return bytes_to_hex(raw);
}

std::string digest(const std::string &material, const std::string &keyHex) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg{};
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status < 0) throw std::runtime_error("HMAC provider open failed");
    auto keyBytes = keyHex.empty() ? std::vector<std::uint8_t>() : hex_to_bytes(keyHex);
    BCRYPT_HASH_HANDLE hash{};
    status = BCryptCreateHash(alg, &hash, nullptr, 0, keyBytes.empty() ? nullptr : keyBytes.data(), (ULONG)keyBytes.size(), 0);
    if (status < 0) { BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("HMAC create failed"); }
    status = BCryptHashData(hash, (PUCHAR)material.data(), (ULONG)material.size(), 0);
    if (status < 0) { BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("HMAC data failed"); }
    std::vector<std::uint8_t> out(32);
    status = BCryptFinishHash(hash, out.data(), (ULONG)out.size(), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg,0);
    if (status < 0) throw std::runtime_error("HMAC finish failed");
    return bytes_to_hex(out);
#else
    throw std::runtime_error("HMAC requires Windows (bcrypt) in this build");
#endif
}

std::string encrypt(const std::string &plain, const std::string &keyHex, const std::string &salt) {
    auto keyBytes = hex_to_bytes(keyHex);
    auto ivBytes = random_bytes(12);

#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg{};
    BCRYPT_KEY_HANDLE key{};
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (status < 0) throw std::runtime_error("AES provider open failed");
    status = BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (status < 0) { BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("Set chaining mode failed"); }

    status = BCryptGenerateSymmetricKey(alg, &key, nullptr, 0, keyBytes.data(), (ULONG)keyBytes.size(), 0);
    if (status < 0) { BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("Key gen failed"); }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO gcmInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(gcmInfo);
    gcmInfo.pbNonce = ivBytes.data();
    gcmInfo.cbNonce = (ULONG)ivBytes.size();
    gcmInfo.pbAuthData = (PUCHAR)salt.data();
    gcmInfo.cbAuthData = (ULONG)salt.size();
    std::array<std::uint8_t, 16> tag{};
    gcmInfo.pbTag = tag.data();
    gcmInfo.cbTag = (ULONG)tag.size();

    ULONG outSize = 0;
    status = BCryptEncrypt(key, (PUCHAR)plain.data(), (ULONG)plain.size(), &gcmInfo, nullptr, 0, nullptr, 0, &outSize, 0);
    if (status < 0) { BCryptDestroyKey(key); BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("Encrypt size failed"); }
    std::string cipher(outSize, '\0');
    status = BCryptEncrypt(key, (PUCHAR)plain.data(), (ULONG)plain.size(), &gcmInfo, nullptr, 0, (PUCHAR)cipher.data(), outSize, &outSize, 0);
    if (status < 0) { BCryptDestroyKey(key); BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("Encrypt failed"); }
    cipher.resize(outSize);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg,0);

    // pack iv|tag|cipher
    std::string packed;
    packed.reserve(ivBytes.size() + tag.size() + cipher.size());
    packed.append(reinterpret_cast<const char*>(ivBytes.data()), ivBytes.size());
    packed.append(reinterpret_cast<const char*>(tag.data()), tag.size());
    packed.append(cipher);
    return base64_encode(packed);
#else
    throw std::runtime_error("AES-GCM requires Windows (bcrypt) in this build");
#endif
}

std::string decrypt(const std::string &cipherB64, const std::string &keyHex, const std::string &salt) {
    auto keyBytes = hex_to_bytes(keyHex);
    auto packed = base64_decode(cipherB64);
    const std::size_t ivLen = 12;
    const std::size_t tagLen = 16;
    if (packed.size() < ivLen + tagLen) throw std::runtime_error("Cipher too short");
    std::string iv(packed.data(), ivLen);
    std::string tag(packed.data() + ivLen, tagLen);
    std::string body(packed.data() + ivLen + tagLen, packed.size() - ivLen - tagLen);

#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg{};
    BCRYPT_KEY_HANDLE key{};
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0);
    if (status < 0) throw std::runtime_error("AES provider open failed");
    status = BCryptSetProperty(alg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
    if (status < 0) { BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("Set chaining mode failed"); }
    status = BCryptGenerateSymmetricKey(alg, &key, nullptr, 0, keyBytes.data(), (ULONG)keyBytes.size(), 0);
    if (status < 0) { BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("Key gen failed"); }

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO gcmInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(gcmInfo);
    gcmInfo.pbNonce = reinterpret_cast<PUCHAR>(iv.data());
    gcmInfo.cbNonce = (ULONG)iv.size();
    gcmInfo.pbAuthData = (PUCHAR)salt.data();
    gcmInfo.cbAuthData = (ULONG)salt.size();
    gcmInfo.pbTag = reinterpret_cast<PUCHAR>(tag.data());
    gcmInfo.cbTag = (ULONG)tag.size();

    ULONG outSize = 0;
    status = BCryptDecrypt(key, (PUCHAR)body.data(), (ULONG)body.size(), &gcmInfo, nullptr, 0, nullptr, 0, &outSize, 0);
    if (status < 0) { BCryptDestroyKey(key); BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("Decrypt size failed"); }
    std::string plain(outSize, '\0');
    status = BCryptDecrypt(key, (PUCHAR)body.data(), (ULONG)body.size(), &gcmInfo, nullptr, 0, (PUCHAR)plain.data(), outSize, &outSize, 0);
    if (status < 0) { BCryptDestroyKey(key); BCryptCloseAlgorithmProvider(alg,0); throw std::runtime_error("Decrypt failed"); }
    plain.resize(outSize);
    BCryptDestroyKey(key);
    BCryptCloseAlgorithmProvider(alg,0);
    return plain;
#else
    throw std::runtime_error("AES-GCM requires Windows (bcrypt) in this build");
#endif
}

} // namespace crypto
