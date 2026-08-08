#pragma once
#include <filesystem>
#include <string>

namespace Crypto {
std::string sha256File(const std::filesystem::path& file);
}
