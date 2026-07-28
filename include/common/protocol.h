#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

#define MAGIC_NUMBER
#define UDP_PAYLOAD_MAX 1024 //Mỗi gói chở tối đa 1024 byte dữ liệu

#pragma pack(push, 1) // Ép chặt struct
typedef struct {
    uint16_t magic; // 2 byte: nhận diện gói hợp lệ
    uint16_t seq;   // 2 byte: số thứ tự gói
    uint16_t ack;   // 2 byte: số ACK (gói xác nhận) 
    uint16_t checksum;
} udp_header_t;
#pragma pack(pop)
#endif