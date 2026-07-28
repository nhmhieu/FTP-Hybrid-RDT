#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <cstdint>
#include <cstddef>

// Tính checksum 16-bit theo RFC 1071 (Internet Checksum)
uint16_t calculate_checksum(const uint8_t* data, size_t len);

// Kiểm tra checksum của toàn bộ gói (header + padload)
bool verify_checksum(const uint8_t* packet, size_t packetLen );
#endif