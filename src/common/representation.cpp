#include "common/representation.h"
#include <fstream>
#include <string>
#include <cstdint>

namespace Representation {
bool toAsciiWire(const std::filesystem::path& source, const std::filesystem::path& output) {
    std::ifstream in(source, std::ios::binary);
    std::ofstream out(output, std::ios::binary | std::ios::trunc);
    if (!in || !out) return false;
    char ch;
    while (in.get(ch)) {
        if (ch == '\r') {
            if (in.peek() == '\n') in.get();
            out.write("\r\n", 2);
        } else if (ch == '\n') {
            out.write("\r\n", 2);
        } else out.put(ch);
    }
    return in.eof() && out.good();
}

bool fromAsciiWire(const std::filesystem::path& file) {
    const auto temp = file.string() + ".ascii.tmp";
    std::ifstream in(file, std::ios::binary);
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!in || !out) return false;
    char ch;
    while (in.get(ch)) {
        if (ch == '\r' && in.peek() == '\n') { in.get(); out.put('\n'); }
        else out.put(ch);
    }
    out.close(); in.close();
    std::error_code ec;
    std::filesystem::remove(file, ec); ec.clear();
    std::filesystem::rename(temp, file, ec);
    return !ec;
}

bool encodeBlock(const std::filesystem::path& source, const std::filesystem::path& output) {
    std::ifstream in(source, std::ios::binary); std::ofstream out(output, std::ios::binary | std::ios::trunc);
    if (!in || !out) return false;
    char buffer[4096];
    while (in) {
        in.read(buffer, sizeof(buffer)); const std::uint32_t n = static_cast<std::uint32_t>(in.gcount());
        if (!n) break;
        const unsigned char length[4] = { static_cast<unsigned char>(n >> 24), static_cast<unsigned char>(n >> 16),
            static_cast<unsigned char>(n >> 8), static_cast<unsigned char>(n) };
        out.write(reinterpret_cast<const char*>(length), 4); out.write(buffer, n);
    }
    const char end[4]{}; out.write(end, 4); return out.good();
}

bool decodeBlock(const std::filesystem::path& file) {
    const auto temp = file.string() + ".block.tmp"; std::ifstream in(file, std::ios::binary);
    std::ofstream out(temp, std::ios::binary | std::ios::trunc); if (!in || !out) return false;
    while (true) {
        unsigned char length[4]; if (!in.read(reinterpret_cast<char*>(length), 4)) return false;
        const std::uint32_t n = (std::uint32_t(length[0]) << 24) | (std::uint32_t(length[1]) << 16) |
            (std::uint32_t(length[2]) << 8) | length[3]; if (n == 0) break;
        std::string data(n, '\0'); if (!in.read(data.data(), n)) return false; out.write(data.data(), n);
    }
    out.close(); in.close(); std::error_code ec; std::filesystem::remove(file, ec); ec.clear();
    std::filesystem::rename(temp, file, ec); return !ec;
}

bool encodeRle(const std::filesystem::path& source, const std::filesystem::path& output) {
    std::ifstream in(source, std::ios::binary); std::ofstream out(output, std::ios::binary | std::ios::trunc);
    if (!in || !out) return false; char current; if (!in.get(current)) return in.eof();
    unsigned count = 1; char next;
    while (in.get(next)) { if (next == current && count < 255) ++count; else { out.put(static_cast<char>(count)); out.put(current); current = next; count = 1; } }
    out.put(static_cast<char>(count)); out.put(current); return out.good();
}

bool decodeRle(const std::filesystem::path& file) {
    const auto temp = file.string() + ".rle.tmp"; std::ifstream in(file, std::ios::binary);
    std::ofstream out(temp, std::ios::binary | std::ios::trunc); if (!in || !out) return false;
    unsigned char count; char value;
    while (in.read(reinterpret_cast<char*>(&count), 1)) { if (!in.get(value) || count == 0) return false; for (unsigned i=0;i<count;++i) out.put(value); }
    out.close(); in.close(); std::error_code ec; std::filesystem::remove(file, ec); ec.clear();
    std::filesystem::rename(temp, file, ec); return !ec;
}
}
