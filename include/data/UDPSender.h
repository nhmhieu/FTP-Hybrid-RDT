#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include <cstdint>
#include <string>
#include <winsock2.h>
#include <atomic>

class UDPSender {
private:
    SOCKET sock;
    sockaddr_in destAddr;
    int timeoutMs;
    int maxRetries;
    bool winsockStarted;
    const std::atomic<bool>* cancelled;

    bool sendPacketAndWaitAck(
        const std::uint8_t* payload,
        std::size_t payloadLen,
        std::uint32_t seq,
        std::uint8_t flags
    );

    bool waitForAck(std::uint32_t expectedSeq, bool expectFinAck);

public:
    UDPSender();
    ~UDPSender();

    bool isReady() const;
    bool sendFile(const std::string& filePath, const std::string& destIP, int destPort,
        const std::atomic<bool>* cancel = nullptr);
};

#endif
