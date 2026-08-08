#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <atomic>
#include <winsock2.h>

class UDPReceiver {
private:
    SOCKET sock;
    sockaddr_in localAddr;
    std::uint32_t expectedSeq;
    bool winsockStarted;

    bool sendAck(
        std::uint32_t ackSeq,
        std::uint8_t extraFlags,
        const sockaddr_in& clientAddr
    );

    bool isPacketValid(const std::uint8_t* packet, std::size_t len) const;
    void lingerForDuplicateFin(
        std::uint32_t finSeq,
        const sockaddr_in& clientAddr
    );

public:
    UDPReceiver();
    ~UDPReceiver();

    bool isReady() const;
    bool receiveFile(
        const std::string& savePath,
        int listenPort,
        std::atomic<int>* readyState = nullptr
    );
};

#endif
