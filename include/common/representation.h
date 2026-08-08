#pragma once
#include <filesystem>

namespace Representation {
bool toAsciiWire(const std::filesystem::path& source, const std::filesystem::path& output);
bool fromAsciiWire(const std::filesystem::path& file);
bool encodeBlock(const std::filesystem::path& source, const std::filesystem::path& output);
bool decodeBlock(const std::filesystem::path& file);
bool encodeRle(const std::filesystem::path& source, const std::filesystem::path& output);
bool decodeRle(const std::filesystem::path& file);
}
