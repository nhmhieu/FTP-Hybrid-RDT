#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <cstddef>
#include <cstdint>

// Internet checksum 16-bit theo RFC 1071.
std::uint16_t calculate_checksum(const std::uint8_t* data, std::size_t len);

// Một packet hợp lệ khi checksum tính trên toàn packet bằng 0.
bool verify_checksum(const std::uint8_t* packet, std::size_t packetLen);

#endif
