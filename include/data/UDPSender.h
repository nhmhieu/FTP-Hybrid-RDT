#ifndef UDPSENDER_H
#define UDPSENDER_H

#include <string>
#include <winsock2.h>
#include <cstdint>

class UDPSender {
private:
    SOCKET sock; //Socket UDP
    sockaddr_in destAddr; // Địa chỉ đích (IP + port)
    int timeoutMs; 
    int maxRetries;

    // Gửi một gói dữ liệu với số thứ tự seq
    bool sendPacket(const uint8_t* data, size_t len, uint16_t seq);
    // Chờ ACK cho seq đã gửi 
    bool waitForAck(uint16_t expectedSeq);

public:
    UDPSender();
    ~UDPSender();
    bool sendFile(const std::string& filePath, const std::string& destIP, int destPort);

};

#endif