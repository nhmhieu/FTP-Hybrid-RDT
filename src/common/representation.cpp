#include "common/representation.h"
#include <fstream>
#include <string>

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
}
