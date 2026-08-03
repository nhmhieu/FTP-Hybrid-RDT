
#ifndef PROTOCOL_H
#define PROTOCOL_H


#include <cstdint>
#include <cstddef>

constexpr std::uint16_t MAGIC_NUMBER = 0xCAFE;
constexpr std::size_t UDP_PAYLOAD_MAX = 1024;

//Môi bit mô tả một loại gói
enum PacketFlages : std::uint8_t {
    FLAG_DATA = 1 << 0,
    FLAG_ACK = 1 << 1,
    FLAG_FIN = 1 << 2

}; 
#pragma pack(push, 1) // Ép chặt struct
 struct  udp_header_t{
    std::uint16_t magic;// 2 byte: nhận diện gói hợp lệ
    std::uint8_t flags; // 1 byte: cờ hiệu (DATA, ACK, FIN)
    std::uint8_t reserved;// 1 byte: dành riêng cho mở rộng sau này
    std::uint32_t seq;   // 2 byte: số thứ tự gói
    std::uint32_t ack;   // 2 byte: số ACK (gói xác nhận) 
    std::uint16_t payloadLen; // 2 bye: độ dài dữ liệu trong gói
    std::uint16_t checksum; 
};
#pragma pack(pop)
static_assert(sizeof(udp_header_t) == 16, "Kích thước struct udp_header_t không đúng 16 bytes");
#endif