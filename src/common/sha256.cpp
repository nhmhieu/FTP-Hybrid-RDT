#include "common/sha256.h"
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace Crypto {
std::string sha256File(const std::filesystem::path& file) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD objectSize = 0, bytes = 0, hashSize = 0;
    std::vector<unsigned char> object;
    std::vector<unsigned char> digest;
    auto cleanup = [&]() {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    };
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
            sizeof(objectSize), &bytes, 0) < 0 ||
        BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashSize),
            sizeof(hashSize), &bytes, 0) < 0) { cleanup(); return {}; }
    object.resize(objectSize); digest.resize(hashSize);
    if (BCryptCreateHash(algorithm, &hash, object.data(), objectSize, nullptr, 0, 0) < 0) {
        cleanup(); return {};
    }
    std::ifstream input(file, std::ios::binary);
    if (!input) { cleanup(); return {}; }
    std::array<unsigned char, 64 * 1024> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const auto count = input.gcount();
        if (count > 0 && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(count), 0) < 0) {
            cleanup(); return {};
        }
    }
    if (!input.eof() || BCryptFinishHash(hash, digest.data(), hashSize, 0) < 0) {
        cleanup(); return {};
    }
    cleanup();
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned char value : digest) output << std::setw(2) << static_cast<unsigned>(value);
    return output.str();
}
}
