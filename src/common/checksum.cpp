#include "common/checksum.h"

std::uint16_t calculate_checksum(const std::uint8_t* data, std::size_t len) {
    std::uint32_t sum = 0;
    std::size_t i = 0;

    // Ghép từng cặp byte theo network byte order.
    while (i + 1 < len) {
        const std::uint16_t word =
            (static_cast<std::uint16_t>(data[i]) << 8) |
            static_cast<std::uint16_t>(data[i + 1]);
        sum += word;
        i += 2;
    }

    // Nếu số byte lẻ, byte cuối được xem là byte cao của word 16-bit.
    if (i < len) {
        sum += static_cast<std::uint16_t>(data[i]) << 8;
    }

    // Fold carry 32-bit xuống 16-bit.
    while ((sum >> 16) != 0) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }

    return static_cast<std::uint16_t>(~sum);
}

bool verify_checksum(const std::uint8_t* packet, std::size_t packetLen) {
    if (packet == nullptr || packetLen == 0) {
        return false;
    }

    return calculate_checksum(packet, packetLen) == 0;
}
