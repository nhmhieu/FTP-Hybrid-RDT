#include "checksum.h"

// Hàm tính checksum: nhận con trỏ dữ liệu và độ dài, trả về checksum 16-bit
uint16_t calculate_checksum(const uint8_t* date, size_t len) {
    uint32_t sum = 0; // dùng 32-bit để tính tổng

    // Duyệt từng cặp 2 byte (16-bit)
    const uint16_t* ptr = (const uint16_t*) date;
    while (len > 1) {
        sum += *ptr++; // cộng giá trị 16-bit vào sum
    }
    
    //Fold: đưa các bit cao xuống thấp cho đến khi chỉ còn 16 bit
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // Lấy phân bù 1 
    return (uint16_t)(~sum);
}

bool verify_checksum(const uint8_t* packet, size_t packetLen) {
    // Lấy checksum từ gói
    uint16_t storedChecksum  = *(uint16_t*)(packet + 6);
    // Tạm thời ghi 0 vào vị trí checksum để tính lại
    uint16_t* checksumPtr = (uint16_t*)(packet + 6);
    uint16_t old = *checksumPtr;
    *checksumPtr = 0;
    uint16_t calculated = calculate_checksum(packet, packetLen);
    *checksumPtr = old; // Khôi phục lại
    return (calculated = storedChecksum);
}