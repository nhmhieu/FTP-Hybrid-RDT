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
    destAddr.sin_addr.s_addr = inet_addr(destIP.c_str());

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
        size_t packetLen = sizeof(udp_header_t) + bytesRead;
        header->checksum = htons(calculate_checksum((uint8_t*)buffer, packetLen));

        //Gửi gói và chờ ACK
        bool sent = false; 
        for (int attempt = 0; attempt < maxRetries; attempt++) {
            //Gửi
            int result = sendto(sock, buffer, packetLen, 0, (sockaddr*)&destAddr, sizeof(destAddr));
            if (result == SOCKET_ERROR) {
                std::cout << "Lỗi gửi gói seq" << seq << std::endl;
                continue;
            }
            std::cout << "Đã gửi gói seq=" << seq << ", kích thước=" << packetLen << std::endl;

            //Chờ ACK
            if (waitForAck(seq)) {
                sent = true;
                break;
            } 
            else {
             std::cout << "Timeout, thử lại gói seq=" << seq << std::endl;
            }
        }

        if (!sent) {
            std::cout << "Không thể gửi gói seq=" << seq << " sau " << maxRetries << "lần thử " << std::endl;
            success = false;
            break;  
        }
        seq++;
    }

    file.close();
    return success;
}

bool UDPSender::sendPacket(const uint8_t* data, size_t len, uint16_t seq) {
    return true;
}

bool UDPSender::waitForAck(uint16_t expectedSeq) {
    int timeout = timeoutMs;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    
    char  recvBuffer[1024];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);

    while(true) {
        int recvLen = recvfrom(sock, recvBuffer, sizeof(recvBuffer), 0, (sockaddr*)&fromAddr, &fromLen);
        if (recvLen == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                return false;
            }
            std::cout << "Lỗi recvfrom: " << error << std::endl;
            return false;
        }
        
        // Kiểm tra gói ACK
        udp_header_t* header = (udp_header_t*)recvBuffer;
        if (ntohs(header->magic) != MAGIC_NUMBER) continue;

        // Kiểm tra checksum
        size_t pktLen = recvLen;
        if (!verify_checksum((uint8_t*)recvBuffer, pktLen)) {
            std::cout << "Checksum sai, bỏ qua gói ACK." << std::endl;
            continue;
        }

        uint16_t ackSeq = ntohs(header->ack);
        if (ackSeq == expectedSeq) {
            std::cout << "Nhận ACK cho seq=" << expectedSeq << std::endl;
            return true;
        }
        else {
            std::cout << "ACK không khớp, nhận" << ackSeq << ", mong đợi" << expectedSeq << std::endl;
        }


    }

}