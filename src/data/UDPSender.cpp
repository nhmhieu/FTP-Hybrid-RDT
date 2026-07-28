#include "UDPSender.h"
#include "protocol.h"
#include "checksum.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>

// Khởi tạo Winsock 
#pragma comment(lib, "ws2_32.lib")

UDPSender::UDPSender() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    
    //tạo socket UDP
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cout << "Không thể tạo socket UDP" << std::endl;
    }

    // Thiết lập timeout cho việc nhận ACK
    timeoutMs = 500;
    maxRetries = 5;
}

UDPSender::~UDPSender() {
    closesocket(sock);
    WSACleanup();
}

bool UDPSender::sendFile(const std::string& filePath, const std::string& destIP, int destPort) {
    //1. Mở bằng file binary
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        std::cout << "Không thể mở file: " << filePath << std::endl;
        return false;
    }
    //2. Thiết lập địa chỉ đích
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(destPort);
    destAddr.sin_addr.s_addr = inet_addr(destIP.c_str());\

    //3. Đọc file và gửi từng chunk
    uint16_t seq = 0;
    bool success = true;
    char buffer[UDP_PAYLOAD_MAX + sizeof(UDP_PAYLOAD_MAX)]; // vùng nhớ đệm cho gói
    
    while (!file.eof()) {
        // Đọc tối đa UDP_PAYLOAD_MAX byte từng file
        file.read(buffer + sizeof(udp_header_t), UDP_PAYLOAD_MAX);
        size_t bytesRead = file.gcount(); // số byte thực tế đọc được

        if (bytesRead == 0) break; // hết file

        // Xây dựng Header
        udp_header_t* header = (udp_header_t*)buffer;
        header->magic = htons(MAGIC_NUMBER);
        header->seq = htons(seq);
        header->ack = 0; 
        header->checksum = 0;

        //Tính checksum cho toàn bộ gói
        size_t packetLen = sizeof(udp)

    }
}