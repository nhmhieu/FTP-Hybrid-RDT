#pragma once
#include <filesystem>

namespace Representation {
bool toAsciiWire(const std::filesystem::path& source, const std::filesystem::path& output);
bool fromAsciiWire(const std::filesystem::path& file);
}
